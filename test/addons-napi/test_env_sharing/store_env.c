#include <node_api.h>
#include "../common.h"

void Init(napi_env env, napi_value exports, napi_value module, void* context) {
  napi_value external;
  NAPI_CALL_RETURN_VOID(env,
    napi_create_external(env, env, NULL, NULL, &external));
  NAPI_CALL_RETURN_VOID(env,
    napi_set_named_property(env, module, "exports", external));
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
