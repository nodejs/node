#include <js_native_api.h>
#include "../common.h"

static node_api_value
BaseClass(node_api_env env, node_api_callback_info info) {
  node_api_value newTargetArg;
  NODE_API_CALL(env, node_api_get_new_target(env, info, &newTargetArg));
  node_api_value thisArg;
  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, NULL, NULL, &thisArg, NULL));
  node_api_value undefined;
  NODE_API_CALL(env, node_api_get_undefined(env, &undefined));

  // this !== new.target since we are being invoked through super()
  bool result;
  NODE_API_CALL(env,
      node_api_strict_equals(env, newTargetArg, thisArg, &result));
  NODE_API_ASSERT(env, !result, "this !== new.target");

  // new.target !== undefined because we should be called as a new expression
  NODE_API_ASSERT(env, newTargetArg != NULL, "newTargetArg != NULL");
  NODE_API_CALL(env,
      node_api_strict_equals(env, newTargetArg, undefined, &result));
  NODE_API_ASSERT(env, !result, "new.target !== undefined");

  return thisArg;
}

static node_api_value
Constructor(node_api_env env, node_api_callback_info info) {
  bool result;
  node_api_value newTargetArg;
  NODE_API_CALL(env, node_api_get_new_target(env, info, &newTargetArg));
  size_t argc = 1;
  node_api_value argv;
  node_api_value thisArg;
  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, &argv, &thisArg, NULL));
  node_api_value undefined;
  NODE_API_CALL(env, node_api_get_undefined(env, &undefined));

  // new.target !== undefined because we should be called as a new expression
  NODE_API_ASSERT(env, newTargetArg != NULL, "newTargetArg != NULL");
  NODE_API_CALL(env,
      node_api_strict_equals(env, newTargetArg, undefined, &result));
  NODE_API_ASSERT(env, !result, "new.target !== undefined");

  // arguments[0] should be Constructor itself (test harness passed it)
  NODE_API_CALL(env, node_api_strict_equals(env, newTargetArg, argv, &result));
  NODE_API_ASSERT(env, result, "new.target === Constructor");

  return thisArg;
}

static node_api_value
OrdinaryFunction(node_api_env env, node_api_callback_info info) {
  node_api_value newTargetArg;
  NODE_API_CALL(env, node_api_get_new_target(env, info, &newTargetArg));

  NODE_API_ASSERT(env, newTargetArg == NULL, "newTargetArg == NULL");

  node_api_value _true;
  NODE_API_CALL(env, node_api_get_boolean(env, true, &_true));
  return _true;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  const node_api_property_descriptor desc[] = {
    DECLARE_NODE_API_PROPERTY("BaseClass", BaseClass),
    DECLARE_NODE_API_PROPERTY("OrdinaryFunction", OrdinaryFunction),
    DECLARE_NODE_API_PROPERTY("Constructor", Constructor)
  };
  NODE_API_CALL(env, node_api_define_properties(env, exports, 3, desc));
  return exports;
}
EXTERN_C_END
