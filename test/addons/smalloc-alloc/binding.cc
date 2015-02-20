#include <node.h>
#include <smalloc.h>
#include <v8.h>

using namespace v8;

void Alloc(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Object> obj = Object::New(isolate);
  size_t len = args[0]->Uint32Value();
  node::smalloc::Alloc(isolate, obj, len);
  args.GetReturnValue().Set(obj);
}

void Dispose(const FunctionCallbackInfo<Value>& args) {
  node::smalloc::AllocDispose(args.GetIsolate(), args[0].As<Object>());
}

void HasExternalData(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(
      node::smalloc::HasExternalData(args.GetIsolate(), args[0].As<Object>()));
}

void init(Handle<Object> target) {
  NODE_SET_METHOD(target, "alloc", Alloc);
  NODE_SET_METHOD(target, "dispose", Dispose);
  NODE_SET_METHOD(target, "hasExternalData", HasExternalData);
}

NODE_MODULE(binding, init);
