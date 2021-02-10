#include <stdlib.h>
#include <js_native_api.h>
#include "../common.h"

static void Destructor(napi_env env, void* data, void* nothing) {
  napi_ref* ref = data;
  NODE_API_CALL_RETURN_VOID(env, napi_delete_reference(env, *ref));
  free(ref);
}

static napi_value New(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value js_this;
  napi_ref* ref = malloc(sizeof(*ref));

  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, NULL, &js_this, NULL));
  NODE_API_CALL(env, napi_wrap(env, js_this, ref, Destructor, NULL, ref)); 
  NODE_API_CALL(env, napi_reference_ref(env, *ref, NULL));

  return js_this;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_value myobj_ctor;
  NODE_API_CALL(env,
      napi_define_class(
          env, "MyObject", NAPI_AUTO_LENGTH, New, NULL, 0, NULL, &myobj_ctor));
  NODE_API_CALL(env,
      napi_set_named_property(env, exports, "MyObject", myobj_ctor));
  return exports;
}
EXTERN_C_END
