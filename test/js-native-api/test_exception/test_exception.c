#include <js_native_api.h>
#include "../common.h"

static bool exceptionWasPending = false;

static node_api_value
returnException(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  node_api_value global;
  NODE_API_CALL(env, node_api_get_global(env, &global));

  node_api_value result;
  node_api_status status = node_api_call_function(
      env, global, args[0], 0, 0, &result);
  if (status == node_api_pending_exception) {
    node_api_value ex;
    NODE_API_CALL(env, node_api_get_and_clear_last_exception(env, &ex));
    return ex;
  }

  return NULL;
}

static node_api_value
allowException(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  node_api_value global;
  NODE_API_CALL(env, node_api_get_global(env, &global));

  node_api_value result;
  node_api_call_function(env, global, args[0], 0, 0, &result);
  // Ignore status and check node_api_is_exception_pending() instead.

  NODE_API_CALL(env, node_api_is_exception_pending(env, &exceptionWasPending));
  return NULL;
}

static node_api_value
wasPending(node_api_env env, node_api_callback_info info) {
  node_api_value result;
  NODE_API_CALL(env, node_api_get_boolean(env, exceptionWasPending, &result));

  return result;
}

static void finalizer(node_api_env env, void *data, void *hint) {
  NODE_API_CALL_RETURN_VOID(env,
      node_api_throw_error(env, NULL, "Error during Finalize"));
}

static node_api_value
createExternal(node_api_env env, node_api_callback_info info) {
  node_api_value external;

  NODE_API_CALL(env,
      node_api_create_external(env, NULL, finalizer, NULL, &external));

  return external;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("returnException", returnException),
    DECLARE_NODE_API_PROPERTY("allowException", allowException),
    DECLARE_NODE_API_PROPERTY("wasPending", wasPending),
    DECLARE_NODE_API_PROPERTY("createExternal", createExternal),
  };
  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  node_api_value error, code, message;
  NODE_API_CALL(env, node_api_create_string_utf8(env, "Error during Init",
      NODE_API_AUTO_LENGTH, &message));
  NODE_API_CALL(env, node_api_create_string_utf8(env, "", NODE_API_AUTO_LENGTH, &code));
  NODE_API_CALL(env, node_api_create_error(env, code, message, &error));
  NODE_API_CALL(env, node_api_set_named_property(env, error, "binding", exports));
  NODE_API_CALL(env, node_api_throw(env, error));

  return exports;
}
EXTERN_C_END
