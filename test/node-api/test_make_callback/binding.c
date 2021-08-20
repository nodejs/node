#include <node_api.h>
#include <assert.h>
#include "../../js-native-api/common.h"

#define MAX_ARGUMENTS 10
#define RESERVED_ARGS 3

static napi_value MakeCallback(napi_env env, napi_callback_info info) {
  size_t argc = MAX_ARGUMENTS;
  size_t n;
  napi_value args[MAX_ARGUMENTS];
  // NOLINTNEXTLINE (readability/null_usage)
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc > 0, "Wrong number of arguments");

  napi_value resource = args[0];
  napi_value recv = args[1];
  napi_value func = args[2];

  napi_value argv[MAX_ARGUMENTS - RESERVED_ARGS];
  for (n = RESERVED_ARGS; n < argc; n += 1) {
    argv[n - RESERVED_ARGS] = args[n];
  }

  napi_valuetype func_type;

  NODE_API_CALL(env, napi_typeof(env, func, &func_type));

  napi_value resource_name;
  NODE_API_CALL(env, napi_create_string_utf8(
      env, "test", NAPI_AUTO_LENGTH, &resource_name));

  napi_async_context context;
  NODE_API_CALL(env, napi_async_init(env, resource, resource_name, &context));

  napi_value result;
  if (func_type == napi_function) {
    NODE_API_CALL(env, napi_make_callback(
        env, context, recv, func, argc - RESERVED_ARGS, argv, &result));
  } else {
    NODE_API_ASSERT(env, false, "Unexpected argument type");
  }

  NODE_API_CALL(env, napi_async_destroy(env, context));

  return result;
}

static
napi_value Init(napi_env env, napi_value exports) {
  napi_value fn;
  NODE_API_CALL(env, napi_create_function(
      // NOLINTNEXTLINE (readability/null_usage)
      env, NULL, NAPI_AUTO_LENGTH, MakeCallback, NULL, &fn));
  NODE_API_CALL(env, napi_set_named_property(env, exports, "makeCallback", fn));
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
