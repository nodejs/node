#include <v8.h>
#include <node.h>

using namespace v8;

static int c = 0;

void Hello(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(c++);
}

extern "C" void init (Local<Object> target) {
  HandleScope scope(Isolate::GetCurrent());
  NODE_SET_METHOD(target, "hello", Hello);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)
