#include <node_api.h>
#include "../../js-native-api/common.h"

static void finalizer(node_api_env env, void *data, void *hint) {
  NODE_API_CALL_RETURN_VOID(env,
      node_api_throw_error(env, NULL, "Error during Finalize"));
}

static char buffer_data[12];

static node_api_value
createExternalBuffer(node_api_env env, node_api_callback_info info) {
  node_api_value buffer;
  NODE_API_CALL(env, node_api_create_external_buffer(env, sizeof(buffer_data),
      buffer_data, finalizer, NULL, &buffer));
  return buffer;
}

static node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("createExternalBuffer", createExternalBuffer),
  };
  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));
  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
