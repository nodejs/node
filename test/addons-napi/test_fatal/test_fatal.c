#include <node_api.h>
#include "../common.h"

napi_value Test(napi_env env, napi_callback_info info) {
  napi_fatal_error("test_fatal::Test", "fatal message");
  return NULL;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor properties[] = {
    DECLARE_NAPI_PROPERTY("Test", Test),
  };

  NAPI_CALL_RETURN_VOID(env, napi_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));
}

NAPI_MODULE(addon, Init)
