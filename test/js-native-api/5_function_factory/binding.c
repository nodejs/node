#include <js_native_api.h>
#include "../common.h"

static napi_value MyFunction(napi_env env, napi_callback_info info) {
  napi_value str;
  NAPI_CALL(env, napi_create_string_utf8(env, "hello world", -1, &str));
  return str;
}

static napi_value CreateFunction(napi_env env, napi_callback_info info) {
  napi_value fn;
  NAPI_CALL(env,
    napi_create_function(env, "theFunction", -1, MyFunction, NULL, &fn));
  return fn;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  NAPI_CALL(env,
      napi_create_function(env, "exports", -1, CreateFunction, NULL, &exports));
  return exports;
}
EXTERN_C_END
