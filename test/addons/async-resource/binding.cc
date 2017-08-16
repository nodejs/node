#include "node.h"

#include <assert.h>
#include <vector>

namespace {

using node::AsyncResource;
using v8::External;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Value;

void CreateAsyncResource(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  assert(args[0]->IsObject());
  AsyncResource* r;
  if (args[1]->IsInt32()) {
    r = new AsyncResource(isolate, args[0].As<Object>(), "foobär",
                          args[1].As<Integer>()->Value());
  } else {
    r = new AsyncResource(isolate, args[0].As<Object>(), "foobär");
  }

  args.GetReturnValue().Set(
      External::New(isolate, static_cast<void*>(r)));
}

void DestroyAsyncResource(const FunctionCallbackInfo<Value>& args) {
  assert(args[0]->IsExternal());
  auto r = static_cast<AsyncResource*>(args[0].As<External>()->Value());
  delete r;
}

void CallViaFunction(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  assert(args[0]->IsExternal());
  auto r = static_cast<AsyncResource*>(args[0].As<External>()->Value());

  Local<String> name =
      String::NewFromUtf8(isolate, "methöd", v8::NewStringType::kNormal)
      .ToLocalChecked();
  Local<Value> fn =
      r->get_resource()->Get(isolate->GetCurrentContext(), name)
      .ToLocalChecked();
  assert(fn->IsFunction());

  Local<Value> arg = Integer::New(isolate, 42);
  MaybeLocal<Value> ret = r->MakeCallback(fn.As<Function>(), 1, &arg);
  args.GetReturnValue().Set(ret.FromMaybe(Local<Value>()));
}

void CallViaString(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  assert(args[0]->IsExternal());
  auto r = static_cast<AsyncResource*>(args[0].As<External>()->Value());

  Local<String> name =
      String::NewFromUtf8(isolate, "methöd", v8::NewStringType::kNormal)
      .ToLocalChecked();

  Local<Value> arg = Integer::New(isolate, 42);
  MaybeLocal<Value> ret = r->MakeCallback(name, 1, &arg);
  args.GetReturnValue().Set(ret.FromMaybe(Local<Value>()));
}

void CallViaUtf8Name(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  assert(args[0]->IsExternal());
  auto r = static_cast<AsyncResource*>(args[0].As<External>()->Value());

  Local<Value> arg = Integer::New(isolate, 42);
  MaybeLocal<Value> ret = r->MakeCallback("methöd", 1, &arg);
  args.GetReturnValue().Set(ret.FromMaybe(Local<Value>()));
}

void GetAsyncId(const FunctionCallbackInfo<Value>& args) {
  assert(args[0]->IsExternal());
  auto r = static_cast<AsyncResource*>(args[0].As<External>()->Value());
  args.GetReturnValue().Set(r->get_async_id());
}

void GetTriggerAsyncId(const FunctionCallbackInfo<Value>& args) {
  assert(args[0]->IsExternal());
  auto r = static_cast<AsyncResource*>(args[0].As<External>()->Value());
  args.GetReturnValue().Set(r->get_trigger_async_id());
}

void GetResource(const FunctionCallbackInfo<Value>& args) {
  assert(args[0]->IsExternal());
  auto r = static_cast<AsyncResource*>(args[0].As<External>()->Value());
  args.GetReturnValue().Set(r->get_resource());
}

void Initialize(Local<Object> exports) {
  NODE_SET_METHOD(exports, "createAsyncResource", CreateAsyncResource);
  NODE_SET_METHOD(exports, "destroyAsyncResource", DestroyAsyncResource);
  NODE_SET_METHOD(exports, "callViaFunction", CallViaFunction);
  NODE_SET_METHOD(exports, "callViaString", CallViaString);
  NODE_SET_METHOD(exports, "callViaUtf8Name", CallViaUtf8Name);
  NODE_SET_METHOD(exports, "getAsyncId", GetAsyncId);
  NODE_SET_METHOD(exports, "getTriggerAsyncId", GetTriggerAsyncId);
  NODE_SET_METHOD(exports, "getResource", GetResource);
}

}  // namespace

NODE_MODULE(binding, Initialize)
