#include <js_native_api.h>
#include "../common.h"
#include "test_null.h"

static node_api_value AsBool(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  bool value;
  NODE_API_CALL(env, node_api_get_value_bool(env, args[0], &value));

  node_api_value output;
  NODE_API_CALL(env, node_api_get_boolean(env, value, &output));

  return output;
}

static node_api_value AsInt32(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  int32_t value;
  NODE_API_CALL(env, node_api_get_value_int32(env, args[0], &value));

  node_api_value output;
  NODE_API_CALL(env, node_api_create_int32(env, value, &output));

  return output;
}

static node_api_value AsUInt32(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  uint32_t value;
  NODE_API_CALL(env, node_api_get_value_uint32(env, args[0], &value));

  node_api_value output;
  NODE_API_CALL(env, node_api_create_uint32(env, value, &output));

  return output;
}

static node_api_value AsInt64(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  int64_t value;
  NODE_API_CALL(env, node_api_get_value_int64(env, args[0], &value));

  node_api_value output;
  NODE_API_CALL(env, node_api_create_int64(env, (double)value, &output));

  return output;
}

static node_api_value AsDouble(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  double value;
  NODE_API_CALL(env, node_api_get_value_double(env, args[0], &value));

  node_api_value output;
  NODE_API_CALL(env, node_api_create_double(env, value, &output));

  return output;
}

static node_api_value AsString(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  char value[100];
  NODE_API_CALL(env,
    node_api_get_value_string_utf8(env, args[0], value, sizeof(value), NULL));

  node_api_value output;
  NODE_API_CALL(env, node_api_create_string_utf8(
      env, value, NODE_API_AUTO_LENGTH, &output));

  return output;
}

static node_api_value ToBool(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  node_api_value output;
  NODE_API_CALL(env, node_api_coerce_to_bool(env, args[0], &output));

  return output;
}

static node_api_value ToNumber(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  node_api_value output;
  NODE_API_CALL(env, node_api_coerce_to_number(env, args[0], &output));

  return output;
}

static node_api_value ToObject(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  node_api_value output;
  NODE_API_CALL(env, node_api_coerce_to_object(env, args[0], &output));

  return output;
}

static node_api_value ToString(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  node_api_value output;
  NODE_API_CALL(env, node_api_coerce_to_string(env, args[0], &output));

  return output;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("asBool", AsBool),
    DECLARE_NODE_API_PROPERTY("asInt32", AsInt32),
    DECLARE_NODE_API_PROPERTY("asUInt32", AsUInt32),
    DECLARE_NODE_API_PROPERTY("asInt64", AsInt64),
    DECLARE_NODE_API_PROPERTY("asDouble", AsDouble),
    DECLARE_NODE_API_PROPERTY("asString", AsString),
    DECLARE_NODE_API_PROPERTY("toBool", ToBool),
    DECLARE_NODE_API_PROPERTY("toNumber", ToNumber),
    DECLARE_NODE_API_PROPERTY("toObject", ToObject),
    DECLARE_NODE_API_PROPERTY("toString", ToString),
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  init_test_null(env, exports);

  return exports;
}
EXTERN_C_END
