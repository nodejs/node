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

  node_api_value async_context_wrap = args[0];
  node_api_value recv = args[1];
  node_api_value func = args[2];

  node_api_value argv[MAX_ARGUMENTS - RESERVED_ARGS];
  for (n = RESERVED_ARGS; n < argc; n += 1) {
    argv[n - RESERVED_ARGS] = args[n];
  }

  node_api_valuetype func_type;
  NODE_API_CALL(env, node_api_typeof(env, func, &func_type));

  node_api_async_context context;
  NODE_API_CALL(env,
      node_api_unwrap(env, async_context_wrap, (void**)&context));

  node_api_value result;
  if (func_type == node_api_function) {
    NODE_API_CALL(env, node_api_make_callback(
        env, context, recv, func, argc - RESERVED_ARGS, argv, &result));
  } else {
    NODE_API_ASSERT(env, false, "Unexpected argument type");
  }

  return result;
}

static void AsyncDestroyCb(node_api_env env, void* data, void* hint) {
  node_api_status status =
      node_api_async_destroy(env, (node_api_async_context)data);
  // We cannot use NODE_API_ASSERT_RETURN_VOID because we need to have a JS
  // stack below in order to use exceptions.
  assert(status == node_api_ok);
}

#define CREATE_ASYNC_RESOURCE_ARGC 2

static node_api_value
CreateAsyncResource(node_api_env env, node_api_callback_info info) {
  node_api_value async_context_wrap;
  NODE_API_CALL(env, node_api_create_object(env, &async_context_wrap));

  size_t argc = CREATE_ASYNC_RESOURCE_ARGC;
  node_api_value args[CREATE_ASYNC_RESOURCE_ARGC];
  // NOLINTNEXTLINE (readability/null_usage)
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  node_api_value resource = args[0];
  node_api_value js_destroy_on_finalizer = args[1];
  node_api_valuetype resource_type;
  NODE_API_CALL(env, node_api_typeof(env, resource, &resource_type));
  if (resource_type != node_api_object) {
    resource = NULL;
  }

  node_api_value resource_name;
  NODE_API_CALL(env, node_api_create_string_utf8(
      env, "test_async", NODE_API_AUTO_LENGTH, &resource_name));

  node_api_async_context context;
  NODE_API_CALL(env,
      node_api_async_init(env, resource, resource_name, &context));

  bool destroy_on_finalizer = true;
  if (argc == 2) {
    NODE_API_CALL(env,
        node_api_get_value_bool(
            env, js_destroy_on_finalizer, &destroy_on_finalizer));
  }
  if (resource_type == node_api_object && destroy_on_finalizer) {
    NODE_API_CALL(env, node_api_add_finalizer(
        env, resource, (void*)context, AsyncDestroyCb, NULL, NULL));
  }
  NODE_API_CALL(env,
      node_api_wrap(env, async_context_wrap, context, NULL, NULL, NULL));
  return async_context_wrap;
}

#define DESTROY_ASYNC_RESOURCE_ARGC 1

static node_api_value
DestroyAsyncResource(node_api_env env, node_api_callback_info info) {
  size_t argc = DESTROY_ASYNC_RESOURCE_ARGC;
  node_api_value args[DESTROY_ASYNC_RESOURCE_ARGC];
  // NOLINTNEXTLINE (readability/null_usage)
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));
  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");

  node_api_value async_context_wrap = args[0];

  node_api_async_context async_context;
  NODE_API_CALL(env,
      node_api_remove_wrap(env, async_context_wrap, (void**)&async_context));
  NODE_API_CALL(env, node_api_async_destroy(env, async_context));

  return async_context_wrap;
}

static
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_value fn;
  NODE_API_CALL(env, node_api_create_function(
      // NOLINTNEXTLINE (readability/null_usage)
      env, NULL, NODE_API_AUTO_LENGTH, MakeCallback, NULL, &fn));
  NODE_API_CALL(env,
      node_api_set_named_property(env, exports, "makeCallback", fn));
  NODE_API_CALL(env, node_api_create_function(
      // NOLINTNEXTLINE (readability/null_usage)
      env, NULL, NODE_API_AUTO_LENGTH, CreateAsyncResource, NULL, &fn));
  NODE_API_CALL(env, node_api_set_named_property(
      env, exports, "createAsyncResource", fn));

  NODE_API_CALL(env, node_api_create_function(
      // NOLINTNEXTLINE (readability/null_usage)
      env, NULL, NODE_API_AUTO_LENGTH, DestroyAsyncResource, NULL, &fn));
  NODE_API_CALL(env, node_api_set_named_property(
      env, exports, "destroyAsyncResource", fn));

  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
