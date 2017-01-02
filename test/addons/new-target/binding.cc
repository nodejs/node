#include <node.h>
#include <v8.h>

namespace {

inline void NewClass(const v8::FunctionCallbackInfo<v8::Value>&) {}

inline void Initialize(v8::Local<v8::Object> binding) {
  auto isolate = binding->GetIsolate();
  binding->Set(v8::String::NewFromUtf8(isolate, "Class"),
               v8::FunctionTemplate::New(isolate, NewClass)->GetFunction());
}

NODE_MODULE(binding, Initialize)

}  // anonymous namespace
