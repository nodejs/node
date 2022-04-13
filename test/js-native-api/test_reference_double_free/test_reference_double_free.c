#include <stdlib.h>
#include <js_native_api.h>
#include "../common.h"

static size_t g_call_count = 0;

static void Destructor(napi_env env, void* data, void* nothing) {
  napi_ref* ref = data;
  NODE_API_CALL_RETURN_VOID(env, napi_delete_reference(env, *ref));
  free(ref);
}

static void NoDeleteDestructor(napi_env env, void* data, void* hint) {
  napi_ref* ref = data;
  size_t* call_count = hint;

  // This destructor must be called exactly once.
  if ((*call_count) > 0) abort();
  *call_count = ((*call_count) + 1);
  free(ref);
}

static napi_value New(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value js_this, js_delete;
  bool delete;
  napi_ref* ref = malloc(sizeof(*ref));

  NODE_API_CALL(env,
      napi_get_cb_info(env, info, &argc, &js_delete, &js_this, NULL));
  NODE_API_CALL(env, napi_get_value_bool(env, js_delete, &delete));

  if (delete) {
    NODE_API_CALL(env,
        napi_wrap(env, js_this, ref, Destructor, NULL, ref));
  } else {
    NODE_API_CALL(env,
        napi_wrap(env, js_this, ref, NoDeleteDestructor, &g_call_count, ref));
  }
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
