#include "node_internals.h"
#include "async_wrap.h"
#include "v8-profiler.h"
#include "node_buffer.h"
#include "node_platform.h"

#if defined(_MSC_VER)
#define getpid GetCurrentProcessId
#else
#include <unistd.h>
#endif

#include <stdio.h>
#include <algorithm>

namespace node {

using v8::Context;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Message;
using v8::Private;
using v8::StackFrame;
using v8::StackTrace;
using v8::String;

IsolateData::IsolateData(Isolate* isolate,
                         uv_loop_t* event_loop,
                         MultiIsolatePlatform* platform,
                         uint32_t* zero_fill_field) :
    isolate_(isolate),
    event_loop_(event_loop),
    zero_fill_field_(zero_fill_field),
    platform_(platform) {
  if (platform_ != nullptr)
    platform_->RegisterIsolate(this, event_loop);

  // Create string and private symbol properties as internalized one byte
  // strings after the platform is properly initialized.
  //
  // Internalized because it makes property lookups a little faster and
  // because the string is created in the old space straight away.  It's going
  // to end up in the old space sooner or later anyway but now it doesn't go
  // through v8::Eternal's new space handling first.
  //
  // One byte because our strings are ASCII and we can safely skip V8's UTF-8
  // decoding step.

#define V(PropertyName, StringValue)                                        \
    PropertyName ## _.Set(                                                  \
        isolate,                                                            \
        Private::New(                                                       \
            isolate,                                                        \
            String::NewFromOneByte(                                         \
                isolate,                                                    \
                reinterpret_cast<const uint8_t*>(StringValue),              \
                v8::NewStringType::kInternalized,                           \
                sizeof(StringValue) - 1).ToLocalChecked()));
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(V)
#undef V
#define V(PropertyName, StringValue)                                        \
    PropertyName ## _.Set(                                                  \
        isolate,                                                            \
        String::NewFromOneByte(                                             \
            isolate,                                                        \
            reinterpret_cast<const uint8_t*>(StringValue),                  \
            v8::NewStringType::kInternalized,                               \
            sizeof(StringValue) - 1).ToLocalChecked());
  PER_ISOLATE_STRING_PROPERTIES(V)
#undef V
}

IsolateData::~IsolateData() {
  if (platform_ != nullptr)
    platform_->UnregisterIsolate(this);
}

Environment::Environment(IsolateData* isolate_data,
                         Local<Context> context)
    : isolate_(context->GetIsolate()),
      isolate_data_(isolate_data),
      timer_base_(uv_now(isolate_data->event_loop())),
      using_domains_(false),
      printed_error_(false),
      trace_sync_io_(false),
      abort_on_uncaught_exception_(false),
      makecallback_cntr_(0),
      scheduled_immediate_count_(isolate_, 1),
#if HAVE_INSPECTOR
      inspector_agent_(new inspector::Agent(this)),
#endif
      handle_cleanup_waiting_(0),
      http_parser_buffer_(nullptr),
      fs_stats_field_array_(nullptr),
      context_(context->GetIsolate(), context) {
  // We'll be creating new objects so make sure we've entered the context.
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(context);
  set_as_external(v8::External::New(isolate(), this));

  v8::Local<v8::Primitive> null = v8::Null(isolate());
  v8::Local<v8::Object> binding_cache_object = v8::Object::New(isolate());
  CHECK(binding_cache_object->SetPrototype(context, null).FromJust());
  set_binding_cache_object(binding_cache_object);

  v8::Local<v8::Object> internal_binding_cache_object =
      v8::Object::New(isolate());
  CHECK(internal_binding_cache_object->SetPrototype(context, null).FromJust());
  set_internal_binding_cache_object(internal_binding_cache_object);

  set_module_load_list_array(v8::Array::New(isolate()));

  AssignToContext(context);

  destroy_async_id_list_.reserve(512);
  performance_state_.reset(new performance::performance_state(isolate()));
  performance_state_->milestones[
      performance::NODE_PERFORMANCE_MILESTONE_ENVIRONMENT] =
          PERFORMANCE_NOW();
  performance_state_->milestones[
    performance::NODE_PERFORMANCE_MILESTONE_NODE_START] =
        performance::performance_node_start;
  performance_state_->milestones[
    performance::NODE_PERFORMANCE_MILESTONE_V8_START] =
        performance::performance_v8_start;
}

Environment::~Environment() {
  v8::HandleScope handle_scope(isolate());

#if HAVE_INSPECTOR
  // Destroy inspector agent before erasing the context. The inspector
  // destructor depends on the context still being accessible.
  inspector_agent_.reset();
#endif

  context()->SetAlignedPointerInEmbedderData(kContextEmbedderDataIndex,
                                             nullptr);
#define V(PropertyName, TypeName) PropertyName ## _.Reset();
  ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)
#undef V

  delete[] heap_statistics_buffer_;
  delete[] heap_space_statistics_buffer_;
  delete[] http_parser_buffer_;
}

void Environment::Start(int argc,
                        const char* const* argv,
                        int exec_argc,
                        const char* const* exec_argv,
                        bool start_profiler_idle_notifier) {
  HandleScope handle_scope(isolate());
  Context::Scope context_scope(context());

  uv_check_init(event_loop(), immediate_check_handle());
  uv_unref(reinterpret_cast<uv_handle_t*>(immediate_check_handle()));

  uv_idle_init(event_loop(), immediate_idle_handle());

  // Inform V8's CPU profiler when we're idle.  The profiler is sampling-based
  // but not all samples are created equal; mark the wall clock time spent in
  // epoll_wait() and friends so profiling tools can filter it out.  The samples
  // still end up in v8.log but with state=IDLE rather than state=EXTERNAL.
  // TODO(bnoordhuis) Depends on a libuv implementation detail that we should
  // probably fortify in the API contract, namely that the last started prepare
  // or check watcher runs first.  It's not 100% foolproof; if an add-on starts
  // a prepare or check watcher after us, any samples attributed to its callback
  // will be recorded with state=IDLE.
  uv_prepare_init(event_loop(), &idle_prepare_handle_);
  uv_check_init(event_loop(), &idle_check_handle_);
  uv_unref(reinterpret_cast<uv_handle_t*>(&idle_prepare_handle_));
  uv_unref(reinterpret_cast<uv_handle_t*>(&idle_check_handle_));

  auto close_and_finish = [](Environment* env, uv_handle_t* handle, void* arg) {
    handle->data = env;

    uv_close(handle, [](uv_handle_t* handle) {
      static_cast<Environment*>(handle->data)->FinishHandleCleanup(handle);
    });
  };

  RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(immediate_check_handle()),
      close_and_finish,
      nullptr);
  RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(immediate_idle_handle()),
      close_and_finish,
      nullptr);
  RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(&idle_prepare_handle_),
      close_and_finish,
      nullptr);
  RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(&idle_check_handle_),
      close_and_finish,
      nullptr);

  if (start_profiler_idle_notifier) {
    StartProfilerIdleNotifier();
  }

  auto process_template = FunctionTemplate::New(isolate());
  process_template->SetClassName(FIXED_ONE_BYTE_STRING(isolate(), "process"));

  auto process_object =
      process_template->GetFunction()->NewInstance(context()).ToLocalChecked();
  set_process_object(process_object);

  SetupProcessObject(this, argc, argv, exec_argc, exec_argv);
  LoadAsyncWrapperInfo(this);
}

void Environment::CleanupHandles() {
  while (HandleCleanup* hc = handle_cleanup_queue_.PopFront()) {
    handle_cleanup_waiting_++;
    hc->cb_(this, hc->handle_, hc->arg_);
    delete hc;
  }

  while (handle_cleanup_waiting_ != 0)
    uv_run(event_loop(), UV_RUN_ONCE);
}

void Environment::StartProfilerIdleNotifier() {
  uv_prepare_start(&idle_prepare_handle_, [](uv_prepare_t* handle) {
    Environment* env = ContainerOf(&Environment::idle_prepare_handle_, handle);
    env->isolate()->GetCpuProfiler()->SetIdle(true);
  });

  uv_check_start(&idle_check_handle_, [](uv_check_t* handle) {
    Environment* env = ContainerOf(&Environment::idle_check_handle_, handle);
    env->isolate()->GetCpuProfiler()->SetIdle(false);
  });
}

void Environment::StopProfilerIdleNotifier() {
  uv_prepare_stop(&idle_prepare_handle_);
  uv_check_stop(&idle_check_handle_);
}

void Environment::PrintSyncTrace() const {
  if (!trace_sync_io_)
    return;

  HandleScope handle_scope(isolate());
  Local<v8::StackTrace> stack =
      StackTrace::CurrentStackTrace(isolate(), 10, StackTrace::kDetailed);

  fprintf(stderr, "(node:%u) WARNING: Detected use of sync API\n",
          uv_os_getpid());

  for (int i = 0; i < stack->GetFrameCount() - 1; i++) {
    Local<StackFrame> stack_frame = stack->GetFrame(i);
    node::Utf8Value fn_name_s(isolate(), stack_frame->GetFunctionName());
    node::Utf8Value script_name(isolate(), stack_frame->GetScriptName());
    const int line_number = stack_frame->GetLineNumber();
    const int column = stack_frame->GetColumn();

    if (stack_frame->IsEval()) {
      if (stack_frame->GetScriptId() == Message::kNoScriptIdInfo) {
        fprintf(stderr, "    at [eval]:%i:%i\n", line_number, column);
      } else {
        fprintf(stderr,
                "    at [eval] (%s:%i:%i)\n",
                *script_name,
                line_number,
                column);
      }
      break;
    }

    if (fn_name_s.length() == 0) {
      fprintf(stderr, "    at %s:%i:%i\n", *script_name, line_number, column);
    } else {
      fprintf(stderr,
              "    at %s (%s:%i:%i)\n",
              *fn_name_s,
              *script_name,
              line_number,
              column);
    }
  }
  fflush(stderr);
}

void Environment::RunCleanup() {
  while (!cleanup_hooks_.empty()) {
    // Copy into a vector, since we can't sort an unordered_set in-place.
    std::vector<CleanupHookCallback> callbacks(
        cleanup_hooks_.begin(), cleanup_hooks_.end());
    // We can't erase the copied elements from `cleanup_hooks_` yet, because we
    // need to be able to check whether they were un-scheduled by another hook.

    std::sort(callbacks.begin(), callbacks.end(),
              [](const CleanupHookCallback& a, const CleanupHookCallback& b) {
      // Sort in descending order so that the most recently inserted callbacks
      // are run first.
      return a.insertion_order_counter_ > b.insertion_order_counter_;
    });

    for (const CleanupHookCallback& cb : callbacks) {
      if (cleanup_hooks_.count(cb) == 0) {
        // This hook was removed from the `cleanup_hooks_` set during another
        // hook that was run earlier. Nothing to do here.
        continue;
      }

      cb.fn_(cb.arg_);
      cleanup_hooks_.erase(cb);
      CleanupHandles();
    }
  }
}

void Environment::RunAtExitCallbacks() {
  for (AtExitCallback at_exit : at_exit_functions_) {
    at_exit.cb_(at_exit.arg_);
  }
  at_exit_functions_.clear();
}

void Environment::AtExit(void (*cb)(void* arg), void* arg) {
  at_exit_functions_.push_back(AtExitCallback{cb, arg});
}

void Environment::AddPromiseHook(promise_hook_func fn, void* arg) {
  auto it = std::find_if(
      promise_hooks_.begin(), promise_hooks_.end(),
      [&](const PromiseHookCallback& hook) {
        return hook.cb_ == fn && hook.arg_ == arg;
      });
  if (it != promise_hooks_.end()) {
    it->enable_count_++;
    return;
  }
  promise_hooks_.push_back(PromiseHookCallback{fn, arg, 1});

  if (promise_hooks_.size() == 1) {
    isolate_->SetPromiseHook(EnvPromiseHook);
  }
}

bool Environment::RemovePromiseHook(promise_hook_func fn, void* arg) {
  auto it = std::find_if(
      promise_hooks_.begin(), promise_hooks_.end(),
      [&](const PromiseHookCallback& hook) {
        return hook.cb_ == fn && hook.arg_ == arg;
      });

  if (it == promise_hooks_.end()) return false;

  if (--it->enable_count_ > 0) return true;

  promise_hooks_.erase(it);
  if (promise_hooks_.empty()) {
    isolate_->SetPromiseHook(nullptr);
  }

  return true;
}

void Environment::EnvPromiseHook(v8::PromiseHookType type,
                                 v8::Local<v8::Promise> promise,
                                 v8::Local<v8::Value> parent) {
  Environment* env = Environment::GetCurrent(promise->CreationContext());
  for (const PromiseHookCallback& hook : env->promise_hooks_) {
    hook.cb_(type, promise, parent, hook.arg_);
  }
}

void Environment::RunAndClearNativeImmediates() {
  size_t count = native_immediate_callbacks_.size();
  if (count > 0) {
    std::vector<NativeImmediateCallback> list;
    native_immediate_callbacks_.swap(list);
    for (const auto& cb : list) {
      cb.cb_(this, cb.data_);
      if (cb.keep_alive_)
        cb.keep_alive_->Reset();
    }

#ifdef DEBUG
    CHECK_GE(scheduled_immediate_count_[0], count);
#endif
    scheduled_immediate_count_[0] = scheduled_immediate_count_[0] - count;
  }
}

static bool MaybeStopImmediate(Environment* env) {
  if (env->scheduled_immediate_count()[0] == 0) {
    uv_check_stop(env->immediate_check_handle());
    uv_idle_stop(env->immediate_idle_handle());
    return true;
  }
  return false;
}


void Environment::CheckImmediate(uv_check_t* handle) {
  Environment* env = Environment::from_immediate_check_handle(handle);
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());

  if (MaybeStopImmediate(env))
    return;

  env->RunAndClearNativeImmediates();

  MakeCallback(env->isolate(),
               env->process_object(),
               env->immediate_callback_string(),
               0,
               nullptr,
               {0, 0}).ToLocalChecked();

  MaybeStopImmediate(env);
}

void Environment::ActivateImmediateCheck() {
  uv_check_start(&immediate_check_handle_, CheckImmediate);
  // Idle handle is needed only to stop the event loop from blocking in poll.
  uv_idle_start(&immediate_idle_handle_, [](uv_idle_t*){ });
}


void Environment::AsyncHooks::grow_async_ids_stack() {
  const uint32_t old_capacity = async_ids_stack_.Length() / 2;
  const uint32_t new_capacity = old_capacity * 1.5;
  AliasedBuffer<double, v8::Float64Array> new_buffer(
      env()->isolate(), new_capacity * 2);

  for (uint32_t i = 0; i < old_capacity * 2; ++i)
    new_buffer[i] = async_ids_stack_[i];
  async_ids_stack_ = std::move(new_buffer);

  env()->async_hooks_binding()->Set(
      env()->context(),
      env()->async_ids_stack_string(),
      async_ids_stack_.GetJSArray()).FromJust();
}

}  // namespace node
