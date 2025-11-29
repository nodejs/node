#include <stdlib.h>
#include <node_api.h>
#include "../../js-native-api/common.h"

static void MyObject_fini(napi_env env, void* data, void* hint) {
  napi_ref* ref = data;
  napi_value global;
  napi_value cleanup;
  NODE_API_CALL_RETURN_VOID(env, napi_get_global(env, &global));
  NODE_API_CALL_RETURN_VOID(
      env, napi_get_named_property(env, global, "cleanup", &cleanup));
  napi_status status = napi_call_function(env, global, cleanup, 0, NULL, NULL);
  // We may not be allowed to call into JS, in which case a pending exception
  // will be returned.
  NODE_API_ASSERT_RETURN_VOID(env,
      status == napi_ok || status == napi_pending_exception,
      "Unexpected status for napi_call_function");
  NODE_API_CALL_RETURN_VOID(env, napi_delete_reference(env, *ref));
  free(ref);
}

static napi_value MyObject(napi_env env, napi_callback_info info) {
  napi_value js_this;
  napi_ref* ref = malloc(sizeof(*ref));
  NODE_API_CALL(env, napi_get_cb_info(env, info, NULL, NULL, &js_this, NULL));
  NODE_API_CALL(env, napi_wrap(env, js_this, ref, MyObject_fini, NULL, ref));
  return NULL;
}

NAPI_MODULE_INIT() {
  napi_value ctor;
  NODE_API_CALL(
      env, napi_define_class(
          env, "MyObject", NAPI_AUTO_LENGTH, MyObject, NULL, 0, NULL, &ctor));
  NODE_API_CALL(env, napi_set_named_property(env, exports, "MyObject",  ctor));
  return exports;
}
