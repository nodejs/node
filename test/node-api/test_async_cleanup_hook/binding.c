#include "node_api.h"
#include "assert.h"
#include "uv.h"
#include <stdlib.h>
#include "../../js-native-api/common.h"

static void MustNotCall(napi_async_cleanup_hook_handle hook, void* arg) {
  assert(0);
}

struct AsyncData {
  uv_async_t async;
  napi_env env;
  napi_async_cleanup_hook_handle handle;
};

static struct AsyncData* CreateAsyncData() {
  struct AsyncData* data = (struct AsyncData*) malloc(sizeof(struct AsyncData));
  data->handle = NULL;
  return data;
}

static void AfterCleanupHookTwo(uv_handle_t* handle) {
  struct AsyncData* data = (struct AsyncData*) handle->data;
  napi_status status = napi_remove_async_cleanup_hook(data->handle);
  assert(status == napi_ok);
  free(data);
}

static void AfterCleanupHookOne(uv_async_t* async) {
  uv_close((uv_handle_t*) async, AfterCleanupHookTwo);
}

static void AsyncCleanupHook(napi_async_cleanup_hook_handle handle, void* arg) {
  struct AsyncData* data = (struct AsyncData*) arg;
  uv_loop_t* loop;
  napi_status status = napi_get_uv_event_loop(data->env, &loop);
  assert(status == napi_ok);
  int err = uv_async_init(loop, &data->async, AfterCleanupHookOne);
  assert(err == 0);

  data->async.data = data;
  data->handle = handle;
  uv_async_send(&data->async);
}

static napi_value Init(napi_env env, napi_value exports) {
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
    napi_remove_async_cleanup_hook(must_not_call_handle);
  }

  return NULL;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
