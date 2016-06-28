#include "node.h"
#include "v8.h"

#include "../../../src/util.h"

using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::Value;

namespace {

void MakeCallback(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsObject());
  CHECK(args[1]->IsFunction());
  Isolate* isolate = args.GetIsolate();
  Local<Object> recv = args[0].As<Object>();
  Local<Function> method = args[1].As<Function>();

  node::MakeCallback(isolate, recv, method, 0, nullptr);
}

void Initialize(Local<Object> target) {
  NODE_SET_METHOD(target, "makeCallback", MakeCallback);
}

}  // namespace

NODE_MODULE(binding, Initialize)
