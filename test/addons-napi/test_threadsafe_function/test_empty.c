#include <stdlib.h>
#include <uv.h>
#include <node_api.h>
#include "addon.h"

static napi_threadsafe_function async_func = NULL;
static uv_thread_t thread;

static void EmptyTestThread(void* thread_data) {
  if (napi_call_threadsafe_function((napi_threadsafe_function)thread_data) !=
      napi_ok) {
    napi_fatal_error("EmptyTestThread", NAPI_AUTO_LENGTH,
        "Failed to call async function", NAPI_AUTO_LENGTH);
  }
}

static napi_value StartEmptyThread(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value js_cb;
  napi_status status =
      napi_get_cb_info(env, info, &argc, &js_cb, NULL, NULL);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to retrieve JS callback");
    return NULL;
  }

  if (async_func != NULL) {
    napi_throw_error(env, NULL, "There is already an async function in place");
    return NULL;
  }

  status = napi_create_threadsafe_function(env, js_cb, NULL, 0, NULL, NULL,
      &async_func);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to create async function");
    return NULL;
  }

  if (uv_thread_create(&thread, EmptyTestThread, async_func) != 0) {
    napi_delete_threadsafe_function(env, async_func);
    napi_throw_error(env, NULL, "Failed to start thread");
    return NULL;
  }

  return NULL;
}

static napi_value StopEmptyThread(napi_env env, napi_callback_info info) {
  if (async_func == NULL) {
    napi_throw_error(env, NULL,
        "There is no asynchronous function currently open");
    return NULL;
  }

  if (uv_thread_join(&thread) != 0) {
    napi_throw_error(env, NULL, "Failed to stop test thread");
    return NULL;
  }

  if (napi_delete_threadsafe_function(env, async_func) != napi_ok) {
    napi_throw_error(env, NULL, "Failed to delete async function");
  }

  async_func = NULL;
  return NULL;
}

void InitTestEmpty(napi_env env, napi_value exports, AddonInit init) {
  napi_property_descriptor props[] = {
    {"StartEmptyThread", NULL, StartEmptyThread, NULL, NULL, NULL,
        napi_enumerable, NULL},
    {"StopEmptyThread", NULL, StopEmptyThread, NULL, NULL, NULL,
        napi_enumerable, NULL}
  };
  init(env, exports, sizeof(props)/sizeof(props[0]), props);
}
