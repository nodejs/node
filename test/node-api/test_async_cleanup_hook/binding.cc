#define NAPI_EXPERIMENTAL
#include "node_api.h"
#include "assert.h"
#include "uv.h"
#include "../../js-native-api/common.h"

void MustNotCall(void* arg, void(*cb)(void*), void* cbarg) {
  assert(0);
}

struct AsyncData {
  uv_async_t async;
  napi_env env;
  napi_async_cleanup_hook_handle handle = nullptr;
  void (*done_cb)(void*);
  void* done_arg;
};

void AsyncCleanupHook(void* arg, void(*cb)(void*), void* cbarg) {
  AsyncData* data = static_cast<AsyncData*>(arg);
  uv_loop_t* loop;
  napi_status status = napi_get_uv_event_loop(data->env, &loop);
  assert(status == napi_ok);
  int err = uv_async_init(loop, &data->async, [](uv_async_t* async) {
    AsyncData* data = static_cast<AsyncData*>(async->data);
    if (data->handle != nullptr) {
      // Verify that removing the hook is okay between starting and finishing
      // of its execution.
      napi_status status =
          napi_remove_async_cleanup_hook(data->env, data->handle);
      assert(status == napi_ok);
    }

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

napi_value Init(napi_env env, napi_value exports) {
  {
    AsyncData* data = new AsyncData();
    data->env = env;
    napi_add_async_cleanup_hook(env, AsyncCleanupHook, data, &data->handle);
  }

  {
    AsyncData* data = new AsyncData();
    data->env = env;
    napi_add_async_cleanup_hook(env, AsyncCleanupHook, data, nullptr);
  }

  {
    napi_async_cleanup_hook_handle must_not_call_handle;
    napi_add_async_cleanup_hook(
        env, MustNotCall, nullptr, &must_not_call_handle);
    napi_remove_async_cleanup_hook(env, must_not_call_handle);
  }

  return nullptr;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
