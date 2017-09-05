#include <node_api.h>
#include "../common.h"

napi_value MyFunction(napi_env env, napi_callback_info info) {
  napi_value str;
  NAPI_CALL(env, napi_create_string_utf8(env, "hello world", -1, &str));

  return str;
}

napi_value CreateFunction(napi_env env, napi_callback_info info) {
  napi_value fn;
  NAPI_CALL(env,
    napi_create_function(env, "theFunction", MyFunction, NULL, &fn));

  return fn;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor desc =
    DECLARE_NAPI_PROPERTY("exports", CreateFunction);
  NAPI_CALL_RETURN_VOID(env, napi_define_properties(env, module, 1, &desc));
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
