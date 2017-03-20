#include <node_api.h>

void MyFunction(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value str;
  status = napi_create_string_utf8(env, "hello world", -1, &str);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, str);
  if (status != napi_ok) return;
}

void CreateFunction(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value fn;
  status = napi_create_function(env, "theFunction", MyFunction, NULL, &fn);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, fn);
  if (status != napi_ok) return;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;
  napi_property_descriptor desc =
      DECLARE_NAPI_METHOD("exports", CreateFunction);
  status = napi_define_properties(env, module, 1, &desc);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
