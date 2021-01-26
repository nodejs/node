#include <stdio.h>
#include <node_api.h>
#include <assert.h>
#include "../../js-native-api/common.h"

node_api_value Test(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value recv;
  node_api_value argv[1];
  node_api_status status;

  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, argv, &recv, NULL));
  NODE_API_ASSERT(env, argc >= 1, "Not enough arguments, expected 1.");

  node_api_valuetype t;
  NODE_API_CALL(env, node_api_typeof(env, argv[0], &t));
  NODE_API_ASSERT(env, t == node_api_function,
      "Wrong first argument, function expected.");

  status = node_api_call_function(env, recv, argv[0], 0, NULL, NULL);
  assert(status == node_api_ok);
  status = node_api_call_function(env, recv, argv[0], 0, NULL, NULL);
  assert(status == node_api_pending_exception);

  return NULL;
}

node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor properties[] = {
    DECLARE_NODE_API_PROPERTY("Test", Test)
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}

// Do not start using NODE_API_MODULE_INIT() here, so that we can test
// compatibility of Workers with NODE_API_MODULE().
NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
