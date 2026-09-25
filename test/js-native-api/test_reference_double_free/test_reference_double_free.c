#include <js_native_api.h>
#include <stdlib.h>
#include "../common.h"
#include "../entry_point.h"

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

static void NoopDeleter(napi_env env, void* data, void* hint) {}

// Tests that calling napi_remove_wrap and napi_delete_reference consecutively
// doesn't crash the process.
// This is analogous to the test https://github.com/nodejs/node-addon-api/blob/main/test/objectwrap_constructor_exception.cc.
// In which the Napi::ObjectWrap<> is being destructed immediately after napi_wrap.
// As Napi::ObjectWrap<> is a subclass of Napi::Reference<>, napi_remove_wrap
// in the destructor of Napi::ObjectWrap<> is called before napi_delete_reference
// in the destructor of Napi::Reference<>.
static napi_value DeleteImmediately(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value js_obj;
  napi_ref ref;
  napi_valuetype type;

  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, &js_obj, NULL, NULL));

  NODE_API_CALL(env, napi_typeof(env, js_obj, &type));
  NODE_API_ASSERT(env, type == napi_object, "Expected object parameter");

  NODE_API_CALL(env, napi_wrap(env, js_obj, NULL, NoopDeleter, NULL, &ref));
  NODE_API_CALL(env, napi_remove_wrap(env, js_obj, NULL));
  NODE_API_CALL(env, napi_delete_reference(env, ref));

  return NULL;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_value myobj_ctor;
  NODE_API_CALL(env,
      napi_define_class(
          env, "MyObject", NAPI_AUTO_LENGTH, New, NULL, 0, NULL, &myobj_ctor));
  NODE_API_CALL(env,
      napi_set_named_property(env, exports, "MyObject", myobj_ctor));

  napi_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("deleteImmediately", DeleteImmediately),
  };
  NODE_API_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
