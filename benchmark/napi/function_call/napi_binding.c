#include <assert.h>
#include <node_api.h>

static int32_t increment = 0;

static napi_value Hello(napi_env env, napi_callback_info info) {
  napi_value result;
  napi_status status = napi_create_int32(env, increment++, &result);
  assert(status == napi_ok);
  return result;
}

NAPI_MODULE_INIT() {
  napi_value hello;
  napi_status status =
      napi_create_function(env,
                           "hello",
                           NAPI_AUTO_LENGTH,
                           Hello,
                           NULL,
                           &hello);
  assert(status == napi_ok);
  status = napi_set_named_property(env, exports, "hello", hello);
  assert(status == napi_ok);
  return exports;
}
