#define NAPI_VERSION 9
#include <js_native_api.h>
#include "../common.h"
#include "../entry_point.h"

static napi_value checkError(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  bool r;
  NODE_API_CALL(env, napi_is_error(env, args[0], &r));

  napi_value result;
  NODE_API_CALL(env, napi_get_boolean(env, r, &result));

  return result;
}

static napi_value throwExistingError(napi_env env, napi_callback_info info) {
  napi_value message;
  napi_value error;
  NODE_API_CALL(env, napi_create_string_utf8(
      env, "existing error", NAPI_AUTO_LENGTH, &message));
  NODE_API_CALL(env, napi_create_error(env, NULL,  message, &error));
  NODE_API_CALL(env, napi_throw(env, error));
  return NULL;
}

static napi_value throwError(napi_env env, napi_callback_info info) {
  NODE_API_CALL(env, napi_throw_error(env, NULL, "error"));
  return NULL;
}

static napi_value throwRangeError(napi_env env, napi_callback_info info) {
  NODE_API_CALL(env, napi_throw_range_error(env, NULL, "range error"));
  return NULL;
}

static napi_value throwTypeError(napi_env env, napi_callback_info info) {
  NODE_API_CALL(env, napi_throw_type_error(env, NULL, "type error"));
  return NULL;
}

static napi_value throwSyntaxError(napi_env env, napi_callback_info info) {
  NODE_API_CALL(env, node_api_throw_syntax_error(env, NULL, "syntax error"));
  return NULL;
}

static napi_value throwErrorCode(napi_env env, napi_callback_info info) {
  NODE_API_CALL(env, napi_throw_error(env, "ERR_TEST_CODE", "Error [error]"));
  return NULL;
}

static napi_value throwRangeErrorCode(napi_env env, napi_callback_info info) {
  NODE_API_CALL(env,
      napi_throw_range_error(env, "ERR_TEST_CODE", "RangeError [range error]"));
  return NULL;
}

static napi_value throwTypeErrorCode(napi_env env, napi_callback_info info) {
  NODE_API_CALL(env,
      napi_throw_type_error(env, "ERR_TEST_CODE", "TypeError [type error]"));
  return NULL;
}

static napi_value throwSyntaxErrorCode(napi_env env, napi_callback_info info) {
  NODE_API_CALL(env,
      node_api_throw_syntax_error(env, "ERR_TEST_CODE", "SyntaxError [syntax error]"));
  return NULL;
}

static napi_value createError(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_value message;
  NODE_API_CALL(env, napi_create_string_utf8(
      env, "error", NAPI_AUTO_LENGTH, &message));
  NODE_API_CALL(env, napi_create_error(env, NULL, message, &result));
  return result;
}

static napi_value createRangeError(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_value message;
  NODE_API_CALL(env, napi_create_string_utf8(
      env, "range error", NAPI_AUTO_LENGTH, &message));
  NODE_API_CALL(env, napi_create_range_error(env, NULL, message, &result));
  return result;
}

static napi_value createTypeError(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_value message;
  NODE_API_CALL(env, napi_create_string_utf8(
      env, "type error", NAPI_AUTO_LENGTH, &message));
  NODE_API_CALL(env, napi_create_type_error(env, NULL, message, &result));
  return result;
}

static napi_value createSyntaxError(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_value message;
  NODE_API_CALL(env, napi_create_string_utf8(
      env, "syntax error", NAPI_AUTO_LENGTH, &message));
  NODE_API_CALL(env, node_api_create_syntax_error(env, NULL, message, &result));
  return result;
}

static napi_value createErrorCode(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_value message;
  napi_value code;
  NODE_API_CALL(env, napi_create_string_utf8(
      env, "Error [error]", NAPI_AUTO_LENGTH, &message));
  NODE_API_CALL(env, napi_create_string_utf8(
      env, "ERR_TEST_CODE", NAPI_AUTO_LENGTH, &code));
  NODE_API_CALL(env, napi_create_error(env, code, message, &result));
  return result;
}

static napi_value createRangeErrorCode(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_value message;
  napi_value code;
  NODE_API_CALL(env,
      napi_create_string_utf8(
          env, "RangeError [range error]", NAPI_AUTO_LENGTH, &message));
  NODE_API_CALL(env, napi_create_string_utf8(
      env, "ERR_TEST_CODE", NAPI_AUTO_LENGTH, &code));
  NODE_API_CALL(env, napi_create_range_error(env, code, message, &result));
  return result;
}

static napi_value createTypeErrorCode(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_value message;
  napi_value code;
  NODE_API_CALL(env,
      napi_create_string_utf8(
          env, "TypeError [type error]", NAPI_AUTO_LENGTH, &message));
  NODE_API_CALL(env, napi_create_string_utf8(
      env, "ERR_TEST_CODE", NAPI_AUTO_LENGTH, &code));
  NODE_API_CALL(env, napi_create_type_error(env, code, message, &result));
  return result;
}

static napi_value createSyntaxErrorCode(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_value message;
  napi_value code;
  NODE_API_CALL(env,
      napi_create_string_utf8(
          env, "SyntaxError [syntax error]", NAPI_AUTO_LENGTH, &message));
  NODE_API_CALL(env, napi_create_string_utf8(
      env, "ERR_TEST_CODE", NAPI_AUTO_LENGTH, &code));
  NODE_API_CALL(env, node_api_create_syntax_error(env, code, message, &result));
  return result;
}

static napi_value throwArbitrary(napi_env env, napi_callback_info info) {
  napi_value arbitrary;
  size_t argc = 1;
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, &arbitrary, NULL, NULL));
  NODE_API_CALL(env, napi_throw(env, arbitrary));
  return NULL;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("checkError", checkError),
    DECLARE_NODE_API_PROPERTY("throwExistingError", throwExistingError),
    DECLARE_NODE_API_PROPERTY("throwError", throwError),
    DECLARE_NODE_API_PROPERTY("throwRangeError", throwRangeError),
    DECLARE_NODE_API_PROPERTY("throwTypeError", throwTypeError),
    DECLARE_NODE_API_PROPERTY("throwSyntaxError", throwSyntaxError),
    DECLARE_NODE_API_PROPERTY("throwErrorCode", throwErrorCode),
    DECLARE_NODE_API_PROPERTY("throwRangeErrorCode", throwRangeErrorCode),
    DECLARE_NODE_API_PROPERTY("throwTypeErrorCode", throwTypeErrorCode),
    DECLARE_NODE_API_PROPERTY("throwSyntaxErrorCode", throwSyntaxErrorCode),
    DECLARE_NODE_API_PROPERTY("throwArbitrary", throwArbitrary),
    DECLARE_NODE_API_PROPERTY("createError", createError),
    DECLARE_NODE_API_PROPERTY("createRangeError", createRangeError),
    DECLARE_NODE_API_PROPERTY("createTypeError", createTypeError),
    DECLARE_NODE_API_PROPERTY("createSyntaxError", createSyntaxError),
    DECLARE_NODE_API_PROPERTY("createErrorCode", createErrorCode),
    DECLARE_NODE_API_PROPERTY("createRangeErrorCode", createRangeErrorCode),
    DECLARE_NODE_API_PROPERTY("createTypeErrorCode", createTypeErrorCode),
    DECLARE_NODE_API_PROPERTY("createSyntaxErrorCode", createSyntaxErrorCode),
  };

  NODE_API_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
