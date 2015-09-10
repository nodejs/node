#include <node.h>
#include <uv.h>

using namespace v8;

void Method(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  uv_run(uv_default_loop(), UV_RUN_ONCE);
  args.GetReturnValue().Set(Undefined(isolate));
}

void init(Handle<Object> exports) {
  NODE_SET_METHOD(exports, "run", Method);
}

NODE_MODULE(deasync, init)
