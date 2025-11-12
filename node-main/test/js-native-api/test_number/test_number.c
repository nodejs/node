#include <js_native_api.h>
#include "../common.h"
#include "../entry_point.h"
#include "test_null.h"

static napi_value Test(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NODE_API_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == napi_number,
      "Wrong type of arguments. Expects a number as first argument.");

  double input;
  NODE_API_CALL(env, napi_get_value_double(env, args[0], &input));

  napi_value output;
  NODE_API_CALL(env, napi_create_double(env, input, &output));

  return output;
}

static napi_value TestUint32Truncation(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NODE_API_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == napi_number,
      "Wrong type of arguments. Expects a number as first argument.");

  uint32_t input;
  NODE_API_CALL(env, napi_get_value_uint32(env, args[0], &input));

  napi_value output;
  NODE_API_CALL(env, napi_create_uint32(env, input, &output));

  return output;
}

static napi_value TestInt32Truncation(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NODE_API_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == napi_number,
      "Wrong type of arguments. Expects a number as first argument.");

  int32_t input;
  NODE_API_CALL(env, napi_get_value_int32(env, args[0], &input));

  napi_value output;
  NODE_API_CALL(env, napi_create_int32(env, input, &output));

  return output;
}

static napi_value TestInt64Truncation(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NODE_API_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == napi_number,
      "Wrong type of arguments. Expects a number as first argument.");

  int64_t input;
  NODE_API_CALL(env, napi_get_value_int64(env, args[0], &input));

  napi_value output;
  NODE_API_CALL(env, napi_create_int64(env, input, &output));

  return output;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("Test", Test),
    DECLARE_NODE_API_PROPERTY("TestInt32Truncation", TestInt32Truncation),
    DECLARE_NODE_API_PROPERTY("TestUint32Truncation", TestUint32Truncation),
    DECLARE_NODE_API_PROPERTY("TestInt64Truncation", TestInt64Truncation),
  };

  NODE_API_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  init_test_null(env, exports);

  return exports;
}
EXTERN_C_END
