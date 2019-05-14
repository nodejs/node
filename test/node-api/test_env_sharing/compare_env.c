#include <node_api.h>
#include "../../js-native-api/common.h"

static napi_value compare(napi_env env, napi_callback_info info) {
  napi_value external;
  size_t argc = 1;
  void* data;
  napi_value return_value;

  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &external, NULL, NULL));
  NAPI_CALL(env, napi_get_value_external(env, external, &data));
  NAPI_CALL(env, napi_get_boolean(env, ((napi_env)data) == env, &return_value));

  return return_value;
}

static napi_value Init(napi_env env, napi_value exports) {
  NAPI_CALL(env, napi_create_function(
      env, "exports", NAPI_AUTO_LENGTH, compare, NULL, &exports));
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
