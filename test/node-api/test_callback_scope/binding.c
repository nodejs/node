#include <stdlib.h>
#include "node_api.h"
#include "uv.h"
#include "../../js-native-api/common.h"

static node_api_value
RunInCallbackScope(node_api_env env, node_api_callback_info info) {
  size_t argc;
  node_api_value args[3];

  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, NULL, NULL, NULL));
  NODE_API_ASSERT(env, argc == 3 , "Wrong number of arguments");

  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  node_api_valuetype valuetype;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype));
  NODE_API_ASSERT(env, valuetype == node_api_object,
      "Wrong type of arguments. Expects an object as first argument.");

  NODE_API_CALL(env, node_api_typeof(env, args[1], &valuetype));
  NODE_API_ASSERT(env, valuetype == node_api_string,
      "Wrong type of arguments. Expects a string as second argument.");

  NODE_API_CALL(env, node_api_typeof(env, args[2], &valuetype));
  NODE_API_ASSERT(env, valuetype == node_api_function,
      "Wrong type of arguments. Expects a function as third argument.");

  node_api_async_context context;
  NODE_API_CALL(env, node_api_async_init(env, args[0], args[1], &context));

  node_api_callback_scope scope = NULL;
  NODE_API_CALL(
      env,
      node_api_open_callback_scope(env,
                                   args[0],
                                   context,
                                   &scope));

  // if the function has an exception pending after the call that is ok
  // so we don't use NODE_API_CALL as we must close the callback scope
  // regardless
  node_api_value result = NULL;
  node_api_status function_call_result =
      node_api_call_function(env, args[0], args[2], 0, NULL, &result);
  if (function_call_result != node_api_ok) {
    GET_AND_THROW_LAST_ERROR((env));
  }

  NODE_API_CALL(env, node_api_close_callback_scope(env, scope));
  NODE_API_CALL(env, node_api_async_destroy(env, context));

  return result;
}

static node_api_env shared_env = NULL;
static node_api_deferred deferred = NULL;

static void Callback(uv_work_t* req, int ignored) {
  node_api_env env = shared_env;

  node_api_handle_scope handle_scope = NULL;
  NODE_API_CALL_RETURN_VOID(env,
      node_api_open_handle_scope(env, &handle_scope));

  node_api_value resource_name;
  NODE_API_CALL_RETURN_VOID(env, node_api_create_string_utf8(
      env, "test", NODE_API_AUTO_LENGTH, &resource_name));
  node_api_async_context context;
  NODE_API_CALL_RETURN_VOID(env,
      node_api_async_init(env, NULL, resource_name, &context));

  node_api_value resource_object;
  NODE_API_CALL_RETURN_VOID(env,
      node_api_create_object(env, &resource_object));

  node_api_value undefined_value;
  NODE_API_CALL_RETURN_VOID(env,
      node_api_get_undefined(env, &undefined_value));

  node_api_callback_scope scope = NULL;
  NODE_API_CALL_RETURN_VOID(env,
      node_api_open_callback_scope(env, resource_object, context, &scope));

  NODE_API_CALL_RETURN_VOID(env,
      node_api_resolve_deferred(env, deferred, undefined_value));

  NODE_API_CALL_RETURN_VOID(env, node_api_close_callback_scope(env, scope));

  NODE_API_CALL_RETURN_VOID(env,
      node_api_close_handle_scope(env, handle_scope));
  NODE_API_CALL_RETURN_VOID(env, node_api_async_destroy(env, context));
  free(req);
}

static void NoopWork(uv_work_t* work) { (void) work; }

static node_api_value
TestResolveAsync(node_api_env env, node_api_callback_info info) {
  node_api_value promise = NULL;
  if (deferred == NULL) {
    shared_env = env;
    NODE_API_CALL(env, node_api_create_promise(env, &deferred, &promise));

    uv_loop_t* loop = NULL;
    NODE_API_CALL(env, node_api_get_uv_event_loop(env, &loop));

    uv_work_t* req = malloc(sizeof(*req));
    uv_queue_work(loop,
                  req,
                  NoopWork,
                  Callback);
  }
  return promise;
}

static node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("runInCallbackScope", RunInCallbackScope),
    DECLARE_NODE_API_PROPERTY("testResolveAsync", TestResolveAsync)
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
