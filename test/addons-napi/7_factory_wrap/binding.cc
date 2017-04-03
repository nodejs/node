#include "myobject.h"

void CreateObject(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value args[1];
  status = napi_get_cb_args(env, info, args, 1);
  if (status != napi_ok) return;

  napi_value instance;
  status = MyObject::NewInstance(env, args[0], &instance);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, instance);
  if (status != napi_ok) return;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  status = MyObject::Init(env);
  if (status != napi_ok) return;

  napi_property_descriptor desc = DECLARE_NAPI_METHOD("exports", CreateObject);
  status = napi_define_properties(env, module, 1, &desc);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
