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

napi_value Add(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  MyObject* obj1;
  NAPI_CALL(env, napi_unwrap(env, args[0], reinterpret_cast<void**>(&obj1)));

  MyObject* obj2;
  NAPI_CALL(env, napi_unwrap(env, args[1], reinterpret_cast<void**>(&obj2)));

  napi_value sum;
  NAPI_CALL(env, napi_create_double(env, obj1->Val() + obj2->Val(), &sum));

  return sum;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  MyObject::Init(env);

  napi_property_descriptor desc[] = {
    DECLARE_NAPI_PROPERTY("createObject", CreateObject),
    DECLARE_NAPI_PROPERTY("add", Add),
  };

  NAPI_CALL_RETURN_VOID(env,
    napi_define_properties(env, exports, sizeof(desc) / sizeof(*desc), desc));
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
