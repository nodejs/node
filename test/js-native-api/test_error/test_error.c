#include <js_native_api.h>
#include "../common.h"

static napi_value checkError(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  bool r;
  NAPI_CALL(env, napi_is_error(env, args[0], &r));

  napi_value result;
  NAPI_CALL(env, napi_get_boolean(env, r, &result));

  return result;
}

static napi_value throwExistingError(napi_env env, napi_callback_info info) {
  napi_value message;
  napi_value error;
  NAPI_CALL(env, napi_create_string_utf8(
      env, "existing error", NAPI_AUTO_LENGTH, &message));
  NAPI_CALL(env, napi_create_error(env, NULL,  message, &error));
  NAPI_CALL(env, napi_throw(env, error));
  return NULL;
}

static napi_value throwError(napi_env env, napi_callback_info info) {
  NAPI_CALL(env, napi_throw_error(env, NULL, "error"));
  return NULL;
}

static napi_value throwRangeError(napi_env env, napi_callback_info info) {
  NAPI_CALL(env, napi_throw_range_error(env, NULL, "range error"));
  return NULL;
}

static napi_value throwTypeError(napi_env env, napi_callback_info info) {
  NAPI_CALL(env, napi_throw_type_error(env, NULL, "type error"));
  return NULL;
}

static napi_value throwErrorCode(napi_env env, napi_callback_info info) {
  NAPI_CALL(env, napi_throw_error(env, "ERR_TEST_CODE", "Error [error]"));
  return NULL;
}

static napi_value throwRangeErrorCode(napi_env env, napi_callback_info info) {
  NAPI_CALL(env, napi_throw_range_error(env,
                                        "ERR_TEST_CODE",
                                        "RangeError [range error]"));
  return NULL;
}

static napi_value throwTypeErrorCode(napi_env env, napi_callback_info info) {
  NAPI_CALL(env, napi_throw_type_error(env,
                                       "ERR_TEST_CODE",
                                       "TypeError [type error]"));
  return NULL;
}


static napi_value createError(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_value message;
  NAPI_CALL(env, napi_create_string_utf8(
      env, "error", NAPI_AUTO_LENGTH, &message));
  NAPI_CALL(env, napi_create_error(env, NULL, message, &result));
  return result;
}

static napi_value createRangeError(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_value message;
  NAPI_CALL(env, napi_create_string_utf8(
      env, "range error", NAPI_AUTO_LENGTH, &message));
  NAPI_CALL(env, napi_create_range_error(env, NULL, message, &result));
  return result;
}

static napi_value createTypeError(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_value message;
  NAPI_CALL(env, napi_create_string_utf8(
      env, "type error", NAPI_AUTO_LENGTH, &message));
  NAPI_CALL(env, napi_create_type_error(env, NULL, message, &result));
  return result;
}

static napi_value createErrorCode(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_value message;
  napi_value code;
  NAPI_CALL(env, napi_create_string_utf8(
      env, "Error [error]", NAPI_AUTO_LENGTH, &message));
  NAPI_CALL(env, napi_create_string_utf8(
      env, "ERR_TEST_CODE", NAPI_AUTO_LENGTH, &code));
  NAPI_CALL(env, napi_create_error(env, code, message, &result));
  return result;
}

static napi_value createRangeErrorCode(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_value message;
  napi_value code;
  NAPI_CALL(env, napi_create_string_utf8(env,
                                         "RangeError [range error]",
                                         NAPI_AUTO_LENGTH,
                                         &message));
  NAPI_CALL(env, napi_create_string_utf8(
      env, "ERR_TEST_CODE", NAPI_AUTO_LENGTH, &code));
  NAPI_CALL(env, napi_create_range_error(env, code, message, &result));
  return result;
}

static napi_value createTypeErrorCode(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_value message;
  napi_value code;
  NAPI_CALL(env, napi_create_string_utf8(env,
                                         "TypeError [type error]",
                                         NAPI_AUTO_LENGTH,
                                         &message));
  NAPI_CALL(env, napi_create_string_utf8(
      env, "ERR_TEST_CODE", NAPI_AUTO_LENGTH, &code));
  NAPI_CALL(env, napi_create_type_error(env, code, message, &result));
  return result;
}

static napi_value throwArbitrary(napi_env env, napi_callback_info info) {
  napi_value arbitrary;
  size_t argc = 1;
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &arbitrary, NULL, NULL));
  NAPI_CALL(env, napi_throw(env, arbitrary));
  return NULL;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("checkError", checkError),
    DECLARE_NAPI_PROPERTY("throwExistingError", throwExistingError),
    DECLARE_NAPI_PROPERTY("throwError", throwError),
    DECLARE_NAPI_PROPERTY("throwRangeError", throwRangeError),
    DECLARE_NAPI_PROPERTY("throwTypeError", throwTypeError),
    DECLARE_NAPI_PROPERTY("throwErrorCode", throwErrorCode),
    DECLARE_NAPI_PROPERTY("throwRangeErrorCode", throwRangeErrorCode),
    DECLARE_NAPI_PROPERTY("throwTypeErrorCode", throwTypeErrorCode),
    DECLARE_NAPI_PROPERTY("throwArbitrary", throwArbitrary),
    DECLARE_NAPI_PROPERTY("createError", createError),
    DECLARE_NAPI_PROPERTY("createRangeError", createRangeError),
    DECLARE_NAPI_PROPERTY("createTypeError", createTypeError),
    DECLARE_NAPI_PROPERTY("createErrorCode", createErrorCode),
    DECLARE_NAPI_PROPERTY("createRangeErrorCode", createRangeErrorCode),
    DECLARE_NAPI_PROPERTY("createTypeErrorCode", createTypeErrorCode),
  };

  NAPI_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
