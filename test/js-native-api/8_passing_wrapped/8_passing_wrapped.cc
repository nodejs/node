#include <js_native_api.h>
#include "myobject.h"
#include "../common.h"

extern size_t finalize_count;

static napi_value CreateObject(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  napi_value instance;
  NAPI_CALL(env, MyObject::NewInstance(env, args[0], &instance));

  return instance;
}

static napi_value Add(napi_env env, napi_callback_info info) {
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

static napi_value FinalizeCount(napi_env env, napi_callback_info info) {
  napi_value return_value;
  NAPI_CALL(env, napi_create_uint32(env, finalize_count, &return_value));
  return return_value;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  MyObject::Init(env);

  napi_property_descriptor desc[] = {
    DECLARE_NAPI_PROPERTY("createObject", CreateObject),
    DECLARE_NAPI_PROPERTY("add", Add),
    DECLARE_NAPI_PROPERTY("finalizeCount", FinalizeCount),
  };

  NAPI_CALL(env,
      napi_define_properties(env, exports, sizeof(desc) / sizeof(*desc), desc));

  return exports;
}
EXTERN_C_END
