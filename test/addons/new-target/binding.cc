#include <node.h>
#include <v8.h>

namespace {

inline void NewClass(const v8::FunctionCallbackInfo<v8::Value>& args) {
  // this != new.target since we are being invoked through super().
  assert(args.IsConstructCall());
  assert(!args.This()->StrictEquals(args.NewTarget()));
}

inline void Initialize(v8::Local<v8::Object> binding) {
  auto isolate = binding->GetIsolate();
  auto context = isolate->GetCurrentContext();
  binding->Set(context, v8::String::NewFromUtf8(
        isolate, "Class").ToLocalChecked(),
               v8::FunctionTemplate::New(isolate, NewClass)
                   ->GetFunction(context)
                   .ToLocalChecked()).FromJust();
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)

}  // anonymous namespace
