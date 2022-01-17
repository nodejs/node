#include <node_api.h>
#include "../../js-native-api/common.h"

static void finalizer(napi_env env, void *data, void *hint) {
  NODE_API_CALL_RETURN_VOID(env,
      napi_throw_error(env, NULL, "Error during Finalize"));
}

static char buffer_data[12];

static napi_value createExternalBuffer(napi_env env, napi_callback_info info) {
  napi_value buffer;
  NODE_API_CALL(env, napi_create_external_buffer(env, sizeof(buffer_data),
      buffer_data, finalizer, NULL, &buffer));
  return buffer;
}

static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("createExternalBuffer", createExternalBuffer),
  };
  NODE_API_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
