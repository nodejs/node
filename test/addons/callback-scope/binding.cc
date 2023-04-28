#include "node.h"
#include "v8.h"
#include "uv.h"

#include <assert.h>
#include <vector>
#include <memory>

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

static void Callback(uv_work_t* req, int ignored) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope scope(isolate);
  node::CallbackScope callback_scope(isolate, v8::Object::New(isolate),
                                     node::async_context{0, 0});
  std::unique_ptr<v8::Global<v8::Promise::Resolver>> persistent {
      static_cast<v8::Global<v8::Promise::Resolver>*>(req->data) };
  v8::Local<v8::Promise::Resolver> local = persistent->Get(isolate);
  local->Resolve(isolate->GetCurrentContext(),
                 v8::Undefined(isolate)).ToChecked();
  delete req;
}

static void TestResolveAsync(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();

  v8::Global<v8::Promise::Resolver>* persistent =
      new v8::Global<v8::Promise::Resolver>(
          isolate,
          v8::Promise::Resolver::New(
              isolate->GetCurrentContext()).ToLocalChecked());

  uv_work_t* req = new uv_work_t;
  req->data = static_cast<void*>(persistent);

  uv_queue_work(node::GetCurrentEventLoop(isolate),
                req,
                [](uv_work_t*) {},
                Callback);

  v8::Local<v8::Promise::Resolver> local = persistent->Get(isolate);

  args.GetReturnValue().Set(local->GetPromise());
}

void Initialize(v8::Local<v8::Object> exports) {
  NODE_SET_METHOD(exports, "runInCallbackScope", RunInCallbackScope);
  NODE_SET_METHOD(exports, "testResolveAsync", TestResolveAsync);
}

}  // anonymous namespace

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)
