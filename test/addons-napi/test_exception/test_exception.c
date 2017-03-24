#include <node_api.h>
#include "../common.h"

static bool exceptionWasPending = false;

napi_value returnException(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value global;
  NAPI_CALL(env, napi_get_global(env, &global));

  napi_value result;
  napi_status status = napi_call_function(env, global, args[0], 0, 0, &result);
  if (status == napi_pending_exception) {
    napi_value ex;
    NAPI_CALL(env, napi_get_and_clear_last_exception(env, &ex));
    return ex;
  }

  return NULL;
}

napi_value allowException(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value global;
  NAPI_CALL(env, napi_get_global(env, &global));

  napi_value result;
  napi_call_function(env, global, args[0], 0, 0, &result);
  // Ignore status and check napi_is_exception_pending() instead.

  NAPI_CALL(env, napi_is_exception_pending(env, &exceptionWasPending));
  return NULL;
}

napi_value wasPending(napi_env env, napi_callback_info info) {
  napi_value result;
  NAPI_CALL(env, napi_get_boolean(env, exceptionWasPending, &result));

  return result;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("returnException", returnException),
    DECLARE_NAPI_PROPERTY("allowException", allowException),
    DECLARE_NAPI_PROPERTY("wasPending", wasPending),
  };

  NAPI_CALL_RETURN_VOID(env, napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));
}

NAPI_MODULE(addon, Init)
