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

void SetupTraceCategoryState(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsFunction());
  env->set_trace_category_state_function(args[0].As<Function>());
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

  Local<Array> ret = Array::New(isolate, 2);
  ret->Set(context, 0, env->tick_info()->fields().GetJSArray()).FromJust();
  ret->Set(context, 1, run_microtasks_fn).FromJust();

  args.GetReturnValue().Set(ret);
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
  BOOTSTRAP_METHOD(_setupTraceCategoryState, SetupTraceCategoryState);
  BOOTSTRAP_METHOD(_setupNextTick, SetupNextTick);
  BOOTSTRAP_METHOD(_setupPromises, SetupPromises);
  BOOTSTRAP_METHOD(_chdir, Chdir);
  BOOTSTRAP_METHOD(_cpuUsage, CPUUsage);
  BOOTSTRAP_METHOD(_hrtime, Hrtime);
  BOOTSTRAP_METHOD(_hrtimeBigInt, HrtimeBigInt);
  BOOTSTRAP_METHOD(_memoryUsage, MemoryUsage);
  BOOTSTRAP_METHOD(_rawDebug, RawDebug);
  BOOTSTRAP_METHOD(_umask, Umask);

#if defined(__POSIX__) && !defined(__ANDROID__) && !defined(__CloudABI__)
  if (env->is_main_thread()) {
    BOOTSTRAP_METHOD(_initgroups, InitGroups);
    BOOTSTRAP_METHOD(_setegid, SetEGid);
    BOOTSTRAP_METHOD(_seteuid, SetEUid);
    BOOTSTRAP_METHOD(_setgid, SetGid);
    BOOTSTRAP_METHOD(_setuid, SetUid);
    BOOTSTRAP_METHOD(_setgroups, SetGroups);
  }
#endif  // __POSIX__ && !defined(__ANDROID__) && !defined(__CloudABI__)

  Local<String> should_abort_on_uncaught_toggle =
      FIXED_ONE_BYTE_STRING(env->isolate(), "_shouldAbortOnUncaughtToggle");
  CHECK(bootstrapper->Set(env->context(),
                       should_abort_on_uncaught_toggle,
                       env->should_abort_on_uncaught_toggle().GetJSArray())
                           .FromJust());
}
#undef BOOTSTRAP_METHOD

namespace symbols {

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
#define V(PropertyName, StringValue)                                        \
    target->Set(env->context(),                                             \
               env->PropertyName()->Name(),                                 \
               env->PropertyName()).FromJust();
  PER_ISOLATE_SYMBOL_PROPERTIES(V)
#undef V
}

}  // namespace symbols
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(symbols, node::symbols::Initialize)
