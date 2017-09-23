#include "node.h"
#include "v8.h"

#include <assert.h>

using namespace v8;

namespace {

void MakeCallback(const FunctionCallbackInfo<Value>& args) {
  assert(args[0]->IsObject());
  assert(args[1]->IsFunction());
  Isolate* isolate = args.GetIsolate();
  Local<Object> recv = args[0].As<Object>();
  Local<Function> method = args[1].As<Function>();

  node::MakeCallback(isolate, recv, method, 0, nullptr);
}

void Initialize(Local<Object> exports) {
  NODE_SET_METHOD(exports, "makeCallback", MakeCallback);
}

}  // namespace

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)
