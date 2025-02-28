#include <js_native_api.h>
#include "../common.h"
#include "../entry_point.h"

static napi_value New(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value description = NULL;
  if (argc >= 1) {
    napi_valuetype valuetype;
    NODE_API_CALL(env, napi_typeof(env, args[0], &valuetype));

    NODE_API_ASSERT(env, valuetype == napi_string,
        "Wrong type of arguments. Expects a string.");

    description = args[0];
  }

  napi_value symbol;
  NODE_API_CALL(env, napi_create_symbol(env, description, &symbol));

  return symbol;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor properties[] = {
    DECLARE_NODE_API_PROPERTY("New", New),
  };

  NODE_API_CALL(env, napi_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}
EXTERN_C_END
