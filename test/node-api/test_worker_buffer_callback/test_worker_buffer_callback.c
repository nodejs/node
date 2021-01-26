#include <stdio.h>
#include <stdlib.h>
#include <node_api.h>
#include <assert.h>
#include "../../js-native-api/common.h"

uint32_t free_call_count = 0;

node_api_value
GetFreeCallCount(node_api_env env, node_api_callback_info info) {
  node_api_value value;
  NODE_API_CALL(env, node_api_create_uint32(env, free_call_count, &value));
  return value;
}

static void finalize_cb(node_api_env env, void* finalize_data, void* hint) {
  free(finalize_data);
  free_call_count++;
}

NODE_API_MODULE_INIT() {
  node_api_property_descriptor properties[] = {
    DECLARE_NODE_API_PROPERTY("getFreeCallCount", GetFreeCallCount)
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));

  // This is a slight variation on the non-Node.js-API test: We create an ArrayBuffer
  // rather than a Node.js Buffer, since testing the latter would only test
  // the same code paths and not the ones specific to the Node.js API.
  node_api_value buffer;

  char* data = malloc(sizeof(char));

  NODE_API_CALL(env, node_api_create_external_arraybuffer(
      env,
      data,
      sizeof(char),
      finalize_cb,
      NULL,
      &buffer));

  NODE_API_CALL(env,
      node_api_set_named_property(env, exports, "buffer", buffer));

  return exports;
}
