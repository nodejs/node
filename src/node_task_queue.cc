#include "async_wrap.h"
#include "env-inl.h"
#include "node.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_internals.h"
#include "node_process-inl.h"
#include "util-inl.h"
#include "v8.h"

#include <atomic>

namespace node {

using errors::TryCatchScope;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Just;
using v8::kPromiseHandlerAddedAfterReject;
using v8::kPromiseRejectAfterResolved;
using v8::kPromiseRejectWithNoHandler;
using v8::kPromiseResolveAfterResolved;
using v8::Local;
using v8::Maybe;
using v8::Number;
using v8::Object;
using v8::Promise;
using v8::PromiseRejectEvent;
using v8::PromiseRejectMessage;
using v8::Value;

static Maybe<double> GetAssignedPromiseAsyncId(Environment* env,
                                               Local<Promise> promise,
                                               Local<Value> id_symbol) {
  Local<Value> maybe_async_id;
  if (!promise->Get(env->context(), id_symbol).ToLocal(&maybe_async_id)) {
    return v8::Just(AsyncWrap::kInvalidAsyncId);
  }
  return maybe_async_id->IsNumber()
      ? maybe_async_id->NumberValue(env->context())
      : v8::Just(AsyncWrap::kInvalidAsyncId);
}

void PromiseRejectCallback(PromiseRejectMessage message) {
  static std::atomic<uint64_t> unhandledRejections{0};
  static std::atomic<uint64_t> rejectionsHandledAfter{0};

  Local<Promise> promise = message.GetPromise();
  Isolate* isolate = Isolate::GetCurrent();
  PromiseRejectEvent event = message.GetEvent();

  Environment* env = Environment::GetCurrent(isolate);

  if (env == nullptr || !env->can_call_into_js()) return;

  Local<Function> callback = env->promise_reject_callback();
  // The promise is rejected before JS land calls SetPromiseRejectCallback
  // to initializes the promise reject callback during bootstrap.
  CHECK(!callback.IsEmpty());

  Local<Value> value;
  Local<Value> type = Number::New(env->isolate(), event);

  if (event == kPromiseRejectWithNoHandler) {
    value = message.GetValue();
    unhandledRejections++;
    TRACE_COUNTER2(TRACING_CATEGORY_NODE2(promises, rejections),
                  "rejections",
                  "unhandled", unhandledRejections,
                  "handledAfter", rejectionsHandledAfter);
  } else if (event == kPromiseHandlerAddedAfterReject) {
    // If this notification was triggered by ObservePromise's internal .then()
    // call, suppress it so the promise remains in pendingUnhandledRejections
    // and unhandledRejection still fires.
    if (env->observing_promise()) return;
    value = Undefined(isolate);
    rejectionsHandledAfter++;
    TRACE_COUNTER2(TRACING_CATEGORY_NODE2(promises, rejections),
                  "rejections",
                  "unhandled", unhandledRejections,
                  "handledAfter", rejectionsHandledAfter);
  } else if (event == kPromiseResolveAfterResolved) {
    value = message.GetValue();
  } else if (event == kPromiseRejectAfterResolved) {
    value = message.GetValue();
  } else {
    return;
  }

  if (value.IsEmpty()) {
    value = Undefined(isolate);
  }

  Local<Value> args[] = { type, promise, value };

  double async_id = AsyncWrap::kInvalidAsyncId;
  double trigger_async_id = AsyncWrap::kInvalidAsyncId;
  TryCatchScope try_catch(env);

  if (!GetAssignedPromiseAsyncId(env, promise, env->async_id_symbol())
          .To(&async_id)) return;
  if (!GetAssignedPromiseAsyncId(env, promise, env->trigger_async_id_symbol())
          .To(&trigger_async_id)) return;

  Local<Object> promise_as_obj = promise;
  if (async_id != AsyncWrap::kInvalidAsyncId &&
      trigger_async_id != AsyncWrap::kInvalidAsyncId) {
    env->async_hooks()->push_async_context(
        async_id, trigger_async_id, &promise_as_obj);
  }

  USE(callback->Call(
      env->context(), Undefined(isolate), arraysize(args), args));

  if (async_id != AsyncWrap::kInvalidAsyncId &&
      trigger_async_id != AsyncWrap::kInvalidAsyncId &&
      env->execution_async_id() == async_id) {
    // This condition might not be true if async_hooks was enabled during
    // the promise callback execution.
    env->async_hooks()->pop_async_context(async_id);
  }

  // V8 does not expect this callback to have a scheduled exceptions once it
  // returns, so we print them out in a best effort to do something about it
  // without failing silently and without crashing the process.
  if (try_catch.HasCaught() && !try_catch.HasTerminated()) {
    fprintf(stderr, "Exception in PromiseRejectCallback:\n");
    PrintCaughtException(isolate, env->context(), try_catch);
  }
}
namespace task_queue {

static void EnqueueMicrotask(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  CHECK(args[0]->IsFunction());

  isolate->GetCurrentContext()->GetMicrotaskQueue()
      ->EnqueueMicrotask(isolate, args[0].As<Function>());
}

static void RunMicrotasks(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  env->context()->GetMicrotaskQueue()->PerformCheckpoint(env->isolate());
}

static void SetTickCallback(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsFunction());
  env->set_tick_callback_function(args[0].As<Function>());
}

static void SetPromiseRejectCallback(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsFunction());
  env->set_promise_reject_callback(args[0].As<Function>());
}

static void ObservePromise(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsPromise());
  CHECK(args[1]->IsFunction());
  CHECK(args[2]->IsFunction());

  Local<Promise> promise = args[0].As<Promise>();
  Local<Function> on_fulfilled = args[1].As<Function>();
  Local<Function> on_rejected = args[2].As<Function>();

  bool was_handled = promise->HasHandler();

  // Set flag BEFORE .Then() so that if V8 fires kPromiseHandlerAddedAfterReject
  // synchronously (because the promise is already rejected), PromiseRejectCallback
  // suppresses it and the promise stays in pendingUnhandledRejections.
  env->set_observing_promise(true);

  Local<Promise> derived;
  if (!promise->Then(env->context(), on_fulfilled, on_rejected)
           .ToLocal(&derived)) {
    env->set_observing_promise(false);
    return;
  }

  env->set_observing_promise(false);

  // The derived promise from .then() should never itself trigger unhandled
  // rejection warnings — it's an internal observer chain.
  derived->MarkAsHandled();

  // Restore the original unhandled state so unhandledRejection still fires.
  // Only clear if it wasn't already handled by a real handler before we observed.
  if (!was_handled) {
    promise->MarkAsUnhandled();
  }
}

static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  SetMethod(context, target, "enqueueMicrotask", EnqueueMicrotask);
  SetMethod(context, target, "setTickCallback", SetTickCallback);
  SetMethod(context, target, "runMicrotasks", RunMicrotasks);
  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(isolate, "tickInfo"),
              env->tick_info()->fields().GetJSArray()).Check();

  Local<Object> events = Object::New(isolate);
  NODE_DEFINE_CONSTANT(events, kPromiseRejectWithNoHandler);
  NODE_DEFINE_CONSTANT(events, kPromiseHandlerAddedAfterReject);
  NODE_DEFINE_CONSTANT(events, kPromiseResolveAfterResolved);
  NODE_DEFINE_CONSTANT(events, kPromiseRejectAfterResolved);

  target->Set(env->context(),
              FIXED_ONE_BYTE_STRING(isolate, "promiseRejectEvents"),
              events).Check();
  SetMethod(
      context, target, "setPromiseRejectCallback", SetPromiseRejectCallback);
  SetMethod(context, target, "observePromise", ObservePromise);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(EnqueueMicrotask);
  registry->Register(SetTickCallback);
  registry->Register(RunMicrotasks);
  registry->Register(SetPromiseRejectCallback);
  registry->Register(ObservePromise);
}

}  // namespace task_queue
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(task_queue, node::task_queue::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(task_queue,
                                node::task_queue::RegisterExternalReferences)
