#include <node_api.h>
#include "addon.h"

static void AddProperties(napi_env env,
                          napi_value exports,
                          size_t prop_count,
                          napi_property_descriptor* props) {
  if (napi_define_properties(env, exports, prop_count, props) != napi_ok) {
    napi_fatal_error("async_function", NAPI_AUTO_LENGTH,
        "Failed to define properties on exports object", NAPI_AUTO_LENGTH);
  }
}

static napi_value Init(napi_env env, napi_value exports) {
  InitTestBasic(env, exports, AddProperties);
  InitTestEmpty(env, exports, AddProperties);
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
