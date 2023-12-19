#include <node_api.h>
#include "../../js-native-api/common.h"

// Testing calling into JavaScript
static void ThreadSafeFunctionFinalize(napi_env env,
                                       void* finalize_data,
                                       void* finalize_hint) {
  napi_ref js_func_ref = (napi_ref)finalize_data;
  napi_value js_func;
  napi_value recv;
  NODE_API_CALL_RETURN_VOID(
      env, napi_get_reference_value(env, js_func_ref, &js_func));
  NODE_API_CALL_RETURN_VOID(env, napi_get_global(env, &recv));
  NODE_API_CALL_RETURN_VOID(
      env, napi_call_function(env, recv, js_func, 0, NULL, NULL));
  NODE_API_CALL_RETURN_VOID(env, napi_delete_reference(env, js_func_ref));
}

static void ThreadSafeFunctionNogcFinalize(node_api_nogc_env env,
                                           void* data,
                                           void* hint) {
#ifdef NAPI_EXPERIMENTAL
  NODE_API_NOGC_CALL_RETURN_VOID(
      env,
      node_api_post_finalizer(env, ThreadSafeFunctionFinalize, data, hint));
#else
  ThreadSafeFunctionFinalize(env, data, hint);
#endif
}

// Testing calling into JavaScript
static napi_value CallIntoModule(napi_env env, napi_callback_info info) {
  size_t argc = 4;
  napi_value argv[4];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));

  napi_ref finalize_func;
  NODE_API_CALL(env, napi_create_reference(env, argv[3], 1, &finalize_func));

  napi_threadsafe_function tsfn;
  NODE_API_CALL(env,
                napi_create_threadsafe_function(env,
                                                argv[0],
                                                argv[1],
                                                argv[2],
                                                0,
                                                1,
                                                finalize_func,
                                                ThreadSafeFunctionNogcFinalize,
                                                NULL,
                                                NULL,
                                                &tsfn));
  NODE_API_CALL(env,
                napi_call_threadsafe_function(tsfn, NULL, napi_tsfn_blocking));
  NODE_API_CALL(env, napi_release_threadsafe_function(tsfn, napi_tsfn_release));
  return NULL;
}

// Module init
static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor properties[] = {
      DECLARE_NODE_API_PROPERTY("CallIntoModule", CallIntoModule),
  };

  NODE_API_CALL(
      env,
      napi_define_properties(env,
                             exports,
                             sizeof(properties) / sizeof(properties[0]),
                             properties));

  return exports;
}
NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
