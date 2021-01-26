#include <js_native_api.h>
#include "../common.h"

static node_api_value Add(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 2, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  node_api_valuetype valuetype1;
  NODE_API_CALL(env, node_api_typeof(env, args[1], &valuetype1));

  NODE_API_ASSERT(env, 
      valuetype0 == node_api_number && valuetype1 == node_api_number,
      "Wrong argument type. Numbers expected.");

  double value0;
  NODE_API_CALL(env, node_api_get_value_double(env, args[0], &value0));

  double value1;
  NODE_API_CALL(env, node_api_get_value_double(env, args[1], &value1));

  node_api_value sum;
  NODE_API_CALL(env, node_api_create_double(env, value0 + value1, &sum));

  return sum;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor desc = DECLARE_NODE_API_PROPERTY("add", Add);
  NODE_API_CALL(env, node_api_define_properties(env, exports, 1, &desc));
  return exports;
}
EXTERN_C_END
