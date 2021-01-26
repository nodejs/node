#include <js_native_api.h>
#include "../common.h"

node_api_deferred deferred = NULL;

static node_api_value
createPromise(node_api_env env, node_api_callback_info info) {
  node_api_value promise;

  // We do not overwrite an existing deferred.
  if (deferred != NULL) {
    return NULL;
  }

  NODE_API_CALL(env, node_api_create_promise(env, &deferred, &promise));

  return promise;
}

static node_api_value
concludeCurrentPromise(node_api_env env, node_api_callback_info info) {
  node_api_value argv[2];
  size_t argc = 2;
  bool resolution;

  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, argv, NULL, NULL));
  NODE_API_CALL(env, node_api_get_value_bool(env, argv[1], &resolution));
  if (resolution) {
    NODE_API_CALL(env, node_api_resolve_deferred(env, deferred, argv[0]));
  } else {
    NODE_API_CALL(env, node_api_reject_deferred(env, deferred, argv[0]));
  }

  deferred = NULL;

  return NULL;
}

static node_api_value
isPromise(node_api_env env, node_api_callback_info info) {
  node_api_value promise, result;
  size_t argc = 1;
  bool is_promise;

  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, &promise, NULL, NULL));
  NODE_API_CALL(env, node_api_is_promise(env, promise, &is_promise));
  NODE_API_CALL(env, node_api_get_boolean(env, is_promise, &result));

  return result;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("createPromise", createPromise),
    DECLARE_NODE_API_PROPERTY("concludeCurrentPromise",
        concludeCurrentPromise),
    DECLARE_NODE_API_PROPERTY("isPromise", isPromise),
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
