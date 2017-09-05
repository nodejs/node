#include "node.h"

namespace {

using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::Value;

void GetExecutionAsyncId(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(
    node::AsyncHooksGetExecutionAsyncId(args.GetIsolate()));
}

void GetTriggerAsyncId(const FunctionCallbackInfo<Value>& args) {
  args.GetReturnValue().Set(
    node::AsyncHooksGetTriggerAsyncId(args.GetIsolate()));
}

void EmitAsyncInit(const FunctionCallbackInfo<Value>& args) {
  assert(args[0]->IsObject());
  node::async_uid uid =
      node::EmitAsyncInit(args.GetIsolate(), args[0].As<Object>(), "foobar");
  args.GetReturnValue().Set(uid);
}

void Initialize(Local<Object> exports) {
  NODE_SET_METHOD(exports, "getExecutionAsyncId", GetExecutionAsyncId);
  NODE_SET_METHOD(exports, "getTriggerAsyncId", GetTriggerAsyncId);
  NODE_SET_METHOD(exports, "emitAsyncInit", EmitAsyncInit);
}

}  // namespace

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)
