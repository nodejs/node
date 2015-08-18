#include <node.h>
#include <v8.h>

void Method(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope scope(isolate);
  args.GetReturnValue().Set(v8::String::NewFromUtf8(isolate, "world"));
}

void init(v8::Handle<v8::Object> exports, v8::Handle<v8::Object> module) {
  NODE_SET_METHOD(module, "exports", Method);
}

NODE_MODULE(binding, init);
