#include <node_api.h>
#include "../../js-native-api/common.h"

static node_api_value
MakeCallback(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];
  // NOLINTNEXTLINE (readability/null_usage)
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  node_api_value recv = args[0];
  node_api_value func = args[1];

  node_api_status status =
      node_api_make_callback(env, NULL /* async_context */, recv, func,
          0 /* argc */, NULL /* argv */, NULL /* result */);

  bool isExceptionPending;
  NODE_API_CALL(env, node_api_is_exception_pending(env, &isExceptionPending));
  if (isExceptionPending && !(status == node_api_pending_exception)) {
    // if there is an exception pending we don't expect any
    // other error
    node_api_value pending_error;
    status = node_api_get_and_clear_last_exception(env, &pending_error);
    NODE_API_CALL(env,
      node_api_throw_error((env),
                        NULL,
                        "error when only pending exception expected"));
  }

  return recv;
}

static node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_value fn;
  NODE_API_CALL(env, node_api_create_function(
      // NOLINTNEXTLINE (readability/null_usage)
      env, NULL, NODE_API_AUTO_LENGTH, MakeCallback, NULL, &fn));
  NODE_API_CALL(env,
      node_api_set_named_property(env, exports, "makeCallback", fn));
  return exports;
}

NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
