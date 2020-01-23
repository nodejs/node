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
  v8::Global<v8::Function> callback;
  node::async_context context;
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
  v8::Isolate* isolate = req->isolate;
  v8::HandleScope scope(isolate);

  v8::Local<v8::Value> argv[2] = {
    v8::Null(isolate),
    v8::Integer::New(isolate, req->output)
  };

  v8::TryCatch try_catch(isolate);

  v8::Local<v8::Object> global = isolate->GetCurrentContext()->Global();
  v8::Local<v8::Function> callback =
      v8::Local<v8::Function>::New(isolate, req->callback);

  if (use_makecallback) {
    v8::Local<v8::Value> ret =
        node::MakeCallback(isolate, global, callback, 2, argv, req->context)
            .ToLocalChecked();
    // This should be changed to an empty handle.
    assert(!ret.IsEmpty());
  } else {
    callback->Call(isolate->GetCurrentContext(),
                   global, 2, argv).ToLocalChecked();
  }

  // None of the following operations should allocate handles into this scope.
  v8::SealHandleScope seal_handle_scope(isolate);
  // cleanup
  node::EmitAsyncDestroy(isolate, req->context);
  delete req;

  if (try_catch.HasCaught()) {
    node::FatalException(isolate, try_catch);
  }
}

template <bool use_makecallback>
void Method(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();

  async_req* req = new async_req;
  req->req.data = req;

  req->input = args[0].As<v8::Integer>()->Value();
  req->output = 0;
  req->isolate = isolate;
  req->context = node::EmitAsyncInit(isolate, v8::Object::New(isolate), "test");

  v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(args[1]);
  req->callback.Reset(isolate, callback);

  uv_queue_work(node::GetCurrentEventLoop(isolate),
                &req->req,
                DoAsync,
                (uv_after_work_cb)AfterAsync<use_makecallback>);
}

void init(v8::Local<v8::Object> exports, v8::Local<v8::Object> module) {
  NODE_SET_METHOD(exports, "runCall", Method<false>);
  NODE_SET_METHOD(exports, "runMakeCallback", Method<true>);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)
