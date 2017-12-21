#include <stdlib.h>
#include <uv.h>
#include <node_api.h>
#include "addon.h"

static napi_threadsafe_function async_func = NULL;

typedef struct {
  uv_thread_t thread;
  uv_sem_t may_write;
  int value;
  napi_ref collect_results;
} TestAsyncData;

static void process_result(napi_env env,
                           void* data,
                           napi_value error,
                           napi_value result) {
  TestAsyncData* async = data;
  napi_value collect_results;
  napi_status status = napi_get_reference_value(env, async->collect_results,
      &collect_results);
  if (status != napi_ok) {
    napi_throw_error(env, NULL,
        "Failed to retrieve return value collector reference");
    return;
  }

  napi_value value_to_collect =
      (error == NULL ? result == NULL ? NULL : result : error);

  if (value_to_collect != NULL) {
    status = napi_call_function(env, collect_results, collect_results, 1,
        &value_to_collect, NULL);
    if (status != napi_ok) {
      napi_throw_error(env, NULL, "Failed to call return value collector");
    }
  }
}

static napi_status marshal_data(napi_env env,
                                void* data,
                                napi_value* recv,
                                size_t argc,
                                napi_value* argv) {
  int value;
  TestAsyncData* async = data;
  napi_status status = napi_create_object(env, recv);
  if (status != napi_ok) {
    return status;
  }

  value = async->value;
  uv_sem_post(&async->may_write);

  status = napi_create_int32(env, value, argv);
  if (status != napi_ok) {
    return status;
  }

  return napi_ok;
}

static void BasicTestThread(void* thread_data) {
  napi_threadsafe_function async = thread_data;
  void* data = NULL;
  TestAsyncData* test_data = NULL;
  int value;
  napi_status status = napi_get_threadsafe_function_data(async, &data);
  if (status != napi_ok) {
    napi_fatal_error("BasicTestThread", NAPI_AUTO_LENGTH,
        "Failed to retrieve async function data", NAPI_AUTO_LENGTH);
  }
  test_data = (TestAsyncData*)data;

  for (value = 0; value < 5; value++) {
    uv_sem_wait(&test_data->may_write);
    test_data->value = value;
    status = napi_call_threadsafe_function(async);
    if (status != napi_ok) {
      napi_fatal_error("BasicTestThread", NAPI_AUTO_LENGTH,
          "Failed to call async function", NAPI_AUTO_LENGTH);
    }
  }
}

static napi_value StartThread(napi_env env, napi_callback_info info) {
  const char* error;
  size_t argc = 2;
  napi_value argv[2];
  napi_status status =
      napi_get_cb_info(env, info, &argc, argv, NULL, NULL);
  if (status != napi_ok) {
    error = "Failed to retrieve JS callback";
    goto throw;
  }

  if (async_func) {
    error = "There is already an async function in place";
    goto throw;
  }

  TestAsyncData* async_data = malloc(sizeof(*async_data));
  if (async_data == NULL) {
    error = "Failed to allocate memory for test data";
    goto throw;
  }
  async_data->value = -1;

  status = napi_create_reference(env, argv[1], 1, &async_data->collect_results);
  if (status != napi_ok) {
    error = "Failed to create reference to function that collects the "
        "JS callback return values";
    goto free_data;
  }

  status = napi_create_threadsafe_function(env, argv[0], async_data, 1,
      marshal_data, process_result, &async_func);
  if (status != napi_ok) {
    error = "Failed to create async function";
    goto delete_reference;
  }

  if (uv_sem_init(&async_data->may_write, 1) != 0) {
    error = "Failed to initialize write sem";
    goto delete_async_function;
  }

  if (uv_thread_create(&async_data->thread, BasicTestThread, async_func) != 0) {
    error = "Failed to start thread";
    goto destroy_semaphore;
  }

  goto done;

destroy_semaphore:
  uv_sem_destroy(&async_data->may_write);
delete_async_function:
  napi_delete_threadsafe_function(env, async_func);
delete_reference:
  napi_delete_reference(env, async_data->collect_results);
free_data:
  free(async_data);
throw:
  napi_throw_error(env, NULL, error);
done:
  return NULL;
}

static napi_value StopThread(napi_env env, napi_callback_info info) {
  if (async_func == NULL) {
    napi_throw_error(env, NULL,
        "There is no asynchronous function currently open");
    return NULL;
  }

  void* data;
  napi_status status =
      napi_get_threadsafe_function_data(async_func, &data);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to retrieve async function data");
    return NULL;
  }
  TestAsyncData* test_data = data;

  if (uv_thread_join(&test_data->thread) != 0) {
    napi_throw_error(env, NULL, "Failed to stop test thread");
    return NULL;
  }

  status = napi_delete_threadsafe_function(env, async_func);
  if (status != napi_ok) {
    napi_throw_error(env, NULL, "Failed to delete async function");
  }

  uv_sem_destroy(&test_data->may_write);
  free(test_data);
  async_func = NULL;
  return NULL;
}

void InitTestBasic(napi_env env, napi_value exports, AddonInit init) {
  napi_property_descriptor props[] = {
    {"StartThread", NULL, StartThread, NULL, NULL, NULL, napi_enumerable, NULL},
    {"StopThread", NULL, StopThread, NULL, NULL, NULL, napi_enumerable, NULL}
  };
  init(env, exports, sizeof(props)/sizeof(props[0]), props);
}
