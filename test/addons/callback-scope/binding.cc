#include "node.h"
#include "v8.h"

#include <assert.h>
#include <vector>

namespace {

void RunInCallbackScope(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();

  assert(args.Length() == 4);
  assert(args[0]->IsObject());
  assert(args[1]->IsNumber());
  assert(args[2]->IsNumber());
  assert(args[3]->IsFunction());

  node::async_context asyncContext = {
    args[1].As<v8::Number>()->Value(),
    args[2].As<v8::Number>()->Value()
  };

  node::CallbackScope scope(isolate, args[0].As<v8::Object>(), asyncContext);
  v8::Local<v8::Function> fn = args[3].As<v8::Function>();

  v8::MaybeLocal<v8::Value> ret =
      fn->Call(isolate->GetCurrentContext(), args[0], 0, nullptr);

  if (!ret.IsEmpty())
    args.GetReturnValue().Set(ret.ToLocalChecked());
}

void Initialize(v8::Local<v8::Object> exports) {
  NODE_SET_METHOD(exports, "runInCallbackScope", RunInCallbackScope);
}

}  // namespace

NODE_MODULE(binding, Initialize)
