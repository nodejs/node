#define NAPI_EXPERIMENTAL
#include "node_api.h"
#include "assert.h"
#include "uv.h"
#include <stdlib.h>
#include "../../js-native-api/common.h"

void MustNotCall(void* arg, void(*cb)(void*), void* cbarg) {
  assert(0);
}

struct AsyncData {
  uv_async_t async;
  napi_env env;
  napi_async_cleanup_hook_handle handle;
  void (*done_cb)(void*);
  void* done_arg;
};

struct AsyncData* CreateAsyncData() {
  struct AsyncData* data = (struct AsyncData*) malloc(sizeof(struct AsyncData));
  data->handle = NULL;
  return data;
}

void AfterCleanupHookTwo(uv_handle_t* handle) {
  struct AsyncData* data = (struct AsyncData*) handle->data;
  data->done_cb(data->done_arg);
  free(data);
}

void AfterCleanupHookOne(uv_async_t* async) {
  struct AsyncData* data = (struct AsyncData*) async->data;
  if (data->handle != NULL) {
    // Verify that removing the hook is okay between starting and finishing
    // of its execution.
    napi_status status =
        napi_remove_async_cleanup_hook(data->env, data->handle);
    assert(status == napi_ok);
  }

  uv_close((uv_handle_t*) async, AfterCleanupHookTwo);
}

void AsyncCleanupHook(void* arg, void(*cb)(void*), void* cbarg) {
  struct AsyncData* data = (struct AsyncData*) arg;
  uv_loop_t* loop;
  napi_status status = napi_get_uv_event_loop(data->env, &loop);
  assert(status == napi_ok);
  int err = uv_async_init(loop, &data->async, AfterCleanupHookOne);
  assert(err == 0);

  data->async.data = data;
  data->done_cb = cb;
  data->done_arg = cbarg;
  uv_async_send(&data->async);
}

napi_value Init(napi_env env, napi_value exports) {
  {
    struct AsyncData* data = CreateAsyncData();
    data->env = env;
    napi_add_async_cleanup_hook(env, AsyncCleanupHook, data, &data->handle);
  }

  {
    struct AsyncData* data = CreateAsyncData();
    data->env = env;
    napi_add_async_cleanup_hook(env, AsyncCleanupHook, data, NULL);
  }

  {
    napi_async_cleanup_hook_handle must_not_call_handle;
    napi_add_async_cleanup_hook(
        env, MustNotCall, NULL, &must_not_call_handle);
    napi_remove_async_cleanup_hook(env, must_not_call_handle);
  }

  return NULL;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
