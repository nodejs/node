#include <node_api.h>
#include "../../js-native-api/common.h"

static int wrapped_data;

static void NoopFinalize(node_api_basic_env env, void* data, void* hint) {
  (void)env;
  (void)data;
  (void)hint;
}

// Wrap(object): attaches a finalizer that does nothing to `object`.
static napi_value Wrap(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value object;
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, &object, NULL, NULL));
  NODE_API_CALL(
      env, napi_wrap(env, object, &wrapped_data, NoopFinalize, NULL, NULL));
  return NULL;
}

static void ThrowFromCallJs(napi_env env,
                            napi_value js_callback,
                            void* context,
                            void* data) {
  (void)js_callback;
  (void)context;
  (void)data;
  NODE_API_CALL_RETURN_VOID(env, napi_throw_error(env, NULL, "call_js error"));
}

// ThrowFromThreadSafeFunction(): calls a thread-safe function once, whose
// call_js callback throws.
static napi_value ThrowFromThreadSafeFunction(napi_env env,
                                              napi_callback_info info) {
  (void)info;
  napi_value name;
  NODE_API_CALL(env,
                napi_create_string_utf8(env, "test", NAPI_AUTO_LENGTH, &name));
  napi_threadsafe_function tsfn;
  NODE_API_CALL(env,
                napi_create_threadsafe_function(env,
                                                NULL,
                                                NULL,
                                                name,
                                                0,
                                                1,
                                                NULL,
                                                NULL,
                                                NULL,
                                                ThrowFromCallJs,
                                                &tsfn));
  NODE_API_CALL(env,
                napi_call_threadsafe_function(tsfn, NULL, napi_tsfn_blocking));
  NODE_API_CALL(env, napi_release_threadsafe_function(tsfn, napi_tsfn_release));
  return NULL;
}

// CallFunction(fn): returns the result of calling `fn` through
// napi_call_function, which does not run while an exception is pending.
static napi_value CallFunction(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value fn;
  napi_value recv;
  napi_value result;
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, &fn, NULL, NULL));
  NODE_API_CALL(env, napi_get_undefined(env, &recv));
  NODE_API_CALL(env, napi_call_function(env, recv, fn, 0, NULL, &result));
  return result;
}

static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor properties[] = {
      DECLARE_NODE_API_PROPERTY("Wrap", Wrap),
      DECLARE_NODE_API_PROPERTY("ThrowFromThreadSafeFunction",
                                ThrowFromThreadSafeFunction),
      DECLARE_NODE_API_PROPERTY("CallFunction", CallFunction),
  };

  NODE_API_CALL(
      env,
      napi_define_properties(env,
                             exports,
                             sizeof(properties) / sizeof(properties[0]),
                             properties));

  return exports;
}
NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
