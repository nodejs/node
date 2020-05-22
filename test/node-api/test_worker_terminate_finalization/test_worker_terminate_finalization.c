#include <stdio.h>
#include <node_api.h>
#include <assert.h>
#include <malloc.h>
#include "../../js-native-api/common.h"

int wrappedNativeData;
napi_ref ref;
void WrapFinalizer(napi_env env, void* data, void* hint) {
  uint32_t count;
  NAPI_CALL_RETURN_VOID(env, napi_reference_unref(env, ref, &count));
}

napi_value Test(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  napi_value result;
  napi_ref ref;
  napi_ref wrapResult;

  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  NAPI_CALL(env, napi_create_external_buffer(env, 4, (void*) malloc(4), NULL, NULL, &result));
  NAPI_CALL(env, napi_create_reference(env, result, 1, &ref));
  NAPI_CALL(env, napi_wrap(env, argv[0], (void*) &wrappedNativeData, WrapFinalizer, NULL, &wrapResult));
  return NULL;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor properties[] = {
    DECLARE_NAPI_PROPERTY("Test", Test)
  };

  NAPI_CALL(env, napi_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}

// Do not start using NAPI_MODULE_INIT() here, so that we can test
// compatibility of Workers with NAPI_MODULE().
NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
