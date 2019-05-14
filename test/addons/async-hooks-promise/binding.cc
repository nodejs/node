#include <node.h>
#include <v8.h>

namespace {

using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::NewStringType;
using v8::Object;
using v8::Promise;
using v8::String;
using v8::Value;

static void ThrowError(Isolate* isolate, const char* err_msg) {
  Local<String> str = String::NewFromOneByte(
      isolate,
      reinterpret_cast<const uint8_t*>(err_msg),
      NewStringType::kNormal).ToLocalChecked();
  isolate->ThrowException(str);
}

static void GetPromiseField(const FunctionCallbackInfo<Value>& args) {
  auto isolate = args.GetIsolate();

  if (!args[0]->IsPromise())
    return ThrowError(isolate, "arg is not an Promise");

  auto p = args[0].As<Promise>();
  if (p->InternalFieldCount() < 1)
    return ThrowError(isolate, "Promise has no internal field");

  auto l = p->GetInternalField(0);
  args.GetReturnValue().Set(l);
}

inline void Initialize(v8::Local<v8::Object> binding) {
  NODE_SET_METHOD(binding, "getPromiseField", GetPromiseField);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)

}  // anonymous namespace
