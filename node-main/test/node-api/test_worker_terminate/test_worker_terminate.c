#include <stdio.h>
#include <node_api.h>
#include <assert.h>
#include "../../js-native-api/common.h"

napi_value Test(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value recv;
  napi_value argv[1];
  napi_status status;

  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, argv, &recv, NULL));
  NODE_API_ASSERT(env, argc >= 1, "Not enough arguments, expected 1.");

  napi_valuetype t;
  NODE_API_CALL(env, napi_typeof(env, argv[0], &t));
  NODE_API_ASSERT(env, t == napi_function,
      "Wrong first argument, function expected.");

  status = napi_call_function(env, recv, argv[0], 0, NULL, NULL);
  assert(status == napi_pending_exception);

  return NULL;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor properties[] = {
    DECLARE_NODE_API_PROPERTY("Test", Test)
  };

  NODE_API_CALL(env, napi_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}

// Do not start using NAPI_MODULE_INIT() here, so that we can test
// compatibility of Workers with NAPI_MODULE().
NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
