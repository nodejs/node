#include "myobject.h"
#include "../common.h"

napi_value CreateObject(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, nullptr, nullptr));

  napi_value instance;
  NAPI_CALL(env, MyObject::NewInstance(env, args[0], &instance));

  return instance;
}

napi_value Init(napi_env env, napi_value exports) {
  NAPI_CALL(env, MyObject::Init(env));

  NAPI_CALL(env,
      // NOLINTNEXTLINE (readability/null_usage)
      napi_create_function(env, "exports", -1, CreateObject, NULL, &exports));
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
