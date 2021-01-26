#define NODE_API_EXPERIMENTAL
#include "node_api.h"
#include "assert.h"
#include "uv.h"
#include <stdlib.h>
#include "../../js-native-api/common.h"

static void MustNotCall(node_api_async_cleanup_hook_handle hook, void* arg) {
  assert(0);
}

struct AsyncData {
  uv_async_t async;
  node_api_env env;
  node_api_async_cleanup_hook_handle handle;
};

static struct AsyncData* CreateAsyncData() {
  struct AsyncData* data = (struct AsyncData*) malloc(sizeof(struct AsyncData));
  data->handle = NULL;
  return data;
}

static void AfterCleanupHookTwo(uv_handle_t* handle) {
  struct AsyncData* data = (struct AsyncData*) handle->data;
  node_api_status status = node_api_remove_async_cleanup_hook(data->handle);
  assert(status == node_api_ok);
  free(data);
}

static void AfterCleanupHookOne(uv_async_t* async) {
  uv_close((uv_handle_t*) async, AfterCleanupHookTwo);
}

static void AsyncCleanupHook(node_api_async_cleanup_hook_handle handle, void* arg) {
  struct AsyncData* data = (struct AsyncData*) arg;
  uv_loop_t* loop;
  node_api_status status = node_api_get_uv_event_loop(data->env, &loop);
  assert(status == node_api_ok);
  int err = uv_async_init(loop, &data->async, AfterCleanupHookOne);
  assert(err == 0);

  data->async.data = data;
  data->handle = handle;
  uv_async_send(&data->async);
}

static node_api_value Init(node_api_env env, node_api_value exports) {
  {
    struct AsyncData* data = CreateAsyncData();
    data->env = env;
    node_api_add_async_cleanup_hook(env, AsyncCleanupHook, data, &data->handle);
  }

  {
    struct AsyncData* data = CreateAsyncData();
    data->env = env;
    node_api_add_async_cleanup_hook(env, AsyncCleanupHook, data, NULL);
  }

  {
    node_api_async_cleanup_hook_handle must_not_call_handle;
    node_api_add_async_cleanup_hook(
        env, MustNotCall, NULL, &must_not_call_handle);
    node_api_remove_async_cleanup_hook(must_not_call_handle);
  }

  return NULL;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
