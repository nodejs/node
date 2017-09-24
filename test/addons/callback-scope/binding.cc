#include "node.h"
#include "v8.h"
#include "uv.h"

#include <assert.h>
#include <vector>

using namespace v8;
namespace {

  void RunInCallbackScope(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
  
    assert(args.Length() == 4);
    assert(args[0]->IsObject());
    assert(args[1]->IsNumber());
    assert(args[2]->IsNumber());
    assert(args[3]->IsFunction());
  
    node::async_context asyncContext = {
      args[1].As<Number>()->Value(),
      args[2].As<Number>()->Value()
    };
  
    node::CallbackScope scope(isolate, args[0].As<Object>(), asyncContext);
    Local<Function> fn = args[3].As<Function>();
  
    MaybeLocal<Value> ret =
        fn->Call(isolate->GetCurrentContext(), args[0], 0, nullptr);
  
    if (!ret.IsEmpty())
      args.GetReturnValue().Set(ret.ToLocalChecked());
  }
  
  static Persistent<Promise::Resolver> persistent;
  
  static void Callback(uv_work_t* req, int ignored) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    node::CallbackScope callback_scope(isolate, Object::New(isolate), {0, 0});
  
    Local<Promise::Resolver> local =
        Local<Promise::Resolver>::New(isolate, persistent);
    local->Resolve(Undefined(isolate));
    delete req;
  }
  
  static void TestResolveAsync(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();
  
    if (persistent.IsEmpty()) {
      persistent.Reset(isolate, Promise::Resolver::New(isolate));
  
      uv_work_t* req = new uv_work_t;
  
      uv_queue_work(uv_default_loop(), req, [](uv_work_t*) {}, Callback);
    }
  
    Local<Promise::Resolver> local =
        Local<Promise::Resolver>::New(isolate, persistent);
  
    args.GetReturnValue().Set(local->GetPromise());
  }
  
  void Initialize(Local<Object> exports) {
    NODE_SET_METHOD(exports, "runInCallbackScope", RunInCallbackScope);
    NODE_SET_METHOD(exports, "testResolveAsync", TestResolveAsync);
  }

}  // namespace

NODE_MODULE(binding, Initialize)
