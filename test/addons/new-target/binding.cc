#include <node.h>
#include <v8.h>

namespace {

using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::String;
using v8::Object;
using v8::Value;
using v8::Local;

inline void NewClass(const FunctionCallbackInfo<Value>& args) {
  // this != new.target since we are being invoked through super().
  assert(args.IsConstructCall());
  assert(!args.This()->StrictEquals(args.NewTarget()));
}

inline void Initialize(Local<Object> binding) {
  auto isolate = binding->GetIsolate();
  binding->Set(String::NewFromUtf8(isolate, "Class"),
               FunctionTemplate::New(isolate, NewClass)->GetFunction());
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)

}  // anonymous namespace
