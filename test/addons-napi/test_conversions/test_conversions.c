#include <node_api.h>

void ThrowLastError(napi_env env) {
  const napi_extended_error_info* error_info;
  napi_get_last_error_info(env, &error_info);
  if (error_info->error_code != napi_ok) {
    napi_throw_error(env, error_info->error_message);
  }
}

void AsBool(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value input;
  status = napi_get_cb_args(env, info, &input, 1);
  if (status != napi_ok) goto done;

  bool value;
  status = napi_get_value_bool(env, input, &value);
  if (status != napi_ok) goto done;

  napi_value output;
  status = napi_get_boolean(env, value, &output);
  if (status != napi_ok) goto done;

  status = napi_set_return_value(env, info, output);
  if (status != napi_ok) goto done;

done:
  if (status != napi_ok) ThrowLastError(env);
}

void AsInt32(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value input;
  status = napi_get_cb_args(env, info, &input, 1);
  if (status != napi_ok) goto done;

  int32_t value;
  status = napi_get_value_int32(env, input, &value);
  if (status != napi_ok) goto done;

  napi_value output;
  status = napi_create_number(env, value, &output);
  if (status != napi_ok) goto done;

  status = napi_set_return_value(env, info, output);
  if (status != napi_ok) goto done;

done:
  if (status != napi_ok) ThrowLastError(env);
}

void AsUInt32(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value input;
  status = napi_get_cb_args(env, info, &input, 1);
  if (status != napi_ok) goto done;

  uint32_t value;
  status = napi_get_value_uint32(env, input, &value);
  if (status != napi_ok) goto done;

  napi_value output;
  status = napi_create_number(env, value, &output);
  if (status != napi_ok) goto done;

  status = napi_set_return_value(env, info, output);
  if (status != napi_ok) goto done;

done:
  if (status != napi_ok) ThrowLastError(env);
}

void AsInt64(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value input;
  status = napi_get_cb_args(env, info, &input, 1);
  if (status != napi_ok) goto done;

  int64_t value;
  status = napi_get_value_int64(env, input, &value);
  if (status != napi_ok) goto done;

  napi_value output;
  status = napi_create_number(env, (double)value, &output);
  if (status != napi_ok) goto done;

  status = napi_set_return_value(env, info, output);
  if (status != napi_ok) goto done;

done:
  if (status != napi_ok) ThrowLastError(env);
}

void AsDouble(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value input;
  status = napi_get_cb_args(env, info, &input, 1);
  if (status != napi_ok) goto done;

  double value;
  status = napi_get_value_double(env, input, &value);
  if (status != napi_ok) goto done;

  napi_value output;
  status = napi_create_number(env, value, &output);
  if (status != napi_ok) goto done;

  status = napi_set_return_value(env, info, output);
  if (status != napi_ok) goto done;

done:
  if (status != napi_ok) ThrowLastError(env);
}

void AsString(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value input;
  status = napi_get_cb_args(env, info, &input, 1);
  if (status != napi_ok) goto done;

  char value[100];
  status = napi_get_value_string_utf8(env, input, value, sizeof(value), NULL);
  if (status != napi_ok) goto done;

  napi_value output;
  status = napi_create_string_utf8(env, value, -1, &output);
  if (status != napi_ok) goto done;

  status = napi_set_return_value(env, info, output);
  if (status != napi_ok) goto done;

done:
  if (status != napi_ok) ThrowLastError(env);
}

void ToBool(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value input;
  status = napi_get_cb_args(env, info, &input, 1);
  if (status != napi_ok) goto done;

  napi_value output;
  status = napi_coerce_to_bool(env, input, &output);
  if (status != napi_ok) goto done;

  status = napi_set_return_value(env, info, output);
  if (status != napi_ok) goto done;

done:
  if (status != napi_ok) ThrowLastError(env);
}

void ToNumber(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value input;
  status = napi_get_cb_args(env, info, &input, 1);
  if (status != napi_ok) goto done;

  napi_value output;
  status = napi_coerce_to_number(env, input, &output);
  if (status != napi_ok) goto done;

  status = napi_set_return_value(env, info, output);
  if (status != napi_ok) goto done;

done:
  if (status != napi_ok) ThrowLastError(env);
}

void ToObject(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value input;
  status = napi_get_cb_args(env, info, &input, 1);
  if (status != napi_ok) goto done;

  napi_value output;
  status = napi_coerce_to_object(env, input, &output);
  if (status != napi_ok) goto done;

  status = napi_set_return_value(env, info, output);
  if (status != napi_ok) goto done;

done:
  if (status != napi_ok) ThrowLastError(env);
}

void ToString(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value input;
  status = napi_get_cb_args(env, info, &input, 1);
  if (status != napi_ok) goto done;

  napi_value output;
  status = napi_coerce_to_string(env, input, &output);
  if (status != napi_ok) goto done;

  status = napi_set_return_value(env, info, output);
  if (status != napi_ok) goto done;

done:
  if (status != napi_ok) ThrowLastError(env);
}

#define DECLARE_NAPI_METHOD(name, func)     \
  { name, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor descriptors[] = {
      DECLARE_NAPI_METHOD("asBool", AsBool),
      DECLARE_NAPI_METHOD("asInt32", AsInt32),
      DECLARE_NAPI_METHOD("asUInt32", AsUInt32),
      DECLARE_NAPI_METHOD("asInt64", AsInt64),
      DECLARE_NAPI_METHOD("asDouble", AsDouble),
      DECLARE_NAPI_METHOD("asString", AsString),
      DECLARE_NAPI_METHOD("toBool", ToBool),
      DECLARE_NAPI_METHOD("toNumber", ToNumber),
      DECLARE_NAPI_METHOD("toObject", ToObject),
      DECLARE_NAPI_METHOD("toString", ToString),
  };

  napi_status status = napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
