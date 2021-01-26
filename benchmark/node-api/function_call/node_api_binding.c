#include <assert.h>
#include <node_api.h>

static int32_t increment = 0;

static node_api_value Hello(node_api_env env, node_api_callback_info info) {
  node_api_value result;
  node_api_status status = node_api_create_int32(env, increment++, &result);
  assert(status == node_api_ok);
  return result;
}

NODE_API_MODULE_INIT() {
  node_api_value hello;
  node_api_status status =
      node_api_create_function(env,
                               "hello",
                               NODE_API_AUTO_LENGTH,
                               Hello,
                               NULL,
                               &hello);
  assert(status == node_api_ok);
  status = node_api_set_named_property(env, exports, "hello", hello);
  assert(status == node_api_ok);
  return exports;
}
