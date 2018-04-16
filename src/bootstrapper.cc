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
using v8::Exception;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Message;
using v8::Object;
using v8::Promise;
using v8::PromiseRejectEvent;
using v8::PromiseRejectMessage;
using v8::String;
using v8::Value;
using v8::WeakCallbackInfo;
using v8::WeakCallbackType;

enum ExceptionOrigin {
  kFromPromise = true
};

// The internal field 0 is used by PromiseHook() in async-wrap.cc.
static constexpr int kUnhandledPromiseField = 1;

void SetupProcessObject(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsFunction());
  env->set_push_values_to_array_function(args[0].As<Function>());
}

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

void WeakCallback(
    const v8::WeakCallbackInfo<Persistent<Promise> >& data) {
  Environment* env = Environment::GetCurrent(data.GetIsolate());
  Persistent<Promise>* p = data.GetParameter();
  p->ClearWeak();

  env->SetImmediate([](Environment* env, void* data) {
    HandleScope handle_scope(env->isolate());
    Persistent<Promise>* p = static_cast<Persistent<Promise>*>(data);
    Local<Promise> promise = p->Get(env->isolate());
    Local<Value> err = promise->Result();
    Local<Message> message = Exception::CreateMessage(env->isolate(), err);
    InternalFatalException(env->isolate(), err, message, kFromPromise);
    delete p;
  }, p);
}

void PromiseRejectCallback(PromiseRejectMessage message) {
  static std::atomic<uint64_t> unhandledRejections{0};
  static std::atomic<uint64_t> rejectionsHandledAfter{0};

  Local<Promise> promise = message.GetPromise();
  Isolate* isolate = promise->GetIsolate();
  PromiseRejectEvent event = message.GetEvent();

  Environment* env = Environment::GetCurrent(isolate);
  Local<Function> callback;
  Local<Value> value;

  if (event == v8::kPromiseRejectWithNoHandler) {
    callback = env->promise_reject_unhandled_function();

    // Call fatalException in case the unhandled promise is garbage collected
    // without being handled later on.
    Persistent<Promise>* p = new Persistent<Promise>(env->isolate(), promise);
    p->SetWeak(p, WeakCallback, WeakCallbackType::kFinalizer);
    CHECK_EQ(promise->GetAlignedPointerFromInternalField(
      kUnhandledPromiseField), nullptr);
    promise->SetAlignedPointerInInternalField(kUnhandledPromiseField, p);

    value = message.GetValue();
    if (value.IsEmpty())
      value = Undefined(isolate);

    unhandledRejections++;
  } else if (event == v8::kPromiseHandlerAddedAfterReject) {
    Persistent<Promise>* p = static_cast<Persistent<Promise>*>(
        promise->GetAlignedPointerFromInternalField(
            kUnhandledPromiseField));
    delete p;
    promise->SetAlignedPointerInInternalField(
      kUnhandledPromiseField, nullptr);
    callback = env->promise_reject_handled_function();
    value = Undefined(isolate);

    rejectionsHandledAfter++;
  } else {
    return;
  }

  TRACE_COUNTER2(TRACING_CATEGORY_NODE2(promises, rejections),
                 "rejections",
                 "unhandled", unhandledRejections,
                 "handledAfter", rejectionsHandledAfter);


  Local<Value> args[] = { promise, value };
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
  CHECK(args[1]->IsFunction());

  isolate->SetPromiseRejectCallback(PromiseRejectCallback);
  env->set_promise_reject_unhandled_function(args[0].As<Function>());
  env->set_promise_reject_handled_function(args[1].As<Function>());
}

#define BOOTSTRAP_METHOD(name, fn) env->SetMethod(bootstrapper, #name, fn)

// The Bootstrapper object is an ephemeral object that is used only during
// the bootstrap process of the Node.js environment. A reference to the
// bootstrap object must not be kept around after the bootstrap process
// completes so that it can be gc'd as soon as possible.
void SetupBootstrapObject(Environment* env,
                          Local<Object> bootstrapper) {
  BOOTSTRAP_METHOD(_setupTraceCategoryState, SetupTraceCategoryState);
  BOOTSTRAP_METHOD(_setupProcessObject, SetupProcessObject);
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
