#include <node_api.h>

void checkError(napi_env e, napi_callback_info info) {
  napi_status status;
  napi_value jsError;

  status = napi_get_cb_args(e, info, &jsError, 1);
  if (status != napi_ok) return;

  bool r;
  status = napi_is_error(e, jsError, &r);
  if (status != napi_ok) return;

  napi_value result;
  status = napi_get_boolean(e, r, &result);
  if (status != napi_ok) return;

  status = napi_set_return_value(e, info, result);
  if (status != napi_ok) return;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  napi_property_descriptor descriptors[] = {
      DECLARE_NAPI_METHOD("checkError", checkError),
  };

  status = napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
