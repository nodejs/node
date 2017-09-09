#include <node.h>
#include <v8.h>

void Method(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  v8::HandleScope scope(isolate);
  args.GetReturnValue().Set(node::ErrnoException(isolate,
                                                10,
                                                "syscall",
                                                "some error msg",
                                                "päth"));
}

void init(v8::Local<v8::Object> exports) {
  NODE_SET_METHOD(exports, "errno", Method);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)
