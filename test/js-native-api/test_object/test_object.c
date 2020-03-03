#include <js_native_api.h>
#include "../common.h"
#include <string.h>
#include "test_null.h"

static int test_value = 3;

static napi_value Get(napi_env env, napi_callback_info info) {
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

static napi_value GetNamed(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  char key[256] = "";
  size_t key_length;
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc >= 2, "Wrong number of arguments");

  napi_valuetype value_type0;
  NAPI_CALL(env, napi_typeof(env, args[0], &value_type0));

  NAPI_ASSERT(env, value_type0 == napi_object,
      "Wrong type of arguments. Expects an object as first argument.");

  napi_valuetype value_type1;
  NAPI_CALL(env, napi_typeof(env, args[1], &value_type1));

  NAPI_ASSERT(env, value_type1 == napi_string,
      "Wrong type of arguments. Expects a string as second.");

  napi_value object = args[0];
  NAPI_CALL(env,
      napi_get_value_string_utf8(env, args[1], key, 255, &key_length));
  key[255] = 0;
  NAPI_ASSERT(env, key_length <= 255,
      "Cannot accommodate keys longer than 255 bytes");
  napi_value output;
  NAPI_CALL(env, napi_get_named_property(env, object, key, &output));

  return output;
}

static napi_value GetPropertyNames(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc >= 1, "Wrong number of arguments");

  napi_valuetype value_type0;
  NAPI_CALL(env, napi_typeof(env, args[0], &value_type0));

  NAPI_ASSERT(env, value_type0 == napi_object,
      "Wrong type of arguments. Expects an object as first argument.");

  napi_value output;
  NAPI_CALL(env, napi_get_property_names(env, args[0], &output));

  return output;
}

static napi_value GetSymbolNames(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc >= 1, "Wrong number of arguments");

  napi_valuetype value_type0;
  NAPI_CALL(env, napi_typeof(env, args[0], &value_type0));

  NAPI_ASSERT(env,
              value_type0 == napi_object,
              "Wrong type of arguments. Expects an object as first argument.");

  napi_value output;
  NAPI_CALL(env,
            napi_get_all_property_names(
                env,
                args[0],
                napi_key_include_prototypes,
                napi_key_skip_strings,
                napi_key_numbers_to_strings,
                &output));

  return output;
}

static napi_value Set(napi_env env, napi_callback_info info) {
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

static napi_value SetNamed(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value args[3];
  char key[256] = "";
  size_t key_length;
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc >= 3, "Wrong number of arguments");

  napi_valuetype value_type0;
  NAPI_CALL(env, napi_typeof(env, args[0], &value_type0));

  NAPI_ASSERT(env, value_type0 == napi_object,
      "Wrong type of arguments. Expects an object as first argument.");

  napi_valuetype value_type1;
  NAPI_CALL(env, napi_typeof(env, args[1], &value_type1));

  NAPI_ASSERT(env, value_type1 == napi_string,
      "Wrong type of arguments. Expects a string as second.");

  NAPI_CALL(env,
      napi_get_value_string_utf8(env, args[1], key, 255, &key_length));
  key[255] = 0;
  NAPI_ASSERT(env, key_length <= 255,
      "Cannot accommodate keys longer than 255 bytes");

  NAPI_CALL(env, napi_set_named_property(env, args[0], key, args[2]));

  napi_value value_true;
  NAPI_CALL(env, napi_get_boolean(env, true, &value_true));

  return value_true;
}

static napi_value Has(napi_env env, napi_callback_info info) {
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

static napi_value HasNamed(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  char key[256] = "";
  size_t key_length;
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc >= 2, "Wrong number of arguments");

  napi_valuetype value_type0;
  NAPI_CALL(env, napi_typeof(env, args[0], &value_type0));

  NAPI_ASSERT(env, value_type0 == napi_object,
      "Wrong type of arguments. Expects an object as first argument.");

  napi_valuetype value_type1;
  NAPI_CALL(env, napi_typeof(env, args[1], &value_type1));

  NAPI_ASSERT(env, value_type1 == napi_string || value_type1 == napi_symbol,
      "Wrong type of arguments. Expects a string as second.");

  NAPI_CALL(env,
      napi_get_value_string_utf8(env, args[1], key, 255, &key_length));
  key[255] = 0;
  NAPI_ASSERT(env, key_length <= 255,
      "Cannot accommodate keys longer than 255 bytes");

  bool has_property;
  NAPI_CALL(env, napi_has_named_property(env, args[0], key, &has_property));

  napi_value ret;
  NAPI_CALL(env, napi_get_boolean(env, has_property, &ret));

  return ret;
}

static napi_value HasOwn(napi_env env, napi_callback_info info) {
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

static napi_value Delete(napi_env env, napi_callback_info info) {
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

static napi_value New(napi_env env, napi_callback_info info) {
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

static napi_value Inflate(napi_env env, napi_callback_info info) {
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

static napi_value Wrap(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value arg;
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &arg, NULL, NULL));

  NAPI_CALL(env, napi_wrap(env, arg, &test_value, NULL, NULL, NULL));
  return NULL;
}

static napi_value Unwrap(napi_env env, napi_callback_info info) {
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

static napi_value TestSetProperty(napi_env env,
                                  napi_callback_info info) {
  napi_status status;
  napi_value object, key, value;

  NAPI_CALL(env, napi_create_object(env, &object));

  NAPI_CALL(env, napi_create_string_utf8(env, "", NAPI_AUTO_LENGTH, &key));

  NAPI_CALL(env, napi_create_object(env, &value));

  status = napi_set_property(NULL, object, key, value);

  add_returned_status(env,
                      "envIsNull",
                      object,
                      "Invalid argument",
                      napi_invalid_arg,
                      status);

  napi_set_property(env, NULL, key, value);

  add_last_status(env, "objectIsNull", object);

  napi_set_property(env, object, NULL, value);

  add_last_status(env, "keyIsNull", object);

  napi_set_property(env, object, key, NULL);

  add_last_status(env, "valueIsNull", object);

  return object;
}

static napi_value TestHasProperty(napi_env env,
                                  napi_callback_info info) {
  napi_status status;
  napi_value object, key;
  bool result;

  NAPI_CALL(env, napi_create_object(env, &object));

  NAPI_CALL(env, napi_create_string_utf8(env, "", NAPI_AUTO_LENGTH, &key));

  status = napi_has_property(NULL, object, key, &result);

  add_returned_status(env,
                      "envIsNull",
                      object,
                      "Invalid argument",
                      napi_invalid_arg,
                      status);

  napi_has_property(env, NULL, key, &result);

  add_last_status(env, "objectIsNull", object);

  napi_has_property(env, object, NULL, &result);

  add_last_status(env, "keyIsNull", object);

  napi_has_property(env, object, key, NULL);

  add_last_status(env, "resultIsNull", object);

  return object;
}

static napi_value TestGetProperty(napi_env env,
                                  napi_callback_info info) {
  napi_status status;
  napi_value object, key, result;

  NAPI_CALL(env, napi_create_object(env, &object));

  NAPI_CALL(env, napi_create_string_utf8(env, "", NAPI_AUTO_LENGTH, &key));

  NAPI_CALL(env, napi_create_object(env, &result));

  status = napi_get_property(NULL, object, key, &result);

  add_returned_status(env,
                      "envIsNull",
                      object,
                      "Invalid argument",
                      napi_invalid_arg,
                      status);

  napi_get_property(env, NULL, key, &result);

  add_last_status(env, "objectIsNull", object);

  napi_get_property(env, object, NULL, &result);

  add_last_status(env, "keyIsNull", object);

  napi_get_property(env, object, key, NULL);

  add_last_status(env, "resultIsNull", object);

  return object;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("Get", Get),
    DECLARE_NAPI_PROPERTY("GetNamed", GetNamed),
    DECLARE_NAPI_PROPERTY("GetPropertyNames", GetPropertyNames),
    DECLARE_NAPI_PROPERTY("GetSymbolNames", GetSymbolNames),
    DECLARE_NAPI_PROPERTY("Set", Set),
    DECLARE_NAPI_PROPERTY("SetNamed", SetNamed),
    DECLARE_NAPI_PROPERTY("Has", Has),
    DECLARE_NAPI_PROPERTY("HasNamed", HasNamed),
    DECLARE_NAPI_PROPERTY("HasOwn", HasOwn),
    DECLARE_NAPI_PROPERTY("Delete", Delete),
    DECLARE_NAPI_PROPERTY("New", New),
    DECLARE_NAPI_PROPERTY("Inflate", Inflate),
    DECLARE_NAPI_PROPERTY("Wrap", Wrap),
    DECLARE_NAPI_PROPERTY("Unwrap", Unwrap),
    DECLARE_NAPI_PROPERTY("TestSetProperty", TestSetProperty),
    DECLARE_NAPI_PROPERTY("TestHasProperty", TestHasProperty),
    DECLARE_NAPI_PROPERTY("TestGetProperty", TestGetProperty),
  };

  init_test_null(env, exports);

  NAPI_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
