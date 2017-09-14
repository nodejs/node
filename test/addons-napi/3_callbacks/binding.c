#include <node_api.h>
#include "../common.h"
#include <string.h>

napi_value RunCallback(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc == 1,
      "Wrong number of arguments. Expects a single argument.");

  napi_valuetype valuetype0;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));
  NAPI_ASSERT(env, valuetype0 == napi_function,
      "Wrong type of arguments. Expects a function as first argument.");

  napi_valuetype valuetype1;
  NAPI_CALL(env, napi_typeof(env, args[1], &valuetype1));
  NAPI_ASSERT(env, valuetype1 == napi_undefined,
      "Additional arguments should be undefined.");

  napi_value argv[1];
  const char* str = "hello world";
  size_t str_len = strlen(str);
  NAPI_CALL(env, napi_create_string_utf8(env, str, str_len, argv));

  napi_value global;
  NAPI_CALL(env, napi_get_global(env, &global));

  napi_value cb = args[0];
  NAPI_CALL(env, napi_call_function(env, global, cb, 1, argv, NULL));

  return NULL;
}

napi_value RunCallbackWithRecv(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value cb = args[0];
  napi_value recv = args[1];
  NAPI_CALL(env, napi_call_function(env, recv, cb, 0, NULL, NULL));
  return NULL;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor desc[2] = {
    DECLARE_NAPI_PROPERTY("RunCallback", RunCallback),
    DECLARE_NAPI_PROPERTY("RunCallbackWithRecv", RunCallbackWithRecv),
  };
  NAPI_CALL(env, napi_define_properties(env, exports, 2, desc));
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
