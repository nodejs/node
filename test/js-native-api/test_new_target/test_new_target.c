#include <js_native_api.h>
#include "../common.h"

static napi_value BaseClass(napi_env env, napi_callback_info info) {
  napi_value newTargetArg;
  NODE_API_CALL(env, napi_get_new_target(env, info, &newTargetArg));
  napi_value thisArg;
  NODE_API_CALL(env, napi_get_cb_info(env, info, NULL, NULL, &thisArg, NULL));
  napi_value undefined;
  NODE_API_CALL(env, napi_get_undefined(env, &undefined));

  // this !== new.target since we are being invoked through super()
  bool result;
  NODE_API_CALL(env, napi_strict_equals(env, newTargetArg, thisArg, &result));
  NODE_API_ASSERT(env, !result, "this !== new.target");

  // new.target !== undefined because we should be called as a new expression
  NODE_API_ASSERT(env, newTargetArg != NULL, "newTargetArg != NULL");
  NODE_API_CALL(env, napi_strict_equals(env, newTargetArg, undefined, &result));
  NODE_API_ASSERT(env, !result, "new.target !== undefined");

  return thisArg;
}

static napi_value Constructor(napi_env env, napi_callback_info info) {
  bool result;
  napi_value newTargetArg;
  NODE_API_CALL(env, napi_get_new_target(env, info, &newTargetArg));
  size_t argc = 1;
  napi_value argv;
  napi_value thisArg;
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, &argv, &thisArg, NULL));
  napi_value undefined;
  NODE_API_CALL(env, napi_get_undefined(env, &undefined));

  // new.target !== undefined because we should be called as a new expression
  NODE_API_ASSERT(env, newTargetArg != NULL, "newTargetArg != NULL");
  NODE_API_CALL(env, napi_strict_equals(env, newTargetArg, undefined, &result));
  NODE_API_ASSERT(env, !result, "new.target !== undefined");

  // arguments[0] should be Constructor itself (test harness passed it)
  NODE_API_CALL(env, napi_strict_equals(env, newTargetArg, argv, &result));
  NODE_API_ASSERT(env, result, "new.target === Constructor");

  return thisArg;
}

static napi_value OrdinaryFunction(napi_env env, napi_callback_info info) {
  napi_value newTargetArg;
  NODE_API_CALL(env, napi_get_new_target(env, info, &newTargetArg));

  NODE_API_ASSERT(env, newTargetArg == NULL, "newTargetArg == NULL");

  napi_value _true;
  NODE_API_CALL(env, napi_get_boolean(env, true, &_true));
  return _true;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  const napi_property_descriptor desc[] = {
    DECLARE_NODE_API_PROPERTY("BaseClass", BaseClass),
    DECLARE_NODE_API_PROPERTY("OrdinaryFunction", OrdinaryFunction),
    DECLARE_NODE_API_PROPERTY("Constructor", Constructor)
  };
  NODE_API_CALL(env, napi_define_properties(env, exports, 3, desc));
  return exports;
}
EXTERN_C_END
