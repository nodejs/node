#include <node_api.h>
#include "../common.h"

napi_ref constructor_;

napi_value New(napi_env env, napi_callback_info info) {
  napi_value _this;
  NAPI_CALL(env, napi_get_cb_info(env, info, NULL, NULL, &_this, NULL));

  return _this;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_value cons;
  NAPI_CALL(env, napi_define_class(
      env, "MyObject_Extra", 8, New, NULL, 0, NULL, &cons));

  NAPI_CALL(env,
      napi_create_reference(env, cons, 1, &constructor_));
  return cons;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
