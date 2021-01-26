#include <stdio.h>
#include <stdlib.h>
#include <js_native_api.h>
#include "../common.h"

typedef struct {
  size_t value;
  bool print;
  node_api_ref js_cb_ref;
} AddonData;

static node_api_value
Increment(node_api_env env, node_api_callback_info info) {
  AddonData* data;
  node_api_value result;

  NODE_API_CALL(env, node_api_get_instance_data(env, (void**)&data));
  NODE_API_CALL(env, node_api_create_uint32(env, ++data->value, &result));

  return result;
}

static void DeleteAddonData(node_api_env env, void* raw_data, void* hint) {
  AddonData* data = raw_data;
  if (data->print) {
    printf("deleting addon data\n");
  }
  if (data->js_cb_ref != NULL) {
    NODE_API_CALL_RETURN_VOID(env,
        node_api_delete_reference(env, data->js_cb_ref));
  }
  free(data);
}

static node_api_value
SetPrintOnDelete(node_api_env env, node_api_callback_info info) {
  AddonData* data;

  NODE_API_CALL(env, node_api_get_instance_data(env, (void**)&data));
  data->print = true;

  return NULL;
}

static void TestFinalizer(node_api_env env, void* raw_data, void* hint) {
  (void) raw_data;
  (void) hint;

  AddonData* data;
  NODE_API_CALL_RETURN_VOID(env,
      node_api_get_instance_data(env, (void**)&data));
  node_api_value js_cb, undefined;
  NODE_API_CALL_RETURN_VOID(env,
      node_api_get_reference_value(env, data->js_cb_ref, &js_cb));
  NODE_API_CALL_RETURN_VOID(env, node_api_get_undefined(env, &undefined));
  NODE_API_CALL_RETURN_VOID(env,
      node_api_call_function(env, undefined, js_cb, 0, NULL, NULL));
  NODE_API_CALL_RETURN_VOID(env,
      node_api_delete_reference(env, data->js_cb_ref));
  data->js_cb_ref = NULL;
}

static node_api_value
ObjectWithFinalizer(node_api_env env, node_api_callback_info info) {
  AddonData* data;
  node_api_value result, js_cb;
  size_t argc = 1;

  NODE_API_CALL(env, node_api_get_instance_data(env, (void**)&data));
  NODE_API_ASSERT(env, data->js_cb_ref == NULL, "reference must be NULL");
  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, &js_cb, NULL, NULL));
  NODE_API_CALL(env, node_api_create_object(env, &result));
  NODE_API_CALL(env,
      node_api_add_finalizer(env, result, NULL, TestFinalizer, NULL, NULL));
  NODE_API_CALL(env,
      node_api_create_reference(env, js_cb, 1, &data->js_cb_ref));

  return result;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  AddonData* data = malloc(sizeof(*data));
  data->value = 41;
  data->print = false;
  data->js_cb_ref = NULL;

  NODE_API_CALL(env,
      node_api_set_instance_data(env, data, DeleteAddonData, NULL));

  node_api_property_descriptor props[] = {
    DECLARE_NODE_API_PROPERTY("increment", Increment),
    DECLARE_NODE_API_PROPERTY("setPrintOnDelete", SetPrintOnDelete),
    DECLARE_NODE_API_PROPERTY("objectWithFinalizer", ObjectWithFinalizer),
  };

  NODE_API_CALL(env, node_api_define_properties(env,
                                                exports,
                                                sizeof(props) / sizeof(*props),
                                                props));

  return exports;
}
EXTERN_C_END
