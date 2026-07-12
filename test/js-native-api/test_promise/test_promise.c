#include <js_native_api.h>
#include "../common.h"
#include "../entry_point.h"

napi_deferred deferred = NULL;

static napi_value createPromise(napi_env env, napi_callback_info info) {
  napi_value promise;

  // We do not overwrite an existing deferred.
  if (deferred != NULL) {
    return NULL;
  }

  NODE_API_CALL(env, napi_create_promise(env, &deferred, &promise));

  return promise;
}

static napi_value
concludeCurrentPromise(napi_env env, napi_callback_info info) {
  napi_value argv[2];
  size_t argc = 2;
  bool resolution;

  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  NODE_API_CALL(env, napi_get_value_bool(env, argv[1], &resolution));
  if (resolution) {
    NODE_API_CALL(env, napi_resolve_deferred(env, deferred, argv[0]));
  } else {
    NODE_API_CALL(env, napi_reject_deferred(env, deferred, argv[0]));
  }

  deferred = NULL;

  return NULL;
}

static napi_value isPromise(napi_env env, napi_callback_info info) {
  napi_value promise, result;
  size_t argc = 1;
  bool is_promise;

  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, &promise, NULL, NULL));
  NODE_API_CALL(env, napi_is_promise(env, promise, &is_promise));
  NODE_API_CALL(env, napi_get_boolean(env, is_promise, &result));

  return result;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("createPromise", createPromise),
    DECLARE_NODE_API_PROPERTY("concludeCurrentPromise", concludeCurrentPromise),
    DECLARE_NODE_API_PROPERTY("isPromise", isPromise),
  };

  NODE_API_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
