#include "node_api.h"
#include "../common.h"

static napi_value Init(napi_env env, napi_value exports) {
  napi_value result, answer;

  NAPI_CALL(env, napi_create_object(env, &result));
  NAPI_CALL(env, napi_create_int64(env, 42, &answer));
  NAPI_CALL(env, napi_set_named_property(env, result, "answer", answer));
  return result;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
NAPI_MODULE_FALLBACK(NODE_GYP_MODULE_NAME)
