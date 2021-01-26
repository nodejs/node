#include <js_native_api.h>
#include "../common.h"

static node_api_value Test(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_number,
      "Wrong type of arguments. Expects a number as first argument.");

  double input;
  NODE_API_CALL(env, node_api_get_value_double(env, args[0], &input));

  node_api_value output;
  NODE_API_CALL(env, node_api_create_double(env, input, &output));

  return output;
}

static node_api_value
TestUint32Truncation(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_number,
      "Wrong type of arguments. Expects a number as first argument.");

  uint32_t input;
  NODE_API_CALL(env, node_api_get_value_uint32(env, args[0], &input));

  node_api_value output;
  NODE_API_CALL(env, node_api_create_uint32(env, input, &output));

  return output;
}

static node_api_value
TestInt32Truncation(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_number,
      "Wrong type of arguments. Expects a number as first argument.");

  int32_t input;
  NODE_API_CALL(env, node_api_get_value_int32(env, args[0], &input));

  node_api_value output;
  NODE_API_CALL(env, node_api_create_int32(env, input, &output));

  return output;
}

static node_api_value
TestInt64Truncation(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_number,
      "Wrong type of arguments. Expects a number as first argument.");

  int64_t input;
  NODE_API_CALL(env, node_api_get_value_int64(env, args[0], &input));

  node_api_value output;
  NODE_API_CALL(env, node_api_create_int64(env, input, &output));

  return output;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("Test", Test),
    DECLARE_NODE_API_PROPERTY("TestInt32Truncation", TestInt32Truncation),
    DECLARE_NODE_API_PROPERTY("TestUint32Truncation", TestUint32Truncation),
    DECLARE_NODE_API_PROPERTY("TestInt64Truncation", TestInt64Truncation),
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
