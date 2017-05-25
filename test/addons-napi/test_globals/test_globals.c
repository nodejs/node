#include <node_api.h>
#include "../common.h"

napi_value getNull(napi_env env, napi_callback_info info) {
  napi_value result;
  NAPI_CALL(env, napi_get_null(env, &result));
  return result;
}

napi_value getUndefined(napi_env env, napi_callback_info info) {
  napi_value result;
  NAPI_CALL(env, napi_get_undefined(env, &result));
  return result;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("getUndefined", getUndefined),
    DECLARE_NAPI_PROPERTY("getNull", getNull),
  };

  NAPI_CALL_RETURN_VOID(env, napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));
}

NAPI_MODULE(addon, Init)
