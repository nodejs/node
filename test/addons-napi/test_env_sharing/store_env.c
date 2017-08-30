#include <node_api.h>
#include "../common.h"

napi_value Init(napi_env env, napi_value exports) {
  napi_value external;
  NAPI_CALL(env, napi_create_external(env, env, NULL, NULL, &external));
  return external;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
