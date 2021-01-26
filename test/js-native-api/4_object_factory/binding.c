#include <js_native_api.h>
#include "../common.h"

static node_api_value
CreateObject(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  node_api_value obj;
  NODE_API_CALL(env, node_api_create_object(env, &obj));

  NODE_API_CALL(env, node_api_set_named_property(env, obj, "msg", args[0]));

  return obj;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  NODE_API_CALL(env, node_api_create_function(env,
                                              "exports",
                                              NODE_API_AUTO_LENGTH,
                                              CreateObject,
                                              NULL,
                                              &exports));
  return exports;
}
EXTERN_C_END
