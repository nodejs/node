#include <stdio.h>
#include <stdlib.h>
#include <node_api.h>
#include <assert.h>
#include "../../js-native-api/common.h"

uint32_t free_call_count = 0;

napi_value GetFreeCallCount(napi_env env, napi_callback_info info) {
  napi_value value;
  NODE_API_CALL(env, napi_create_uint32(env, free_call_count, &value));
  return value;
}

static void finalize_cb(napi_env env, void* finalize_data, void* hint) {
  free(finalize_data);
  free_call_count++;
}

NAPI_MODULE_INIT() {
  napi_property_descriptor properties[] = {
    DECLARE_NODE_API_PROPERTY("getFreeCallCount", GetFreeCallCount)
  };

  NODE_API_CALL(env, napi_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));

  // This is a slight variation on the non-N-API test: We create an ArrayBuffer
  // rather than a Node.js Buffer, since testing the latter would only test
  // the same code paths and not the ones specific to N-API.
  napi_value buffer;

  char* data = malloc(sizeof(char));

  NODE_API_CALL(env, napi_create_external_arraybuffer(
      env,
      data,
      sizeof(char),
      finalize_cb,
      NULL,
      &buffer));

  NODE_API_CALL(env, napi_set_named_property(env, exports, "buffer", buffer));

  return exports;
}
