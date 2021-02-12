#define NAPI_EXPERIMENTAL
#include <node_api.h>
#include <stdlib.h>
#include "../../js-native-api/common.h"

static napi_value testGetNodeVersion(napi_env env, napi_callback_info info) {
  const napi_node_version* node_version;
  napi_value result, major, minor, patch, release;
  NAPI_CALL(env, napi_get_node_version(env, &node_version));
  NAPI_CALL(env, napi_create_uint32(env, node_version->major, &major));
  NAPI_CALL(env, napi_create_uint32(env, node_version->minor, &minor));
  NAPI_CALL(env, napi_create_uint32(env, node_version->patch, &patch));
  NAPI_CALL(env, napi_create_string_utf8(env,
                                         node_version->release,
                                         NAPI_AUTO_LENGTH,
                                         &release));
  NAPI_CALL(env, napi_create_array_with_length(env, 4, &result));
  NAPI_CALL(env, napi_set_element(env, result, 0, major));
  NAPI_CALL(env, napi_set_element(env, result, 1, minor));
  NAPI_CALL(env, napi_set_element(env, result, 2, patch));
  NAPI_CALL(env, napi_set_element(env, result, 3, release));
  return result;
}

static napi_value GetFilename(napi_env env, napi_callback_info info) {
  const char* filename;
  napi_value result;

  NAPI_CALL(env, node_api_get_module_file_name(env, &filename));
  NAPI_CALL(env,
      napi_create_string_utf8(env, filename, NAPI_AUTO_LENGTH, &result));

  return result;
}

static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("testGetNodeVersion", testGetNodeVersion),
    DECLARE_NAPI_GETTER("filename", GetFilename),
  };

  NAPI_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
