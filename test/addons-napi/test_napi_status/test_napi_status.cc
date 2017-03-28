#include <node_api.h>

#define DECLARE_NAPI_METHOD(func) \
  { #func, func, 0, 0, 0, napi_default, 0 }

void createNapiError(napi_env env, napi_callback_info info) {
  napi_status status;
  napi_value value;
  double double_value;

  status = napi_create_string_utf8(env, "xyz", 3, &value);
  if (status != napi_ok) return;

  status = napi_get_value_double(env, value, &double_value);
  if (status == napi_ok) {
    napi_throw_error(env, "Failed to produce error condition");
  }
}

void testNapiErrorCleanup(napi_env env, napi_callback_info info) {
  napi_status status;
  const napi_extended_error_info *error_info = 0;
  napi_value result;

  status = napi_get_last_error_info(env, &error_info);
  if (status != napi_ok) return;

  status = napi_get_boolean(env, (error_info->error_code == napi_ok), &result);
  if (status != napi_ok) return;

  napi_set_return_value(env, info, result);
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  napi_property_descriptor descriptors[] = {
      DECLARE_NAPI_METHOD(createNapiError),
      DECLARE_NAPI_METHOD(testNapiErrorCleanup)
  };

  status = napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
