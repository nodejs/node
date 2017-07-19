#include <node_api.h>
#include "../common.h"
#include <string.h>
#include <stdlib.h>

static int test_value = 3;

napi_value Get(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc >= 2, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NAPI_ASSERT(env, valuetype0 == napi_object,
    "Wrong type of arguments. Expects an object as first argument.");

  napi_valuetype valuetype1;
  NAPI_CALL(env, napi_typeof(env, args[1], &valuetype1));

  NAPI_ASSERT(env, valuetype1 == napi_string || valuetype1 == napi_symbol,
    "Wrong type of arguments. Expects a string or symbol as second.");

  napi_value object = args[0];
  napi_value output;
  NAPI_CALL(env, napi_get_property(env, object, args[1], &output));

  return output;
}

napi_value Set(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value args[3];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc >= 3, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NAPI_ASSERT(env, valuetype0 == napi_object,
    "Wrong type of arguments. Expects an object as first argument.");

  napi_valuetype valuetype1;
  NAPI_CALL(env, napi_typeof(env, args[1], &valuetype1));

  NAPI_ASSERT(env, valuetype1 == napi_string || valuetype1 == napi_symbol,
    "Wrong type of arguments. Expects a string or symbol as second.");

  NAPI_CALL(env, napi_set_property(env, args[0], args[1], args[2]));

  napi_value valuetrue;
  NAPI_CALL(env, napi_get_boolean(env, true, &valuetrue));

  return valuetrue;
}

napi_value Has(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc >= 2, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NAPI_ASSERT(env, valuetype0 == napi_object,
    "Wrong type of arguments. Expects an object as first argument.");

  napi_valuetype valuetype1;
  NAPI_CALL(env, napi_typeof(env, args[1], &valuetype1));

  NAPI_ASSERT(env, valuetype1 == napi_string || valuetype1 == napi_symbol,
    "Wrong type of arguments. Expects a string or symbol as second.");

  bool has_property;
  NAPI_CALL(env, napi_has_property(env, args[0], args[1], &has_property));

  napi_value ret;
  NAPI_CALL(env, napi_get_boolean(env, has_property, &ret));

  return ret;
}

napi_value HasOwn(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc == 2, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NAPI_ASSERT(env, valuetype0 == napi_object,
    "Wrong type of arguments. Expects an object as first argument.");

  // napi_valuetype valuetype1;
  // NAPI_CALL(env, napi_typeof(env, args[1], &valuetype1));
  //
  // NAPI_ASSERT(env, valuetype1 == napi_string || valuetype1 == napi_symbol,
  //   "Wrong type of arguments. Expects a string or symbol as second.");

  bool has_property;
  NAPI_CALL(env, napi_has_own_property(env, args[0], args[1], &has_property));

  napi_value ret;
  NAPI_CALL(env, napi_get_boolean(env, has_property, &ret));

  return ret;
}

napi_value Delete(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];

  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));
  NAPI_ASSERT(env, argc == 2, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));
  NAPI_ASSERT(env, valuetype0 == napi_object,
    "Wrong type of arguments. Expects an object as first argument.");

  napi_valuetype valuetype1;
  NAPI_CALL(env, napi_typeof(env, args[1], &valuetype1));
  NAPI_ASSERT(env, valuetype1 == napi_string || valuetype1 == napi_symbol,
    "Wrong type of arguments. Expects a string or symbol as second.");

  bool result;
  napi_value ret;
  NAPI_CALL(env, napi_delete_property(env, args[0], args[1], &result));
  NAPI_CALL(env, napi_get_boolean(env, result, &ret));

  return ret;
}

napi_value New(napi_env env, napi_callback_info info) {
  napi_value ret;
  NAPI_CALL(env, napi_create_object(env, &ret));

  napi_value num;
  NAPI_CALL(env, napi_create_int32(env, 987654321, &num));

  NAPI_CALL(env, napi_set_named_property(env, ret, "test_number", num));

  napi_value str;
  const char* str_val = "test string";
  size_t str_len = strlen(str_val);
  NAPI_CALL(env, napi_create_string_utf8(env, str_val, str_len, &str));

  NAPI_CALL(env, napi_set_named_property(env, ret, "test_string", str));

  return ret;
}

napi_value Inflate(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc >= 1, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NAPI_ASSERT(env, valuetype0 == napi_object,
    "Wrong type of arguments. Expects an object as first argument.");

  napi_value obj = args[0];
  napi_value propertynames;
  NAPI_CALL(env, napi_get_property_names(env, obj, &propertynames));

  uint32_t i, length;
  NAPI_CALL(env, napi_get_array_length(env, propertynames, &length));

  for (i = 0; i < length; i++) {
    napi_value property_str;
    NAPI_CALL(env, napi_get_element(env, propertynames, i, &property_str));

    napi_value value;
    NAPI_CALL(env, napi_get_property(env, obj, property_str, &value));

    double double_val;
    NAPI_CALL(env, napi_get_value_double(env, value, &double_val));
    NAPI_CALL(env, napi_create_double(env, double_val + 1, &value));
    NAPI_CALL(env, napi_set_property(env, obj, property_str, value));
  }

  return obj;
}

napi_value Wrap(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value arg;
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &arg, NULL, NULL));

  int32_t* data = malloc(sizeof(int32_t));
  *data = test_value;
  NAPI_CALL(env, napi_wrap(env, arg, data, NULL, NULL, NULL));
  return NULL;
}

napi_value Unwrap(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value arg;
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &arg, NULL, NULL));

  void* data;
  NAPI_CALL(env, napi_unwrap(env, arg, &data));

  bool is_expected = (data != NULL && *(int*)data == 3);
  napi_value result;
  NAPI_CALL(env, napi_get_boolean(env, is_expected, &result));
  return result;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("Get", Get),
    DECLARE_NAPI_PROPERTY("Set", Set),
    DECLARE_NAPI_PROPERTY("Has", Has),
    DECLARE_NAPI_PROPERTY("HasOwn", HasOwn),
    DECLARE_NAPI_PROPERTY("Delete", Delete),
    DECLARE_NAPI_PROPERTY("New", New),
    DECLARE_NAPI_PROPERTY("Inflate", Inflate),
    DECLARE_NAPI_PROPERTY("Wrap", Wrap),
    DECLARE_NAPI_PROPERTY("Unwrap", Unwrap),
  };

  NAPI_CALL_RETURN_VOID(env, napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));
}

NAPI_MODULE(addon, Init)
