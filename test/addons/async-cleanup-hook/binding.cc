#include <node.h>
#include <uv.h>
#include <assert.h>

void MustNotCall(void* arg, void(*cb)(void*), void* cbarg) {
  assert(0);
}

struct AsyncData {
  uv_async_t async;
  v8::Isolate* isolate;
  node::AsyncCleanupHookHandle handle;
  void (*done_cb)(void*);
  void* done_arg;
};

void AsyncCleanupHook(void* arg, void(*cb)(void*), void* cbarg) {
  AsyncData* data = static_cast<AsyncData*>(arg);
  uv_loop_t* loop = node::GetCurrentEventLoop(data->isolate);
  assert(loop != nullptr);
  int err = uv_async_init(loop, &data->async, [](uv_async_t* async) {
    AsyncData* data = static_cast<AsyncData*>(async->data);
    // Attempting to remove the cleanup hook here should be a no-op since it
    // has already been started.
    node::RemoveEnvironmentCleanupHook(std::move(data->handle));

    uv_close(reinterpret_cast<uv_handle_t*>(async), [](uv_handle_t* handle) {
      AsyncData* data = static_cast<AsyncData*>(handle->data);
      data->done_cb(data->done_arg);
      delete data;
    });
  });
  assert(err == 0);

  data->async.data = data;
  data->done_cb = cb;
  data->done_arg = cbarg;
  uv_async_send(&data->async);
}

void Initialize(v8::Local<v8::Object> exports,
                v8::Local<v8::Value> module,
                v8::Local<v8::Context> context) {
  AsyncData* data = new AsyncData();
  data->isolate = context->GetIsolate();
  auto handle = node::AddEnvironmentCleanupHook(
      context->GetIsolate(),
      AsyncCleanupHook,
      data);
  data->handle = std::move(handle);

  auto must_not_call_handle = node::AddEnvironmentCleanupHook(
      context->GetIsolate(),
      MustNotCall,
      nullptr);
  node::RemoveEnvironmentCleanupHook(std::move(must_not_call_handle));
}

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)
