#include <node_api.h>
#include "../common.h"

napi_value Test(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc >= 1, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NAPI_ASSERT(env, valuetype0 == napi_number,
      "Wrong type of arguments. Expects a number as first argument.");

  double input;
  NAPI_CALL(env, napi_get_value_double(env, args[0], &input));

  napi_value output;
  NAPI_CALL(env, napi_create_double(env, input, &output));

  return output;
}

napi_value TestInt32Truncation(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc >= 1, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NAPI_ASSERT(env, valuetype0 == napi_number,
      "Wrong type of arguments. Expects a number as first argument.");

  int32_t input;
  NAPI_CALL(env, napi_get_value_int32(env, args[0], &input));

  napi_value output;
  NAPI_CALL(env, napi_create_int32(env, input, &output));

  return output;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("Test", Test),
    DECLARE_NAPI_PROPERTY("TestInt32Truncation", TestInt32Truncation),
  };

  NAPI_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
