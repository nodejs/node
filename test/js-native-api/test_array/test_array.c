#include <js_native_api.h>
#include <string.h>
#include "../common.h"

static node_api_value
TestGetElement(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 2, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_object,
      "Wrong type of arguments. Expects an array as first argument.");

  node_api_valuetype valuetype1;
  NODE_API_CALL(env, node_api_typeof(env, args[1], &valuetype1));

  NODE_API_ASSERT(env, valuetype1 == node_api_number,
      "Wrong type of arguments. Expects an integer as second argument.");

  node_api_value array = args[0];
  int32_t index;
  NODE_API_CALL(env, node_api_get_value_int32(env, args[1], &index));

  NODE_API_ASSERT(env, index >= 0, "Invalid index. Expects a positive integer.");

  bool isarray;
  NODE_API_CALL(env, node_api_is_array(env, array, &isarray));

  if (!isarray) {
    return NULL;
  }

  uint32_t length;
  NODE_API_CALL(env, node_api_get_array_length(env, array, &length));

  NODE_API_ASSERT(env, ((uint32_t)index < length), "Index out of bounds!");

  node_api_value ret;
  NODE_API_CALL(env, node_api_get_element(env, array, index, &ret));

  return ret;
}

static node_api_value
TestHasElement(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 2, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_object,
      "Wrong type of arguments. Expects an array as first argument.");

  node_api_valuetype valuetype1;
  NODE_API_CALL(env, node_api_typeof(env, args[1], &valuetype1));

  NODE_API_ASSERT(env, valuetype1 == node_api_number,
      "Wrong type of arguments. Expects an integer as second argument.");

  node_api_value array = args[0];
  int32_t index;
  NODE_API_CALL(env, node_api_get_value_int32(env, args[1], &index));

  bool isarray;
  NODE_API_CALL(env, node_api_is_array(env, array, &isarray));

  if (!isarray) {
    return NULL;
  }

  bool has_element;
  NODE_API_CALL(env, node_api_has_element(env, array, index, &has_element));

  node_api_value ret;
  NODE_API_CALL(env, node_api_get_boolean(env, has_element, &ret));

  return ret;
}

static node_api_value
TestDeleteElement(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];

  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));
  NODE_API_ASSERT(env, argc == 2, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));
  NODE_API_ASSERT(env, valuetype0 == node_api_object,
      "Wrong type of arguments. Expects an array as first argument.");

  node_api_valuetype valuetype1;
  NODE_API_CALL(env, node_api_typeof(env, args[1], &valuetype1));
  NODE_API_ASSERT(env, valuetype1 == node_api_number,
      "Wrong type of arguments. Expects an integer as second argument.");

  node_api_value array = args[0];
  int32_t index;
  bool result;
  node_api_value ret;

  NODE_API_CALL(env, node_api_get_value_int32(env, args[1], &index));
  NODE_API_CALL(env, node_api_is_array(env, array, &result));

  if (!result) {
    return NULL;
  }

  NODE_API_CALL(env, node_api_delete_element(env, array, index, &result));
  NODE_API_CALL(env, node_api_get_boolean(env, result, &ret));

  return ret;
}

static node_api_value New(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_object,
      "Wrong type of arguments. Expects an array as first argument.");

  node_api_value ret;
  NODE_API_CALL(env, node_api_create_array(env, &ret));

  uint32_t i, length;
  NODE_API_CALL(env, node_api_get_array_length(env, args[0], &length));

  for (i = 0; i < length; i++) {
    node_api_value e;
    NODE_API_CALL(env, node_api_get_element(env, args[0], i, &e));
    NODE_API_CALL(env, node_api_set_element(env, ret, i, e));
  }

  return ret;
}

static node_api_value
NewWithLength(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_number,
      "Wrong type of arguments. Expects an integer the first argument.");

  int32_t array_length;
  NODE_API_CALL(env, node_api_get_value_int32(env, args[0], &array_length));

  node_api_value ret;
  NODE_API_CALL(env, node_api_create_array_with_length(env, array_length, &ret));

  return ret;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("TestGetElement", TestGetElement),
    DECLARE_NODE_API_PROPERTY("TestHasElement", TestHasElement),
    DECLARE_NODE_API_PROPERTY("TestDeleteElement", TestDeleteElement),
    DECLARE_NODE_API_PROPERTY("New", New),
    DECLARE_NODE_API_PROPERTY("NewWithLength", NewWithLength),
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
