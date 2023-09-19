#include <stdlib.h>
#include "node_api.h"
#include "uv.h"
#include "../../js-native-api/common.h"

static napi_value RunInCallbackScope(napi_env env, napi_callback_info info) {
  size_t argc;
  napi_value args[3];

  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, NULL, NULL, NULL));
  NODE_API_ASSERT(env, argc == 3 , "Wrong number of arguments");

  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_valuetype valuetype;
  NODE_API_CALL(env, napi_typeof(env, args[0], &valuetype));
  NODE_API_ASSERT(env, valuetype == napi_object,
      "Wrong type of arguments. Expects an object as first argument.");

  NODE_API_CALL(env, napi_typeof(env, args[1], &valuetype));
  NODE_API_ASSERT(env, valuetype == napi_string,
      "Wrong type of arguments. Expects a string as second argument.");

  NODE_API_CALL(env, napi_typeof(env, args[2], &valuetype));
  NODE_API_ASSERT(env, valuetype == napi_function,
      "Wrong type of arguments. Expects a function as third argument.");

  napi_async_context context;
  NODE_API_CALL(env, napi_async_init(env, args[0], args[1], &context));

  napi_callback_scope scope = NULL;
  NODE_API_CALL(env,
      napi_open_callback_scope(env, args[0], context, &scope));

  // If the function has an exception pending after the call that is ok
  // so we don't use NODE_API_CALL as we must close the callback scope
  // regardless.
  napi_value result = NULL;
  napi_status function_call_result =
      napi_call_function(env, args[0], args[2], 0, NULL, &result);
  if (function_call_result != napi_ok) {
    GET_AND_THROW_LAST_ERROR((env));
  }

  NODE_API_CALL(env, napi_close_callback_scope(env, scope));
  NODE_API_CALL(env, napi_async_destroy(env, context));

  return result;
}

static napi_env shared_env = NULL;
static napi_deferred deferred = NULL;

static void Callback(uv_work_t* req, int ignored) {
  napi_env env = shared_env;

  napi_handle_scope handle_scope = NULL;
  NODE_API_CALL_RETURN_VOID(env, napi_open_handle_scope(env, &handle_scope));

  napi_value resource_name;
  NODE_API_CALL_RETURN_VOID(env, napi_create_string_utf8(
      env, "test", NAPI_AUTO_LENGTH, &resource_name));
  napi_async_context context;
  NODE_API_CALL_RETURN_VOID(env,
      napi_async_init(env, NULL, resource_name, &context));

  napi_value resource_object;
  NODE_API_CALL_RETURN_VOID(env, napi_create_object(env, &resource_object));

  napi_value undefined_value;
  NODE_API_CALL_RETURN_VOID(env, napi_get_undefined(env, &undefined_value));

  napi_callback_scope scope = NULL;
  NODE_API_CALL_RETURN_VOID(env,
      napi_open_callback_scope(env, resource_object, context, &scope));

  NODE_API_CALL_RETURN_VOID(env,
      napi_resolve_deferred(env, deferred, undefined_value));

  NODE_API_CALL_RETURN_VOID(env, napi_close_callback_scope(env, scope));

  NODE_API_CALL_RETURN_VOID(env, napi_close_handle_scope(env, handle_scope));
  NODE_API_CALL_RETURN_VOID(env, napi_async_destroy(env, context));
  free(req);
}

static void NoopWork(uv_work_t* work) { (void) work; }

static napi_value TestResolveAsync(napi_env env, napi_callback_info info) {
  napi_value promise = NULL;
  if (deferred == NULL) {
    shared_env = env;
    NODE_API_CALL(env, napi_create_promise(env, &deferred, &promise));

    uv_loop_t* loop = NULL;
    NODE_API_CALL(env, napi_get_uv_event_loop(env, &loop));

    uv_work_t* req = malloc(sizeof(*req));
    uv_queue_work(loop,
                  req,
                  NoopWork,
                  Callback);
  }
  return promise;
}

static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("runInCallbackScope", RunInCallbackScope),
    DECLARE_NODE_API_PROPERTY("testResolveAsync", TestResolveAsync)
  };

  NODE_API_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
