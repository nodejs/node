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

void AsyncRequest::Install(Environment* env, void* data, uv_async_cb target) {
  Mutex::ScopedLock lock(mutex_);
  env_ = env;
  async_ = new uv_async_t;
  if (data != nullptr) async_->data = data;
  CHECK_EQ(uv_async_init(env_->event_loop(), async_, target), 0);
}

void AsyncRequest::Uninstall() {
  Mutex::ScopedLock lock(mutex_);
  if (async_ != nullptr)
    env_->CloseHandle(async_, [](uv_async_t* async) { delete async; });
}

void AsyncRequest::Stop() {
  Mutex::ScopedLock lock(mutex_);
  stop_ = true;
  if (async_ != nullptr) uv_async_send(async_);
}

void AsyncRequest::SetStopped(bool flag) {
  Mutex::ScopedLock lock(mutex_);
  stop_ = flag;
}

bool AsyncRequest::IsStopped() const {
  Mutex::ScopedLock lock(mutex_);
  return stop_;
}

uv_async_t* AsyncRequest::GetHandle() {
  Mutex::ScopedLock lock(mutex_);
  return async_;
}

void AsyncRequest::MemoryInfo(MemoryTracker* tracker) const {
  Mutex::ScopedLock lock(mutex_);
  if (async_ != nullptr) tracker->TrackField("async_request", *async_);
}

Worker::Worker(Environment* env,
               Local<Object> wrap,
               const std::string& url,
               std::shared_ptr<PerIsolateOptions> per_isolate_opts,
               std::vector<std::string>&& exec_argv)
    : AsyncWrap(env, wrap, AsyncWrap::PROVIDER_WORKER),
      url_(url),
      per_isolate_opts_(per_isolate_opts),
      exec_argv_(exec_argv),
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
  return thread_stopper_.IsStopped();
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
      isolate->SetStackLimit(w_->stack_base_);

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

    // Need to run the loop twice more to close the platform's uv_async_t
    // TODO(addaleax): It would be better for the platform itself to provide
    // some kind of notification when it has fully cleaned up.
    uv_run(&loop_, UV_RUN_ONCE);
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
        thread_stopper_.Uninstall();
        thread_stopper_.SetStopped(true);
        env_->stop_sub_worker_contexts();
        env_->RunCleanup();
        RunAtExit(env_.get());
#if NODE_USE_V8_PLATFORM && HAVE_INSPECTOR
        if (inspector_started)
          WaitForWorkerInspectorToStop(env_.get());
#endif

        // This call needs to be made while the `Environment` is still alive
        // because we assume that it is available for async tracking in the
        // NodePlatform implementation.
        platform_->DrainTasks(isolate_);
      }
    });

    if (thread_stopper_.IsStopped()) return;
    {
      HandleScope handle_scope(isolate_);
      Local<Context> context = NewContext(isolate_);

      if (thread_stopper_.IsStopped()) return;
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
                             std::move(exec_argv_));
      }

      Debug(this, "Created Environment for worker with id %llu", thread_id_);

      if (is_stopped()) return;
      thread_stopper_.Install(env_.get(), env_.get(), [](uv_async_t* handle) {
        Environment* env_ = static_cast<Environment*>(handle->data);
        uv_stop(env_->event_loop());
      });
      uv_unref(reinterpret_cast<uv_handle_t*>(thread_stopper_.GetHandle()));

      Debug(this, "Created Environment for worker with id %llu", thread_id_);
      if (thread_stopper_.IsStopped()) return;
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

      if (thread_stopper_.IsStopped()) return;
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

      if (thread_stopper_.IsStopped()) return;
      {
        SealHandleScope seal(isolate_);
        bool more;
        env_->performance_state()->Mark(
            node::performance::NODE_PERFORMANCE_MILESTONE_LOOP_START);
        do {
          if (thread_stopper_.IsStopped()) break;
          uv_run(&data.loop_, UV_RUN_DEFAULT);
          if (thread_stopper_.IsStopped()) break;

          platform_->DrainTasks(isolate_);

          more = uv_loop_alive(&data.loop_);
          if (more && !thread_stopper_.IsStopped()) continue;

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
      bool stopped = thread_stopper_.IsStopped();
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
  OnThreadStopped();
  on_thread_finished_.Uninstall();
}

void Worker::OnThreadStopped() {
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

  CHECK(thread_stopper_.IsStopped());
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

  std::vector<std::string> exec_argv_out;
  bool has_explicit_exec_argv = false;

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
      has_explicit_exec_argv = true;
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
      options_parser::Parse(
          &exec_argv,
          &exec_argv_out,
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
  if (!has_explicit_exec_argv)
    exec_argv_out = env->exec_argv();
  new Worker(env, args.This(), url, per_isolate_opts, std::move(exec_argv_out));
}

void Worker::StartThread(const FunctionCallbackInfo<Value>& args) {
  Worker* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.This());
  Mutex::ScopedLock lock(w->mutex_);

  w->env()->add_sub_worker_context(w);
  w->thread_joined_ = false;
  w->thread_stopper_.SetStopped(false);

  w->on_thread_finished_.Install(w->env(), w, [](uv_async_t* handle) {
    Worker* w_ = static_cast<Worker*>(handle->data);
    CHECK(w_->thread_stopper_.IsStopped());
    w_->parent_port_ = nullptr;
    w_->JoinThread();
  });

  uv_thread_options_t thread_options;
  thread_options.flags = UV_THREAD_HAS_STACK_SIZE;
  thread_options.stack_size = kStackSize;
  CHECK_EQ(uv_thread_create_ex(&w->tid_, &thread_options, [](void* arg) {
    Worker* w = static_cast<Worker*>(arg);
    const uintptr_t stack_top = reinterpret_cast<uintptr_t>(&arg);

    // Leave a few kilobytes just to make sure we're within limits and have
    // some space to do work in C++ land.
    w->stack_base_ = stack_top - (kStackSize - kStackBufferSize);

    w->Run();

    Mutex::ScopedLock lock(w->mutex_);
    w->on_thread_finished_.Stop();
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
  uv_ref(reinterpret_cast<uv_handle_t*>(w->on_thread_finished_.GetHandle()));
}

void Worker::Unref(const FunctionCallbackInfo<Value>& args) {
  Worker* w;
  ASSIGN_OR_RETURN_UNWRAP(&w, args.This());
  uv_unref(reinterpret_cast<uv_handle_t*>(w->on_thread_finished_.GetHandle()));
}

void Worker::Exit(int code) {
  Mutex::ScopedLock lock(mutex_);

  Debug(this, "Worker %llu called Exit(%d)", thread_id_, code);
  if (!thread_stopper_.IsStopped()) {
    exit_code_ = code;
    Debug(this, "Received StopEventLoop request");
    thread_stopper_.Stop();
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
