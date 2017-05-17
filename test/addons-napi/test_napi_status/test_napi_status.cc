#include <node_api.h>
#include "../common.h"

napi_value createNapiError(napi_env env, napi_callback_info info) {
  napi_value value;
  NAPI_CALL(env, napi_create_string_utf8(env, "xyz", 3, &value));

  double double_value;
  napi_status status = napi_get_value_double(env, value, &double_value);

  NAPI_ASSERT(env, status != napi_ok, "Failed to produce error condition");

  const napi_extended_error_info *error_info = 0;
  NAPI_CALL(env, napi_get_last_error_info(env, &error_info));

  NAPI_ASSERT(env, error_info->error_code == status,
    "Last error info code should match last status");
  NAPI_ASSERT(env, error_info->error_message,
    "Last error info message should not be null");

  return nullptr;
}

napi_value testNapiErrorCleanup(napi_env env, napi_callback_info info) {
  const napi_extended_error_info *error_info = 0;
  NAPI_CALL(env, napi_get_last_error_info(env, &error_info));

  napi_value result;
  bool is_ok = error_info->error_code == napi_ok;
  NAPI_CALL(env, napi_get_boolean(env, is_ok, &result));

  return result;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("createNapiError", createNapiError),
    DECLARE_NAPI_PROPERTY("testNapiErrorCleanup", testNapiErrorCleanup),
  };

  NAPI_CALL_RETURN_VOID(env, napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));
}

NAPI_MODULE(addon, Init)
