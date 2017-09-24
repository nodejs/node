#include "node.h"

using namespace v8;

namespace {

  void GetExecutionAsyncId(const FunctionCallbackInfo<Value>& args) {
    args.GetReturnValue().Set(
      node::AsyncHooksGetExecutionAsyncId(args.GetIsolate()));
  }
  
  void GetTriggerAsyncId(const FunctionCallbackInfo<Value>& args) {
    args.GetReturnValue().Set(
      node::AsyncHooksGetTriggerAsyncId(args.GetIsolate()));
  }
  
  void Initialize(Local<Object> exports) {
    NODE_SET_METHOD(exports, "getExecutionAsyncId", GetExecutionAsyncId);
    NODE_SET_METHOD(exports, "getTriggerAsyncId", GetTriggerAsyncId);
  }

}  // namespace

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)
