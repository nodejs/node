#include <js_native_api.h>
#include "../common.h"

static node_api_value New(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  node_api_value description = NULL;
  if (argc >= 1) {
    node_api_valuetype valuetype;
    NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype));

    NODE_API_ASSERT(env, valuetype == node_api_string,
        "Wrong type of arguments. Expects a string.");

    description = args[0];
  }

  node_api_value symbol;
  NODE_API_CALL(env, node_api_create_symbol(env, description, &symbol));

  return symbol;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor properties[] = {
    DECLARE_NODE_API_PROPERTY("New", New),
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}
EXTERN_C_END
