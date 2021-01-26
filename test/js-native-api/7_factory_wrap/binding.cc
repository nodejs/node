#include <js_native_api.h>
#include "myobject.h"
#include "../common.h"

node_api_value CreateObject(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  node_api_value instance;
  NODE_API_CALL(env, MyObject::NewInstance(env, args[0], &instance));

  return instance;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  NODE_API_CALL(env, MyObject::Init(env));

  node_api_property_descriptor descriptors[] = {
    DECLARE_NODE_API_GETTER("finalizeCount", MyObject::GetFinalizeCount),
    DECLARE_NODE_API_PROPERTY("createObject", CreateObject),
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
