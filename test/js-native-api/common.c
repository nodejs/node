#include <js_native_api.h>
#include "common.h"

#include <stdio.h>

void add_returned_status(node_api_env env,
                         const char* key,
                         node_api_value object,
                         char* expected_message,
                         node_api_status expected_status,
                         node_api_status actual_status) {

  char node_api_message_string[100] = "";
  node_api_value prop_value;

  if (actual_status != expected_status) {
    snprintf(node_api_message_string,
             sizeof(node_api_message_string),
             "Invalid status [%d]", actual_status);
  }

  NODE_API_CALL_RETURN_VOID(env,
                        node_api_create_string_utf8(
                            env,
                            (actual_status == expected_status ?
                                expected_message :
                                node_api_message_string),
                            NODE_API_AUTO_LENGTH,
                            &prop_value));
  NODE_API_CALL_RETURN_VOID(env,
                            node_api_set_named_property(env,
                                                        object,
                                                        key,
                                                        prop_value));
}

void add_last_status(node_api_env env, const char* key, node_api_value return_value) {
    node_api_value prop_value;
    const node_api_extended_error_info* p_last_error;
    NODE_API_CALL_RETURN_VOID(env, node_api_get_last_error_info(env, &p_last_error));

    NODE_API_CALL_RETURN_VOID(env,
        node_api_create_string_utf8(env,
                                   (p_last_error->error_message == NULL ?
                                       "node_api_ok" :
                                       p_last_error->error_message),
                                   NODE_API_AUTO_LENGTH,
                                   &prop_value));
    NODE_API_CALL_RETURN_VOID(env, node_api_set_named_property(env,
                                                               return_value,
                                                               key,
                                                               prop_value));
}
