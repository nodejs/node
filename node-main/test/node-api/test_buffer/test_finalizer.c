#include <node_api.h>
#include <stdlib.h>
#include <string.h>
#include "../../js-native-api/common.h"

static const char theText[] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit.";

static void malignDeleter(napi_env env, void* data, void* finalize_hint) {
  NODE_API_ASSERT_RETURN_VOID(
      env, data != NULL && strcmp(data, theText) == 0, "invalid data");
  napi_ref finalizer_ref = (napi_ref)finalize_hint;
  napi_value js_finalizer;
  napi_value recv;
  NODE_API_CALL_RETURN_VOID(
      env, napi_get_reference_value(env, finalizer_ref, &js_finalizer));
  NODE_API_CALL_RETURN_VOID(env, napi_get_global(env, &recv));
  NODE_API_CALL_RETURN_VOID(
      env, napi_call_function(env, recv, js_finalizer, 0, NULL, NULL));
  NODE_API_CALL_RETURN_VOID(env, napi_delete_reference(env, finalizer_ref));
}

static napi_value malignFinalizerBuffer(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");
  napi_value finalizer = args[0];
  napi_valuetype finalizer_valuetype;
  NODE_API_CALL(env, napi_typeof(env, finalizer, &finalizer_valuetype));
  NODE_API_ASSERT(env,
                  finalizer_valuetype == napi_function,
                  "Wrong type of first argument");
  napi_ref finalizer_ref;
  NODE_API_CALL(env, napi_create_reference(env, finalizer, 1, &finalizer_ref));

  napi_value theBuffer;
  NODE_API_CALL(env,
                napi_create_external_buffer(env,
                                            sizeof(theText),
                                            (void*)theText,
                                            malignDeleter,
                                            finalizer_ref,  // finalize_hint
                                            &theBuffer));
  return theBuffer;
}

static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor methods[] = {
      DECLARE_NODE_API_PROPERTY("malignFinalizerBuffer", malignFinalizerBuffer),
  };

  NODE_API_CALL(
      env,
      napi_define_properties(
          env, exports, sizeof(methods) / sizeof(methods[0]), methods));

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
