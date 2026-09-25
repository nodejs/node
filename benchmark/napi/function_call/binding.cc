#include <v8.h>
#include <node.h>

static int c = 0;

void Hello(const v8::FunctionCallbackInfo<v8::Value>& args) {
  args.GetReturnValue().Set(c++);
}

void Initialize(v8::Local<v8::Object> target,
                v8::Local<v8::Value> module,
                void* data) {
  NODE_SET_METHOD(target, "hello", Hello);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)
