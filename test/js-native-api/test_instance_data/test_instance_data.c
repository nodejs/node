#include <stdio.h>
#include <stdlib.h>
#include <js_native_api.h>
#include "../common.h"

typedef struct {
  size_t value;
  bool print;
  napi_ref js_cb_ref;
} AddonData;

static napi_value Increment(napi_env env, napi_callback_info info) {
  AddonData* data;
  napi_value result;

  NAPI_CALL(env, napi_get_instance_data(env, (void**)&data));
  NAPI_CALL(env, napi_create_uint32(env, ++data->value, &result));

  return result;
}

static void DeleteAddonData(napi_env env, void* raw_data, void* hint) {
  AddonData* data = raw_data;
  if (data->print) {
    printf("deleting addon data\n");
  }
  if (data->js_cb_ref != NULL) {
    NAPI_CALL_RETURN_VOID(env, napi_delete_reference(env, data->js_cb_ref));
  }
  free(data);
}

static napi_value SetPrintOnDelete(napi_env env, napi_callback_info info) {
  AddonData* data;

  NAPI_CALL(env, napi_get_instance_data(env, (void**)&data));
  data->print = true;

  return NULL;
}

static void TestFinalizer(napi_env env, void* raw_data, void* hint) {
  (void) raw_data;
  (void) hint;

  AddonData* data;
  NAPI_CALL_RETURN_VOID(env, napi_get_instance_data(env, (void**)&data));
  napi_value js_cb, undefined;
  NAPI_CALL_RETURN_VOID(env,
      napi_get_reference_value(env, data->js_cb_ref, &js_cb));
  NAPI_CALL_RETURN_VOID(env, napi_get_undefined(env, &undefined));
  NAPI_CALL_RETURN_VOID(env,
      napi_call_function(env, undefined, js_cb, 0, NULL, NULL));
  NAPI_CALL_RETURN_VOID(env, napi_delete_reference(env, data->js_cb_ref));
  data->js_cb_ref = NULL;
}

static napi_value ObjectWithFinalizer(napi_env env, napi_callback_info info) {
  AddonData* data;
  napi_value result, js_cb;
  size_t argc = 1;

  NAPI_CALL(env, napi_get_instance_data(env, (void**)&data));
  NAPI_ASSERT(env, data->js_cb_ref == NULL, "reference must be NULL");
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &js_cb, NULL, NULL));
  NAPI_CALL(env, napi_create_object(env, &result));
  NAPI_CALL(env,
      napi_add_finalizer(env, result, NULL, TestFinalizer, NULL, NULL));
  NAPI_CALL(env, napi_create_reference(env, js_cb, 1, &data->js_cb_ref));

  return result;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  AddonData* data = malloc(sizeof(*data));
  data->value = 41;
  data->print = false;
  data->js_cb_ref = NULL;

  NAPI_CALL(env, napi_set_instance_data(env, data, DeleteAddonData, NULL));

  napi_property_descriptor props[] = {
    DECLARE_NAPI_PROPERTY("increment", Increment),
    DECLARE_NAPI_PROPERTY("setPrintOnDelete", SetPrintOnDelete),
    DECLARE_NAPI_PROPERTY("objectWithFinalizer", ObjectWithFinalizer),
  };

  NAPI_CALL(env, napi_define_properties(env,
                                        exports,
                                        sizeof(props) / sizeof(*props),
                                        props));

  return exports;
}
EXTERN_C_END
