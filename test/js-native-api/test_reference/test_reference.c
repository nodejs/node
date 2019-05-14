#include <stdlib.h>
#include <assert.h>
#include <js_native_api.h>
#include "../common.h"

static int test_value = 1;
static int finalize_count = 0;
static napi_ref test_reference = NULL;

static napi_value GetFinalizeCount(napi_env env, napi_callback_info info) {
  napi_value result;
  NAPI_CALL(env, napi_create_int32(env, finalize_count, &result));
  return result;
}

static void FinalizeExternal(napi_env env, void* data, void* hint) {
  int *actual_value = data;
  NAPI_ASSERT_RETURN_VOID(env, actual_value == &test_value,
      "The correct pointer was passed to the finalizer");
  finalize_count++;
}

static napi_value CreateExternal(napi_env env, napi_callback_info info) {
  int* data = &test_value;

  napi_value result;
  NAPI_CALL(env,
      napi_create_external(env,
                           data,
                           NULL, /* finalize_cb */
                           NULL, /* finalize_hint */
                           &result));

  finalize_count = 0;
  return result;
}

static napi_value
CreateExternalWithFinalize(napi_env env, napi_callback_info info) {
  napi_value result;
  NAPI_CALL(env,
      napi_create_external(env,
                           &test_value,
                           FinalizeExternal,
                           NULL, /* finalize_hint */
                           &result));

  finalize_count = 0;
  return result;
}

static napi_value CheckExternal(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value arg;
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &arg, NULL, NULL));

  NAPI_ASSERT(env, argc == 1, "Expected one argument.");

  napi_valuetype argtype;
  NAPI_CALL(env, napi_typeof(env, arg, &argtype));

  NAPI_ASSERT(env, argtype == napi_external, "Expected an external value.");

  void* data;
  NAPI_CALL(env, napi_get_value_external(env, arg, &data));

  NAPI_ASSERT(env, data != NULL && *(int*)data == test_value,
      "An external data value of 1 was expected.");

  return NULL;
}

static napi_value CreateReference(napi_env env, napi_callback_info info) {
  NAPI_ASSERT(env, test_reference == NULL,
      "The test allows only one reference at a time.");

  size_t argc = 2;
  napi_value args[2];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
  NAPI_ASSERT(env, argc == 2, "Expected two arguments.");

  uint32_t initial_refcount;
  NAPI_CALL(env, napi_get_value_uint32(env, args[1], &initial_refcount));

  NAPI_CALL(env,
      napi_create_reference(env, args[0], initial_refcount, &test_reference));

  NAPI_ASSERT(env, test_reference != NULL,
      "A reference should have been created.");

  return NULL;
}

static napi_value DeleteReference(napi_env env, napi_callback_info info) {
  NAPI_ASSERT(env, test_reference != NULL,
      "A reference must have been created.");

  NAPI_CALL(env, napi_delete_reference(env, test_reference));
  test_reference = NULL;
  return NULL;
}

static napi_value IncrementRefcount(napi_env env, napi_callback_info info) {
  NAPI_ASSERT(env, test_reference != NULL,
      "A reference must have been created.");

  uint32_t refcount;
  NAPI_CALL(env, napi_reference_ref(env, test_reference, &refcount));

  napi_value result;
  NAPI_CALL(env, napi_create_uint32(env, refcount, &result));
  return result;
}

static napi_value DecrementRefcount(napi_env env, napi_callback_info info) {
  NAPI_ASSERT(env, test_reference != NULL,
      "A reference must have been created.");

  uint32_t refcount;
  NAPI_CALL(env, napi_reference_unref(env, test_reference, &refcount));

  napi_value result;
  NAPI_CALL(env, napi_create_uint32(env, refcount, &result));
  return result;
}

static napi_value GetReferenceValue(napi_env env, napi_callback_info info) {
  NAPI_ASSERT(env, test_reference != NULL,
      "A reference must have been created.");

  napi_value result;
  NAPI_CALL(env, napi_get_reference_value(env, test_reference, &result));
  return result;
}

static void DeleteBeforeFinalizeFinalizer(
    napi_env env, void* finalize_data, void* finalize_hint) {
  napi_ref* ref = (napi_ref*)finalize_data;
  napi_value value;
  assert(napi_get_reference_value(env, *ref, &value) == napi_ok);
  assert(value == NULL);
  napi_delete_reference(env, *ref);
  free(ref);
}

static napi_value ValidateDeleteBeforeFinalize(napi_env env, napi_callback_info info) {
  napi_value wrapObject;
  size_t argc = 1;
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &wrapObject, NULL, NULL));

  napi_ref* ref_t = malloc(sizeof(napi_ref));
  NAPI_CALL(env, napi_wrap(env,
                          wrapObject,
                          ref_t,
                          DeleteBeforeFinalizeFinalizer,
                          NULL,
                          NULL));

  // Create a reference that will be eligible for collection at the same
  // time as the wrapped object by passing in the same wrapObject.
  // This means that the FinalizeOrderValidation callback may be run
  // before the finalizer for the newly created reference (there is a finalizer
  // behind the scenes even though it cannot be passed to napi_create_reference)
  // The Finalizer for the wrap (which is different than the finalizer
  // for the reference) calls napi_delete_reference validating that
  // napi_delete_reference can be called before the finalizer for the
  // reference runs.
  NAPI_CALL(env, napi_create_reference(env, wrapObject, 0, ref_t));
  return wrapObject;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_GETTER("finalizeCount", GetFinalizeCount),
    DECLARE_NAPI_PROPERTY("createExternal", CreateExternal),
    DECLARE_NAPI_PROPERTY("createExternalWithFinalize",
        CreateExternalWithFinalize),
    DECLARE_NAPI_PROPERTY("checkExternal", CheckExternal),
    DECLARE_NAPI_PROPERTY("createReference", CreateReference),
    DECLARE_NAPI_PROPERTY("deleteReference", DeleteReference),
    DECLARE_NAPI_PROPERTY("incrementRefcount", IncrementRefcount),
    DECLARE_NAPI_PROPERTY("decrementRefcount", DecrementRefcount),
    DECLARE_NAPI_GETTER("referenceValue", GetReferenceValue),
    DECLARE_NAPI_PROPERTY("validateDeleteBeforeFinalize",
                          ValidateDeleteBeforeFinalize),
  };

  NAPI_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
