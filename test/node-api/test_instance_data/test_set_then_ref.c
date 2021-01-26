#include <stdio.h>
#include <stdlib.h>
#include <node_api.h>

node_api_value
addon_new(node_api_env env, node_api_value exports, bool ref_first);

// static node_api_value
NODE_API_MODULE_INIT(/*node_api_env env, node_api_value exports */) {
  return addon_new(env, exports, false);
}
