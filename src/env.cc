#include "env.h"
#include "allocated_buffer-inl.h"
#include "async_wrap.h"
#include "base_object-inl.h"
#include "debug_utils-inl.h"
#include "memory_tracker-inl.h"
#include "node_buffer.h"
#include "node_context_data.h"
#include "node_errors.h"
#include "node_internals.h"
#include "node_options-inl.h"
#include "node_process.h"
#include "node_v8_platform-inl.h"
#include "node_worker.h"
#include "req_wrap-inl.h"
#include "stream_base.h"
#include "tracing/agent.h"
#include "tracing/traced_value.h"
#include "util-inl.h"
#include "v8-profiler.h"

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <cstdio>
#include <iostream>
#include <memory>

namespace node {

using errors::TryCatchScope;
using v8::Boolean;
using v8::Context;
using v8::EmbedderGraph;
using v8::Function;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Number;
using v8::Object;
using v8::Private;
using v8::Script;
using v8::SnapshotCreator;
using v8::StackTrace;
using v8::String;
using v8::Symbol;
using v8::TracingController;
using v8::TryCatch;
using v8::Undefined;
using v8::Value;
using worker::Worker;

int const Environment::kNodeContextTag = 0x6e6f64;
void* const Environment::kNodeContextTagPtr = const_cast<void*>(
    static_cast<const void*>(&Environment::kNodeContextTag));

std::vector<size_t> IsolateData::Serialize(SnapshotCreator* creator) {
  Isolate* isolate = creator->GetIsolate();
  std::vector<size_t> indexes;
  HandleScope handle_scope(isolate);
  // XXX(joyeecheung): technically speaking, the indexes here should be
  // consecutive and we could just return a range instead of an array,
  // but that's not part of the V8 API contract so we use an array
  // just to be safe.

#define VP(PropertyName, StringValue) V(Private, PropertyName)
#define VY(PropertyName, StringValue) V(Symbol, PropertyName)
#define VS(PropertyName, StringValue) V(String, PropertyName)
#define V(TypeName, PropertyName)                                              \
  indexes.push_back(creator->AddData(PropertyName##_.Get(isolate)));
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
  PER_ISOLATE_SYMBOL_PROPERTIES(VY)
  PER_ISOLATE_STRING_PROPERTIES(VS)
#undef V
#undef VY
#undef VS
#undef VP
  for (size_t i = 0; i < AsyncWrap::PROVIDERS_LENGTH; i++)
    indexes.push_back(creator->AddData(async_wrap_provider(i)));

  return indexes;
}

void IsolateData::DeserializeProperties(const std::vector<size_t>* indexes) {
  size_t i = 0;
  HandleScope handle_scope(isolate_);

#define VP(PropertyName, StringValue) V(Private, PropertyName)
#define VY(PropertyName, StringValue) V(Symbol, PropertyName)
#define VS(PropertyName, StringValue) V(String, PropertyName)
#define V(TypeName, PropertyName)                                              \
  do {                                                                         \
    MaybeLocal<TypeName> field =                                               \
        isolate_->GetDataFromSnapshotOnce<TypeName>((*indexes)[i++]);          \
    if (field.IsEmpty()) {                                                     \
      fprintf(stderr, "Failed to deserialize " #PropertyName "\n");            \
    }                                                                          \
    PropertyName##_.Set(isolate_, field.ToLocalChecked());                     \
  } while (0);
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
  PER_ISOLATE_SYMBOL_PROPERTIES(VY)
  PER_ISOLATE_STRING_PROPERTIES(VS)
#undef V
#undef VY
#undef VS
#undef VP

  for (size_t j = 0; j < AsyncWrap::PROVIDERS_LENGTH; j++) {
    MaybeLocal<String> field =
        isolate_->GetDataFromSnapshotOnce<String>((*indexes)[i++]);
    if (field.IsEmpty()) {
      fprintf(stderr, "Failed to deserialize AsyncWrap provider %zu\n", j);
    }
    async_wrap_providers_[j].Set(isolate_, field.ToLocalChecked());
  }
}

void IsolateData::CreateProperties() {
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

  HandleScope handle_scope(isolate_);

#define V(PropertyName, StringValue)                                           \
  PropertyName##_.Set(                                                         \
      isolate_,                                                                \
      Private::New(isolate_,                                                   \
                   String::NewFromOneByte(                                     \
                       isolate_,                                               \
                       reinterpret_cast<const uint8_t*>(StringValue),          \
                       NewStringType::kInternalized,                           \
                       sizeof(StringValue) - 1)                                \
                       .ToLocalChecked()));
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(V)
#undef V
#define V(PropertyName, StringValue)                                           \
  PropertyName##_.Set(                                                         \
      isolate_,                                                                \
      Symbol::New(isolate_,                                                    \
                  String::NewFromOneByte(                                      \
                      isolate_,                                                \
                      reinterpret_cast<const uint8_t*>(StringValue),           \
                      NewStringType::kInternalized,                            \
                      sizeof(StringValue) - 1)                                 \
                      .ToLocalChecked()));
  PER_ISOLATE_SYMBOL_PROPERTIES(V)
#undef V
#define V(PropertyName, StringValue)                                           \
  PropertyName##_.Set(                                                         \
      isolate_,                                                                \
      String::NewFromOneByte(isolate_,                                         \
                             reinterpret_cast<const uint8_t*>(StringValue),    \
                             NewStringType::kInternalized,                     \
                             sizeof(StringValue) - 1)                          \
          .ToLocalChecked());
  PER_ISOLATE_STRING_PROPERTIES(V)
#undef V

  // Create all the provider strings that will be passed to JS. Place them in
  // an array so the array index matches the PROVIDER id offset. This way the
  // strings can be retrieved quickly.
#define V(Provider)                                                           \
  async_wrap_providers_[AsyncWrap::PROVIDER_ ## Provider].Set(                \
      isolate_,                                                               \
      String::NewFromOneByte(                                                 \
        isolate_,                                                             \
        reinterpret_cast<const uint8_t*>(#Provider),                          \
        NewStringType::kInternalized,                                         \
        sizeof(#Provider) - 1).ToLocalChecked());
  NODE_ASYNC_PROVIDER_TYPES(V)
#undef V
}

IsolateData::IsolateData(Isolate* isolate,
                         uv_loop_t* event_loop,
                         MultiIsolatePlatform* platform,
                         ArrayBufferAllocator* node_allocator,
                         const std::vector<size_t>* indexes)
    : isolate_(isolate),
      event_loop_(event_loop),
      node_allocator_(node_allocator == nullptr ? nullptr
                                                : node_allocator->GetImpl()),
      platform_(platform) {
  options_.reset(
      new PerIsolateOptions(*(per_process::cli_options->per_isolate)));

  if (indexes == nullptr) {
    CreateProperties();
  } else {
    DeserializeProperties(indexes);
  }
}

void IsolateData::MemoryInfo(MemoryTracker* tracker) const {
#define V(PropertyName, StringValue)                                           \
  tracker->TrackField(#PropertyName, PropertyName());
  PER_ISOLATE_SYMBOL_PROPERTIES(V)
#undef V

#define V(PropertyName, StringValue)                                           \
  tracker->TrackField(#PropertyName, PropertyName());
  PER_ISOLATE_STRING_PROPERTIES(V)
#undef V

  tracker->TrackField("async_wrap_providers", async_wrap_providers_);

  if (node_allocator_ != nullptr) {
    tracker->TrackFieldWithSize(
        "node_allocator", sizeof(*node_allocator_), "NodeArrayBufferAllocator");
  }
  tracker->TrackFieldWithSize(
      "platform", sizeof(*platform_), "MultiIsolatePlatform");
  // TODO(joyeecheung): implement MemoryRetainer in the option classes.
}

void InitThreadLocalOnce() {
  CHECK_EQ(0, uv_key_create(&Environment::thread_local_env));
}

void TrackingTraceStateObserver::UpdateTraceCategoryState() {
  if (!env_->owns_process_state() || !env_->can_call_into_js()) {
    // Ideally, we’d have a consistent story that treats all threads/Environment
    // instances equally here. However, tracing is essentially global, and this
    // callback is called from whichever thread calls `StartTracing()` or
    // `StopTracing()`. The only way to do this in a threadsafe fashion
    // seems to be only tracking this from the main thread, and only allowing
    // these state modifications from the main thread.
    return;
  }

  bool async_hooks_enabled = (*(TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
                                 TRACING_CATEGORY_NODE1(async_hooks)))) != 0;

  Isolate* isolate = env_->isolate();
  HandleScope handle_scope(isolate);
  Local<Function> cb = env_->trace_category_state_function();
  if (cb.IsEmpty())
    return;
  TryCatchScope try_catch(env_);
  try_catch.SetVerbose(true);
  Local<Value> args[] = {Boolean::New(isolate, async_hooks_enabled)};
  USE(cb->Call(env_->context(), Undefined(isolate), arraysize(args), args));
}

void Environment::CreateProperties() {
  HandleScope handle_scope(isolate_);
  Local<Context> ctx = context();

  {
    Context::Scope context_scope(ctx);
    Local<FunctionTemplate> templ = FunctionTemplate::New(isolate());
    templ->InstanceTemplate()->SetInternalFieldCount(
        BaseObject::kInternalFieldCount);
    templ->Inherit(BaseObject::GetConstructorTemplate(this));

    set_binding_data_ctor_template(templ);
  }

  // Store primordials setup by the per-context script in the environment.
  Local<Object> per_context_bindings =
      GetPerContextExports(ctx).ToLocalChecked();
  Local<Value> primordials =
      per_context_bindings->Get(ctx, primordials_string()).ToLocalChecked();
  CHECK(primordials->IsObject());
  set_primordials(primordials.As<Object>());

  Local<Object> process_object =
      node::CreateProcessObject(this).FromMaybe(Local<Object>());
  set_process_object(process_object);
}

std::string GetExecPath(const std::vector<std::string>& argv) {
  char exec_path_buf[2 * PATH_MAX];
  size_t exec_path_len = sizeof(exec_path_buf);
  std::string exec_path;
  if (uv_exepath(exec_path_buf, &exec_path_len) == 0) {
    exec_path = std::string(exec_path_buf, exec_path_len);
  } else {
    exec_path = argv[0];
  }

  // On OpenBSD process.execPath will be relative unless we
  // get the full path before process.execPath is used.
#if defined(__OpenBSD__)
  uv_fs_t req;
  req.ptr = nullptr;
  if (0 ==
      uv_fs_realpath(nullptr, &req, exec_path.c_str(), nullptr)) {
    CHECK_NOT_NULL(req.ptr);
    exec_path = std::string(static_cast<char*>(req.ptr));
  }
  uv_fs_req_cleanup(&req);
#endif

  return exec_path;
}

Environment::Environment(IsolateData* isolate_data,
                         Isolate* isolate,
                         const std::vector<std::string>& args,
                         const std::vector<std::string>& exec_args,
                         const EnvSerializeInfo* env_info,
                         EnvironmentFlags::Flags flags,
                         ThreadId thread_id)
    : isolate_(isolate),
      isolate_data_(isolate_data),
      async_hooks_(isolate, MAYBE_FIELD_PTR(env_info, async_hooks)),
      immediate_info_(isolate, MAYBE_FIELD_PTR(env_info, immediate_info)),
      tick_info_(isolate, MAYBE_FIELD_PTR(env_info, tick_info)),
      timer_base_(uv_now(isolate_data->event_loop())),
      exec_argv_(exec_args),
      argv_(args),
      exec_path_(GetExecPath(args)),
      should_abort_on_uncaught_toggle_(
          isolate_,
          1,
          MAYBE_FIELD_PTR(env_info, should_abort_on_uncaught_toggle)),
      stream_base_state_(isolate_,
                         StreamBase::kNumStreamBaseStateFields,
                         MAYBE_FIELD_PTR(env_info, stream_base_state)),
      environment_start_time_(PERFORMANCE_NOW()),
      flags_(flags),
      thread_id_(thread_id.id == static_cast<uint64_t>(-1)
                     ? AllocateEnvironmentThreadId().id
                     : thread_id.id) {
  // We'll be creating new objects so make sure we've entered the context.
  HandleScope handle_scope(isolate);

  // Set some flags if only kDefaultFlags was passed. This can make API version
  // transitions easier for embedders.
  if (flags_ & EnvironmentFlags::kDefaultFlags) {
    flags_ = flags_ |
        EnvironmentFlags::kOwnsProcessState |
        EnvironmentFlags::kOwnsInspector;
  }

  set_env_vars(per_process::system_environment);
  // TODO(joyeecheung): pass Isolate* and env_vars to it instead of the entire
  // env
  enabled_debug_list_.Parse(this);

  // We create new copies of the per-Environment option sets, so that it is
  // easier to modify them after Environment creation. The defaults are
  // part of the per-Isolate option set, for which in turn the defaults are
  // part of the per-process option set.
  options_.reset(new EnvironmentOptions(*isolate_data->options()->per_env));
  inspector_host_port_.reset(
      new ExclusiveAccess<HostPort>(options_->debug_options().host_port));

  if (flags & EnvironmentFlags::kOwnsProcessState) {
    set_abort_on_uncaught_exception(false);
  }

#if HAVE_INSPECTOR
  // We can only create the inspector agent after having cloned the options.
  inspector_agent_ = std::make_unique<inspector::Agent>(this);
#endif

  static uv_once_t init_once = UV_ONCE_INIT;
  uv_once(&init_once, InitThreadLocalOnce);
  uv_key_set(&thread_local_env, this);

  if (tracing::AgentWriterHandle* writer = GetTracingAgentWriter()) {
    trace_state_observer_ = std::make_unique<TrackingTraceStateObserver>(this);
    if (TracingController* tracing_controller = writer->GetTracingController())
      tracing_controller->AddTraceStateObserver(trace_state_observer_.get());
  }

  destroy_async_id_list_.reserve(512);

  performance_state_ = std::make_unique<performance::PerformanceState>(
      isolate, MAYBE_FIELD_PTR(env_info, performance_state));

  if (*TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(
          TRACING_CATEGORY_NODE1(environment)) != 0) {
    auto traced_value = tracing::TracedValue::Create();
    traced_value->BeginArray("args");
    for (const std::string& arg : args) traced_value->AppendString(arg);
    traced_value->EndArray();
    traced_value->BeginArray("exec_args");
    for (const std::string& arg : exec_args) traced_value->AppendString(arg);
    traced_value->EndArray();
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(TRACING_CATEGORY_NODE1(environment),
                                      "Environment",
                                      this,
                                      "args",
                                      std::move(traced_value));
  }

  // This adjusts the return value of base_object_count() so that tests that
  // check the count do not have to account for internally created BaseObjects.
  initial_base_object_count_ = base_object_count();
}

Environment::Environment(IsolateData* isolate_data,
                         Local<Context> context,
                         const std::vector<std::string>& args,
                         const std::vector<std::string>& exec_args,
                         const EnvSerializeInfo* env_info,
                         EnvironmentFlags::Flags flags,
                         ThreadId thread_id)
    : Environment(isolate_data,
                  context->GetIsolate(),
                  args,
                  exec_args,
                  env_info,
                  flags,
                  thread_id) {
  InitializeMainContext(context, env_info);
}

void Environment::InitializeMainContext(Local<Context> context,
                                        const EnvSerializeInfo* env_info) {
  context_.Reset(context->GetIsolate(), context);
  AssignToContext(context, ContextInfo(""));
  if (env_info != nullptr) {
    DeserializeProperties(env_info);
  } else {
    CreateProperties();
  }

  if (options_->no_force_async_hooks_checks) {
    async_hooks_.no_force_checks();
  }

  // By default, always abort when --abort-on-uncaught-exception was passed.
  should_abort_on_uncaught_toggle_[0] = 1;

  performance_state_->Mark(performance::NODE_PERFORMANCE_MILESTONE_ENVIRONMENT,
                           environment_start_time_);
  performance_state_->Mark(performance::NODE_PERFORMANCE_MILESTONE_NODE_START,
                           per_process::node_start_time);
  performance_state_->Mark(performance::NODE_PERFORMANCE_MILESTONE_V8_START,
                           performance::performance_v8_start);

  // This adjusts the return value of base_object_count() so that tests that
  // check the count do not have to account for internally created BaseObjects.
  initial_base_object_count_ = base_object_count();
}

Environment::~Environment() {
  if (Environment** interrupt_data = interrupt_data_.load()) {
    // There are pending RequestInterrupt() callbacks. Tell them not to run,
    // then force V8 to run interrupts by compiling and running an empty script
    // so as not to leak memory.
    *interrupt_data = nullptr;

    Isolate::AllowJavascriptExecutionScope allow_js_here(isolate());
    HandleScope handle_scope(isolate());
    TryCatch try_catch(isolate());
    Context::Scope context_scope(context());

#ifdef DEBUG
    bool consistency_check = false;
    isolate()->RequestInterrupt([](Isolate*, void* data) {
      *static_cast<bool*>(data) = true;
    }, &consistency_check);
#endif

    Local<Script> script;
    if (Script::Compile(context(), String::Empty(isolate())).ToLocal(&script))
      USE(script->Run(context()));

    DCHECK(consistency_check);
  }

  // FreeEnvironment() should have set this.
  CHECK(is_stopping());

  isolate()->GetHeapProfiler()->RemoveBuildEmbedderGraphCallback(
      BuildEmbedderGraph, this);

  HandleScope handle_scope(isolate());

#if HAVE_INSPECTOR
  // Destroy inspector agent before erasing the context. The inspector
  // destructor depends on the context still being accessible.
  inspector_agent_.reset();
#endif

  context()->SetAlignedPointerInEmbedderData(ContextEmbedderIndex::kEnvironment,
                                             nullptr);

  if (trace_state_observer_) {
    tracing::AgentWriterHandle* writer = GetTracingAgentWriter();
    CHECK_NOT_NULL(writer);
    if (TracingController* tracing_controller = writer->GetTracingController())
      tracing_controller->RemoveTraceStateObserver(trace_state_observer_.get());
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0(
    TRACING_CATEGORY_NODE1(environment), "Environment", this);

  // Do not unload addons on the main thread. Some addons need to retain memory
  // beyond the Environment's lifetime, and unloading them early would break
  // them; with Worker threads, we have the opportunity to be stricter.
  // Also, since the main thread usually stops just before the process exits,
  // this is far less relevant here.
  if (!is_main_thread()) {
    // Dereference all addons that were loaded into this environment.
    for (binding::DLib& addon : loaded_addons_) {
      addon.Close();
    }
  }

  CHECK_EQ(base_object_count_, 0);
}

void Environment::InitializeLibuv() {
  HandleScope handle_scope(isolate());
  Context::Scope context_scope(context());

  CHECK_EQ(0, uv_timer_init(event_loop(), timer_handle()));
  uv_unref(reinterpret_cast<uv_handle_t*>(timer_handle()));

  uv_check_init(event_loop(), immediate_check_handle());
  uv_unref(reinterpret_cast<uv_handle_t*>(immediate_check_handle()));

  uv_idle_init(event_loop(), immediate_idle_handle());

  uv_check_start(immediate_check_handle(), CheckImmediate);

  uv_async_init(
      event_loop(),
      &task_queues_async_,
      [](uv_async_t* async) {
        Environment* env = ContainerOf(
            &Environment::task_queues_async_, async);
        env->RunAndClearNativeImmediates();
      });
  uv_unref(reinterpret_cast<uv_handle_t*>(&task_queues_async_));

  {
    Mutex::ScopedLock lock(native_immediates_threadsafe_mutex_);
    task_queues_async_initialized_ = true;
    if (native_immediates_threadsafe_.size() > 0 ||
        native_immediates_interrupts_.size() > 0) {
      uv_async_send(&task_queues_async_);
    }
  }

  // Register clean-up cb to be called to clean up the handles
  // when the environment is freed, note that they are not cleaned in
  // the one environment per process setup, but will be called in
  // FreeEnvironment.
  RegisterHandleCleanups();
}

void Environment::ExitEnv() {
  set_can_call_into_js(false);
  set_stopping(true);
  isolate_->TerminateExecution();
  SetImmediateThreadsafe([](Environment* env) { uv_stop(env->event_loop()); });
}

void Environment::RegisterHandleCleanups() {
  HandleCleanupCb close_and_finish = [](Environment* env, uv_handle_t* handle,
                                        void* arg) {
    handle->data = env;

    env->CloseHandle(handle, [](uv_handle_t* handle) {
#ifdef DEBUG
      memset(handle, 0xab, uv_handle_size(handle->type));
#endif
    });
  };

  auto register_handle = [&](uv_handle_t* handle) {
    RegisterHandleCleanup(handle, close_and_finish, nullptr);
  };
  register_handle(reinterpret_cast<uv_handle_t*>(timer_handle()));
  register_handle(reinterpret_cast<uv_handle_t*>(immediate_check_handle()));
  register_handle(reinterpret_cast<uv_handle_t*>(immediate_idle_handle()));
  register_handle(reinterpret_cast<uv_handle_t*>(&task_queues_async_));
}

void Environment::CleanupHandles() {
  {
    Mutex::ScopedLock lock(native_immediates_threadsafe_mutex_);
    task_queues_async_initialized_ = false;
  }

  Isolate::DisallowJavascriptExecutionScope disallow_js(isolate(),
      Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);

  RunAndClearNativeImmediates(true /* skip unrefed SetImmediate()s */);

  for (ReqWrapBase* request : req_wrap_queue_)
    request->Cancel();

  for (HandleWrap* handle : handle_wrap_queue_)
    handle->Close();

  for (HandleCleanup& hc : handle_cleanup_queue_)
    hc.cb_(this, hc.handle_, hc.arg_);
  handle_cleanup_queue_.clear();

  while (handle_cleanup_waiting_ != 0 ||
         request_waiting_ != 0 ||
         !handle_wrap_queue_.IsEmpty()) {
    uv_run(event_loop(), UV_RUN_ONCE);
  }
}

void Environment::PrintSyncTrace() const {
  if (!trace_sync_io_) return;

  HandleScope handle_scope(isolate());

  fprintf(
      stderr, "(node:%d) WARNING: Detected use of sync API\n", uv_os_getpid());
  PrintStackTrace(isolate(),
                  StackTrace::CurrentStackTrace(
                      isolate(), stack_trace_limit(), StackTrace::kDetailed));
}

void Environment::RunCleanup() {
  started_cleanup_ = true;
  TraceEventScope trace_scope(TRACING_CATEGORY_NODE1(environment),
                              "RunCleanup", this);
  bindings_.clear();
  initial_base_object_count_ = 0;
  CleanupHandles();

  while (!cleanup_hooks_.empty() ||
         native_immediates_.size() > 0 ||
         native_immediates_threadsafe_.size() > 0 ||
         native_immediates_interrupts_.size() > 0) {
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
    }
    CleanupHandles();
  }

  for (const int fd : unmanaged_fds_) {
    uv_fs_t close_req;
    uv_fs_close(nullptr, &close_req, fd, nullptr);
    uv_fs_req_cleanup(&close_req);
  }
}

void Environment::RunAtExitCallbacks() {
  TraceEventScope trace_scope(TRACING_CATEGORY_NODE1(environment),
                              "AtExit", this);
  for (ExitCallback at_exit : at_exit_functions_) {
    at_exit.cb_(at_exit.arg_);
  }
  at_exit_functions_.clear();
}

void Environment::AtExit(void (*cb)(void* arg), void* arg) {
  at_exit_functions_.push_front(ExitCallback{cb, arg});
}

void Environment::RunAndClearInterrupts() {
  while (native_immediates_interrupts_.size() > 0) {
    NativeImmediateQueue queue;
    {
      Mutex::ScopedLock lock(native_immediates_threadsafe_mutex_);
      queue.ConcatMove(std::move(native_immediates_interrupts_));
    }
    DebugSealHandleScope seal_handle_scope(isolate());

    while (auto head = queue.Shift())
      head->Call(this);
  }
}

void Environment::RunAndClearNativeImmediates(bool only_refed) {
  TraceEventScope trace_scope(TRACING_CATEGORY_NODE1(environment),
                              "RunAndClearNativeImmediates", this);
  HandleScope handle_scope(isolate_);
  InternalCallbackScope cb_scope(this, Object::New(isolate_), { 0, 0 });

  size_t ref_count = 0;

  // Handle interrupts first. These functions are not allowed to throw
  // exceptions, so we do not need to handle that.
  RunAndClearInterrupts();

  auto drain_list = [&](NativeImmediateQueue* queue) {
    TryCatchScope try_catch(this);
    DebugSealHandleScope seal_handle_scope(isolate());
    while (auto head = queue->Shift()) {
      bool is_refed = head->flags() & CallbackFlags::kRefed;
      if (is_refed)
        ref_count++;

      if (is_refed || !only_refed)
        head->Call(this);

      head.reset();  // Destroy now so that this is also observed by try_catch.

      if (UNLIKELY(try_catch.HasCaught())) {
        if (!try_catch.HasTerminated() && can_call_into_js())
          errors::TriggerUncaughtException(isolate(), try_catch);

        return true;
      }
    }
    return false;
  };
  while (drain_list(&native_immediates_)) {}

  immediate_info()->ref_count_dec(ref_count);

  if (immediate_info()->ref_count() == 0)
    ToggleImmediateRef(false);

  // It is safe to check .size() first, because there is a causal relationship
  // between pushes to the threadsafe immediate list and this function being
  // called. For the common case, it's worth checking the size first before
  // establishing a mutex lock.
  // This is intentionally placed after the `ref_count` handling, because when
  // refed threadsafe immediates are created, they are not counted towards the
  // count in immediate_info() either.
  NativeImmediateQueue threadsafe_immediates;
  if (native_immediates_threadsafe_.size() > 0) {
    Mutex::ScopedLock lock(native_immediates_threadsafe_mutex_);
    threadsafe_immediates.ConcatMove(std::move(native_immediates_threadsafe_));
  }
  while (drain_list(&threadsafe_immediates)) {}
}

void Environment::RequestInterruptFromV8() {
  // The Isolate may outlive the Environment, so some logic to handle the
  // situation in which the Environment is destroyed before the handler runs
  // is required.

  // We allocate a new pointer to a pointer to this Environment instance, and
  // try to set it as interrupt_data_. If interrupt_data_ was already set, then
  // callbacks are already scheduled to run and we can delete our own pointer
  // and just return. If it was nullptr previously, the Environment** is stored;
  // ~Environment sets the Environment* contained in it to nullptr, so that
  // the callback can check whether ~Environment has already run and it is thus
  // not safe to access the Environment instance itself.
  Environment** interrupt_data = new Environment*(this);
  Environment** dummy = nullptr;
  if (!interrupt_data_.compare_exchange_strong(dummy, interrupt_data)) {
    delete interrupt_data;
    return;  // Already scheduled.
  }

  isolate()->RequestInterrupt([](Isolate* isolate, void* data) {
    std::unique_ptr<Environment*> env_ptr { static_cast<Environment**>(data) };
    Environment* env = *env_ptr;
    if (env == nullptr) {
      // The Environment has already been destroyed. That should be okay; any
      // callback added before the Environment shuts down would have been
      // handled during cleanup.
      return;
    }
    env->interrupt_data_.store(nullptr);
    env->RunAndClearInterrupts();
  }, interrupt_data);
}

void Environment::ScheduleTimer(int64_t duration_ms) {
  if (started_cleanup_) return;
  uv_timer_start(timer_handle(), RunTimers, duration_ms, 0);
}

void Environment::ToggleTimerRef(bool ref) {
  if (started_cleanup_) return;

  if (ref) {
    uv_ref(reinterpret_cast<uv_handle_t*>(timer_handle()));
  } else {
    uv_unref(reinterpret_cast<uv_handle_t*>(timer_handle()));
  }
}

void Environment::RunTimers(uv_timer_t* handle) {
  Environment* env = Environment::from_timer_handle(handle);
  TraceEventScope trace_scope(TRACING_CATEGORY_NODE1(environment),
                              "RunTimers", env);

  if (!env->can_call_into_js())
    return;

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Object> process = env->process_object();
  InternalCallbackScope scope(env, process, {0, 0});

  Local<Function> cb = env->timers_callback_function();
  MaybeLocal<Value> ret;
  Local<Value> arg = env->GetNow();
  // This code will loop until all currently due timers will process. It is
  // impossible for us to end up in an infinite loop due to how the JS-side
  // is structured.
  do {
    TryCatchScope try_catch(env);
    try_catch.SetVerbose(true);
    ret = cb->Call(env->context(), process, 1, &arg);
  } while (ret.IsEmpty() && env->can_call_into_js());

  // NOTE(apapirovski): If it ever becomes possible that `call_into_js` above
  // is reset back to `true` after being previously set to `false` then this
  // code becomes invalid and needs to be rewritten. Otherwise catastrophic
  // timers corruption will occur and all timers behaviour will become
  // entirely unpredictable.
  if (ret.IsEmpty())
    return;

  // To allow for less JS-C++ boundary crossing, the value returned from JS
  // serves a few purposes:
  // 1. If it's 0, no more timers exist and the handle should be unrefed
  // 2. If it's > 0, the value represents the next timer's expiry and there
  //    is at least one timer remaining that is refed.
  // 3. If it's < 0, the absolute value represents the next timer's expiry
  //    and there are no timers that are refed.
  int64_t expiry_ms =
      ret.ToLocalChecked()->IntegerValue(env->context()).FromJust();

  uv_handle_t* h = reinterpret_cast<uv_handle_t*>(handle);

  if (expiry_ms != 0) {
    int64_t duration_ms =
        llabs(expiry_ms) - (uv_now(env->event_loop()) - env->timer_base());

    env->ScheduleTimer(duration_ms > 0 ? duration_ms : 1);

    if (expiry_ms > 0)
      uv_ref(h);
    else
      uv_unref(h);
  } else {
    uv_unref(h);
  }
}


void Environment::CheckImmediate(uv_check_t* handle) {
  Environment* env = Environment::from_immediate_check_handle(handle);
  TraceEventScope trace_scope(TRACING_CATEGORY_NODE1(environment),
                              "CheckImmediate", env);

  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());

  env->RunAndClearNativeImmediates();

  if (env->immediate_info()->count() == 0 || !env->can_call_into_js())
    return;

  do {
    MakeCallback(env->isolate(),
                 env->process_object(),
                 env->immediate_callback_function(),
                 0,
                 nullptr,
                 {0, 0}).ToLocalChecked();
  } while (env->immediate_info()->has_outstanding() && env->can_call_into_js());

  if (env->immediate_info()->ref_count() == 0)
    env->ToggleImmediateRef(false);
}

void Environment::ToggleImmediateRef(bool ref) {
  if (started_cleanup_) return;

  if (ref) {
    // Idle handle is needed only to stop the event loop from blocking in poll.
    uv_idle_start(immediate_idle_handle(), [](uv_idle_t*){ });
  } else {
    uv_idle_stop(immediate_idle_handle());
  }
}


Local<Value> Environment::GetNow() {
  uv_update_time(event_loop());
  uint64_t now = uv_now(event_loop());
  CHECK_GE(now, timer_base());
  now -= timer_base();
  if (now <= 0xffffffff)
    return Integer::NewFromUnsigned(isolate(), static_cast<uint32_t>(now));
  else
    return Number::New(isolate(), static_cast<double>(now));
}

void CollectExceptionInfo(Environment* env,
                          Local<Object> obj,
                          int errorno,
                          const char* err_string,
                          const char* syscall,
                          const char* message,
                          const char* path,
                          const char* dest) {
  obj->Set(env->context(),
           env->errno_string(),
           Integer::New(env->isolate(), errorno)).Check();

  obj->Set(env->context(), env->code_string(),
           OneByteString(env->isolate(), err_string)).Check();

  if (message != nullptr) {
    obj->Set(env->context(), env->message_string(),
             OneByteString(env->isolate(), message)).Check();
  }

  Local<Value> path_buffer;
  if (path != nullptr) {
    path_buffer =
      Buffer::Copy(env->isolate(), path, strlen(path)).ToLocalChecked();
    obj->Set(env->context(), env->path_string(), path_buffer).Check();
  }

  Local<Value> dest_buffer;
  if (dest != nullptr) {
    dest_buffer =
      Buffer::Copy(env->isolate(), dest, strlen(dest)).ToLocalChecked();
    obj->Set(env->context(), env->dest_string(), dest_buffer).Check();
  }

  if (syscall != nullptr) {
    obj->Set(env->context(), env->syscall_string(),
             OneByteString(env->isolate(), syscall)).Check();
  }
}

void Environment::CollectUVExceptionInfo(Local<Value> object,
                                         int errorno,
                                         const char* syscall,
                                         const char* message,
                                         const char* path,
                                         const char* dest) {
  if (!object->IsObject() || errorno == 0)
    return;

  Local<Object> obj = object.As<Object>();
  const char* err_string = uv_err_name(errorno);

  if (message == nullptr || message[0] == '\0') {
    message = uv_strerror(errorno);
  }

  node::CollectExceptionInfo(this, obj, errorno, err_string,
                             syscall, message, path, dest);
}

ImmediateInfo::ImmediateInfo(v8::Isolate* isolate, const SerializeInfo* info)
    : fields_(isolate, kFieldsCount, MAYBE_FIELD_PTR(info, fields)) {}

ImmediateInfo::SerializeInfo ImmediateInfo::Serialize(
    v8::Local<v8::Context> context, v8::SnapshotCreator* creator) {
  return {fields_.Serialize(context, creator)};
}

void ImmediateInfo::Deserialize(Local<Context> context) {
  fields_.Deserialize(context);
}

std::ostream& operator<<(std::ostream& output,
                         const ImmediateInfo::SerializeInfo& i) {
  output << "{ " << i.fields << " }";
  return output;
}

void ImmediateInfo::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("fields", fields_);
}

TickInfo::SerializeInfo TickInfo::Serialize(v8::Local<v8::Context> context,
                                            v8::SnapshotCreator* creator) {
  return {fields_.Serialize(context, creator)};
}

void TickInfo::Deserialize(Local<Context> context) {
  fields_.Deserialize(context);
}

std::ostream& operator<<(std::ostream& output,
                         const TickInfo::SerializeInfo& i) {
  output << "{ " << i.fields << " }";
  return output;
}

void TickInfo::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("fields", fields_);
}

TickInfo::TickInfo(v8::Isolate* isolate, const SerializeInfo* info)
    : fields_(
          isolate, kFieldsCount, info == nullptr ? nullptr : &(info->fields)) {}

AsyncHooks::AsyncHooks(v8::Isolate* isolate, const SerializeInfo* info)
    : async_ids_stack_(isolate, 16 * 2, MAYBE_FIELD_PTR(info, async_ids_stack)),
      fields_(isolate, kFieldsCount, MAYBE_FIELD_PTR(info, fields)),
      async_id_fields_(
          isolate, kUidFieldsCount, MAYBE_FIELD_PTR(info, async_id_fields)),
      info_(info) {
  v8::HandleScope handle_scope(isolate);
  if (info == nullptr) {
    clear_async_id_stack();

    // Always perform async_hooks checks, not just when async_hooks is enabled.
    // TODO(AndreasMadsen): Consider removing this for LTS releases.
    // See discussion in https://github.com/nodejs/node/pull/15454
    // When removing this, do it by reverting the commit. Otherwise the test
    // and flag changes won't be included.
    fields_[kCheck] = 1;

    // kDefaultTriggerAsyncId should be -1, this indicates that there is no
    // specified default value and it should fallback to the executionAsyncId.
    // 0 is not used as the magic value, because that indicates a missing
    // context which is different from a default context.
    async_id_fields_[AsyncHooks::kDefaultTriggerAsyncId] = -1;

    // kAsyncIdCounter should start at 1 because that'll be the id the execution
    // context during bootstrap (code that runs before entering uv_run()).
    async_id_fields_[AsyncHooks::kAsyncIdCounter] = 1;
  }
}

void AsyncHooks::Deserialize(Local<Context> context) {
  async_ids_stack_.Deserialize(context);
  fields_.Deserialize(context);
  async_id_fields_.Deserialize(context);
  if (info_->js_execution_async_resources != 0) {
    v8::Local<v8::Array> arr = context
                                   ->GetDataFromSnapshotOnce<v8::Array>(
                                       info_->js_execution_async_resources)
                                   .ToLocalChecked();
    js_execution_async_resources_.Reset(context->GetIsolate(), arr);
  }

  native_execution_async_resources_.resize(
      info_->native_execution_async_resources.size());
  for (size_t i = 0; i < info_->native_execution_async_resources.size(); ++i) {
    v8::Local<v8::Object> obj =
        context
            ->GetDataFromSnapshotOnce<v8::Object>(
                info_->native_execution_async_resources[i])
            .ToLocalChecked();
    native_execution_async_resources_[i].Reset(context->GetIsolate(), obj);
  }
  info_ = nullptr;
}

std::ostream& operator<<(std::ostream& output,
                         const std::vector<SnapshotIndex>& v) {
  output << "{ ";
  for (const SnapshotIndex i : v) {
    output << i << ", ";
  }
  output << " }";
  return output;
}

std::ostream& operator<<(std::ostream& output,
                         const AsyncHooks::SerializeInfo& i) {
  output << "{\n"
         << "  " << i.async_ids_stack << ",  // async_ids_stack\n"
         << "  " << i.fields << ",  // fields\n"
         << "  " << i.async_id_fields << ",  // async_id_fields\n"
         << "  " << i.js_execution_async_resources
         << ",  // js_execution_async_resources\n"
         << "  " << i.native_execution_async_resources
         << ",  // native_execution_async_resources\n"
         << "}";
  return output;
}

AsyncHooks::SerializeInfo AsyncHooks::Serialize(Local<Context> context,
                                                SnapshotCreator* creator) {
  SerializeInfo info;
  info.async_ids_stack = async_ids_stack_.Serialize(context, creator);
  info.fields = fields_.Serialize(context, creator);
  info.async_id_fields = async_id_fields_.Serialize(context, creator);
  if (!js_execution_async_resources_.IsEmpty()) {
    info.js_execution_async_resources = creator->AddData(
        context, js_execution_async_resources_.Get(context->GetIsolate()));
    CHECK_NE(info.js_execution_async_resources, 0);
  } else {
    info.js_execution_async_resources = 0;
  }

  info.native_execution_async_resources.resize(
      native_execution_async_resources_.size());
  for (size_t i = 0; i < native_execution_async_resources_.size(); i++) {
    info.native_execution_async_resources[i] = creator->AddData(
        context,
        native_execution_async_resources_[i].Get(context->GetIsolate()));
  }

  return info;
}

void AsyncHooks::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("async_ids_stack", async_ids_stack_);
  tracker->TrackField("fields", fields_);
  tracker->TrackField("async_id_fields", async_id_fields_);
}

void AsyncHooks::grow_async_ids_stack() {
  async_ids_stack_.reserve(async_ids_stack_.Length() * 3);

  env()->async_hooks_binding()->Set(
      env()->context(),
      env()->async_ids_stack_string(),
      async_ids_stack_.GetJSArray()).Check();
}

uv_key_t Environment::thread_local_env = {};

void Environment::Exit(int exit_code) {
  if (options()->trace_exit) {
    HandleScope handle_scope(isolate());

    if (is_main_thread()) {
      fprintf(stderr, "(node:%d) ", uv_os_getpid());
    } else {
      fprintf(stderr, "(node:%d, thread:%" PRIu64 ") ",
              uv_os_getpid(), thread_id());
    }

    fprintf(
        stderr, "WARNING: Exited the environment with code %d\n", exit_code);
    PrintStackTrace(isolate(),
                    StackTrace::CurrentStackTrace(
                        isolate(), stack_trace_limit(), StackTrace::kDetailed));
  }
  process_exit_handler_(this, exit_code);
}

void Environment::stop_sub_worker_contexts() {
  DCHECK_EQ(Isolate::GetCurrent(), isolate());

  while (!sub_worker_contexts_.empty()) {
    Worker* w = *sub_worker_contexts_.begin();
    remove_sub_worker_context(w);
    w->Exit(1);
    w->JoinThread();
  }
}

Environment* Environment::worker_parent_env() const {
  if (worker_context() == nullptr) return nullptr;
  return worker_context()->env();
}

void Environment::AddUnmanagedFd(int fd) {
  if (!tracks_unmanaged_fds()) return;
  auto result = unmanaged_fds_.insert(fd);
  if (!result.second) {
    ProcessEmitWarning(
        this, "File descriptor %d opened in unmanaged mode twice", fd);
  }
}

void Environment::RemoveUnmanagedFd(int fd) {
  if (!tracks_unmanaged_fds()) return;
  size_t removed_count = unmanaged_fds_.erase(fd);
  if (removed_count == 0) {
    ProcessEmitWarning(
        this, "File descriptor %d closed but not opened in unmanaged mode", fd);
  }
}

void Environment::ForEachBaseObject(BaseObjectIterator iterator) {
  size_t i = 0;
  for (const auto& hook : cleanup_hooks_) {
    BaseObject* obj = hook.GetBaseObject();
    if (obj != nullptr) iterator(i, obj);
    i++;
  }
}

void PrintBaseObject(size_t i, BaseObject* obj) {
  std::cout << "#" << i << " " << obj << ": " << obj->MemoryInfoName() << "\n";
}

void Environment::PrintAllBaseObjects() {
  std::cout << "BaseObjects\n";
  ForEachBaseObject(PrintBaseObject);
}

EnvSerializeInfo Environment::Serialize(SnapshotCreator* creator) {
  EnvSerializeInfo info;
  Local<Context> ctx = context();

  // Currently all modules are compiled without cache in builtin snapshot
  // builder.
  info.native_modules = std::vector<std::string>(
      native_modules_without_cache.begin(), native_modules_without_cache.end());

  info.async_hooks = async_hooks_.Serialize(ctx, creator);
  info.immediate_info = immediate_info_.Serialize(ctx, creator);
  info.tick_info = tick_info_.Serialize(ctx, creator);
  info.performance_state = performance_state_->Serialize(ctx, creator);
  info.stream_base_state = stream_base_state_.Serialize(ctx, creator);
  info.should_abort_on_uncaught_toggle =
      should_abort_on_uncaught_toggle_.Serialize(ctx, creator);

  size_t id = 0;
#define V(PropertyName, TypeName)                                              \
  do {                                                                         \
    Local<TypeName> field = PropertyName();                                    \
    if (!field.IsEmpty()) {                                                    \
      size_t index = creator->AddData(field);                                  \
      info.persistent_templates.push_back({#PropertyName, id, index});         \
    }                                                                          \
    id++;                                                                      \
  } while (0);
  ENVIRONMENT_STRONG_PERSISTENT_TEMPLATES(V)
#undef V

  id = 0;
#define V(PropertyName, TypeName)                                              \
  do {                                                                         \
    Local<TypeName> field = PropertyName();                                    \
    if (!field.IsEmpty()) {                                                    \
      size_t index = creator->AddData(ctx, field);                             \
      info.persistent_values.push_back({#PropertyName, id, index});            \
    }                                                                          \
    id++;                                                                      \
  } while (0);
  ENVIRONMENT_STRONG_PERSISTENT_VALUES(V)
#undef V

  info.context = creator->AddData(ctx, context());
  return info;
}

std::ostream& operator<<(std::ostream& output,
                         const std::vector<PropInfo>& vec) {
  output << "{\n";
  for (const auto& info : vec) {
    output << "  { \"" << info.name << "\", " << std::to_string(info.id) << ", "
           << std::to_string(info.index) << " },\n";
  }
  output << "}";
  return output;
}

std::ostream& operator<<(std::ostream& output,
                         const std::vector<std::string>& vec) {
  output << "{\n";
  for (const auto& info : vec) {
    output << "  \"" << info << "\",\n";
  }
  output << "}";
  return output;
}

std::ostream& operator<<(std::ostream& output, const EnvSerializeInfo& i) {
  output << "{\n"
         << "// -- native_modules begins --\n"
         << i.native_modules << ",\n"
         << "// -- native_modules ends --\n"
         << "// -- async_hooks begins --\n"
         << i.async_hooks << ",\n"
         << "// -- async_hooks ends --\n"
         << i.tick_info << ",  // tick_info\n"
         << i.immediate_info << ",  // immediate_info\n"
         << "// -- performance_state begins --\n"
         << i.performance_state << ",\n"
         << "// -- performance_state ends --\n"
         << i.stream_base_state << ",  // stream_base_state\n"
         << i.should_abort_on_uncaught_toggle
         << ",  // should_abort_on_uncaught_toggle\n"
         << "// -- persistent_templates begins --\n"
         << i.persistent_templates << ",\n"
         << "// persistent_templates ends --\n"
         << "// -- persistent_values begins --\n"
         << i.persistent_values << ",\n"
         << "// -- persistent_values ends --\n"
         << i.context << ",  // context\n"
         << "}";
  return output;
}

void Environment::DeserializeProperties(const EnvSerializeInfo* info) {
  Local<Context> ctx = context();

  native_modules_in_snapshot = info->native_modules;
  async_hooks_.Deserialize(ctx);
  immediate_info_.Deserialize(ctx);
  tick_info_.Deserialize(ctx);
  performance_state_->Deserialize(ctx);
  stream_base_state_.Deserialize(ctx);
  should_abort_on_uncaught_toggle_.Deserialize(ctx);

  if (enabled_debug_list_.enabled(DebugCategory::MKSNAPSHOT)) {
    fprintf(stderr, "deserializing...\n");
    std::cerr << *info << "\n";
  }

  const std::vector<PropInfo>& templates = info->persistent_templates;
  size_t i = 0;  // index to the array
  size_t id = 0;
#define V(PropertyName, TypeName)                                              \
  do {                                                                         \
    if (templates.size() > i && id == templates[i].id) {                       \
      const PropInfo& d = templates[i];                                        \
      DCHECK_EQ(d.name, #PropertyName);                                        \
      MaybeLocal<TypeName> maybe_field =                                       \
          isolate_->GetDataFromSnapshotOnce<TypeName>(d.index);                \
      Local<TypeName> field;                                                   \
      if (!maybe_field.ToLocal(&field)) {                                      \
        fprintf(stderr,                                                        \
                "Failed to deserialize environment template " #PropertyName    \
                "\n");                                                         \
      }                                                                        \
      set_##PropertyName(field);                                               \
      i++;                                                                     \
    }                                                                          \
  } while (0);                                                                 \
  id++;
  ENVIRONMENT_STRONG_PERSISTENT_TEMPLATES(V);
#undef V

  i = 0;  // index to the array
  id = 0;
  const std::vector<PropInfo>& values = info->persistent_values;
#define V(PropertyName, TypeName)                                              \
  do {                                                                         \
    if (values.size() > i && id == values[i].id) {                             \
      const PropInfo& d = values[i];                                           \
      DCHECK_EQ(d.name, #PropertyName);                                        \
      MaybeLocal<TypeName> maybe_field =                                       \
          ctx->GetDataFromSnapshotOnce<TypeName>(d.index);                     \
      Local<TypeName> field;                                                   \
      if (!maybe_field.ToLocal(&field)) {                                      \
        fprintf(stderr,                                                        \
                "Failed to deserialize environment value " #PropertyName       \
                "\n");                                                         \
      }                                                                        \
      set_##PropertyName(field);                                               \
      i++;                                                                     \
    }                                                                          \
  } while (0);                                                                 \
  id++;
  ENVIRONMENT_STRONG_PERSISTENT_VALUES(V);
#undef V

  MaybeLocal<Context> maybe_ctx_from_snapshot =
      ctx->GetDataFromSnapshotOnce<Context>(info->context);
  Local<Context> ctx_from_snapshot;
  if (!maybe_ctx_from_snapshot.ToLocal(&ctx_from_snapshot)) {
    fprintf(stderr,
            "Failed to deserialize context back reference from the snapshot\n");
  }
  CHECK_EQ(ctx_from_snapshot, ctx);
}

void Environment::BuildEmbedderGraph(Isolate* isolate,
                                     EmbedderGraph* graph,
                                     void* data) {
  MemoryTracker tracker(isolate, graph);
  Environment* env = static_cast<Environment*>(data);
  tracker.Track(env);
  env->ForEachBaseObject([&](BaseObject* obj) {
    if (obj->IsDoneInitializing())
      tracker.Track(obj);
  });
}

inline size_t Environment::SelfSize() const {
  size_t size = sizeof(*this);
  // Remove non pointer fields that will be tracked in MemoryInfo()
  // TODO(joyeecheung): refactor the MemoryTracker interface so
  // this can be done for common types within the Track* calls automatically
  // if a certain scope is entered.
  size -= sizeof(async_hooks_);
  size -= sizeof(tick_info_);
  size -= sizeof(immediate_info_);
  return size;
}

void Environment::MemoryInfo(MemoryTracker* tracker) const {
  // Iteratable STLs have their own sizes subtracted from the parent
  // by default.
  tracker->TrackField("isolate_data", isolate_data_);
  tracker->TrackField("native_modules_with_cache", native_modules_with_cache);
  tracker->TrackField("native_modules_without_cache",
                      native_modules_without_cache);
  tracker->TrackField("destroy_async_id_list", destroy_async_id_list_);
  tracker->TrackField("exec_argv", exec_argv_);
  tracker->TrackField("should_abort_on_uncaught_toggle",
                      should_abort_on_uncaught_toggle_);
  tracker->TrackField("stream_base_state", stream_base_state_);
  tracker->TrackFieldWithSize(
      "cleanup_hooks", cleanup_hooks_.size() * sizeof(CleanupHookCallback));
  tracker->TrackField("async_hooks", async_hooks_);
  tracker->TrackField("immediate_info", immediate_info_);
  tracker->TrackField("tick_info", tick_info_);

#define V(PropertyName, TypeName)                                              \
  tracker->TrackField(#PropertyName, PropertyName());
  ENVIRONMENT_STRONG_PERSISTENT_VALUES(V)
#undef V

  // FIXME(joyeecheung): track other fields in Environment.
  // Currently MemoryTracker is unable to track these
  // correctly:
  // - Internal types that do not implement MemoryRetainer yet
  // - STL containers with MemoryRetainer* inside
  // - STL containers with numeric types inside that should not have their
  //   nodes elided e.g. numeric keys in maps.
  // We also need to make sure that when we add a non-pointer field as its own
  // node, we shift its sizeof() size out of the Environment node.
}

void Environment::RunWeakRefCleanup() {
  isolate()->ClearKeptObjects();
}

// Not really any better place than env.cc at this moment.
void BaseObject::DeleteMe(void* data) {
  BaseObject* self = static_cast<BaseObject*>(data);
  if (self->has_pointer_data() &&
      self->pointer_data()->strong_ptr_count > 0) {
    return self->Detach();
  }
  delete self;
}

bool BaseObject::IsDoneInitializing() const { return true; }

Local<Object> BaseObject::WrappedObject() const {
  return object();
}

bool BaseObject::IsRootNode() const {
  return !persistent_handle_.IsWeak();
}

Local<FunctionTemplate> BaseObject::GetConstructorTemplate(Environment* env) {
  Local<FunctionTemplate> tmpl = env->base_object_ctor_template();
  if (tmpl.IsEmpty()) {
    tmpl = env->NewFunctionTemplate(nullptr);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "BaseObject"));
    env->set_base_object_ctor_template(tmpl);
  }
  return tmpl;
}

}  // namespace node
