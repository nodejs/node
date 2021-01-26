#include <stdlib.h>
#include <uv.h>
#include <node_api.h>
#include "../../js-native-api/common.h"

typedef struct {
  node_api_ref js_cb_ref;
  node_api_ref js_tsfn_finalizer_ref;
  node_api_threadsafe_function tsfn;
  uv_thread_t thread;
} AddonData;

static void AsyncWorkCbExecute(node_api_env env, void* data) {
  (void) env;
  (void) data;
}

static void
call_cb_and_delete_ref(node_api_env env, node_api_ref* optional_ref) {
  node_api_value js_cb, undefined;

  if (optional_ref == NULL) {
    AddonData* data;
    NODE_API_CALL_RETURN_VOID(env,
        node_api_get_instance_data(env, (void**)&data));
    optional_ref = &data->js_cb_ref;
  }

  NODE_API_CALL_RETURN_VOID(env,
      node_api_get_reference_value(env, *optional_ref, &js_cb));
  NODE_API_CALL_RETURN_VOID(env, node_api_get_undefined(env, &undefined));
  NODE_API_CALL_RETURN_VOID(env,
      node_api_call_function(env, undefined, js_cb, 0, NULL, NULL));
  NODE_API_CALL_RETURN_VOID(env,
      node_api_delete_reference(env, *optional_ref));

  *optional_ref = NULL;
}

static void
AsyncWorkCbComplete(node_api_env env, node_api_status status, void* data) {
  (void) status;
  (void) data;
  call_cb_and_delete_ref(env, NULL);
}

static bool
establish_callback_ref(node_api_env env, node_api_callback_info info) {
  AddonData* data;
  size_t argc = 1;
  node_api_value js_cb;

  NODE_API_CALL_BASE(env,
      node_api_get_instance_data(env, (void**)&data), false);
  NODE_API_ASSERT_BASE(env,
      data->js_cb_ref == NULL,
      "reference must be NULL",
      false);
  NODE_API_CALL_BASE(env,
      node_api_get_cb_info(env, info, &argc, &js_cb, NULL, NULL),
      false);
  NODE_API_CALL_BASE(env,
      node_api_create_reference(env, js_cb, 1, &data->js_cb_ref),
      false);

  return true;
}

static node_api_value
AsyncWorkCallback(node_api_env env, node_api_callback_info info) {
  if (establish_callback_ref(env, info)) {
    node_api_value resource_name;
    node_api_async_work work;

    NODE_API_CALL(env, node_api_create_string_utf8(env,
                                                   "AsyncIncrement",
                                                   NODE_API_AUTO_LENGTH,
                                                   &resource_name));
    NODE_API_CALL(env, node_api_create_async_work(env,
                                                  NULL,
                                                  resource_name,
                                                  AsyncWorkCbExecute,
                                                  AsyncWorkCbComplete,
                                                  NULL,
                                                  &work));
    NODE_API_CALL(env, node_api_queue_async_work(env, work));
  }

  return NULL;
}

static void
TestBufferFinalizerCallback(node_api_env env, void* data, void* hint) {
  (void) data;
  (void) hint;
  call_cb_and_delete_ref(env, NULL);
}

static node_api_value
TestBufferFinalizer(node_api_env env, node_api_callback_info info) {
  node_api_value buffer = NULL;
  if (establish_callback_ref(env, info)) {
    NODE_API_CALL(env,
        node_api_create_external_buffer(env,
                                        sizeof(node_api_callback),
                                        TestBufferFinalizer,
                                        TestBufferFinalizerCallback,
                                        NULL,
                                        &buffer));
  }
  return buffer;
}

static void ThreadsafeFunctionCallJS(node_api_env env,
                                     node_api_value tsfn_cb,
                                     void* context,
                                     void* data) {
  (void) tsfn_cb;
  (void) context;
  (void) data;
  call_cb_and_delete_ref(env, NULL);
}

static void ThreadsafeFunctionTestThread(void* raw_data) {
  AddonData* data = raw_data;
  node_api_status status;

  // No need to call `node_api_acquire_threadsafe_function()` because the main
  // thread has set the refcount to 1 and there is only this one secondary
  // thread.
  status = node_api_call_threadsafe_function(data->tsfn,
                                             ThreadsafeFunctionCallJS,
                                             node_api_tsfn_nonblocking);
  if (status != node_api_ok) {
    node_api_fatal_error("ThreadSafeFunctionTestThread",
                         NODE_API_AUTO_LENGTH,
                         "Failed to call TSFN",
                         NODE_API_AUTO_LENGTH);
  }

  status = node_api_release_threadsafe_function(data->tsfn, node_api_tsfn_release);
  if (status != node_api_ok) {
    node_api_fatal_error("ThreadSafeFunctionTestThread",
                         NODE_API_AUTO_LENGTH,
                         "Failed to release TSFN",
                         NODE_API_AUTO_LENGTH);
  }

}

static void FinalizeThreadsafeFunction(node_api_env env, void* raw, void* hint) {
  AddonData* data;
  NODE_API_CALL_RETURN_VOID(env, node_api_get_instance_data(env, (void**)&data));
  NODE_API_ASSERT_RETURN_VOID(env,
                              uv_thread_join(&data->thread) == 0,
                              "Failed to join the thread");
  call_cb_and_delete_ref(env, &data->js_tsfn_finalizer_ref);
  data->tsfn = NULL;
}

// Ths function accepts two arguments: the JS callback, and the finalize
// callback. The latter moves the test forward.
static node_api_value
TestThreadsafeFunction(node_api_env env, node_api_callback_info info) {
  AddonData* data;
  size_t argc = 2;
  node_api_value argv[2], resource_name;

  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, argv, NULL, NULL));
  NODE_API_CALL(env, node_api_get_instance_data(env, (void**)&data));
  NODE_API_ASSERT(env, data->js_cb_ref == NULL, "reference must be NULL");
  NODE_API_ASSERT(env,
                  data->js_tsfn_finalizer_ref == NULL,
                  "tsfn finalizer reference must be NULL");
  NODE_API_CALL(env,
      node_api_create_reference(env, argv[0], 1, &data->js_cb_ref));
  NODE_API_CALL(env, node_api_create_reference(env,
                                               argv[1],
                                               1,
                                               &data->js_tsfn_finalizer_ref));
  NODE_API_CALL(env, node_api_create_string_utf8(env,
                                                 "TSFN instance data test",
                                                 NODE_API_AUTO_LENGTH,
                                                 &resource_name));
  NODE_API_CALL(env,
      node_api_create_threadsafe_function(env,
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
  NODE_API_ASSERT(env,
              uv_thread_create(&data->thread,
                               ThreadsafeFunctionTestThread,
                               data) == 0,
              "uv_thread_create failed");

  return NULL;
}

static void DeleteAddonData(node_api_env env, void* raw_data, void* hint) {
  AddonData* data = raw_data;
  if (data->js_cb_ref) {
    NODE_API_CALL_RETURN_VOID(env,
        node_api_delete_reference(env, data->js_cb_ref));
  }
  if (data->js_tsfn_finalizer_ref) {
    NODE_API_CALL_RETURN_VOID(env,
                              node_api_delete_reference(env,
                                                 data->js_tsfn_finalizer_ref));
  }
  free(data);
}

static node_api_value Init(node_api_env env, node_api_value exports) {
  AddonData* data = malloc(sizeof(*data));
  data->js_cb_ref = NULL;
  data->js_tsfn_finalizer_ref = NULL;

  NODE_API_CALL(env,
      node_api_set_instance_data(env, data, DeleteAddonData, NULL));

  node_api_property_descriptor props[] = {
    DECLARE_NODE_API_PROPERTY("asyncWorkCallback", AsyncWorkCallback),
    DECLARE_NODE_API_PROPERTY("testBufferFinalizer", TestBufferFinalizer),
    DECLARE_NODE_API_PROPERTY("testThreadsafeFunction",
        TestThreadsafeFunction),
  };

  NODE_API_CALL(env, node_api_define_properties(env,
                                                exports,
                                                sizeof(props) / sizeof(*props),
                                                props));

  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
