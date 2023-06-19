#include <assert.h>
#include <js_native_api.h>
#include <stdlib.h>
#include "../common.h"
#include "../entry_point.h"

static int test_value = 1;
static int finalize_count = 0;

static void FinalizeExternalCallJs(napi_env env, void* data, void* hint) {
  int* actual_value = data;
  NODE_API_ASSERT_RETURN_VOID(
      env,
      actual_value == &test_value,
      "The correct pointer was passed to the finalizer");

  napi_ref finalizer_ref = (napi_ref)hint;
  napi_value js_finalizer;
  napi_value recv;
  NODE_API_CALL_RETURN_VOID(
      env, napi_get_reference_value(env, finalizer_ref, &js_finalizer));
  NODE_API_CALL_RETURN_VOID(env, napi_get_global(env, &recv));
  NODE_API_CALL_RETURN_VOID(
      env, napi_call_function(env, recv, js_finalizer, 0, NULL, NULL));
  NODE_API_CALL_RETURN_VOID(env, napi_delete_reference(env, finalizer_ref));
}

static napi_value CreateExternalWithJsFinalize(napi_env env,
                                               napi_callback_info info) {
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

  napi_value result;
  NODE_API_CALL(env,
                napi_create_external(env,
                                     &test_value,
                                     FinalizeExternalCallJs,
                                     finalizer_ref, /* finalize_hint */
                                     &result));

  finalize_count = 0;
  return result;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
      DECLARE_NODE_API_PROPERTY("createExternalWithJsFinalize",
                                CreateExternalWithJsFinalize),
  };

  NODE_API_CALL(
      env,
      napi_define_properties(env,
                             exports,
                             sizeof(descriptors) / sizeof(*descriptors),
                             descriptors));

  return exports;
}
EXTERN_C_END
