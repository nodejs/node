#include <js_native_api.h>
#include "../common.h"

struct MyObject {
  int32_t val;
};

static napi_value Create(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];

  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));
  NAPI_ASSERT(env, argc >= 1, "Wrong number of arguments");
  napi_valuetype valuetype;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype));
  NAPI_ASSERT(
      env, valuetype == napi_number, "Wrong argument type. Number expected");

  MyObject* external = new MyObject();
  NAPI_CALL(env, napi_get_value_int32(env, args[0], &(external->val)));
  napi_value val;
  NAPI_CALL(env, napi_create_external(env, external, nullptr, nullptr, &val));
  return val;
}

static napi_value ToInt32(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];

  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));
  NAPI_ASSERT(env, argc >= 1, "Wrong number of arguments");
  napi_valuetype valuetype;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype));
  NAPI_ASSERT(env,
              valuetype == napi_external,
              "Wrong argument type. External expected");
  MyObject* external;
  napi_value val;
  NAPI_CALL(env,
            napi_get_value_external(
                env, args[0], reinterpret_cast<void**>(&external)));
  NAPI_CALL(env, napi_create_int32(env, external->val, &val));
  return val;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
      DECLARE_NAPI_PROPERTY("createExternal", Create),
      DECLARE_NAPI_PROPERTY("getVal", ToInt32),
  };
  NAPI_CALL(env, napi_define_properties(env, exports, 2, desc));
  return exports;
}
EXTERN_C_END
