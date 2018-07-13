#include "node.h"
#include "env-inl.h"
#include "node_internals.h"
#include "v8.h"

namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Promise;
using v8::PromiseRejectMessage;
using v8::Uint32Array;
using v8::Value;

void SetupProcessObject(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsFunction());
  env->set_push_values_to_array_function(args[0].As<Function>());
}

void RunMicrotasks(const FunctionCallbackInfo<Value>& args) {
  args.GetIsolate()->RunMicrotasks();
}

void SetupNextTick(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsFunction());
  CHECK(args[1]->IsObject());

  env->set_tick_callback_function(args[0].As<Function>());

  env->SetMethod(args[1].As<Object>(), "runMicrotasks", RunMicrotasks);

  // Values use to cross communicate with processNextTick.
  uint32_t* const fields = env->tick_info()->fields();
  uint32_t const fields_count = env->tick_info()->fields_count();

  Local<ArrayBuffer> array_buffer =
      ArrayBuffer::New(env->isolate(), fields, sizeof(*fields) * fields_count);

  args.GetReturnValue().Set(Uint32Array::New(array_buffer, 0, fields_count));
}

void PromiseRejectCallback(PromiseRejectMessage message) {
  Local<Promise> promise = message.GetPromise();
  Isolate* isolate = promise->GetIsolate();
  Local<Value> value = message.GetValue();
  Local<Integer> event = Integer::New(isolate, message.GetEvent());

  Environment* env = Environment::GetCurrent(isolate);
  Local<Function> callback = env->promise_reject_function();

  if (value.IsEmpty())
    value = Undefined(isolate);

  Local<Value> args[] = { event, promise, value };
  Local<Object> process = env->process_object();

  callback->Call(process, arraysize(args), args);
}

void SetupPromises(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  CHECK(args[0]->IsFunction());

  isolate->SetPromiseRejectCallback(PromiseRejectCallback);
  env->set_promise_reject_function(args[0].As<Function>());
}

#define BOOTSTRAP_METHOD(name, fn) env->SetMethod(bootstrapper, #name, fn)

// The Bootstrapper object is an ephemeral object that is used only during
// the bootstrap process of the Node.js environment. A reference to the
// bootstrap object must not be kept around after the bootstrap process
// completes so that it can be gc'd as soon as possible.
void SetupBootstrapObject(Environment* env,
                          Local<Object> bootstrapper) {
  BOOTSTRAP_METHOD(_setupProcessObject, SetupProcessObject);
  BOOTSTRAP_METHOD(_setupNextTick, SetupNextTick);
  BOOTSTRAP_METHOD(_setupPromises, SetupPromises);
  BOOTSTRAP_METHOD(_cpuUsage, CPUUsage);
  BOOTSTRAP_METHOD(_hrtime, Hrtime);
  BOOTSTRAP_METHOD(_memoryUsage, MemoryUsage);
  BOOTSTRAP_METHOD(_rawDebug, RawDebug);
}
#undef BOOTSTRAP_METHOD

}  // namespace node
