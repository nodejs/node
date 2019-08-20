#include <js_native_api.h>
#include "../common.h"

static napi_value BaseClass(napi_env env, napi_callback_info info) {
  napi_value newTargetArg;
  NAPI_CALL(env, napi_get_new_target(env, info, &newTargetArg));
  napi_value thisArg;
  NAPI_CALL(env, napi_get_cb_info(env, info, NULL, NULL, &thisArg, NULL));
  napi_value undefined;
  NAPI_CALL(env, napi_get_undefined(env, &undefined));

  // this !== new.target since we are being invoked through super()
  bool result;
  NAPI_CALL(env, napi_strict_equals(env, newTargetArg, thisArg, &result));
  NAPI_ASSERT(env, !result, "this !== new.target");

  // new.target !== undefined because we should be called as a new expression
  NAPI_ASSERT(env, newTargetArg != NULL, "newTargetArg != NULL");
  NAPI_CALL(env, napi_strict_equals(env, newTargetArg, undefined, &result));
  NAPI_ASSERT(env, !result, "new.target !== undefined");

  return thisArg;
}

static napi_value Constructor(napi_env env, napi_callback_info info) {
  bool result;
  napi_value newTargetArg;
  NAPI_CALL(env, napi_get_new_target(env, info, &newTargetArg));
  size_t argc = 1;
  napi_value argv;
  napi_value thisArg;
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &argv, &thisArg, NULL));
  napi_value undefined;
  NAPI_CALL(env, napi_get_undefined(env, &undefined));

  // new.target !== undefined because we should be called as a new expression
  NAPI_ASSERT(env, newTargetArg != NULL, "newTargetArg != NULL");
  NAPI_CALL(env, napi_strict_equals(env, newTargetArg, undefined, &result));
  NAPI_ASSERT(env, !result, "new.target !== undefined");

  // arguments[0] should be Constructor itself (test harness passed it)
  NAPI_CALL(env, napi_strict_equals(env, newTargetArg, argv, &result));
  NAPI_ASSERT(env, result, "new.target === Constructor");

  return thisArg;
}

static napi_value OrdinaryFunction(napi_env env, napi_callback_info info) {
  napi_value newTargetArg;
  NAPI_CALL(env, napi_get_new_target(env, info, &newTargetArg));

  NAPI_ASSERT(env, newTargetArg == NULL, "newTargetArg == NULL");

  napi_value _true;
  NAPI_CALL(env, napi_get_boolean(env, true, &_true));
  return _true;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  const napi_property_descriptor desc[] = {
    DECLARE_NAPI_PROPERTY("BaseClass", BaseClass),
    DECLARE_NAPI_PROPERTY("OrdinaryFunction", OrdinaryFunction),
    DECLARE_NAPI_PROPERTY("Constructor", Constructor)
  };
  NAPI_CALL(env, napi_define_properties(env, exports, 3, desc));
  return exports;
}
EXTERN_C_END
