#include <node_api.h>
#include "../common.h"

napi_value checkError(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  bool r;
  NAPI_CALL(env, napi_is_error(env, args[0], &r));

  napi_value result;
  NAPI_CALL(env, napi_get_boolean(env, r, &result));

  return result;
}

napi_value throwExistingError(napi_env env, napi_callback_info info) {
  napi_value message;
  napi_value error;
  NAPI_CALL(env, napi_create_string_utf8(env, "existing error", -1, &message));
  NAPI_CALL(env, napi_create_error(env, message, &error));
  NAPI_CALL(env, napi_throw(env, error));
  return nullptr;
}

napi_value throwError(napi_env env, napi_callback_info info) {
  NAPI_CALL(env, napi_throw_error(env, "error"));
  return nullptr;
}

napi_value throwRangeError(napi_env env, napi_callback_info info) {
  NAPI_CALL(env, napi_throw_range_error(env, "range error"));
  return nullptr;
}

napi_value throwTypeError(napi_env env, napi_callback_info info) {
  NAPI_CALL(env, napi_throw_type_error(env, "type error"));
  return nullptr;
}

napi_value createError(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_value message;
  NAPI_CALL(env, napi_create_string_utf8(env, "error", -1, &message));
  NAPI_CALL(env, napi_create_error(env, message, &result));
  return result;
}

napi_value createRangeError(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_value message;
  NAPI_CALL(env, napi_create_string_utf8(env, "range error", -1, &message));
  NAPI_CALL(env, napi_create_range_error(env, message, &result));
  return result;
}

napi_value createTypeError(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_value message;
  NAPI_CALL(env, napi_create_string_utf8(env, "type error", -1, &message));
  NAPI_CALL(env, napi_create_type_error(env, message, &result));
  return result;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("checkError", checkError),
    DECLARE_NAPI_PROPERTY("throwExistingError", throwExistingError),
    DECLARE_NAPI_PROPERTY("throwError", throwError),
    DECLARE_NAPI_PROPERTY("throwRangeError", throwRangeError),
    DECLARE_NAPI_PROPERTY("throwTypeError", throwTypeError),
    DECLARE_NAPI_PROPERTY("createError", createError),
    DECLARE_NAPI_PROPERTY("createRangeError", createRangeError),
    DECLARE_NAPI_PROPERTY("createTypeError", createTypeError),
  };

  NAPI_CALL_RETURN_VOID(env, napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));
}

NAPI_MODULE(addon, Init)
