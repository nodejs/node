#include "node_worker.h"
#include "debug_utils.h"
#include "node_errors.h"
#include "node_internals.h"
#include "node_buffer.h"
#include "node_perf.h"
#include "util.h"
#include "util-inl.h"
#include "async_wrap.h"
#include "async_wrap-inl.h"

#include <string>

using v8::ArrayBuffer;
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

uint64_t next_thread_id = 1;
Mutex next_thread_id_mutex;

#if NODE_USE_V8_PLATFORM && HAVE_INSPECTOR
void StartWorkerInspector(Environment* child, const std::string& url) {
  child->inspector_agent()->Start(url, nullptr, false);
}

void AddWorkerInspector(Environment* parent,
                        Environment* child,
                        int id,
                        const std::string& url) {
  parent->inspector_agent()->AddWorkerInspector(id, url,
                                                child->inspector_agent());
}

void WaitForWorkerInspectorToStop(Environment* child) {
  child->inspector_agent()->WaitForDisconnect();
  child->inspector_agent()->Stop();
}

#else
// No-ops
void StartWorkerInspector(Environment* child, const std::string& url) {}
void AddWorkerInspector(Environment* parent,
                        Environment* child,
                        int id,
                        const std::string& url) {}
void WaitForWorkerInspectorToStop(Environment* child) {}
#endif

}  // anonymous namespace

Worker::Worker(Environment* env, Local<Object> wrap, const std::string& url)
    : AsyncWrap(env, wrap, AsyncWrap::PROVIDER_WORKER), url_(url) {
  // Generate a new thread id.
  {
    Mutex::ScopedLock next_thread_id_lock(next_thread_id_mutex);
    thread_id_ = next_thread_id++;
  }

  Debug(this, "Creating worker with id %llu", thread_id_);
  wrap->Set(env->context(),
            env->thread_id_string(),
            Number::New(env->isolate(),
                        static_cast<double>(thread_id_))).FromJust();

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

  array_buffer_allocator_.reset(CreateArrayBufferAllocator());

  CHECK_EQ(uv_loop_init(&loop_), 0);
  isolate_ = NewIsolate(array_buffer_allocator_.get(), &loop_);
  CHECK_NE(isolate_, nullptr);

  {
    // Enter an environment capable of executing code in the child Isolate
    // (and only in it).
    Locker locker(isolate_);
    Isolate::Scope isolate_scope(isolate_);
    HandleScope handle_scope(isolate_);

    isolate_data_.reset(CreateIsolateData(isolate_,
                                          &loop_,
                                          env->isolate_data()->platform(),
                                          array_buffer_allocator_.get()));
    CHECK(isolate_data_);

    Local<Context> context = NewContext(isolate_);
    Context::Scope context_scope(context);

    // TODO(addaleax): Use CreateEnvironment(), or generally another public API.
    env_.reset(new Environment(isolate_data_.get(),
                               context,
                               nullptr));
    CHECK_NE(env_, nullptr);
    env_->set_abort_on_uncaught_exception(false);
    env_->set_worker_context(this);
    env_->set_thread_id(thread_id_);

    env_->Start(std::vector<std::string>{},
                std::vector<std::string>{},
                env->profiler_idle_notifier_started());
    // Done while on the parent thread
    AddWorkerInspector(env, env_.get(), thread_id_, url_);
  }

  // The new isolate won't be bothered on this thread again.
  isolate_->DiscardThreadSpecificMetadata();

  Debug(this, "Set up worker with id %llu", thread_id_);
}

bool Worker::is_stopped() const {
  Mutex::ScopedLock stopped_lock(stopped_mutex_);
  return stopped_;
}

void Worker::Run() {
  std::string name = "WorkerThread ";
  name += std::to_string(thread_id_);
  TRACE_EVENT_METADATA1(
      "__metadata", "thread_name", "name",
      TRACE_STR_COPY(name.c_str()));
  MultiIsolatePlatform* platform = isolate_data_->platform();
  CHECK_NE(platform, nullptr);

  Debug(this, "Starting worker with id %llu", thread_id_);
  {
    Locker locker(isolate_);
    Isolate::Scope isolate_scope(isolate_);
    SealHandleScope outer_seal(isolate_);
    bool inspector_started = false;

    {
      Context::Scope context_scope(env_->context());
      HandleScope handle_scope(isolate_);

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

      if (!is_stopped()) {
        StartWorkerInspector(env_.get(), url_);
        inspector_started = true;

        HandleScope handle_scope(isolate_);
        Environment::AsyncCallbackScope callback_scope(env_.get());
        env_->async_hooks()->push_async_ids(1, 0);
        // This loads the Node bootstrapping code.
        LoadEnvironment(env_.get());
        env_->async_hooks()->pop_async_id(1);

        Debug(this, "Loaded environment for worker %llu", thread_id_);
      }

      {
        SealHandleScope seal(isolate_);
        bool more;
        env_->performance_state()->Mark(
            node::performance::NODE_PERFORMANCE_MILESTONE_LOOP_START);
        do {
          if (is_stopped()) break;
          uv_run(&loop_, UV_RUN_DEFAULT);
          if (is_stopped()) break;

          platform->DrainTasks(isolate_);

          more = uv_loop_alive(&loop_);
          if (more && !is_stopped())
            continue;

          EmitBeforeExit(env_.get());

          // Emit `beforeExit` if the loop became alive either after emitting
          // event, or after running some callbacks.
          more = uv_loop_alive(&loop_);
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
      child_port->Close();
      env_->stop_sub_worker_contexts();
      env_->RunCleanup();
      RunAtExit(env_.get());
      if (inspector_started)
        WaitForWorkerInspectorToStop(env_.get());

      {
        Mutex::ScopedLock stopped_lock(stopped_mutex_);
        stopped_ = true;
      }

      env_->RunCleanup();

      // This call needs to be made while the `Environment` is still alive
      // because we assume that it is available for async tracking in the
      // NodePlatform implementation.
      platform->DrainTasks(isolate_);
    }

    env_.reset();
  }

  DisposeIsolate();

  {
    Mutex::ScopedLock lock(mutex_);
    CHECK(thread_exit_async_);
    scheduled_on_thread_stopped_ = true;
    uv_async_send(thread_exit_async_.get());
  }

  Debug(this, "Worker %llu thread stops", thread_id_);
}

void Worker::DisposeIsolate() {
  if (env_) {
    CHECK_NOT_NULL(isolate_);
    Locker locker(isolate_);
    Isolate::Scope isolate_scope(isolate_);
    env_.reset();
  }

  if (isolate_ == nullptr)
    return;

  Debug(this, "Worker %llu dispose isolate", thread_id_);
  CHECK(isolate_data_);
  MultiIsolatePlatform* platform = isolate_data_->platform();
  platform->CancelPendingDelayedTasks(isolate_);

  isolate_data_.reset();
  platform->UnregisterIsolate(isolate_);

  isolate_->Dispose();
  isolate_ = nullptr;
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

    CHECK_EQ(child_port_, nullptr);
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
  CHECK_EQ(child_port_, nullptr);

  // This has most likely already happened within the worker thread -- this
  // is just in case Worker creation failed early.
  DisposeIsolate();

  // Need to run the loop one more time to close the platform's uv_async_t
  uv_run(&loop_, UV_RUN_ONCE);

  CheckedUvLoopClose(&loop_);

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
  // Argument might be a string or URL
  if (args.Length() == 1 && !args[0]->IsNullOrUndefined()) {
    Utf8Value value(
        args.GetIsolate(),
        args[0]->ToString(env->context()).FromMaybe(v8::Local<v8::String>()));
    url.append(value.out(), value.length());
  }
  new Worker(env, args.This(), url);
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
    static_cast<Worker*>(arg)->Run();
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
    CHECK_NE(env_, nullptr);
    stopped_ = true;
    exit_code_ = code;
    if (child_port_ != nullptr)
      child_port_->StopEventLoop();
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
    target->Set(workerString, w->GetFunction(env->context()).ToLocalChecked());
  }

  env->SetMethod(target, "getEnvMessagePort", GetEnvMessagePort);

  auto thread_id_string = FIXED_ONE_BYTE_STRING(env->isolate(), "threadId");
  target->Set(env->context(),
              thread_id_string,
              Number::New(env->isolate(),
                          static_cast<double>(env->thread_id()))).FromJust();
}

}  // anonymous namespace

}  // namespace worker
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(worker, node::worker::InitWorker)
