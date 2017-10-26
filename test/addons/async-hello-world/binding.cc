#include <node.h>
#include <v8.h>
#include <uv.h>

#if defined _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

using v8::Isolate;
using v8::Persistent;
using v8::Function;
using v8::Local;
using v8::HandleScope;
using v8::Value;
using v8::Null;
using v8::Integer;
using v8::Object;
using v8::TryCatch;


struct async_req {
  uv_work_t req;
  int input;
  int output;
  Isolate* isolate;
  Persistent<Function> callback;
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

template <bool use_makecallback>
void AfterAsync(uv_work_t* r) {
  async_req* req = reinterpret_cast<async_req*>(r->data);
  Isolate* isolate = req->isolate;
  HandleScope scope(isolate);

  Local<Value> argv[2] = {
    Null(isolate),
    Integer::New(isolate, req->output)
  };

  TryCatch try_catch(isolate);

  Local<Object> global = isolate->GetCurrentContext()->Global();
  Local<Function> callback =
      Local<Function>::New(isolate, req->callback);

  if (use_makecallback) {
    Local<Value> ret =
        node::MakeCallback(isolate, global, callback, 2, argv);
    // This should be changed to an empty handle.
    assert(!ret.IsEmpty());
  } else {
    callback->Call(global, 2, argv);
  }

  // cleanup
  req->callback.Reset();
  delete req;

  if (try_catch.HasCaught()) {
    node::FatalException(isolate, try_catch);
  }
}

template <bool use_makecallback>
void Method(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();

  async_req* req = new async_req;
  req->req.data = req;

  req->input = args[0]->IntegerValue();
  req->output = 0;
  req->isolate = isolate;

  Local<Function> callback = Local<Function>::Cast(args[1]);
  req->callback.Reset(isolate, callback);

  uv_queue_work(node::GetCurrentEventLoop(isolate),
                &req->req,
                DoAsync,
                (uv_after_work_cb)AfterAsync<use_makecallback>);
}

void init(Local<Object> exports, Local<Object> module) {
  NODE_SET_METHOD(exports, "runCall", Method<false>);
  NODE_SET_METHOD(exports, "runMakeCallback", Method<true>);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)
