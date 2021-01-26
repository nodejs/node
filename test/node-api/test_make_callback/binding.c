#include <node_api.h>
#include <assert.h>
#include "../../js-native-api/common.h"

#define MAX_ARGUMENTS 10
#define RESERVED_ARGS 3

static node_api_value
MakeCallback(node_api_env env, node_api_callback_info info) {
  size_t argc = MAX_ARGUMENTS;
  size_t n;
  node_api_value args[MAX_ARGUMENTS];
  // NOLINTNEXTLINE (readability/null_usage)
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc > 0, "Wrong number of arguments");

  node_api_value resource = args[0];
  node_api_value recv = args[1];
  node_api_value func = args[2];

  node_api_value argv[MAX_ARGUMENTS - RESERVED_ARGS];
  for (n = RESERVED_ARGS; n < argc; n += 1) {
    argv[n - RESERVED_ARGS] = args[n];
  }

  node_api_valuetype func_type;

  NODE_API_CALL(env, node_api_typeof(env, func, &func_type));

  node_api_value resource_name;
  NODE_API_CALL(env, node_api_create_string_utf8(
      env, "test", NODE_API_AUTO_LENGTH, &resource_name));

  node_api_async_context context;
  NODE_API_CALL(env,
      node_api_async_init(env, resource, resource_name, &context));

  node_api_value result;
  if (func_type == node_api_function) {
    NODE_API_CALL(env, node_api_make_callback(
        env, context, recv, func, argc - RESERVED_ARGS, argv, &result));
  } else {
    NODE_API_ASSERT(env, false, "Unexpected argument type");
  }

  NODE_API_CALL(env, node_api_async_destroy(env, context));

  return result;
}

static
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_value fn;
  NODE_API_CALL(env, node_api_create_function(
      // NOLINTNEXTLINE (readability/null_usage)
      env, NULL, NODE_API_AUTO_LENGTH, MakeCallback, NULL, &fn));
  NODE_API_CALL(env,
      node_api_set_named_property(env, exports, "makeCallback", fn));
  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
