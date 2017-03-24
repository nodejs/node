#include <node_api.h>
#include <string.h>
#include "../common.h"

napi_value Test(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc >= 2, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NAPI_ASSERT(env, valuetype0 == napi_object,
    "Wrong type of arguments. Expects an array as first argument.");

  napi_valuetype valuetype1;
  NAPI_CALL(env, napi_typeof(env, args[1], &valuetype1));

  NAPI_ASSERT(env, valuetype1 == napi_number,
    "Wrong type of arguments. Expects an integer as second argument.");

  napi_value array = args[0];
  int32_t index;
  NAPI_CALL(env, napi_get_value_int32(env, args[1], &index));

  NAPI_ASSERT(env, index >= 0, "Invalid index. Expects a positive integer.");

  bool isarray;
  NAPI_CALL(env, napi_is_array(env, array, &isarray));

  if (!isarray) {
    return NULL;
  }

  uint32_t length;
  NAPI_CALL(env, napi_get_array_length(env, array, &length));

  if ((uint32_t)index >= length) {
    napi_value str;
    const char* str_val = "Index out of bound!";
    size_t str_len = strlen(str_val);
    NAPI_CALL(env, napi_create_string_utf8(env, str_val, str_len, &str));

    return str;
  }

  napi_value ret;
  NAPI_CALL(env, napi_get_element(env, array, index, &ret));

  return ret;
}

napi_value New(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc >= 1, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NAPI_ASSERT(env, valuetype0 == napi_object,
    "Wrong type of arguments. Expects an array as first argument.");

  napi_value ret;
  NAPI_CALL(env, napi_create_array(env, &ret));

  uint32_t i, length;
  NAPI_CALL(env, napi_get_array_length(env, args[0], &length));

  for (i = 0; i < length; i++) {
    napi_value e;
    NAPI_CALL(env, napi_get_element(env, args[0], i, &e));
    NAPI_CALL(env, napi_set_element(env, ret, i, e));
  }

  return ret;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("Test", Test),
    DECLARE_NAPI_PROPERTY("New", New),
  };

  NAPI_CALL_RETURN_VOID(env, napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));
}

NAPI_MODULE(addon, Init)
