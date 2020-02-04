#include <stdio.h>
#include <stdlib.h>
#define NAPI_EXPERIMENTAL
#include <node_api.h>

napi_value addon_new(napi_env env, napi_value exports, bool ref_first);

// static napi_value
NAPI_MODULE_INIT(/*napi_env env, napi_value exports */) {
  return addon_new(env, exports, true);
}
