#include <node_api.h>
#include "../common.h"

napi_value TestCallFunction(napi_env env, napi_callback_info info) {
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

napi_value TestFunctionName(napi_env env, napi_callback_info info) {
  return NULL;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_value fn1;
  NAPI_CALL(env, napi_create_function(
      env, NULL, NAPI_AUTO_LENGTH, TestCallFunction, NULL, &fn1));

  napi_value fn2;
  NAPI_CALL(env, napi_create_function(
      env, "Name", NAPI_AUTO_LENGTH, TestFunctionName, NULL, &fn2));

  napi_value fn3;
  NAPI_CALL(env, napi_create_function(
      env, "Name_extra", 5, TestFunctionName, NULL, &fn3));

  NAPI_CALL(env, napi_set_named_property(env, exports, "TestCall", fn1));
  NAPI_CALL(env, napi_set_named_property(env, exports, "TestName", fn2));
  NAPI_CALL(env, napi_set_named_property(env, exports, "TestNameShort", fn3));

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
