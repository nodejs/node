#include <node.h>
#include <v8.h>
#include <uv.h>

#if defined _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif


struct async_req {
  uv_work_t req;
  int input;
  int output;
  v8::Isolate* isolate;
  v8::Persistent<v8::Function> callback;
};

void DoAsync(uv_work_t* r) {
  async_req* req = reinterpret_cast<async_req*>(r->data);
  // Simulate CPU intensive process...
#if defined _WIN32
  Sleep(1000);
#else
  sleep(1);
#endif
  req->output = req->input * 2;
}

void AfterAsync(uv_work_t* r) {
  async_req* req = reinterpret_cast<async_req*>(r->data);
  v8::Isolate* isolate = req->isolate;
  v8::HandleScope scope(isolate);

  v8::Local<v8::Value> argv[2] = {
    v8::Null(isolate),
    v8::Integer::New(isolate, req->output)
  };

  v8::TryCatch try_catch(isolate);

  v8::Local<v8::Function> callback =
      v8::Local<v8::Function>::New(isolate, req->callback);
  callback->Call(isolate->GetCurrentContext()->Global(), 2, argv);

  // cleanup
  req->callback.Reset();
  delete req;

  if (try_catch.HasCaught()) {
    node::FatalException(isolate, try_catch);
  }
}

void Method(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();

  async_req* req = new async_req;
  req->req.data = req;

  req->input = args[0]->IntegerValue();
  req->output = 0;
  req->isolate = isolate;

  v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(args[1]);
  req->callback.Reset(isolate, callback);

  uv_queue_work(uv_default_loop(),
                &req->req,
                DoAsync,
                (uv_after_work_cb)AfterAsync);
}

void init(v8::Local<v8::Object> exports, v8::Local<v8::Object> module) {
  NODE_SET_METHOD(module, "exports", Method);
}

NODE_MODULE(binding, init)
