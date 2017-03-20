#include "myobject.h"

void CreateObject(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value args[1];
  status = napi_get_cb_args(env, info, args, 1);
  if (status != napi_ok) return;

  napi_value instance;
  status = MyObject::NewInstance(env, args[0], &instance);

  status = napi_set_return_value(env, info, instance);
  if (status != napi_ok) return;
}

void Add(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value args[2];
  status = napi_get_cb_args(env, info, args, 2);
  if (status != napi_ok) return;

  MyObject* obj1;
  status = napi_unwrap(env, args[0], reinterpret_cast<void**>(&obj1));
  if (status != napi_ok) return;

  MyObject* obj2;
  status = napi_unwrap(env, args[1], reinterpret_cast<void**>(&obj2));
  if (status != napi_ok) return;

  napi_value sum;
  status = napi_create_number(env, obj1->Val() + obj2->Val(), &sum);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, sum);
  if (status != napi_ok) return;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  MyObject::Init(env);

  napi_property_descriptor desc[] = {
      DECLARE_NAPI_METHOD("createObject", CreateObject),
      DECLARE_NAPI_METHOD("add", Add),
  };
  status =
      napi_define_properties(env, exports, sizeof(desc) / sizeof(*desc), desc);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
