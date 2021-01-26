#include <node_api.h>
#include "../../js-native-api/common.h"

static node_api_value Test(node_api_env env, node_api_callback_info info) {
  node_api_fatal_error("test_fatal::Test", NODE_API_AUTO_LENGTH,
                   "fatal message", NODE_API_AUTO_LENGTH);
  return NULL;
}

static node_api_value
TestStringLength(node_api_env env, node_api_callback_info info) {
  node_api_fatal_error(
      "test_fatal::TestStringLength", 16, "fatal message", 13);
  return NULL;
}

static node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor properties[] = {
    DECLARE_NODE_API_PROPERTY("Test", Test),
    DECLARE_NODE_API_PROPERTY("TestStringLength", TestStringLength),
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
