#include <js_native_api.h>
#include "../common.h"
#include <string.h>

static node_api_value
RunCallback(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 1,
      "Wrong number of arguments. Expects a single argument.");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));
  NODE_API_ASSERT(env, valuetype0 == node_api_function,
      "Wrong type of arguments. Expects a function as first argument.");

  node_api_valuetype valuetype1;
  NODE_API_CALL(env, node_api_typeof(env, args[1], &valuetype1));
  NODE_API_ASSERT(env, valuetype1 == node_api_undefined,
      "Additional arguments should be undefined.");

  node_api_value argv[1];
  const char* str = "hello world";
  size_t str_len = strlen(str);
  NODE_API_CALL(env, node_api_create_string_utf8(env, str, str_len, argv));

  node_api_value global;
  NODE_API_CALL(env, node_api_get_global(env, &global));

  node_api_value cb = args[0];
  NODE_API_CALL(env, node_api_call_function(env, global, cb, 1, argv, NULL));

  return NULL;
}

static node_api_value
RunCallbackWithRecv(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  node_api_value cb = args[0];
  node_api_value recv = args[1];
  NODE_API_CALL(env, node_api_call_function(env, recv, cb, 0, NULL, NULL));
  return NULL;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor desc[2] = {
    DECLARE_NODE_API_PROPERTY("RunCallback", RunCallback),
    DECLARE_NODE_API_PROPERTY("RunCallbackWithRecv", RunCallbackWithRecv),
  };
  NODE_API_CALL(env, node_api_define_properties(env, exports, 2, desc));
  return exports;
}
EXTERN_C_END
