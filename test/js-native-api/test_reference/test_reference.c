#include <stdlib.h>
#include <assert.h>
#include <js_native_api.h>
#include "../common.h"

static int test_value = 1;
static int finalize_count = 0;
static node_api_ref test_reference = NULL;

static node_api_value
GetFinalizeCount(node_api_env env, node_api_callback_info info) {
  node_api_value result;
  NODE_API_CALL(env, node_api_create_int32(env, finalize_count, &result));
  return result;
}

static void FinalizeExternal(node_api_env env, void* data, void* hint) {
  int *actual_value = data;
  NODE_API_ASSERT_RETURN_VOID(env, actual_value == &test_value,
      "The correct pointer was passed to the finalizer");
  finalize_count++;
}

static node_api_value
CreateExternal(node_api_env env, node_api_callback_info info) {
  int* data = &test_value;

  node_api_value result;
  NODE_API_CALL(env,
      node_api_create_external(env,
                               data,
                               NULL, /* finalize_cb */
                               NULL, /* finalize_hint */
                               &result));

  finalize_count = 0;
  return result;
}

static node_api_value
CreateExternalWithFinalize(node_api_env env, node_api_callback_info info) {
  node_api_value result;
  NODE_API_CALL(env,
      node_api_create_external(env,
                               &test_value,
                               FinalizeExternal,
                               NULL, /* finalize_hint */
                               &result));

  finalize_count = 0;
  return result;
}

static node_api_value
CheckExternal(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value arg;
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, &arg, NULL, NULL));

  NODE_API_ASSERT(env, argc == 1, "Expected one argument.");

  node_api_valuetype argtype;
  NODE_API_CALL(env, node_api_typeof(env, arg, &argtype));

  NODE_API_ASSERT(env, argtype == node_api_external,
      "Expected an external value.");

  void* data;
  NODE_API_CALL(env, node_api_get_value_external(env, arg, &data));

  NODE_API_ASSERT(env, data != NULL && *(int*)data == test_value,
      "An external data value of 1 was expected.");

  return NULL;
}

static node_api_value
CreateReference(node_api_env env, node_api_callback_info info) {
  NODE_API_ASSERT(env, test_reference == NULL,
      "The test allows only one reference at a time.");

  size_t argc = 2;
  node_api_value args[2];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));
  NODE_API_ASSERT(env, argc == 2, "Expected two arguments.");

  uint32_t initial_refcount;
  NODE_API_CALL(env,
      node_api_get_value_uint32(env, args[1], &initial_refcount));

  NODE_API_CALL(env,
      node_api_create_reference(
          env, args[0], initial_refcount, &test_reference));

  NODE_API_ASSERT(env, test_reference != NULL,
      "A reference should have been created.");

  return NULL;
}

static node_api_value
DeleteReference(node_api_env env, node_api_callback_info info) {
  NODE_API_ASSERT(env, test_reference != NULL,
      "A reference must have been created.");

  NODE_API_CALL(env, node_api_delete_reference(env, test_reference));
  test_reference = NULL;
  return NULL;
}

static node_api_value
IncrementRefcount(node_api_env env, node_api_callback_info info) {
  NODE_API_ASSERT(env, test_reference != NULL,
      "A reference must have been created.");

  uint32_t refcount;
  NODE_API_CALL(env, node_api_reference_ref(env, test_reference, &refcount));

  node_api_value result;
  NODE_API_CALL(env, node_api_create_uint32(env, refcount, &result));
  return result;
}

static node_api_value
DecrementRefcount(node_api_env env, node_api_callback_info info) {
  NODE_API_ASSERT(env, test_reference != NULL,
      "A reference must have been created.");

  uint32_t refcount;
  NODE_API_CALL(env, node_api_reference_unref(env, test_reference, &refcount));

  node_api_value result;
  NODE_API_CALL(env, node_api_create_uint32(env, refcount, &result));
  return result;
}

static node_api_value
GetReferenceValue(node_api_env env, node_api_callback_info info) {
  NODE_API_ASSERT(env, test_reference != NULL,
      "A reference must have been created.");

  node_api_value result;
  NODE_API_CALL(env,
      node_api_get_reference_value(env, test_reference, &result));
  return result;
}

static void DeleteBeforeFinalizeFinalizer(
    node_api_env env, void* finalize_data, void* finalize_hint) {
  node_api_ref* ref = (node_api_ref*)finalize_data;
  node_api_value value;
  assert(node_api_get_reference_value(env, *ref, &value) == node_api_ok);
  assert(value == NULL);
  node_api_delete_reference(env, *ref);
  free(ref);
}

static node_api_value
ValidateDeleteBeforeFinalize(node_api_env env, node_api_callback_info info) {
  node_api_value wrapObject;
  size_t argc = 1;
  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, &wrapObject, NULL, NULL));

  node_api_ref* ref_t = malloc(sizeof(node_api_ref));
  NODE_API_CALL(env, node_api_wrap(env,
                                   wrapObject,
                                   ref_t,
                                   DeleteBeforeFinalizeFinalizer,
                                   NULL,
                                   NULL));

  // Create a reference that will be eligible for collection at the same
  // time as the wrapped object by passing in the same wrapObject.
  // This means that the FinalizeOrderValidation callback may be run
  // before the finalizer for the newly created reference (there is a finalizer
  // behind the scenes even though it cannot be passed to
  // node_api_create_reference). The Finalizer for the wrap (which is different
  // than the finalizer for the reference) calls node_api_delete_reference
  // validating that node_api_delete_reference can be called before the
  // finalizer for the reference runs.
  NODE_API_CALL(env, node_api_create_reference(env, wrapObject, 0, ref_t));
  return wrapObject;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor descriptors[] = {
    DECLARE_NODE_API_GETTER("finalizeCount", GetFinalizeCount),
    DECLARE_NODE_API_PROPERTY("createExternal", CreateExternal),
    DECLARE_NODE_API_PROPERTY("createExternalWithFinalize",
        CreateExternalWithFinalize),
    DECLARE_NODE_API_PROPERTY("checkExternal", CheckExternal),
    DECLARE_NODE_API_PROPERTY("createReference", CreateReference),
    DECLARE_NODE_API_PROPERTY("deleteReference", DeleteReference),
    DECLARE_NODE_API_PROPERTY("incrementRefcount", IncrementRefcount),
    DECLARE_NODE_API_PROPERTY("decrementRefcount", DecrementRefcount),
    DECLARE_NODE_API_GETTER("referenceValue", GetReferenceValue),
    DECLARE_NODE_API_PROPERTY("validateDeleteBeforeFinalize",
                          ValidateDeleteBeforeFinalize),
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
