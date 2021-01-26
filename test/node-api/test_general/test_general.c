#include <node_api.h>
#include <stdlib.h>
#include "../../js-native-api/common.h"

static node_api_value
testGetNodeVersion(node_api_env env, node_api_callback_info info) {
  const node_api_node_version* node_version;
  node_api_value result, major, minor, patch, release;
  NODE_API_CALL(env, node_api_get_node_version(env, &node_version));
  NODE_API_CALL(env, node_api_create_uint32(env, node_version->major, &major));
  NODE_API_CALL(env, node_api_create_uint32(env, node_version->minor, &minor));
  NODE_API_CALL(env, node_api_create_uint32(env, node_version->patch, &patch));
  NODE_API_CALL(env, node_api_create_string_utf8(env,
                                                 node_version->release,
                                                 NODE_API_AUTO_LENGTH,
                                                 &release));
  NODE_API_CALL(env, node_api_create_array_with_length(env, 4, &result));
  NODE_API_CALL(env, node_api_set_element(env, result, 0, major));
  NODE_API_CALL(env, node_api_set_element(env, result, 1, minor));
  NODE_API_CALL(env, node_api_set_element(env, result, 2, patch));
  NODE_API_CALL(env, node_api_set_element(env, result, 3, release));
  return result;
}

static node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("testGetNodeVersion", testGetNodeVersion),
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
