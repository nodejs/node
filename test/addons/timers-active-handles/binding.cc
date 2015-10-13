#include <node.h>
#include <uv.h>

using namespace v8;

void Method(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  int hCnt = uv_default_loop()->active_handles;
  uv_run(uv_default_loop(), UV_RUN_ONCE);
  args.GetReturnValue().Set(Integer::New(isolate, hCnt));
}

void init(Handle<Object> exports) {
  NODE_SET_METHOD(exports, "run", Method);
}

NODE_MODULE(deasync, init)
