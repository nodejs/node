#include <node_api.h>
#include "../common.h"

napi_value Test(napi_env env, napi_callback_info info) {
  size_t argc = 10;
  napi_value args[10];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc > 0, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NAPI_ASSERT(env, valuetype0 == napi_function,
    "Wrong type of arguments. Expects a function as first argument.");

  napi_value* argv = args + 1;
  argc = argc - 1;

  napi_value global;
  NAPI_CALL(env, napi_get_global(env, &global));

  napi_value result;
  NAPI_CALL(env, napi_call_function(env, global, args[0], argc, argv, &result));

  return result;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_value fn;
  NAPI_CALL_RETURN_VOID(env, napi_create_function(env, NULL, Test, NULL, &fn));
  NAPI_CALL_RETURN_VOID(env, napi_set_named_property(env, exports, "Test", fn));
}

NAPI_MODULE(addon, Init)
