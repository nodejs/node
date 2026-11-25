#include <node_api.h>
#include "../../js-native-api/common.h"

// Throws after creating new exports. require() must throw the error instead
// of returning the new exports.
static napi_value Init(napi_env env, napi_value exports) {
  napi_value new_exports;
  napi_value value;
  NODE_API_CALL(env, napi_create_object(env, &new_exports));
  NODE_API_CALL(
      env, napi_create_string_utf8(env, "value", NAPI_AUTO_LENGTH, &value));
  NODE_API_CALL(env, napi_set_named_property(env, new_exports, "key", value));
  NODE_API_CALL(env, napi_throw_error(env, NULL, "Error during Init"));
  return new_exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
