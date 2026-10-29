#include "node_api.h"
#include "assert.h"
#include "uv.h"
#include <stdlib.h>
#include "../../js-native-api/common.h"

static int cleanup_hook_count = 0;
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
  cleanup_hook_count++;
  struct AsyncData* data = (struct AsyncData*) handle->data;
  napi_status status = napi_remove_async_cleanup_hook(data->handle);
  assert(status == napi_ok);
  free(data);
}

static void AfterCleanupHookOne(uv_async_t* async) {
  cleanup_hook_count++;
  uv_close((uv_handle_t*) async, AfterCleanupHookTwo);
}

static void AsyncCleanupHook(napi_async_cleanup_hook_handle handle, void* arg) {
  cleanup_hook_count++;
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

static void ObjectFinalizer(napi_env env, void* data, void* hint) {
  // AsyncCleanupHook and its subsequent callbacks are called twice.
  assert(cleanup_hook_count == 6);

  napi_ref* ref = data;
  NODE_API_CALL_RETURN_VOID(env, napi_delete_reference(env, *ref));
  free(ref);
}

static void CreateObjectWrap(napi_env env) {
  napi_value js_obj;
  napi_ref* ref = malloc(sizeof(*ref));
  NODE_API_CALL_RETURN_VOID(env, napi_create_object(env, &js_obj));
  NODE_API_CALL_RETURN_VOID(
      env, napi_wrap(env, js_obj, ref, ObjectFinalizer, NULL, ref));
  // create a strong reference so that the finalizer is called at shutdown.
  NODE_API_CALL_RETURN_VOID(env, napi_reference_ref(env, *ref, NULL));
}

static napi_value Init(napi_env env, napi_value exports) {
  // Reinitialize the static variable to be compatible with musl libc.
  cleanup_hook_count = 0;
  // Create object wrap before cleanup hooks.
  CreateObjectWrap(env);

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

  // Create object wrap after cleanup hooks.
  CreateObjectWrap(env);

  return NULL;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
