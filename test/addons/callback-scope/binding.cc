#include "node.h"
#include "v8.h"
#include "uv.h"

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

static v8::Persistent<v8::Promise::Resolver> persistent;

static void Callback(uv_work_t* req, int ignored) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope scope(isolate);
  node::CallbackScope callback_scope(isolate, v8::Object::New(isolate), {0, 0});

  v8::Local<v8::Promise::Resolver> local =
      v8::Local<v8::Promise::Resolver>::New(isolate, persistent);
  local->Resolve(v8::Undefined(isolate));
  delete req;
}

static void TestResolveAsync(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();

  if (persistent.IsEmpty()) {
    persistent.Reset(isolate, v8::Promise::Resolver::New(isolate));

    uv_work_t* req = new uv_work_t;

    uv_queue_work(node::GetCurrentEventLoop(isolate),
                  req,
                  [](uv_work_t*) {},
                  Callback);
  }

  v8::Local<v8::Promise::Resolver> local =
      v8::Local<v8::Promise::Resolver>::New(isolate, persistent);

  args.GetReturnValue().Set(local->GetPromise());
}

void Initialize(v8::Local<v8::Object> exports) {
  NODE_SET_METHOD(exports, "runInCallbackScope", RunInCallbackScope);
  NODE_SET_METHOD(exports, "testResolveAsync", TestResolveAsync);
}

}  // namespace

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)
