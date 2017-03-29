#include <node_api.h>

#define NAPI_CALL(env, theCall)                                                \
  if ((theCall) != napi_ok) {                                                  \
    const napi_extended_error_info* error;                                     \
    napi_get_last_error_info((env), &error);                                   \
    const char* errorMessage = error->error_message;                           \
    errorMessage = errorMessage ? errorMessage : "empty error message";        \
    napi_throw_error((env), errorMessage);                                     \
    return;                                                                    \
  }

void RunCallback(napi_env env, napi_callback_info info) {
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_args(env, info, args, 1));

  napi_value cb = args[0];

  napi_value argv[1];
  NAPI_CALL(env, napi_create_string_utf8(env, "hello world", -1, argv));

  napi_value global;
  NAPI_CALL(env, napi_get_global(env, &global));

  NAPI_CALL(env, napi_call_function(env, global, cb, 1, argv, NULL));
}

void RunCallbackWithRecv(napi_env env, napi_callback_info info) {
  napi_value args[2];
  NAPI_CALL(env, napi_get_cb_args(env, info, args, 2));

  napi_value cb = args[0];
  napi_value recv = args[1];

  NAPI_CALL(env, napi_call_function(env, recv, cb, 0, NULL, NULL));
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;
  napi_property_descriptor desc[2] = {
      DECLARE_NAPI_METHOD("RunCallback", RunCallback),
      DECLARE_NAPI_METHOD("RunCallbackWithRecv", RunCallbackWithRecv),
  };
  status = napi_define_properties(env, exports, 2, desc);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
