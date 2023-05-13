#include <node_api.h>
#include "../../js-native-api/common.h"
#include "stdlib.h"

static void Finalize(napi_env env, void* data, void* hint) {
  napi_value cb;
  napi_ref* ref = data;
#ifdef NAPI_EXPERIMENTAL
  napi_status expected_status = napi_cannot_run_js;
#else
  napi_status expected_status = napi_pending_exception;
#endif  // NAPI_EXPERIMENTAL

  if (napi_get_reference_value(env, *ref, &cb) != napi_ok) abort();
  if (napi_delete_reference(env, *ref) != napi_ok) abort();
  if (napi_call_function(env, cb, cb, 0, NULL, NULL) != expected_status)
    abort();
  free(ref);
}

static napi_value CreateRef(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value cb;
  napi_valuetype value_type;
  napi_ref* ref = malloc(sizeof(*ref));
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, &cb, NULL, NULL));
  NODE_API_ASSERT(env, argc == 1, "Function takes only one argument");
  NODE_API_CALL(env, napi_typeof(env, cb, &value_type));
  NODE_API_ASSERT(
      env, value_type == napi_function, "argument must be function");
  NODE_API_CALL(env, napi_add_finalizer(env, cb, ref, Finalize, NULL, ref));
  return cb;
}

NAPI_MODULE_INIT() {
  napi_property_descriptor properties[] = {
      DECLARE_NODE_API_PROPERTY("createRef", CreateRef),
  };

  NODE_API_CALL(
      env,
      napi_define_properties(
          env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}
