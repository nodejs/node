#include <node_api.h>
#include "../common.h"

napi_value AsBool(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  bool value;
  NAPI_CALL(env, napi_get_value_bool(env, args[0], &value));

  napi_value output;
  NAPI_CALL(env, napi_get_boolean(env, value, &output));

  return output;
}

napi_value AsInt32(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  int32_t value;
  NAPI_CALL(env, napi_get_value_int32(env, args[0], &value));

  napi_value output;
  NAPI_CALL(env, napi_create_int32(env, value, &output));

  return output;
}

napi_value AsUInt32(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  uint32_t value;
  NAPI_CALL(env, napi_get_value_uint32(env, args[0], &value));

  napi_value output;
  NAPI_CALL(env, napi_create_uint32(env, value, &output));

  return output;
}

napi_value AsInt64(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  int64_t value;
  NAPI_CALL(env, napi_get_value_int64(env, args[0], &value));

  napi_value output;
  NAPI_CALL(env, napi_create_int64(env, (double)value, &output));

  return output;
}

napi_value AsDouble(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  double value;
  NAPI_CALL(env, napi_get_value_double(env, args[0], &value));

  napi_value output;
  NAPI_CALL(env, napi_create_double(env, value, &output));

  return output;
}

napi_value AsString(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  char value[100];
  NAPI_CALL(env,
    napi_get_value_string_utf8(env, args[0], value, sizeof(value), NULL));

  napi_value output;
  NAPI_CALL(env, napi_create_string_utf8(env, value, -1, &output));

  return output;
}

napi_value ToBool(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value output;
  NAPI_CALL(env, napi_coerce_to_bool(env, args[0], &output));

  return output;
}

napi_value ToNumber(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value output;
  NAPI_CALL(env, napi_coerce_to_number(env, args[0], &output));

  return output;
}

napi_value ToObject(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value output;
  NAPI_CALL(env, napi_coerce_to_object(env, args[0], &output));

  return output;
}

napi_value ToString(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value output;
  NAPI_CALL(env, napi_coerce_to_string(env, args[0], &output));

  return output;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("asBool", AsBool),
    DECLARE_NAPI_PROPERTY("asInt32", AsInt32),
    DECLARE_NAPI_PROPERTY("asUInt32", AsUInt32),
    DECLARE_NAPI_PROPERTY("asInt64", AsInt64),
    DECLARE_NAPI_PROPERTY("asDouble", AsDouble),
    DECLARE_NAPI_PROPERTY("asString", AsString),
    DECLARE_NAPI_PROPERTY("toBool", ToBool),
    DECLARE_NAPI_PROPERTY("toNumber", ToNumber),
    DECLARE_NAPI_PROPERTY("toObject", ToObject),
    DECLARE_NAPI_PROPERTY("toString", ToString),
  };

  NAPI_CALL_RETURN_VOID(env, napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
