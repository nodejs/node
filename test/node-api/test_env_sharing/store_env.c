#include <node_api.h>
#include "../../js-native-api/common.h"

static napi_value Init(napi_env env, napi_value exports) {
  napi_value external;
  NAPI_CALL(env, napi_create_external(env, env, NULL, NULL, &external));
  return external;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
