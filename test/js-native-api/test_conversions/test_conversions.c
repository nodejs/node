#include <js_native_api.h>
#include "../common.h"
#include "../entry_point.h"
#include "test_null.h"

static napi_value AsBool(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  bool value;
  NODE_API_CALL(env, napi_get_value_bool(env, args[0], &value));

  napi_value output;
  NODE_API_CALL(env, napi_get_boolean(env, value, &output));

  return output;
}

static napi_value AsInt32(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  int32_t value;
  NODE_API_CALL(env, napi_get_value_int32(env, args[0], &value));

  napi_value output;
  NODE_API_CALL(env, napi_create_int32(env, value, &output));

  return output;
}

static napi_value AsUInt32(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  uint32_t value;
  NODE_API_CALL(env, napi_get_value_uint32(env, args[0], &value));

  napi_value output;
  NODE_API_CALL(env, napi_create_uint32(env, value, &output));

  return output;
}

static napi_value AsInt64(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  int64_t value;
  NODE_API_CALL(env, napi_get_value_int64(env, args[0], &value));

  napi_value output;
  NODE_API_CALL(env, napi_create_int64(env, (double)value, &output));

  return output;
}

static napi_value AsDouble(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  double value;
  NODE_API_CALL(env, napi_get_value_double(env, args[0], &value));

  napi_value output;
  NODE_API_CALL(env, napi_create_double(env, value, &output));

  return output;
}

static napi_value AsString(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  char value[100];
  NODE_API_CALL(env,
    napi_get_value_string_utf8(env, args[0], value, sizeof(value), NULL));

  napi_value output;
  NODE_API_CALL(env, napi_create_string_utf8(
      env, value, NAPI_AUTO_LENGTH, &output));

  return output;
}

static napi_value ToBool(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value output;
  NODE_API_CALL(env, napi_coerce_to_bool(env, args[0], &output));

  return output;
}

static napi_value ToNumber(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value output;
  NODE_API_CALL(env, napi_coerce_to_number(env, args[0], &output));

  return output;
}

static napi_value ToObject(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value output;
  NODE_API_CALL(env, napi_coerce_to_object(env, args[0], &output));

  return output;
}

static napi_value ToString(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value output;
  NODE_API_CALL(env, napi_coerce_to_string(env, args[0], &output));

  return output;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
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

  NODE_API_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  init_test_null(env, exports);

  return exports;
}
EXTERN_C_END
