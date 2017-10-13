#include <node_api.h>
#include "../common.h"

napi_value Init(napi_env env, napi_value exports) {
  napi_value result;
  NAPI_CALL(env,
    napi_create_uint32(env, 1337, &result));
  return result;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
