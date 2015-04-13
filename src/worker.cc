#include "worker.h"

#include "node.h"
#include "node_internals.h"
#include "uv.h"
#include "env.h"
#include "env-inl.h"
#include "v8.h"
#include "libplatform/libplatform.h"
#include "v8-debug.h"
#include "util.h"
#include "util-inl.h"
#include "producer-consumer-queue.h"

#include <new>

namespace node {

using v8::Handle;
using v8::Object;
using v8::FunctionCallbackInfo;
using v8::Value;
using v8::String;
using v8::FunctionTemplate;
using v8::Local;
using v8::Context;
using v8::HandleScope;
using v8::SealHandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Function;
using v8::Persistent;
using v8::Locker;
using v8::TryCatch;

WorkerContext::WorkerContext(Environment* owner_env,
                             Handle<Object> owner_wrapper,
                             uv_loop_t* event_loop,
                             int argc,
                             const char** argv,
                             int exec_argc,
                             const char** exec_argv,
                             bool keep_alive)
    : owner_env_(owner_env),
      worker_isolate_(Isolate::New()),
      owner_wrapper_(owner_env->isolate(), owner_wrapper),
      keep_alive_(keep_alive),
      event_loop_(event_loop),
      owner_notifications_(owner_event_loop(),
                           OwnerNotificationCallback,
                           this),
      worker_notifications_(worker_event_loop(),
                            WorkerNotificationCallback,
                            this),
      argc_(argc),
      argv_(argv),
      exec_argc_(exec_argc),
      exec_argv_(exec_argv) {
  CHECK_NE(event_loop, nullptr);
  CHECK_NE(event_loop, uv_default_loop());
  CHECK_CALLED_FROM_OWNER(this);
  node::Wrap(owner_wrapper, this);

  owner_env->AddSubWorkerContext(this);

  CHECK_EQ(uv_mutex_init(termination_mutex()), 0);
  CHECK_EQ(uv_mutex_init(api_mutex()), 0);
  CHECK_EQ(uv_mutex_init(initialization_mutex()), 0);
  CHECK_EQ(uv_mutex_init(to_owner_messages_mutex()), 0);
  CHECK_EQ(uv_mutex_init(to_worker_messages_mutex()), 0);
}

void WorkerContext::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  HandleScope scope(env->isolate());
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsObject());
  Local<String> entry_module_path = args[0].As<String>();
  bool keep_alive =
      args[1].As<Object>()->Get(env->keep_alive_string())->BooleanValue();

  uv_loop_t* worker_event_loop = static_cast<uv_loop_t*>(
      malloc(sizeof(uv_loop_t)));
  int err = uv_loop_init(worker_event_loop);
  if (err != 0) {
    free(worker_event_loop);
    return env->ThrowUVException(err, "uv_loop_init");
  }

  // Heap allocated cause this stack frame can be gone by the time the worker
  // needs them.
  int argc = 2;
  char** argv = new char*[argc];
  // TODO(petkaantonov) should use correct executable name here
  argv[0] = const_cast<char*>("node");
  argv[1] = ToUtf8Value(env->isolate(), entry_module_path);
  // TODO(petkaantonov) should use the process's real exec args
  int exec_argc = 0;
  char** exec_argv = new char*[exec_argc];

  WorkerContext* context =
      new WorkerContext(env,
                        args.This(),
                        worker_event_loop,
                        argc,
                        const_cast<const char**>(argv),
                        exec_argc,
                        const_cast<const char**>(exec_argv),
                        keep_alive);

  err = uv_thread_create(context->worker_thread(), RunWorkerInstance, context);

  if (err != 0) {
    context->worker_notifications()->Dispose();
    context->Dispose();
    CHECK_EQ(uv_loop_close(worker_event_loop), 0);
    free(worker_event_loop);
    return env->ThrowUVException(err, "uv_thread_create");
  }
}

void WorkerContext::PostMessageToOwner(WorkerMessage* message) {
  if (termination_kind() != NONE) {
    delete message;
    return;
  }

  if (!to_owner_messages_primary_.PushBack(message)) {
    ScopedLock::Mutex lock(to_owner_messages_mutex());
    to_owner_messages_backup_.PushBack(message);
  }
  owner_notifications()->Notify();
}

void WorkerContext::PostMessageToOwner(
    const FunctionCallbackInfo<Value>& args) {
  CHECK_EQ(args.Length(), 3);
  WorkerContext* context = node::Unwrap<WorkerContext>(args.Holder());
  if (context == nullptr)
    return;
  CHECK_CALLED_FROM_WORKER(context);
  CHECK(args[0]->IsString());
  // transferList ignored for now
  CHECK(args[1]->IsArray());
  CHECK(args[2]->IsInt32());
  Local<String> json = args[0].As<String>();
  WorkerMessageType message_type =
      static_cast<WorkerMessageType>(args[2]->Int32Value());
  WorkerMessage* message =
      new WorkerMessage(ToUtf8Value(context->worker_env()->isolate(), json),
                        message_type);
  context->PostMessageToOwner(message);
}

void WorkerContext::PostMessageToWorker(WorkerMessage* message) {
  if (termination_kind() != NONE) {
    delete message;
    return;
  }

  if (!to_worker_messages_primary_.PushBack(message)) {
    ScopedLock::Mutex lock(to_worker_messages_mutex());
    to_worker_messages_backup_.PushBack(message);
  }

  worker_notifications()->Notify();
}

void WorkerContext::PostMessageToWorker(
    const FunctionCallbackInfo<Value>& args) {
  WorkerContext* context = node::Unwrap<WorkerContext>(args.Holder());
  if (context == nullptr)
    return;
  CHECK_CALLED_FROM_OWNER(context);
  CHECK(args[0]->IsString());
  // transferList ignored for now
  CHECK(args[1]->IsArray());
  CHECK(args[2]->IsInt32());
  Local<String> json = args[0].As<String>();
  WorkerMessageType message_type =
      static_cast<WorkerMessageType>(args[2]->Int32Value());
  Environment* env = Environment::GetCurrent(args);
  WorkerMessage* message = new WorkerMessage(ToUtf8Value(env->isolate(),
                                                         json),
                                             message_type);
  context->PostMessageToWorker(message);
}

void WorkerContext::WrapperNew(const FunctionCallbackInfo<Value>& args) {
  HandleScope handle_scope(args.GetIsolate());
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  if (!args.IsConstructCall())
    return env->ThrowError("Illegal invocation");
  if (args.Length() < 1)
    return env->ThrowError("entryModulePath is required");

  Local<Object> recv = args.This().As<Object>();
  Local<Function> init = recv->Get(env->worker_init_symbol()).As<Function>();
  CHECK(init->IsFunction());
  Local<Value>* argv = new Local<Value>[args.Length()];
  for (int i = 0; i < args.Length(); ++i)
    argv[i] = args[i];
  init->Call(recv, args.Length(), argv);
  delete[] argv;
}

// MSVC work-around for intrusive lists.
namespace worker {
// Deleting WorkerContexts in response to their notification signals
// will cause use-after-free inside libuv. So the final `delete this`
// call must be made somewhere else.
static ListHead <WorkerContext,
                 &WorkerContext::cleanup_queue_member_> cleanup_queue_;
static uv_async_t global_cleanup_signal_async_;
static uv_mutex_t global_cleanup_mutex_;
}

void WorkerContext::QueueGlobalCleanup() {
  using namespace worker;  // NOLINT
  {
    ScopedLock::Mutex lock(&global_cleanup_mutex_);
    cleanup_queue_.PushBack(this);
  }
  uv_async_send(&global_cleanup_signal_async_);
}

void WorkerContext::GlobalCleanupCallback(uv_async_t* handle) {
  using namespace worker;  // NOLINT
  ScopedLock::Mutex lock(&global_cleanup_mutex_);
  while (WorkerContext* context = cleanup_queue_.PopFront())
    delete context;
}

void WorkerContext::InitWorkersOnce() {
  using namespace worker;  // NOLINT
  CHECK_EQ(0, uv_async_init(uv_default_loop(),
                         &global_cleanup_signal_async_,
                         GlobalCleanupCallback));
  uv_unref(reinterpret_cast<uv_handle_t*>(&global_cleanup_signal_async_));
  CHECK_EQ(uv_mutex_init(&global_cleanup_mutex_), 0);
}

void WorkerContext::Initialize(Handle<Object> target,
                        Handle<Value> unused,
                        Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  if (!node::experimental_workers)
    return env->ThrowError("Experimental workers are disabled");

  static uv_once_t init_once = UV_ONCE_INIT;
  uv_once(&init_once, InitWorkersOnce);

  Local<FunctionTemplate> t = env->NewFunctionTemplate(WorkerContext::New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "WorkerBinding"));

  env->SetProtoMethod(t, "terminate", Terminate);
  env->SetProtoMethod(t, "postMessage", PostMessageToWorker);
  env->SetProtoMethod(t, "ref", Ref);
  env->SetProtoMethod(t, "unref", Unref);

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "WorkerContext"),
              t->GetFunction());

#define EXPORT_MESSAGE_TYPE(type)                                             \
  do {                                                                        \
  target->ForceSet(FIXED_ONE_BYTE_STRING(env->isolate(), #type),              \
                   Integer::New(env->isolate(),                               \
                                static_cast<int>(WorkerMessageType::type)),   \
                   v8::ReadOnly);                                             \
  } while (0);
  WORKER_MESSAGE_TYPES(EXPORT_MESSAGE_TYPE)
#undef EXPORT_MESSAGE_TYPE

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "initSymbol"),
              env->worker_init_symbol());

  Local<FunctionTemplate> ctor_t =
      FunctionTemplate::New(env->isolate(), WorkerContext::WrapperNew);
  ctor_t->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "Worker"));
  ctor_t->SetLength(1);
  ctor_t->GetFunction()->SetName(
      FIXED_ONE_BYTE_STRING(env->isolate(), "Worker"));

  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "JsConstructor"),
              ctor_t->GetFunction());


  if (env->is_worker_instance()) {
    Local<FunctionTemplate> t = FunctionTemplate::New(env->isolate());
    t->InstanceTemplate()->SetInternalFieldCount(1);
    Local<Object> worker_wrapper = t->GetFunction()->NewInstance();

    node::Wrap<WorkerContext>(worker_wrapper, env->worker_context());
    env->worker_context()->set_worker_wrapper(worker_wrapper);

    env->SetMethod(worker_wrapper,
                   "_postMessage",
                   PostMessageToOwner);

    target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "workerWrapper"),
                worker_wrapper);
  }
}

bool WorkerContext::LoopWouldEnd() {
  CHECK_CALLED_FROM_WORKER(this);
  bool is_not_terminated;
  {
    ScopedLock::Mutex lock(termination_mutex());
    is_not_terminated = termination_kind() == NONE;
  }

  if (is_not_terminated) {
    CHECK_NE(worker_env(), nullptr);
    node::EmitBeforeExit(worker_env());
    {
      ScopedLock::Mutex lock(termination_mutex());
      if (termination_kind() == NONE) {
        lock.unlock();
        bool ret = uv_loop_alive(worker_event_loop());
          if (uv_run(worker_event_loop(), UV_RUN_NOWAIT))
            ret = termination_kind() == NONE;
        return ret;
      }
    }
  }
  return false;
}

void WorkerContext::LoopEnded() {
  CHECK_CALLED_FROM_WORKER(this);
  ScopedLock::Mutex lock(termination_mutex());

  if (termination_kind() == NONE) {
    CHECK_NE(worker_env(), nullptr);
    set_termination_kind(NORMAL);
    lock.unlock();
    int exit_code = node::EmitExit(worker_env_);
    set_exit_code(exit_code);
    DisposeWorker(NORMAL);
  } else if (termination_kind() == ABRUPT) {
    lock.unlock();
    DisposeWorker(ABRUPT);
  }
}

void WorkerContext::Ref(const FunctionCallbackInfo<Value>& args) {
  WorkerContext* context = node::Unwrap<WorkerContext>(args.Holder());
  if (context == nullptr)
    return;
  CHECK_CALLED_FROM_OWNER(context);
  context->owner_notifications()->Ref();
}

void WorkerContext::Unref(const FunctionCallbackInfo<Value>& args) {
  WorkerContext* context = node::Unwrap<WorkerContext>(args.Holder());
  if (context == nullptr)
    return;
  CHECK_CALLED_FROM_OWNER(context);
  context->owner_notifications()->Unref();
}

void WorkerContext::Exit(int exit_code) {
  CHECK_CALLED_FROM_WORKER(this);
  CHECK(is_initialized());

  set_exit_code(exit_code);
  ScopedLock::Mutex lock(termination_mutex());

  if (termination_kind() != NONE)
    return;

  set_termination_kind(ABRUPT);
  worker_notifications()->Unref();
  uv_stop(worker_event_loop());
  worker_isolate()->TerminateExecution();
}

void WorkerContext::Terminate(const FunctionCallbackInfo<Value>& args) {
  WorkerContext* context = node::Unwrap<WorkerContext>(args.Holder());
  if (context == nullptr)
    return;
  context->Terminate();
}

void WorkerContext::Terminate(bool should_emit_exit_event) {
  CHECK_CALLED_FROM_OWNER(this);
  ScopedLock::Mutex init_lock(initialization_mutex());
  ScopedLock::Mutex api_lock(api_mutex());
  ScopedLock::Mutex term_lock(termination_mutex());

  if (termination_kind() != NONE)
    return;

  set_termination_kind(ABRUPT);

  if (is_initialized()) {
    should_emit_exit_event_ = should_emit_exit_event;
    worker_isolate()->TerminateExecution();
    worker_notifications()->Notify();
  }
}

bool WorkerContext::ProcessMessageToWorker(Isolate* isolate,
                                           WorkerMessage* message) {
  Local<Value> argv[] = {
    String::NewFromUtf8(isolate, message->payload()),
    Integer::New(isolate, message->type())
  };
  delete message;
  MakeCallback(isolate,
               worker_wrapper(),
               "_onmessage",
               ARRAY_SIZE(argv),
               argv);
  return worker_env()->CanCallIntoJs();
}

void WorkerContext::ProcessMessagesToWorker() {
  CHECK_CALLED_FROM_WORKER(this);

  Isolate* isolate = worker_env()->isolate();
  HandleScope scope(isolate);
  Context::Scope context_scope(worker_env()->context());

  flush_primary_queue:
  uint32_t messages_processed = 0;
  while (WorkerMessage* message = to_worker_messages_primary_.PopFront()) {
    if (!ProcessMessageToWorker(isolate, message))
      return;
    messages_processed++;
  }

  if (messages_processed >= kMaxPrimaryQueueMessages) {
    while (true) {
      WorkerMessage* message = nullptr;
      {
        ScopedLock::Mutex lock(to_worker_messages_mutex());
        message = to_worker_messages_backup_.PopFront();
      }

      if (message == nullptr)
        goto flush_primary_queue;

      if (!ProcessMessageToWorker(isolate, message))
        return;
    }
  }
}

bool WorkerContext::ProcessMessageToOwner(Isolate* isolate,
                                          WorkerMessage* message) {
  Local<Value> argv[] = {
    String::NewFromUtf8(isolate, message->payload()),
    Integer::New(isolate, message->type())
  };
  delete message;
  MakeCallback(isolate,
               owner_wrapper(),
               "_onmessage",
               ARRAY_SIZE(argv),
               argv);
  return owner_env()->CanCallIntoJs();
}

void WorkerContext::ProcessMessagesToOwner() {
  CHECK_CALLED_FROM_OWNER(this);
  Isolate* isolate = owner_env()->isolate();
  HandleScope scope(isolate);
  Context::Scope context_scope(owner_env()->context());

  flush_primary_queue:
  uint32_t messages_processed = 0;
  while (WorkerMessage* message = to_owner_messages_primary_.PopFront()) {
    if (!ProcessMessageToOwner(isolate, message))
      return;
    messages_processed++;
  }

  if (messages_processed >= kMaxPrimaryQueueMessages) {
    while (true) {
      WorkerMessage* message = nullptr;
      {
        ScopedLock::Mutex lock(to_owner_messages_mutex());
        message = to_owner_messages_backup_.PopFront();
      }

      if (message == nullptr)
        goto flush_primary_queue;

      if (!ProcessMessageToOwner(isolate, message))
        return;
    }
  }
}

void WorkerContext::Dispose() {
  CHECK_CALLED_FROM_OWNER(this);

  owner_notifications()->Dispose();

  node::ClearWrap(owner_wrapper());
  owner_wrapper_.Reset();

  uv_mutex_destroy(termination_mutex());
  uv_mutex_destroy(api_mutex());
  uv_mutex_destroy(initialization_mutex());
  uv_mutex_destroy(to_owner_messages_mutex());
  uv_mutex_destroy(to_worker_messages_mutex());

  delete[] argv_;
  delete[] exec_argv_;

  while (WorkerMessage* message = to_worker_messages_backup_.PopFront())
    delete message;
  while (WorkerMessage* message = to_owner_messages_backup_.PopFront())
    delete message;

  owner_env()->RemoveSubWorkerContext(this);
  QueueGlobalCleanup();
}

void WorkerContext::DisposeWorker(TerminationKind termination_kind) {
  CHECK_CALLED_FROM_WORKER(this);
  CHECK(termination_kind != NONE);

  if (worker_env() != nullptr) {
    worker_env()->TerminateSubWorkers();
    worker_env()->CleanupHandles();
    if (!worker_wrapper_.IsEmpty()) {
      node::ClearWrap(worker_wrapper());
      worker_wrapper_.Reset();
    } else {
      CHECK(termination_kind == ABRUPT);
    }
  }

  worker_notifications()->Dispose();

  if (worker_env() != nullptr) {
    worker_env()->Dispose();
    worker_env_ = nullptr;
  }
  pending_owner_cleanup_ = true;
  owner_notifications()->Notify();
}

void WorkerContext::WorkerNotificationCallback(void* arg) {
  WorkerContext* context = static_cast<WorkerContext*>(arg);
  CHECK_CALLED_FROM_WORKER(context);
  if (context->termination_kind() == NONE)
      context->ProcessMessagesToWorker();
  else
      uv_stop(context->worker_event_loop());
}

void WorkerContext::OwnerNotificationCallback(void* arg) {
  WorkerContext* context = static_cast<WorkerContext*>(arg);
  CHECK_CALLED_FROM_OWNER(context);
  if (context->owner_env()->CanCallIntoJs())
    context->ProcessMessagesToOwner();
  context->DisposeIfNecessary();
}

void WorkerContext::DisposeIfNecessary() {
  CHECK_CALLED_FROM_OWNER(this);
  if (pending_owner_cleanup_) {
    pending_owner_cleanup_ = false;
    uv_thread_join(worker_thread());
    int loop_was_not_empty = uv_loop_close(worker_event_loop());
    // TODO(petkaantonov) replace this with a message that also reports
    // what is still on the loop if https://github.com/libuv/libuv/pull/291
    // is accepted.
    CHECK(!loop_was_not_empty);
    free(worker_event_loop());
    event_loop_ = nullptr;

    if (should_emit_exit_event_) {
      Environment* env = owner_env();
      HandleScope scope(env->isolate());
      Context::Scope context_scope(env->context());
      Local<Value> argv[] = {
        Integer::New(env->isolate(), exit_code())
      };
      MakeCallback(env->isolate(),
                   owner_wrapper(),
                   "_onexit",
                   ARRAY_SIZE(argv),
                   argv);
    }
    Dispose();
  }
}

void WorkerContext::Run() {
  CHECK_CALLED_FROM_WORKER(this);

  if (!keep_alive_)
    worker_notifications()->Unref();

  {
    ScopedLock::Mutex lock(initialization_mutex());
    if (!JsExecutionAllowed()) {
      DisposeWorker(ABRUPT);
    } else {
      Isolate::Scope isolate_scope(worker_isolate());
      Locker locker(worker_isolate());

      // Based on the lowest common denominator, OS X, where
      // pthreads get by default 512kb of stack space.
      // On Linux it is 2MB and on Windows 1MB.
      static const size_t kStackSize = 512 * 1024;
      // Currently a multiplier of 2 is enough but I left some room to grow.
      static const size_t kStackAlreadyUsedGuess = 8 * 1024;
      uint32_t stack_pointer_source;
      uintptr_t stack_limit = reinterpret_cast<uintptr_t>(
          &stack_pointer_source - ((kStackSize - kStackAlreadyUsedGuess) /
                                  sizeof(stack_pointer_source)));
      worker_isolate()->SetStackLimit(stack_limit);

      HandleScope handle_scope(worker_isolate());
      Local<Context> context = Context::New(worker_isolate());
      Context::Scope context_scope(context);
      Environment* env = node::CreateEnvironment(worker_isolate(),
                                                 context,
                                                 this);
      node::LoadEnvironment(env);
      Local<Function> entry =
          env->process_object()->Get(
              FIXED_ONE_BYTE_STRING(env->isolate(), "_runMain")).As<Function>();
      set_initialized();
      // Must not hold any locks when calling into JS
      lock.unlock();
      entry->Call(env->process_object(), 0, nullptr);
      SealHandleScope seal(worker_isolate());
      bool more;
      {
        do {
          v8::platform::PumpMessageLoop(platform(), worker_isolate());
          env->ProcessNotifications();
          more = uv_run(worker_event_loop(), UV_RUN_ONCE);
          if (!more) {
            v8::platform::PumpMessageLoop(platform(), worker_isolate());
            more = LoopWouldEnd();
          }
        } while (more && JsExecutionAllowed());
      }
      env->set_trace_sync_io(false);
      LoopEnded();
    }
  }
  worker_isolate()->Dispose();
}

// Entry point for worker_context instance threads
void WorkerContext::RunWorkerInstance(void* arg) {
  static_cast<WorkerContext*>(arg)->Run();
}

void WorkerContext::set_worker_wrapper(Local<Object> worker_wrapper) {
  CHECK_CALLED_FROM_WORKER(this);
  new(&worker_wrapper_) Persistent<Object>(worker_env()->isolate(),
                                            worker_wrapper);
}

Local<Object> WorkerContext::owner_wrapper() {
  CHECK_CALLED_FROM_OWNER(this);
  return node::PersistentToLocal(owner_env()->isolate(), owner_wrapper_);
}

Local<Object> WorkerContext::worker_wrapper() {
  CHECK_CALLED_FROM_WORKER(this);
  return node::PersistentToLocal(worker_env()->isolate(), worker_wrapper_);
}

uv_loop_t* WorkerContext::owner_event_loop() const {
  return owner_env()->event_loop();
}

uv_thread_t* WorkerContext::owner_thread() {
  if (owner_env()->is_worker_instance())
    return owner_env()->worker_context()->worker_thread();
  else
    return main_thread();
}

v8::Platform* WorkerContext::platform() {
  return node::default_platform;
}

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(WorkerContext,
                                  node::WorkerContext::Initialize)
