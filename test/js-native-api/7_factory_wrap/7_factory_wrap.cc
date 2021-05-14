#include <js_native_api.h>
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

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  NAPI_CALL(env, MyObject::Init(env));

  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_GETTER("finalizeCount", MyObject::GetFinalizeCount),
    DECLARE_NAPI_PROPERTY("createObject", CreateObject),
  };

  NAPI_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
