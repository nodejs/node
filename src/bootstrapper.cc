#include "node.h"
#include "env-inl.h"
#include "node_internals.h"
#include "v8.h"

#include <atomic>

namespace node {

using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::kPromiseHandlerAddedAfterReject;
using v8::kPromiseRejectAfterResolved;
using v8::kPromiseRejectWithNoHandler;
using v8::kPromiseResolveAfterResolved;
using v8::Local;
using v8::MaybeLocal;
using v8::Number;
using v8::Object;
using v8::Promise;
using v8::PromiseRejectEvent;
using v8::PromiseRejectMessage;
using v8::String;
using v8::Value;

void RunMicrotasks(const FunctionCallbackInfo<Value>& args) {
  args.GetIsolate()->RunMicrotasks();
}

void SetupNextTick(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();

  CHECK(args[0]->IsFunction());

  env->set_tick_callback_function(args[0].As<Function>());

  Local<Function> run_microtasks_fn =
      env->NewFunctionTemplate(RunMicrotasks)->GetFunction(context)
          .ToLocalChecked();
  run_microtasks_fn->SetName(FIXED_ONE_BYTE_STRING(isolate, "runMicrotasks"));

  Local<Value> ret[] = {
    env->tick_info()->fields().GetJSArray(),
    run_microtasks_fn
  };

  args.GetReturnValue().Set(Array::New(isolate, ret, arraysize(ret)));
}

void PromiseRejectCallback(PromiseRejectMessage message) {
  static std::atomic<uint64_t> unhandledRejections{0};
  static std::atomic<uint64_t> rejectionsHandledAfter{0};

  Local<Promise> promise = message.GetPromise();
  Isolate* isolate = promise->GetIsolate();
  PromiseRejectEvent event = message.GetEvent();

  Environment* env = Environment::GetCurrent(isolate);

  if (env == nullptr) return;

  Local<Function> callback = env->promise_handler_function();
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
  MaybeLocal<Value> ret = callback->Call(env->context(),
                                         Undefined(isolate),
                                         arraysize(args),
                                         args);

  if (!ret.IsEmpty() && ret.ToLocalChecked()->IsTrue())
    env->tick_info()->promise_rejections_toggle_on();
}

void SetupPromises(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  CHECK(args[0]->IsFunction());
  CHECK(args[1]->IsObject());

  Local<Object> constants = args[1].As<Object>();

  NODE_DEFINE_CONSTANT(constants, kPromiseRejectWithNoHandler);
  NODE_DEFINE_CONSTANT(constants, kPromiseHandlerAddedAfterReject);
  NODE_DEFINE_CONSTANT(constants, kPromiseResolveAfterResolved);
  NODE_DEFINE_CONSTANT(constants, kPromiseRejectAfterResolved);

  isolate->SetPromiseRejectCallback(PromiseRejectCallback);
  env->set_promise_handler_function(args[0].As<Function>());
}

#define BOOTSTRAP_METHOD(name, fn) env->SetMethod(bootstrapper, #name, fn)

// The Bootstrapper object is an ephemeral object that is used only during
// the bootstrap process of the Node.js environment. A reference to the
// bootstrap object must not be kept around after the bootstrap process
// completes so that it can be gc'd as soon as possible.
void SetupBootstrapObject(Environment* env,
                          Local<Object> bootstrapper) {
  BOOTSTRAP_METHOD(_setupNextTick, SetupNextTick);
  BOOTSTRAP_METHOD(_setupPromises, SetupPromises);
}
#undef BOOTSTRAP_METHOD

}  // namespace node
