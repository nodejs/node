#include <node_api.h>
#include <stdio.h>

void doInstanceOf(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value arguments[2];

  status = napi_get_cb_args(env, info, arguments, 2);
  if (status != napi_ok) return;

  bool instanceof;
  status = napi_instanceof(env, arguments[0], arguments[1], &instanceof);
  if (status != napi_ok) return;

  napi_value result;
  status = napi_get_boolean(env, instanceof, &result);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, result);
  if (status != napi_ok) return;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  napi_property_descriptor descriptors[] = {
      DECLARE_NAPI_METHOD("doInstanceOf", doInstanceOf),
  };

  status = napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
