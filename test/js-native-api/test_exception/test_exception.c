#include <js_native_api.h>
#include "../common.h"
#include "../entry_point.h"

static bool exceptionWasPending = false;
static int num = 0x23432;

static napi_value returnException(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value global;
  NODE_API_CALL(env, napi_get_global(env, &global));

  napi_value result;
  napi_status status = napi_call_function(env, global, args[0], 0, 0, &result);
  if (status == napi_pending_exception) {
    napi_value ex;
    NODE_API_CALL(env, napi_get_and_clear_last_exception(env, &ex));
    return ex;
  }

  return NULL;
}

static napi_value constructReturnException(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value result;
  napi_status status = napi_new_instance(env, args[0], 0, 0, &result);
  if (status == napi_pending_exception) {
    napi_value ex;
    NODE_API_CALL(env, napi_get_and_clear_last_exception(env, &ex));
    return ex;
  }

  return NULL;
}

static napi_value allowException(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value global;
  NODE_API_CALL(env, napi_get_global(env, &global));

  napi_value result;
  napi_call_function(env, global, args[0], 0, 0, &result);
  // Ignore status and check napi_is_exception_pending() instead.

  NODE_API_CALL(env, napi_is_exception_pending(env, &exceptionWasPending));
  return NULL;
}

static napi_value constructAllowException(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value result;
  napi_new_instance(env, args[0], 0, 0, &result);
  // Ignore status and check napi_is_exception_pending() instead.

  NODE_API_CALL(env, napi_is_exception_pending(env, &exceptionWasPending));
  return NULL;
}

static napi_value wasPending(napi_env env, napi_callback_info info) {
  napi_value result;
  NODE_API_CALL(env, napi_get_boolean(env, exceptionWasPending, &result));

  return result;
}

static void finalizer(napi_env env, void *data, void *hint) {
  NODE_API_CALL_RETURN_VOID(env,
      napi_throw_error(env, NULL, "Error during Finalize"));
}

static napi_value createExternal(napi_env env, napi_callback_info info) {
  napi_value external;

  NODE_API_CALL(env,
      napi_create_external(env, &num, finalizer, NULL, &external));

  return external;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("returnException", returnException),
    DECLARE_NODE_API_PROPERTY("allowException", allowException),
    DECLARE_NODE_API_PROPERTY("constructReturnException", constructReturnException),
    DECLARE_NODE_API_PROPERTY("constructAllowException", constructAllowException),
    DECLARE_NODE_API_PROPERTY("wasPending", wasPending),
    DECLARE_NODE_API_PROPERTY("createExternal", createExternal),
  };
  NODE_API_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  napi_value error, code, message;
  NODE_API_CALL(env, napi_create_string_utf8(env, "Error during Init",
      NAPI_AUTO_LENGTH, &message));
  NODE_API_CALL(env, napi_create_string_utf8(env, "", NAPI_AUTO_LENGTH, &code));
  NODE_API_CALL(env, napi_create_error(env, code, message, &error));
  NODE_API_CALL(env, napi_set_named_property(env, error, "binding", exports));
  NODE_API_CALL(env, napi_throw(env, error));

  return exports;
}
EXTERN_C_END
