#include "node_worker.h"
#include "debug_utils.h"
#include "node_errors.h"
#include "node_buffer.h"
#include "node_perf.h"
#include "util.h"
#include "util-inl.h"
#include "async_wrap.h"
#include "async_wrap-inl.h"

#if NODE_USE_V8_PLATFORM && HAVE_INSPECTOR
#include "inspector/worker_inspector.h"  // ParentInspectorHandle
#endif

#include <string>
#include <vector>

using node::options_parser::kDisallowedInEnvironment;
using v8::ArrayBuffer;
using v8::Boolean;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Locker;
using v8::Number;
using v8::Object;
using v8::SealHandleScope;
using v8::String;
using v8::Value;

namespace node {
namespace worker {

namespace {

#if NODE_USE_V8_PLATFORM && HAVE_INSPECTOR
void StartWorkerInspector(
    Environment* child,
    std::unique_ptr<inspector::ParentInspectorHandle> parent_handle,
    const std::string& url) {
  child->inspector_agent()->SetParentHandle(std::move(parent_handle));
  child->inspector_agent()->Start(url,
                                  child->options()->debug_options(),
                                  child->inspector_host_port(),
                                  false);
}

void WaitForWorkerInspectorToStop(Environment* child) {
  child->inspector_agent()->WaitForDisconnect();
  child->inspector_agent()->Stop();
}
#endif

}  // anonymous namespace

Worker::Worker(Environment* env,
               Local<Object> wrap,
               const std::string& url,
               std::shared_ptr<PerIsolateOptions> per_isolate_opts)
    : AsyncWrap(env, wrap, AsyncWrap::PROVIDER_WORKER),
      url_(url),
      per_isolate_opts_(per_isolate_opts),
      platform_(env->isolate_data()->platform()),
      profiler_idle_notifier_started_(env->profiler_idle_notifier_started()),
      thread_id_(Environment::AllocateThreadId()) {
  Debug(this, "Creating new worker instance with thread id %llu", thread_id_);

  // Set up everything that needs to be set up in the parent environment.
  parent_port_ = MessagePort::New(env, env->context());
  if (parent_port_ == nullptr) {
    // This can happen e.g. because execution is terminating.
    return;
  }

  child_port_data_.reset(new MessagePortData(nullptr));
  MessagePort::Entangle(parent_port_, child_port_data_.get());

  object()->Set(env->context(),
                env->message_port_string(),
                parent_port_->object()).FromJust();

  object()->Set(env->context(),
                env->thread_id_string(),
                Number::New(env->isolate(), static_cast<double>(thread_id_)))
      .FromJust();

#if NODE_USE_V8_PLATFORM && HAVE_INSPECTOR
  inspector_parent_handle_ =
      env->inspector_agent()->GetParentHandle(thread_id_, url);
#endif

  Debug(this, "Preparation for worker %llu finished", thread_id_);
}

bool Worker::is_stopped() const {
  Mutex::ScopedLock stopped_lock(stopped_mutex_);
  return stopped_;
}

// This class contains data that is only relevant to the child thread itself,
// and only while it is running.
// (Eventually, the Environment instance should probably also be moved here.)
class WorkerThreadData {
 public:
  explicit WorkerThreadData(Worker* w)
    : w_(w),
      array_buffer_allocator_(CreateArrayBufferAllocator()) {
    CHECK_EQ(uv_loop_init(&loop_), 0);

    Isolate* isolate = NewIsolate(array_buffer_allocator_.get(), &loop_);
    CHECK_NOT_NULL(isolate);

    {
      Locker locker(isolate);
      Isolate::Scope isolate_scope(isolate);
      HandleScope handle_scope(isolate);
      isolate_data_.reset(CreateIsolateData(isolate,
                                            &loop_,
                                            w_->platform_,
                                            array_buffer_allocator_.get()));
      CHECK(isolate_data_);
      if (w_->per_isolate_opts_)
        isolate_data_->set_options(std::move(w_->per_isolate_opts_));
    }

    Mutex::ScopedLock lock(w_->mutex_);
    w_->isolate_ = isolate;
  }

  ~WorkerThreadData() {
    Debug(w_, "Worker %llu dispose isolate", w_->thread_id_);
    Isolate* isolate;
    {
      Mutex::ScopedLock lock(w_->mutex_);
      isolate = w_->isolate_;
      w_->isolate_ = nullptr;
    }

    w_->platform_->CancelPendingDelayedTasks(isolate);

    isolate_data_.reset();
    w_->platform_->UnregisterIsolate(isolate);

    isolate->Dispose();

    // Need to run the loop one more time to close the platform's uv_async_t
    uv_run(&loop_, UV_RUN_ONCE);

    CheckedUvLoopClose(&loop_);
  }

 private:
  Worker* const w_;
  uv_loop_t loop_;
  DeleteFnPtr<ArrayBufferAllocator, FreeArrayBufferAllocator>
    array_buffer_allocator_;
  DeleteFnPtr<IsolateData, FreeIsolateData> isolate_data_;

  friend class Worker;
};

void Worker::Run() {
  std::string name = "WorkerThread ";
  name += std::to_string(thread_id_);
  TRACE_EVENT_METADATA1(
      "__metadata", "thread_name", "name",
      TRACE_STR_COPY(name.c_str()));
  CHECK_NOT_NULL(platform_);

  Debug(this, "Creating isolate for worker with id %llu", thread_id_);

  WorkerThreadData data(this);

  Debug(this, "Starting worker with id %llu", thread_id_);
  {
    Locker locker(isolate_);
    Isolate::Scope isolate_scope(isolate_);
    SealHandleScope outer_seal(isolate_);
    bool inspector_started = false;

    DeleteFnPtr<Environment, FreeEnvironment> env_;
    OnScopeLeave cleanup_env([&]() {
      if (!env_) return;
      env_->set_can_call_into_js(false);
      Isolate::DisallowJavascriptExecutionScope disallow_js(isolate_,
          Isolate::DisallowJavascriptExecutionScope::THROW_ON_FAILURE);

      // Grab the parent-to-child channel and render is unusable.
      MessagePort* child_port;
      {
        Mutex::ScopedLock lock(mutex_);
        child_port = child_port_;
        child_port_ = nullptr;
      }

      {
        Context::Scope context_scope(env_->context());
        if (child_port != nullptr)
          child_port->Close();
        env_->stop_sub_worker_contexts();
        env_->RunCleanup();
        RunAtExit(env_.get());
#if NODE_USE_V8_PLATFORM && HAVE_INSPECTOR
        if (inspector_started)
          WaitForWorkerInspectorToStop(env_.get());
#endif

        {
          Mutex::ScopedLock stopped_lock(stopped_mutex_);
          stopped_ = true;
        }

        env_->RunCleanup();

        // This call needs to be made while the `Environment` is still alive
        // because we assume that it is available for async tracking in the
        // NodePlatform implementation.
        platform_->DrainTasks(isolate_);
      }
    });

    {
      HandleScope handle_scope(isolate_);
      Local<Context> context = NewContext(isolate_);
      if (is_stopped()) return;

      CHECK(!context.IsEmpty());
      Context::Scope context_scope(context);
      {
        // TODO(addaleax): Use CreateEnvironment(), or generally another
        // public API.
        env_.reset(new Environment(data.isolate_data_.get(),
                                   context,
                                   Environment::kNoFlags,
                                   thread_id_));
        CHECK_NOT_NULL(env_);
        env_->set_abort_on_uncaught_exception(false);
        env_->set_worker_context(this);

        env_->Start(profiler_idle_notifier_started_);
        env_->ProcessCliArgs(std::vector<std::string>{},
                             std::vector<std::string>{});
      }

      Debug(this, "Created Environment for worker with id %llu", thread_id_);

      if (is_stopped()) return;
      {
        HandleScope handle_scope(isolate_);
        Mutex::ScopedLock lock(mutex_);
        // Set up the message channel for receiving messages in the child.
        child_port_ = MessagePort::New(env_.get(),
                                       env_->context(),
                                       std::move(child_port_data_));
        // MessagePort::New() may return nullptr if execution is terminated
        // within it.
        if (child_port_ != nullptr)
          env_->set_message_port(child_port_->object(isolate_));

        Debug(this, "Created message port for worker %llu", thread_id_);
      }

      if (is_stopped()) return;
      {
#if NODE_USE_V8_PLATFORM && HAVE_INSPECTOR
        StartWorkerInspector(env_.get(),
                             std::move(inspector_parent_handle_),
                             url_);
#endif
        inspector_started = true;

        HandleScope handle_scope(isolate_);
        Environment::AsyncCallbackScope callback_scope(env_.get());
        env_->async_hooks()->push_async_ids(1, 0);
        if (!RunBootstrapping(env_.get()).IsEmpty()) {
          USE(StartExecution(env_.get(), "internal/main/worker_thread"));
        }

        env_->async_hooks()->pop_async_id(1);

        Debug(this, "Loaded environment for worker %llu", thread_id_);
      }

      if (is_stopped()) return;
      {
        SealHandleScope seal(isolate_);
        bool more;
        env_->performance_state()->Mark(
            node::performance::NODE_PERFORMANCE_MILESTONE_LOOP_START);
        do {
          if (is_stopped()) break;
          uv_run(&data.loop_, UV_RUN_DEFAULT);
          if (is_stopped()) break;

          platform_->DrainTasks(isolate_);

          more = uv_loop_alive(&data.loop_);
          if (more && !is_stopped())
            continue;

          EmitBeforeExit(env_.get());

          // Emit `beforeExit` if the loop became alive either after emitting
          // event, or after running some callbacks.
          more = uv_loop_alive(&data.loop_);
        } while (more == true);
        env_->performance_state()->Mark(
            node::performance::NODE_PERFORMANCE_MILESTONE_LOOP_EXIT);
      }
    }

    {
      int exit_code;
      bool stopped = is_stopped();
      if (!stopped)
        exit_code = EmitExit(env_.get());
      Mutex::ScopedLock lock(mutex_);
      if (exit_code_ == 0 && !stopped)
        exit_code_ = exit_code;

      Debug(this, "Exiting thread for worker %llu with exit code %d",
            thread_id_, exit_code_);
    }
  }

  Debug(this, "Worker %llu thread stops", thread_id_);
}

void Worker::JoinThread() {
  if (thread_joined_)
    return;
  CHECK_EQ(uv_thread_join(&tid_), 0);
  thread_joined_ = true;

  env()->remove_sub_worker_context(this);

  if (thread_exit_async_) {
    env()->CloseHandle(thread_exit_async_.release(), [](uv_async_t* async) {
      delete async;
    });

    if (scheduled_on_thread_stopped_)
      OnThreadStopped();
  }
}

void Worker::OnThreadStopped() {
  {
    Mutex::ScopedLock lock(mutex_);
    scheduled_on_thread_stopped_ = false;

    Debug(this, "Worker %llu thread stopped", thread_id_);

    {
      Mutex::ScopedLock stopped_lock(stopped_mutex_);
      CHECK(stopped_);
    }

    parent_port_ = nullptr;
  }

  JoinThread();

  {
    HandleScope handle_scope(env()->isolate());
    Context::Scope context_scope(env()->context());

    // Reset the parent port as we're closing it now anyway.
    object()->Set(env()->context(),
                  env()->message_port_string(),
                  Undefined(env()->isolate())).FromJust();

    Local<Value> code = Integer::New(env()->isolate(), exit_code_);
    MakeCallback(env()->onexit_string(), 1, &code);
  }

  // JoinThread() cleared all libuv handles bound to this Worker,
  // the C++ object is no longer needed for anything now.
  MakeWeak();
}

Worker::~Worker() {
  Mutex::ScopedLock lock(mutex_);
  JoinThread();

  CHECK(stopped_);
  CHECK(thread_joined_);

  // This has most likely already happened within the worker thread -- this
  // is just in case Worker creation failed early.

  Debug(this, "Worker %llu destroyed", thread_id_);
}

void Worker::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args.IsConstructCall());

  if (env->isolate_data()->platform() == nullptr) {
    THROW_ERR_MISSING_PLATFORM_FOR_WORKER(env);
    return;
  }

  std::string url;
  std::shared_ptr<PerIsolateOptions> per_isolate_opts = nullptr;

  // Argument might be a string or URL
  if (args.Length() > 0 && !args[0]->IsNullOrUndefined()) {
    Utf8Value value(
        args.GetIsolate(),
        args[0]->ToString(env->context()).FromMaybe(v8::Local<v8::String>()));
    url.append(value.out(), value.length());

    if (args.Length() > 1 && args[1]->IsArray()) {
      v8::Local<v8::Array> array = args[1].As<v8::Array>();
      // The first argument is reserved for program name, but we don't need it
      // in workers.
      std::vector<std::string> exec_argv = {""};
      uint32_t length = array->Length();
      for (uint32_t i = 0; i < length; i++) {
        v8::Local<v8::Value> arg;
        if (!array->Get(env->context(), i).ToLocal(&arg)) {
          return;
        }
        v8::MaybeLocal<v8::String> arg_v8_string =
            arg->ToString(env->context());
        if (arg_v8_string.IsEmpty()) {
          return;
        }
        Utf8Value arg_utf8_value(
            args.GetIsolate(),
            arg_v8_string.FromMaybe(v8::Local<v8::String>()));
        std::string arg_string(arg_utf8_value.out(), arg_utf8_value.length());
        exec_argv.push_back(arg_string);
      }

      std::vector<std::string> invalid_args{};
      std::vector<std::string> errors{};
      per_isolate_opts.reset(new PerIsolateOptions());

      // Using invalid_args as the v8_args argument as it stores unknown
      // options for the per isolate parser.
      options_parser::PerIsolateOptionsParser::instance.Parse(
          &exec_argv,
          nullptr,
          &invalid_args,
          per_isolate_opts.get(),
          kDisallowedInEnvironment,
          &errors);

      // The first argument is program name.
      invalid_args.erase(invalid_args.begin());
      if (errors.size() > 0 || invalid_args.size() > 0) {
        v8::Local<v8::Value> value =
            ToV8Value(env->context(),
                      errors.size() > 0 ? errors : invalid_args)
                .ToLocalChecked();
        Local<String> key =
            FIXED_ONE_BYTE_STRING(env->isolate(), "invalidExecArgv");
        args.This()->Set(env->context(), key, value).FromJust();
        return;
      }
    }
  }
  new Worker(env, args.This(), url, per_isolate_opts);
}

void Worker::StartThread(const FunctionCallbackInfo<Value>& args) {
  Worker* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.This());
  Mutex::ScopedLock lock(w->mutex_);

  w->env()->add_sub_worker_context(w);
  w->stopped_ = false;
  w->thread_joined_ = false;

  w->thread_exit_async_.reset(new uv_async_t);
  w->thread_exit_async_->data = w;
  CHECK_EQ(uv_async_init(w->env()->event_loop(),
                         w->thread_exit_async_.get(),
                         [](uv_async_t* handle) {
    static_cast<Worker*>(handle->data)->OnThreadStopped();
  }), 0);

  CHECK_EQ(uv_thread_create(&w->tid_, [](void* arg) {
    Worker* w = static_cast<Worker*>(arg);
    w->Run();

    Mutex::ScopedLock lock(w->mutex_);
    CHECK(w->thread_exit_async_);
    w->scheduled_on_thread_stopped_ = true;
    uv_async_send(w->thread_exit_async_.get());
  }, static_cast<void*>(w)), 0);
}

void Worker::StopThread(const FunctionCallbackInfo<Value>& args) {
  Worker* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.This());

  Debug(w, "Worker %llu is getting stopped by parent", w->thread_id_);
  w->Exit(1);
  w->JoinThread();
}

void Worker::Ref(const FunctionCallbackInfo<Value>& args) {
  Worker* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.This());
  if (w->thread_exit_async_)
    uv_ref(reinterpret_cast<uv_handle_t*>(w->thread_exit_async_.get()));
}

void Worker::Unref(const FunctionCallbackInfo<Value>& args) {
  Worker* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.This());
  if (w->thread_exit_async_)
    uv_unref(reinterpret_cast<uv_handle_t*>(w->thread_exit_async_.get()));
}

void Worker::Exit(int code) {
  Mutex::ScopedLock lock(mutex_);
  Mutex::ScopedLock stopped_lock(stopped_mutex_);

  Debug(this, "Worker %llu called Exit(%d)", thread_id_, code);

  if (!stopped_) {
    stopped_ = true;
    exit_code_ = code;
    if (child_port_ != nullptr)
      child_port_->StopEventLoop();
    if (isolate_ != nullptr)
      isolate_->TerminateExecution();
  }
}

namespace {

// Return the MessagePort that is global for this Environment and communicates
// with the internal [kPort] port of the JS Worker class in the parent thread.
void GetEnvMessagePort(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Object> port = env->message_port();
  if (!port.IsEmpty()) {
    CHECK_EQ(port->CreationContext()->GetIsolate(), args.GetIsolate());
    args.GetReturnValue().Set(port);
  }
}

void InitWorker(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);

  {
    Local<FunctionTemplate> w = env->NewFunctionTemplate(Worker::New);

    w->InstanceTemplate()->SetInternalFieldCount(1);
    w->Inherit(AsyncWrap::GetConstructorTemplate(env));

    env->SetProtoMethod(w, "startThread", Worker::StartThread);
    env->SetProtoMethod(w, "stopThread", Worker::StopThread);
    env->SetProtoMethod(w, "ref", Worker::Ref);
    env->SetProtoMethod(w, "unref", Worker::Unref);

    Local<String> workerString =
        FIXED_ONE_BYTE_STRING(env->isolate(), "Worker");
    w->SetClassName(workerString);
    target->Set(env->context(),
                workerString,
                w->GetFunction(env->context()).ToLocalChecked()).FromJust();
  }

  env->SetMethod(target, "getEnvMessagePort", GetEnvMessagePort);

  target
      ->Set(env->context(),
            env->thread_id_string(),
            Number::New(env->isolate(), static_cast<double>(env->thread_id())))
      .FromJust();

  target
      ->Set(env->context(),
            FIXED_ONE_BYTE_STRING(env->isolate(), "isMainThread"),
            Boolean::New(env->isolate(), env->is_main_thread()))
      .FromJust();

  target
      ->Set(env->context(),
            FIXED_ONE_BYTE_STRING(env->isolate(), "ownsProcessState"),
            Boolean::New(env->isolate(), env->owns_process_state()))
      .FromJust();
}

}  // anonymous namespace

}  // namespace worker
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(worker, node::worker::InitWorker)
