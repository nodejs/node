#include <unistd.h>
#include <node.h>
#include <v8.h>
#include <uv.h>

using namespace v8;
using namespace node;

struct async_req {
  uv_work_t req;
  int input;
  int output;
  Persistent<Function> callback;
};

void DoAsync(uv_work_t* r) {
  async_req* req = reinterpret_cast<async_req*>(r->data);
  sleep(1); // simulate CPU intensive process...
  req->output = req->input * 2;
}

void AfterAsync(uv_work_t* r) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);
  async_req* req = reinterpret_cast<async_req*>(r->data);

  Handle<Value> argv[2] = {
    Null(isolate),
    Integer::New(isolate, req->output)
  };

  TryCatch try_catch;

  Local<Function> callback = Local<Function>::New(isolate, req->callback);
  callback->Call(isolate->GetCurrentContext()->Global(), 2, argv);

  // cleanup
  req->callback.Reset();
  delete req;

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}

void Method(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = Isolate::GetCurrent();
  HandleScope scope(isolate);

  async_req* req = new async_req;
  req->req.data = req;

  req->input = args[0]->IntegerValue();
  req->output = 0;

  Local<Function> callback = Local<Function>::Cast(args[1]);
  req->callback.Reset(isolate, callback);

  uv_queue_work(uv_default_loop(),
                &req->req,
                DoAsync,
                (uv_after_work_cb)AfterAsync);
}

void init(Handle<Object> exports, Handle<Object> module) {
  NODE_SET_METHOD(module, "exports", Method);
}

NODE_MODULE(binding, init);
