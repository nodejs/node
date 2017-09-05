#include <node_api.h>
#include "../common.h"

napi_value Test(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc >= 1, "Wrong number of arguments");

  napi_valuetype valuetype;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype));

  NAPI_ASSERT(env, valuetype == napi_symbol,
    "Wrong type of argments. Expects a symbol.");

  char buffer[128];
  size_t buffer_size = 128;

  NAPI_CALL(env, napi_get_value_string_utf8(
    env, args[0], buffer, buffer_size, NULL));

  napi_value output;
  NAPI_CALL(env, napi_create_string_utf8(env, buffer, buffer_size, &output));

  return output;
}

napi_value New(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value description = NULL;
  if (argc >= 1) {
    napi_valuetype valuetype;
    NAPI_CALL(env, napi_typeof(env, args[0], &valuetype));

    NAPI_ASSERT(env, valuetype == napi_string,
      "Wrong type of arguments. Expects a string.");

    description = args[0];
  }

  napi_value symbol;
  NAPI_CALL(env, napi_create_symbol(env, description, &symbol));

  return symbol;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor properties[] = {
    DECLARE_NAPI_PROPERTY("New", New),
  };

  NAPI_CALL_RETURN_VOID(env, napi_define_properties(
    env, exports, sizeof(properties) / sizeof(*properties), properties));
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
