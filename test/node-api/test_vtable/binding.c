#include <node_api.h>
#include <string.h>
#include "../../js-native-api/common.h"

// Showcase for the Node-API module v-table.
//
// This is an ordinary Node-API addon. The only thing that makes it use the
// v-table is the `NODE_API_MODULE_USE_VTABLE` define in binding.gyp: with it,
// every `napi_*` call below is a thin wrapper that dispatches through the
// `module_vtable`/`js_vtable` the runtime injected into `napi_env`, instead of
// binding to runtime-exported symbols. The source itself is unchanged from a
// classic addon, which is the whole point of the feature.
//
// The functions exercised here (napi_create_buffer, napi_create_buffer_copy,
// napi_is_buffer, napi_get_buffer_info) live in the module v-table.

static const char kData[] = "vtable-buffer";

// Returns a fresh Buffer initialized with kData.
static napi_value CreateBuffer(napi_env env, napi_callback_info info) {
  void* data = NULL;
  napi_value result;
  NODE_API_CALL(env, napi_create_buffer(env, sizeof(kData), &data, &result));
  memcpy(data, kData, sizeof(kData));
  return result;
}

// Returns a Buffer copied from kData.
static napi_value CopyBuffer(napi_env env, napi_callback_info info) {
  void* data = NULL;
  napi_value result;
  NODE_API_CALL(
      env, napi_create_buffer_copy(env, sizeof(kData), kData, &data, &result));
  return result;
}

// Returns true when the argument is a Buffer whose contents match kData.
static napi_value CheckBuffer(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));

  bool is_buffer = false;
  NODE_API_CALL(env, napi_is_buffer(env, argv[0], &is_buffer));

  bool matches = false;
  if (is_buffer) {
    void* data = NULL;
    size_t length = 0;
    NODE_API_CALL(env, napi_get_buffer_info(env, argv[0], &data, &length));
    matches = length == sizeof(kData) && memcmp(data, kData, length) == 0;
  }

  napi_value result;
  NODE_API_CALL(env, napi_get_boolean(env, is_buffer && matches, &result));
  return result;
}

NAPI_MODULE_INIT() {
  napi_property_descriptor props[] = {
      DECLARE_NODE_API_PROPERTY("createBuffer", CreateBuffer),
      DECLARE_NODE_API_PROPERTY("copyBuffer", CopyBuffer),
      DECLARE_NODE_API_PROPERTY("checkBuffer", CheckBuffer),
  };
  NODE_API_CALL(env,
                napi_define_properties(
                    env, exports, sizeof(props) / sizeof(*props), props));
  return exports;
}
