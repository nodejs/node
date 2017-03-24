#include <node_api.h>
#include "../common.h"

napi_value checkError(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  bool r;
  NAPI_CALL(env, napi_is_error(env, args[0], &r));

  napi_value result;
  NAPI_CALL(env, napi_get_boolean(env, r, &result));

  return result;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("checkError", checkError),
  };

  NAPI_CALL_RETURN_VOID(env, napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));
}

NAPI_MODULE(addon, Init)
