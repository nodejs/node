#include <stdio.h>
#include <node_api.h>
#include <assert.h>
#include <stdlib.h>
#include "../../js-native-api/common.h"

#define BUFFER_SIZE 4

int wrappedNativeData;
node_api_ref ref;
void WrapFinalizer(node_api_env env, void* data, void* hint) {
  uint32_t count;
  NODE_API_CALL_RETURN_VOID(env, node_api_reference_unref(env, ref, &count));
  NODE_API_CALL_RETURN_VOID(env, node_api_delete_reference(env, ref));
}

void BufferFinalizer(node_api_env env, void* data, void* hint) {
  free(hint);
}

node_api_value Test(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value argv[1];
  node_api_value result;
  void* bufferData = malloc(BUFFER_SIZE);

  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, argv, NULL, NULL));
  NODE_API_CALL(env,
      node_api_create_external_buffer(env, BUFFER_SIZE, bufferData,
          BufferFinalizer, bufferData, &result));
  NODE_API_CALL(env, node_api_create_reference(env, result, 1, &ref));
  NODE_API_CALL(env,
      node_api_wrap(env, argv[0], (void*) &wrappedNativeData, WrapFinalizer,
          NULL, NULL));
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
