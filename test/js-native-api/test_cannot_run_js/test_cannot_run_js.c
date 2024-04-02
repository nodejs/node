#include <js_native_api.h>
#include "../common.h"
#include "../entry_point.h"
#include "stdlib.h"

static void Finalize(napi_env env, void* data, void* hint) {
  napi_value global, set_timeout;
  napi_ref* ref = data;
#ifdef NAPI_EXPERIMENTAL
  napi_status expected_status = napi_cannot_run_js;
#else
  napi_status expected_status = napi_pending_exception;
#endif  // NAPI_EXPERIMENTAL

  if (napi_delete_reference(env, *ref) != napi_ok) abort();
  if (napi_get_global(env, &global) != napi_ok) abort();
  if (napi_get_named_property(env, global, "setTimeout", &set_timeout) !=
      expected_status)
    abort();
  free(ref);
}

static void NogcFinalize(node_api_nogc_env env, void* data, void* hint) {
#ifdef NAPI_EXPERIMENTAL
  NODE_API_NOGC_CALL_RETURN_VOID(
      env, node_api_post_finalizer(env, Finalize, data, hint));
#else
  Finalize(env, data, hint);
#endif
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
  NODE_API_CALL(env, napi_add_finalizer(env, cb, ref, NogcFinalize, NULL, ref));
  return cb;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor properties[] = {
      DECLARE_NODE_API_PROPERTY("createRef", CreateRef),
  };

  NODE_API_CALL(
      env,
      napi_define_properties(
          env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}
EXTERN_C_END
