#include <node_api.h>
#include "../../js-native-api/common.h"
#include <string.h>

static node_api_value Method(node_api_env env, node_api_callback_info info) {
  node_api_value world;
  const char* str = "world";
  size_t str_len = strlen(str);
  NODE_API_CALL(env, node_api_create_string_utf8(env, str, str_len, &world));
  return world;
}

NODE_API_MODULE_INIT() {
  node_api_property_descriptor desc =
    DECLARE_NODE_API_PROPERTY("hello", Method);
  NODE_API_CALL(env, node_api_define_properties(env, exports, 1, &desc));
  return exports;
}
