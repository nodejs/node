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

void DoAsync (uv_work_t *r) {
  async_req *req = reinterpret_cast<async_req *>(r->data);
  sleep(1); // simulate CPU intensive process...
  req->output = req->input * 2;
}

void AfterAsync (uv_work_t *r) {
  HandleScope scope;
  async_req *req = reinterpret_cast<async_req *>(r->data);

  Handle<Value> argv[2] = { Null(), Integer::New(req->output) };

  TryCatch try_catch;

  req->callback->Call(Context::GetCurrent()->Global(), 2, argv);

  // cleanup
  req->callback.Dispose();
  delete req;

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }
}

Handle<Value> Method(const Arguments& args) {
  HandleScope scope;

  async_req *req = new async_req;
  req->req.data = req;

  req->input = args[0]->IntegerValue();
  req->output = 0;

  Local<Function> callback = Local<Function>::Cast(args[1]);
  req->callback = Persistent<Function>::New(callback);

  uv_queue_work(uv_default_loop(),
                &req->req,
                DoAsync,
                (uv_after_work_cb)AfterAsync);

  return Undefined();
}

void init(Handle<Object> exports, Handle<Object> module) {
  NODE_SET_METHOD(module, "exports", Method);
}

NODE_MODULE(binding, init);
