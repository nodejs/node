#include <node_api.h>
#include "../common.h"

napi_value testStrictEquals(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  bool bool_result;
  napi_value result;
  NAPI_CALL(env, napi_strict_equals(env, args[0], args[1], &bool_result));
  NAPI_CALL(env, napi_get_boolean(env, bool_result, &result));

  return result;
}

napi_value testGetPrototype(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value result;
  NAPI_CALL(env, napi_get_prototype(env, args[0], &result));

  return result;
}

napi_value testGetVersion(napi_env env, napi_callback_info info) {
  uint32_t version;
  napi_value result;
  NAPI_CALL(env, napi_get_version(env, &version));
  NAPI_CALL(env ,napi_create_number(env, version, &result));
  return result;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("testStrictEquals", testStrictEquals),
    DECLARE_NAPI_PROPERTY("testGetPrototype", testGetPrototype),
    DECLARE_NAPI_PROPERTY("testGetVersion", testGetVersion),
  };

  NAPI_CALL_RETURN_VOID(env, napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));
}

NAPI_MODULE(addon, Init)
