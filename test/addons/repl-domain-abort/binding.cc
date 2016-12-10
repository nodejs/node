#include <node.h>
#include <v8.h>

using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::HandleScope;
using v8::Isolate;
using v8::Object;
using v8::Value;

void Method(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  node::MakeCallback(isolate,
                     isolate->GetCurrentContext()->Global(),
                     args[0].As<Function>(),
                     0,
                     NULL);
}

void init(Local<Object> exports) {
  NODE_SET_METHOD(exports, "method", Method);
}

NODE_MODULE(binding, init);
