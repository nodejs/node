#include <js_native_api.h>
#include "common.h"

#include <stdio.h>

void add_returned_status(napi_env env,
                         const char* key,
                         napi_value object,
                         napi_status expected_status,
                         napi_status actual_status) {

  char p_napi_message[100] = "";
  napi_value prop_value;

  if (actual_status == expected_status) {
    snprintf(p_napi_message, 99, "Invalid argument");
  } else {
    snprintf(p_napi_message, 99, "Invalid status [%d]", actual_status);
  }

  NAPI_CALL_RETURN_VOID(env,
                        napi_create_string_utf8(env,
                                                p_napi_message,
                                                NAPI_AUTO_LENGTH,
                                                &prop_value));
  NAPI_CALL_RETURN_VOID(env,
                        napi_set_named_property(env,
                                                object,
                                                key,
                                                prop_value));
}

void add_last_status(napi_env env, const char* key, napi_value return_value) {
    napi_value prop_value;
    const napi_extended_error_info* p_last_error;
    NAPI_CALL_RETURN_VOID(env, napi_get_last_error_info(env, &p_last_error));

    NAPI_CALL_RETURN_VOID(env,
        napi_create_string_utf8(env,
                                (p_last_error->error_message == NULL ?
                                    "napi_ok" :
                                    p_last_error->error_message),
                                NAPI_AUTO_LENGTH,
                                &prop_value));
    NAPI_CALL_RETURN_VOID(env, napi_set_named_property(env,
                                                       return_value,
                                                       key,
                                                       prop_value));
}
