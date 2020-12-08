#define NAPI_EXPERIMENTAL
#include <node_api.h>
#include "../../js-native-api/common.h"

static napi_value test(napi_env env, napi_callback_info info) {
  bool should_return = false;
  do {
    NAPI_CALL(env, napi_should_return_to_caller(env, &should_return));
  } while (!should_return);

  napi_value result;
  NAPI_CALL(env, napi_get_undefined(env, &result));

  return result;
}

static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("test", test),
  };
  NAPI_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
