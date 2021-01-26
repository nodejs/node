#include <js_native_api.h>
#include "../common.h"

static node_api_value
MyFunction(node_api_env env, node_api_callback_info info) {
  node_api_value str;
  NODE_API_CALL(env,
                node_api_create_string_utf8(env, "hello world", -1, &str));
  return str;
}

static node_api_value
CreateFunction(node_api_env env, node_api_callback_info info) {
  node_api_value fn;
  NODE_API_CALL(env,
      node_api_create_function(env, "theFunction", -1, MyFunction, NULL, &fn));
  return fn;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  NODE_API_CALL(env, node_api_create_function(env,
                                              "exports",
                                              NODE_API_AUTO_LENGTH,
                                              CreateFunction,
                                              NULL,
                                              &exports));
  return exports;
}
EXTERN_C_END
