#include <node_api.h>
#include "../../js-native-api/common.h"

static node_api_value Test(node_api_env env, node_api_callback_info info) {
  node_api_value err;
  size_t argc = 1;

  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, &err, NULL, NULL));

  NODE_API_CALL(env, node_api_fatal_exception(env, err));

  return NULL;
}

static node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor properties[] = {
    DECLARE_NODE_API_PROPERTY("Test", Test),
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
