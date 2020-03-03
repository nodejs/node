#include <stdlib.h>
#include <uv.h>
#include <node_api.h>
#include "../../js-native-api/common.h"

typedef struct {
  napi_ref js_cb_ref;
  napi_ref js_tsfn_finalizer_ref;
  napi_threadsafe_function tsfn;
  uv_thread_t thread;
} AddonData;

static void AsyncWorkCbExecute(napi_env env, void* data) {
  (void) env;
  (void) data;
}

static void call_cb_and_delete_ref(napi_env env, napi_ref* optional_ref) {
  napi_value js_cb, undefined;

  if (optional_ref == NULL) {
    AddonData* data;
    NAPI_CALL_RETURN_VOID(env, napi_get_instance_data(env, (void**)&data));
    optional_ref = &data->js_cb_ref;
  }

  NAPI_CALL_RETURN_VOID(env, napi_get_reference_value(env,
                                                      *optional_ref,
                                                      &js_cb));
  NAPI_CALL_RETURN_VOID(env, napi_get_undefined(env, &undefined));
  NAPI_CALL_RETURN_VOID(env, napi_call_function(env,
                                                undefined,
                                                js_cb,
                                                0,
                                                NULL,
                                                NULL));
  NAPI_CALL_RETURN_VOID(env, napi_delete_reference(env, *optional_ref));

  *optional_ref = NULL;
}

static void AsyncWorkCbComplete(napi_env env,
                                   napi_status status,
                                   void* data) {
  (void) status;
  (void) data;
  call_cb_and_delete_ref(env, NULL);
}

static bool establish_callback_ref(napi_env env, napi_callback_info info) {
  AddonData* data;
  size_t argc = 1;
  napi_value js_cb;

  NAPI_CALL_BASE(env, napi_get_instance_data(env, (void**)&data), false);
  NAPI_ASSERT_BASE(env,
                   data->js_cb_ref == NULL,
                   "reference must be NULL",
                   false);
  NAPI_CALL_BASE(env,
                 napi_get_cb_info(env, info, &argc, &js_cb, NULL, NULL),
                 false);
  NAPI_CALL_BASE(env,
                 napi_create_reference(env, js_cb, 1, &data->js_cb_ref),
                 false);

  return true;
}

static napi_value AsyncWorkCallback(napi_env env, napi_callback_info info) {
  if (establish_callback_ref(env, info)) {
    napi_value resource_name;
    napi_async_work work;

    NAPI_CALL(env, napi_create_string_utf8(env,
                                           "AsyncIncrement",
                                           NAPI_AUTO_LENGTH,
                                           &resource_name));
    NAPI_CALL(env, napi_create_async_work(env,
                                          NULL,
                                          resource_name,
                                          AsyncWorkCbExecute,
                                          AsyncWorkCbComplete,
                                          NULL,
                                          &work));
    NAPI_CALL(env, napi_queue_async_work(env, work));
  }

  return NULL;
}

static void TestBufferFinalizerCallback(napi_env env, void* data, void* hint) {
  (void) data;
  (void) hint;
  call_cb_and_delete_ref(env, NULL);
}

static napi_value TestBufferFinalizer(napi_env env, napi_callback_info info) {
  napi_value buffer = NULL;
  if (establish_callback_ref(env, info)) {
    NAPI_CALL(env, napi_create_external_buffer(env,
                                               sizeof(napi_callback),
                                               TestBufferFinalizer,
                                               TestBufferFinalizerCallback,
                                               NULL,
                                               &buffer));
  }
  return buffer;
}

static void ThreadsafeFunctionCallJS(napi_env env,
                                     napi_value tsfn_cb,
                                     void* context,
                                     void* data) {
  (void) tsfn_cb;
  (void) context;
  (void) data;
  call_cb_and_delete_ref(env, NULL);
}

static void ThreadsafeFunctionTestThread(void* raw_data) {
  AddonData* data = raw_data;
  napi_status status;

  // No need to call `napi_acquire_threadsafe_function()` because the main
  // thread has set the refcount to 1 and there is only this one secondary
  // thread.
  status = napi_call_threadsafe_function(data->tsfn,
                                         ThreadsafeFunctionCallJS,
                                         napi_tsfn_nonblocking);
  if (status != napi_ok) {
    napi_fatal_error("ThreadSafeFunctionTestThread",
                     NAPI_AUTO_LENGTH,
                     "Failed to call TSFN",
                     NAPI_AUTO_LENGTH);
  }

  status = napi_release_threadsafe_function(data->tsfn, napi_tsfn_release);
  if (status != napi_ok) {
    napi_fatal_error("ThreadSafeFunctionTestThread",
                     NAPI_AUTO_LENGTH,
                     "Failed to release TSFN",
                     NAPI_AUTO_LENGTH);
  }

}

static void FinalizeThreadsafeFunction(napi_env env, void* raw, void* hint) {
  AddonData* data;
  NAPI_CALL_RETURN_VOID(env, napi_get_instance_data(env, (void**)&data));
  NAPI_ASSERT_RETURN_VOID(env,
                          uv_thread_join(&data->thread) == 0,
                          "Failed to join the thread");
  call_cb_and_delete_ref(env, &data->js_tsfn_finalizer_ref);
  data->tsfn = NULL;
}

// Ths function accepts two arguments: the JS callback, and the finalize
// callback. The latter moves the test forward.
static napi_value
TestThreadsafeFunction(napi_env env, napi_callback_info info) {
  AddonData* data;
  size_t argc = 2;
  napi_value argv[2], resource_name;

  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  NAPI_CALL(env, napi_get_instance_data(env, (void**)&data));
  NAPI_ASSERT(env, data->js_cb_ref == NULL, "reference must be NULL");
  NAPI_ASSERT(env,
              data->js_tsfn_finalizer_ref == NULL,
              "tsfn finalizer reference must be NULL");
  NAPI_CALL(env, napi_create_reference(env, argv[0], 1, &data->js_cb_ref));
  NAPI_CALL(env, napi_create_reference(env,
                                       argv[1],
                                       1,
                                       &data->js_tsfn_finalizer_ref));
  NAPI_CALL(env, napi_create_string_utf8(env,
                                         "TSFN instance data test",
                                         NAPI_AUTO_LENGTH,
                                         &resource_name));
  NAPI_CALL(env, napi_create_threadsafe_function(env,
                                                 NULL,
                                                 NULL,
                                                 resource_name,
                                                 0,
                                                 1,
                                                 NULL,
                                                 FinalizeThreadsafeFunction,
                                                 NULL,
                                                 ThreadsafeFunctionCallJS,
                                                 &data->tsfn));
  NAPI_ASSERT(env,
              uv_thread_create(&data->thread,
                               ThreadsafeFunctionTestThread,
                               data) == 0,
              "uv_thread_create failed");

  return NULL;
}

static void DeleteAddonData(napi_env env, void* raw_data, void* hint) {
  AddonData* data = raw_data;
  if (data->js_cb_ref) {
    NAPI_CALL_RETURN_VOID(env, napi_delete_reference(env, data->js_cb_ref));
  }
  if (data->js_tsfn_finalizer_ref) {
    NAPI_CALL_RETURN_VOID(env,
                          napi_delete_reference(env,
                                                data->js_tsfn_finalizer_ref));
  }
  free(data);
}

static napi_value Init(napi_env env, napi_value exports) {
  AddonData* data = malloc(sizeof(*data));
  data->js_cb_ref = NULL;
  data->js_tsfn_finalizer_ref = NULL;

  NAPI_CALL(env, napi_set_instance_data(env, data, DeleteAddonData, NULL));

  napi_property_descriptor props[] = {
    DECLARE_NAPI_PROPERTY("asyncWorkCallback", AsyncWorkCallback),
    DECLARE_NAPI_PROPERTY("testBufferFinalizer", TestBufferFinalizer),
    DECLARE_NAPI_PROPERTY("testThreadsafeFunction", TestThreadsafeFunction),
  };

  NAPI_CALL(env, napi_define_properties(env,
                                        exports,
                                        sizeof(props) / sizeof(*props),
                                        props));

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
