#include <node_api.h>
#include "../common.h"

napi_deferred deferred = NULL;

napi_value createPromise(napi_env env, napi_callback_info info) {
  napi_value promise;

  // We do not overwrite an existing deferred.
  if (deferred != NULL) {
    return NULL;
  }

  NAPI_CALL(env, napi_create_promise(env, &deferred, &promise));

  return promise;
}

napi_value concludeCurrentPromise(napi_env env, napi_callback_info info) {
  napi_value argv[2];
  size_t argc = 2;
  bool resolution;

  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  NAPI_CALL(env, napi_get_value_bool(env, argv[1], &resolution));
  if (resolution) {
    NAPI_CALL(env, napi_resolve_deferred(env, deferred, argv[0]));
  } else {
    NAPI_CALL(env, napi_reject_deferred(env, deferred, argv[0]));
  }

  deferred = NULL;

  return NULL;
}

napi_value isPromise(napi_env env, napi_callback_info info) {
  napi_value promise, result;
  size_t argc = 1;
  bool is_promise;

  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &promise, NULL, NULL));
  NAPI_CALL(env, napi_is_promise(env, promise, &is_promise));
  NAPI_CALL(env, napi_get_boolean(env, is_promise, &result));

  return result;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("createPromise", createPromise),
    DECLARE_NAPI_PROPERTY("concludeCurrentPromise", concludeCurrentPromise),
    DECLARE_NAPI_PROPERTY("isPromise", isPromise),
  };

  NAPI_CALL_RETURN_VOID(env, napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));
}

NAPI_MODULE(addon, Init)
