#include <node_api.h>
#include "../common.h"

napi_value compare(napi_env env, napi_callback_info info) {
  napi_value external;
  size_t argc = 1;
  void* data;
  napi_value return_value;

  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &external, NULL, NULL));
  NAPI_CALL(env, napi_get_value_external(env, external, &data));
  NAPI_CALL(env, napi_get_boolean(env, ((napi_env)data) == env, &return_value));

  return return_value;
}

void Init(napi_env env, napi_value exports, napi_value module, void* context) {
  napi_property_descriptor prop = DECLARE_NAPI_PROPERTY("exports", compare);
  NAPI_CALL_RETURN_VOID(env, napi_define_properties(env, module, 1, &prop));
}

NAPI_MODULE(compare_env, Init)
