#include <js_native_api.h>
#include "../common.h"

static node_api_value
checkError(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  bool r;
  NODE_API_CALL(env, node_api_is_error(env, args[0], &r));

  node_api_value result;
  NODE_API_CALL(env, node_api_get_boolean(env, r, &result));

  return result;
}

static node_api_value
throwExistingError(node_api_env env, node_api_callback_info info) {
  node_api_value message;
  node_api_value error;
  NODE_API_CALL(env, node_api_create_string_utf8(
      env, "existing error", NODE_API_AUTO_LENGTH, &message));
  NODE_API_CALL(env, node_api_create_error(env, NULL,  message, &error));
  NODE_API_CALL(env, node_api_throw(env, error));
  return NULL;
}

static node_api_value
throwError(node_api_env env, node_api_callback_info info) {
  NODE_API_CALL(env, node_api_throw_error(env, NULL, "error"));
  return NULL;
}

static node_api_value
throwRangeError(node_api_env env, node_api_callback_info info) {
  NODE_API_CALL(env, node_api_throw_range_error(env, NULL, "range error"));
  return NULL;
}

static node_api_value
throwTypeError(node_api_env env, node_api_callback_info info) {
  NODE_API_CALL(env, node_api_throw_type_error(env, NULL, "type error"));
  return NULL;
}

static node_api_value
throwErrorCode(node_api_env env, node_api_callback_info info) {
  NODE_API_CALL(env,
      node_api_throw_error(env, "ERR_TEST_CODE", "Error [error]"));
  return NULL;
}

static node_api_value
throwRangeErrorCode(node_api_env env, node_api_callback_info info) {
  NODE_API_CALL(env, node_api_throw_range_error(env,
                                                "ERR_TEST_CODE",
                                                "RangeError [range error]"));
  return NULL;
}

static node_api_value
throwTypeErrorCode(node_api_env env, node_api_callback_info info) {
  NODE_API_CALL(env, node_api_throw_type_error(env,
                                               "ERR_TEST_CODE",
                                               "TypeError [type error]"));
  return NULL;
}


static node_api_value
createError(node_api_env env, node_api_callback_info info) {
  node_api_value result;
  node_api_value message;
  NODE_API_CALL(env, node_api_create_string_utf8(
      env, "error", NODE_API_AUTO_LENGTH, &message));
  NODE_API_CALL(env, node_api_create_error(env, NULL, message, &result));
  return result;
}

static node_api_value
createRangeError(node_api_env env, node_api_callback_info info) {
  node_api_value result;
  node_api_value message;
  NODE_API_CALL(env, node_api_create_string_utf8(
      env, "range error", NODE_API_AUTO_LENGTH, &message));
  NODE_API_CALL(env, node_api_create_range_error(env, NULL, message, &result));
  return result;
}

static node_api_value
createTypeError(node_api_env env, node_api_callback_info info) {
  node_api_value result;
  node_api_value message;
  NODE_API_CALL(env, node_api_create_string_utf8(
      env, "type error", NODE_API_AUTO_LENGTH, &message));
  NODE_API_CALL(env, node_api_create_type_error(env, NULL, message, &result));
  return result;
}

static node_api_value
createErrorCode(node_api_env env, node_api_callback_info info) {
  node_api_value result;
  node_api_value message;
  node_api_value code;
  NODE_API_CALL(env, node_api_create_string_utf8(
      env, "Error [error]", NODE_API_AUTO_LENGTH, &message));
  NODE_API_CALL(env, node_api_create_string_utf8(
      env, "ERR_TEST_CODE", NODE_API_AUTO_LENGTH, &code));
  NODE_API_CALL(env, node_api_create_error(env, code, message, &result));
  return result;
}

static node_api_value
createRangeErrorCode(node_api_env env, node_api_callback_info info) {
  node_api_value result;
  node_api_value message;
  node_api_value code;
  NODE_API_CALL(env, node_api_create_string_utf8(env,
                                                 "RangeError [range error]",
                                                 NODE_API_AUTO_LENGTH,
                                                 &message));
  NODE_API_CALL(env, node_api_create_string_utf8(
      env, "ERR_TEST_CODE", NODE_API_AUTO_LENGTH, &code));
  NODE_API_CALL(env, node_api_create_range_error(env, code, message, &result));
  return result;
}

static node_api_value
createTypeErrorCode(node_api_env env, node_api_callback_info info) {
  node_api_value result;
  node_api_value message;
  node_api_value code;
  NODE_API_CALL(env, node_api_create_string_utf8(env,
                                                 "TypeError [type error]",
                                                 NODE_API_AUTO_LENGTH,
                                                 &message));
  NODE_API_CALL(env, node_api_create_string_utf8(
      env, "ERR_TEST_CODE", NODE_API_AUTO_LENGTH, &code));
  NODE_API_CALL(env, node_api_create_type_error(env, code, message, &result));
  return result;
}

static node_api_value
throwArbitrary(node_api_env env, node_api_callback_info info) {
  node_api_value arbitrary;
  size_t argc = 1;
  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, &arbitrary, NULL, NULL));
  NODE_API_CALL(env, node_api_throw(env, arbitrary));
  return NULL;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("checkError", checkError),
    DECLARE_NODE_API_PROPERTY("throwExistingError", throwExistingError),
    DECLARE_NODE_API_PROPERTY("throwError", throwError),
    DECLARE_NODE_API_PROPERTY("throwRangeError", throwRangeError),
    DECLARE_NODE_API_PROPERTY("throwTypeError", throwTypeError),
    DECLARE_NODE_API_PROPERTY("throwErrorCode", throwErrorCode),
    DECLARE_NODE_API_PROPERTY("throwRangeErrorCode", throwRangeErrorCode),
    DECLARE_NODE_API_PROPERTY("throwTypeErrorCode", throwTypeErrorCode),
    DECLARE_NODE_API_PROPERTY("throwArbitrary", throwArbitrary),
    DECLARE_NODE_API_PROPERTY("createError", createError),
    DECLARE_NODE_API_PROPERTY("createRangeError", createRangeError),
    DECLARE_NODE_API_PROPERTY("createTypeError", createTypeError),
    DECLARE_NODE_API_PROPERTY("createErrorCode", createErrorCode),
    DECLARE_NODE_API_PROPERTY("createRangeErrorCode", createRangeErrorCode),
    DECLARE_NODE_API_PROPERTY("createTypeErrorCode", createTypeErrorCode),
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
