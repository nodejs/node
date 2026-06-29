#include <node_api.h>
#include "../../js-native-api/common.h"

static napi_value Test(napi_env env, napi_callback_info info) {
  napi_value err;
  size_t argc = 1;

  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, &err, NULL, NULL));

  NODE_API_CALL(env, napi_fatal_exception(env, err));

  return NULL;
}

static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor properties[] = {
    DECLARE_NODE_API_PROPERTY("Test", Test),
  };

  NODE_API_CALL(env, napi_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
