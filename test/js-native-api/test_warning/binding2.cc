#include <node_api.h>

static napi_value Method(const napi_env env, const napi_callback_info info) {
  napi_value result;
  napi_create_string_utf8(env, "world", NAPI_AUTO_LENGTH, &result);
  return result;
}

static napi_value InitModule(napi_env env, napi_value exports) {
  napi_value hello;
  napi_create_function(env, "hello", NAPI_AUTO_LENGTH, Method, nullptr, &hello);
  napi_set_named_property(env, exports, "hello", hello);
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, InitModule)
