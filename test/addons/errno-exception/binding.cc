#include <node.h>
#include <v8.h>

using namespace v8;
void Method(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  HandleScope scope(isolate);
  args.GetReturnValue().Set(node::ErrnoException(isolate,
                                                10,
                                                "syscall",
                                                "some error msg",
                                                "päth"));
}

void init(Local<Object> exports) {
  NODE_SET_METHOD(exports, "errno", Method);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)
