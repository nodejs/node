#include <node.h>
#include <v8.h>

using v8::FunctionCallbackInfo;
using v8::Value;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::FunctionTemplate;

namespace {

inline void NewClass(const FunctionCallbackInfo<vValue>&) {}

inline void Initialize(Local<Object> binding) {
  auto isolate = binding->GetIsolate();
  binding->Set(String::NewFromUtf8(isolate, "Class"),
               FunctionTemplate::New(isolate, NewClass)->GetFunction());
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)

}  // anonymous namespace
