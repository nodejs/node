#include <js_native_api.h>
#include <string.h>
#include "../common.h"
#include "../entry_point.h"

static napi_value TestGetElement(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 2, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NODE_API_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == napi_object,
      "Wrong type of arguments. Expects an array as first argument.");

  napi_valuetype valuetype1;
  NODE_API_CALL(env, napi_typeof(env, args[1], &valuetype1));

  NODE_API_ASSERT(env, valuetype1 == napi_number,
      "Wrong type of arguments. Expects an integer as second argument.");

  napi_value array = args[0];
  int32_t index;
  NODE_API_CALL(env, napi_get_value_int32(env, args[1], &index));

  NODE_API_ASSERT(env, index >= 0, "Invalid index. Expects a positive integer.");

  bool isarray;
  NODE_API_CALL(env, napi_is_array(env, array, &isarray));

  if (!isarray) {
    return NULL;
  }

  uint32_t length;
  NODE_API_CALL(env, napi_get_array_length(env, array, &length));

  NODE_API_ASSERT(env, ((uint32_t)index < length), "Index out of bounds!");

  napi_value ret;
  NODE_API_CALL(env, napi_get_element(env, array, index, &ret));

  return ret;
}

static napi_value TestHasElement(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 2, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NODE_API_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == napi_object,
      "Wrong type of arguments. Expects an array as first argument.");

  napi_valuetype valuetype1;
  NODE_API_CALL(env, napi_typeof(env, args[1], &valuetype1));

  NODE_API_ASSERT(env, valuetype1 == napi_number,
      "Wrong type of arguments. Expects an integer as second argument.");

  napi_value array = args[0];
  int32_t index;
  NODE_API_CALL(env, napi_get_value_int32(env, args[1], &index));

  bool isarray;
  NODE_API_CALL(env, napi_is_array(env, array, &isarray));

  if (!isarray) {
    return NULL;
  }

  bool has_element;
  NODE_API_CALL(env, napi_has_element(env, array, index, &has_element));

  napi_value ret;
  NODE_API_CALL(env, napi_get_boolean(env, has_element, &ret));

  return ret;
}

static napi_value TestDeleteElement(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];

  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
  NODE_API_ASSERT(env, argc == 2, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NODE_API_CALL(env, napi_typeof(env, args[0], &valuetype0));
  NODE_API_ASSERT(env, valuetype0 == napi_object,
      "Wrong type of arguments. Expects an array as first argument.");

  napi_valuetype valuetype1;
  NODE_API_CALL(env, napi_typeof(env, args[1], &valuetype1));
  NODE_API_ASSERT(env, valuetype1 == napi_number,
      "Wrong type of arguments. Expects an integer as second argument.");

  napi_value array = args[0];
  int32_t index;
  bool result;
  napi_value ret;

  NODE_API_CALL(env, napi_get_value_int32(env, args[1], &index));
  NODE_API_CALL(env, napi_is_array(env, array, &result));

  if (!result) {
    return NULL;
  }

  NODE_API_CALL(env, napi_delete_element(env, array, index, &result));
  NODE_API_CALL(env, napi_get_boolean(env, result, &ret));

  return ret;
}

static napi_value New(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NODE_API_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == napi_object,
      "Wrong type of arguments. Expects an array as first argument.");

  napi_value ret;
  NODE_API_CALL(env, napi_create_array(env, &ret));

  uint32_t i, length;
  NODE_API_CALL(env, napi_get_array_length(env, args[0], &length));

  for (i = 0; i < length; i++) {
    napi_value e;
    NODE_API_CALL(env, napi_get_element(env, args[0], i, &e));
    NODE_API_CALL(env, napi_set_element(env, ret, i, e));
  }

  return ret;
}

static napi_value NewWithLength(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NODE_API_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == napi_number,
      "Wrong type of arguments. Expects an integer the first argument.");

  int32_t array_length;
  NODE_API_CALL(env, napi_get_value_int32(env, args[0], &array_length));

  napi_value ret;
  NODE_API_CALL(env, napi_create_array_with_length(env, array_length, &ret));

  return ret;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("TestGetElement", TestGetElement),
    DECLARE_NODE_API_PROPERTY("TestHasElement", TestHasElement),
    DECLARE_NODE_API_PROPERTY("TestDeleteElement", TestDeleteElement),
    DECLARE_NODE_API_PROPERTY("New", New),
    DECLARE_NODE_API_PROPERTY("NewWithLength", NewWithLength),
  };

  NODE_API_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
