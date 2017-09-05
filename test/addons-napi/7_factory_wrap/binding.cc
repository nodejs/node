#include "myobject.h"
#include "../common.h"

napi_value CreateObject(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  napi_value instance;
  NAPI_CALL(env, MyObject::NewInstance(env, args[0], &instance));

  return instance;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  NAPI_CALL_RETURN_VOID(env, MyObject::Init(env));

  napi_property_descriptor desc =
    DECLARE_NAPI_PROPERTY("exports", CreateObject);
  NAPI_CALL_RETURN_VOID(env, napi_define_properties(env, module, 1, &desc));
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
