#ifndef JS_NATIVE_API_COMMON_INL_H_
#define JS_NATIVE_API_COMMON_INL_H_

#include <js_native_api.h>
#include "common.h"

#include <stdio.h>

inline void add_returned_status(napi_env env,
                                const char* key,
                                napi_value object,
                                char* expected_message,
                                napi_status expected_status,
                                napi_status actual_status) {
  char napi_message_string[100] = "";
  napi_value prop_value;

  if (actual_status != expected_status) {
    snprintf(napi_message_string,
             sizeof(napi_message_string),
             "Invalid status [%d]",
             actual_status);
  }

  NODE_API_CALL_RETURN_VOID(
      env,
      napi_create_string_utf8(
          env,
          (actual_status == expected_status ? expected_message
                                            : napi_message_string),
          NAPI_AUTO_LENGTH,
          &prop_value));
  NODE_API_CALL_RETURN_VOID(
      env, napi_set_named_property(env, object, key, prop_value));
}

inline void add_last_status(napi_env env,
                            const char* key,
                            napi_value return_value) {
  napi_value prop_value;
  napi_value exception;
  const napi_extended_error_info* p_last_error;
  NODE_API_CALL_RETURN_VOID(env, napi_get_last_error_info(env, &p_last_error));
  // Content of p_last_error can be updated in subsequent node-api calls.
  // Retrieve it immediately.
  const char* error_message = p_last_error->error_message == NULL
                                  ? "napi_ok"
                                  : p_last_error->error_message;

  bool is_exception_pending;
  NODE_API_CALL_RETURN_VOID(
      env, napi_is_exception_pending(env, &is_exception_pending));
  if (is_exception_pending) {
    NODE_API_CALL_RETURN_VOID(
        env, napi_get_and_clear_last_exception(env, &exception));
    char exception_key[50];
    snprintf(exception_key, sizeof(exception_key), "%s%s", key, "Exception");
    NODE_API_CALL_RETURN_VOID(
        env,
        napi_set_named_property(env, return_value, exception_key, exception));
  }

  NODE_API_CALL_RETURN_VOID(
      env,
      napi_create_string_utf8(
          env, error_message, NAPI_AUTO_LENGTH, &prop_value));
  NODE_API_CALL_RETURN_VOID(
      env, napi_set_named_property(env, return_value, key, prop_value));
}

#endif  // JS_NATIVE_API_COMMON_INL_H_
