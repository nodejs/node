#define NODE_API_EXPERIMENTAL
#include <js_native_api.h>
#include "../common.h"
#include <string.h>
#include "test_null.h"

static int test_value = 3;

static node_api_value Get(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 2, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_object,
      "Wrong type of arguments. Expects an object as first argument.");

  node_api_valuetype valuetype1;
  NODE_API_CALL(env, node_api_typeof(env, args[1], &valuetype1));

  NODE_API_ASSERT(env,
      valuetype1 == node_api_string || valuetype1 == node_api_symbol,
      "Wrong type of arguments. Expects a string or symbol as second.");

  node_api_value object = args[0];
  node_api_value output;
  NODE_API_CALL(env, node_api_get_property(env, object, args[1], &output));

  return output;
}

static node_api_value GetNamed(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];
  char key[256] = "";
  size_t key_length;
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 2, "Wrong number of arguments");

  node_api_valuetype value_type0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &value_type0));

  NODE_API_ASSERT(env, value_type0 == node_api_object,
      "Wrong type of arguments. Expects an object as first argument.");

  node_api_valuetype value_type1;
  NODE_API_CALL(env, node_api_typeof(env, args[1], &value_type1));

  NODE_API_ASSERT(env, value_type1 == node_api_string,
      "Wrong type of arguments. Expects a string as second.");

  node_api_value object = args[0];
  NODE_API_CALL(env,
      node_api_get_value_string_utf8(env, args[1], key, 255, &key_length));
  key[255] = 0;
  NODE_API_ASSERT(env, key_length <= 255,
      "Cannot accommodate keys longer than 255 bytes");
  node_api_value output;
  NODE_API_CALL(env, node_api_get_named_property(env, object, key, &output));

  return output;
}

static node_api_value
GetPropertyNames(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype value_type0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &value_type0));

  NODE_API_ASSERT(env, value_type0 == node_api_object,
      "Wrong type of arguments. Expects an object as first argument.");

  node_api_value output;
  NODE_API_CALL(env, node_api_get_property_names(env, args[0], &output));

  return output;
}

static node_api_value
GetSymbolNames(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype value_type0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &value_type0));

  NODE_API_ASSERT(env,
              value_type0 == node_api_object,
              "Wrong type of arguments. Expects an object as first argument.");

  node_api_value output;
  NODE_API_CALL(env,
            node_api_get_all_property_names(
                env,
                args[0],
                node_api_key_include_prototypes,
                node_api_key_skip_strings,
                node_api_key_numbers_to_strings,
                &output));

  return output;
}

static node_api_value Set(node_api_env env, node_api_callback_info info) {
  size_t argc = 3;
  node_api_value args[3];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 3, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_object,
      "Wrong type of arguments. Expects an object as first argument.");

  node_api_valuetype valuetype1;
  NODE_API_CALL(env, node_api_typeof(env, args[1], &valuetype1));

  NODE_API_ASSERT(env,
      valuetype1 == node_api_string || valuetype1 == node_api_symbol,
      "Wrong type of arguments. Expects a string or symbol as second.");

  NODE_API_CALL(env, node_api_set_property(env, args[0], args[1], args[2]));

  node_api_value valuetrue;
  NODE_API_CALL(env, node_api_get_boolean(env, true, &valuetrue));

  return valuetrue;
}

static node_api_value SetNamed(node_api_env env, node_api_callback_info info) {
  size_t argc = 3;
  node_api_value args[3];
  char key[256] = "";
  size_t key_length;
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 3, "Wrong number of arguments");

  node_api_valuetype value_type0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &value_type0));

  NODE_API_ASSERT(env, value_type0 == node_api_object,
      "Wrong type of arguments. Expects an object as first argument.");

  node_api_valuetype value_type1;
  NODE_API_CALL(env, node_api_typeof(env, args[1], &value_type1));

  NODE_API_ASSERT(env, value_type1 == node_api_string,
      "Wrong type of arguments. Expects a string as second.");

  NODE_API_CALL(env,
      node_api_get_value_string_utf8(env, args[1], key, 255, &key_length));
  key[255] = 0;
  NODE_API_ASSERT(env, key_length <= 255,
      "Cannot accommodate keys longer than 255 bytes");

  NODE_API_CALL(env, node_api_set_named_property(env, args[0], key, args[2]));

  node_api_value value_true;
  NODE_API_CALL(env, node_api_get_boolean(env, true, &value_true));

  return value_true;
}

static node_api_value Has(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 2, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_object,
      "Wrong type of arguments. Expects an object as first argument.");

  node_api_valuetype valuetype1;
  NODE_API_CALL(env, node_api_typeof(env, args[1], &valuetype1));

  NODE_API_ASSERT(env,
      valuetype1 == node_api_string || valuetype1 == node_api_symbol,
      "Wrong type of arguments. Expects a string or symbol as second.");

  bool has_property;
  NODE_API_CALL(env,
      node_api_has_property(env, args[0], args[1], &has_property));

  node_api_value ret;
  NODE_API_CALL(env, node_api_get_boolean(env, has_property, &ret));

  return ret;
}

static node_api_value HasNamed(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];
  char key[256] = "";
  size_t key_length;
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 2, "Wrong number of arguments");

  node_api_valuetype value_type0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &value_type0));

  NODE_API_ASSERT(env, value_type0 == node_api_object,
      "Wrong type of arguments. Expects an object as first argument.");

  node_api_valuetype value_type1;
  NODE_API_CALL(env, node_api_typeof(env, args[1], &value_type1));

  NODE_API_ASSERT(env,
      value_type1 == node_api_string || value_type1 == node_api_symbol,
      "Wrong type of arguments. Expects a string as second.");

  NODE_API_CALL(env,
      node_api_get_value_string_utf8(env, args[1], key, 255, &key_length));
  key[255] = 0;
  NODE_API_ASSERT(env, key_length <= 255,
      "Cannot accommodate keys longer than 255 bytes");

  bool has_property;
  NODE_API_CALL(env,
      node_api_has_named_property(env, args[0], key, &has_property));

  node_api_value ret;
  NODE_API_CALL(env, node_api_get_boolean(env, has_property, &ret));

  return ret;
}

static node_api_value HasOwn(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 2, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_object,
      "Wrong type of arguments. Expects an object as first argument.");

  // node_api_valuetype valuetype1;
  // NODE_API_CALL(env, node_api_typeof(env, args[1], &valuetype1));
  //
  // NODE_API_ASSERT(env,
  //   valuetype1 == node_api_string || valuetype1 == node_api_symbol,
  //   "Wrong type of arguments. Expects a string or symbol as second.");

  bool has_property;
  NODE_API_CALL(env,
      node_api_has_own_property(env, args[0], args[1], &has_property));

  node_api_value ret;
  NODE_API_CALL(env, node_api_get_boolean(env, has_property, &ret));

  return ret;
}

static node_api_value Delete(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];

  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));
  NODE_API_ASSERT(env, argc == 2, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));
  NODE_API_ASSERT(env, valuetype0 == node_api_object,
      "Wrong type of arguments. Expects an object as first argument.");

  node_api_valuetype valuetype1;
  NODE_API_CALL(env, node_api_typeof(env, args[1], &valuetype1));
  NODE_API_ASSERT(env,
      valuetype1 == node_api_string || valuetype1 == node_api_symbol,
      "Wrong type of arguments. Expects a string or symbol as second.");

  bool result;
  node_api_value ret;
  NODE_API_CALL(env, node_api_delete_property(env, args[0], args[1], &result));
  NODE_API_CALL(env, node_api_get_boolean(env, result, &ret));

  return ret;
}

static node_api_value New(node_api_env env, node_api_callback_info info) {
  node_api_value ret;
  NODE_API_CALL(env, node_api_create_object(env, &ret));

  node_api_value num;
  NODE_API_CALL(env, node_api_create_int32(env, 987654321, &num));

  NODE_API_CALL(env,
      node_api_set_named_property(env, ret, "test_number", num));

  node_api_value str;
  const char* str_val = "test string";
  size_t str_len = strlen(str_val);
  NODE_API_CALL(env, node_api_create_string_utf8(env, str_val, str_len, &str));

  NODE_API_CALL(env,
      node_api_set_named_property(env, ret, "test_string", str));

  return ret;
}

static node_api_value Inflate(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_object,
      "Wrong type of arguments. Expects an object as first argument.");

  node_api_value obj = args[0];
  node_api_value propertynames;
  NODE_API_CALL(env, node_api_get_property_names(env, obj, &propertynames));

  uint32_t i, length;
  NODE_API_CALL(env, node_api_get_array_length(env, propertynames, &length));

  for (i = 0; i < length; i++) {
    node_api_value property_str;
    NODE_API_CALL(env,
        node_api_get_element(env, propertynames, i, &property_str));

    node_api_value value;
    NODE_API_CALL(env, node_api_get_property(env, obj, property_str, &value));

    double double_val;
    NODE_API_CALL(env, node_api_get_value_double(env, value, &double_val));
    NODE_API_CALL(env, node_api_create_double(env, double_val + 1, &value));
    NODE_API_CALL(env, node_api_set_property(env, obj, property_str, value));
  }

  return obj;
}

static node_api_value Wrap(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value arg;
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, &arg, NULL, NULL));

  NODE_API_CALL(env, node_api_wrap(env, arg, &test_value, NULL, NULL, NULL));
  return NULL;
}

static node_api_value Unwrap(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value arg;
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, &arg, NULL, NULL));

  void* data;
  NODE_API_CALL(env, node_api_unwrap(env, arg, &data));

  bool is_expected = (data != NULL && *(int*)data == 3);
  node_api_value result;
  NODE_API_CALL(env, node_api_get_boolean(env, is_expected, &result));
  return result;
}

static node_api_value
TestSetProperty(node_api_env env, node_api_callback_info info) {
  node_api_status status;
  node_api_value object, key, value;

  NODE_API_CALL(env, node_api_create_object(env, &object));

  NODE_API_CALL(env,
      node_api_create_string_utf8(env, "", NODE_API_AUTO_LENGTH, &key));

  NODE_API_CALL(env, node_api_create_object(env, &value));

  status = node_api_set_property(NULL, object, key, value);

  add_returned_status(env,
                      "envIsNull",
                      object,
                      "Invalid argument",
                      node_api_invalid_arg,
                      status);

  node_api_set_property(env, NULL, key, value);

  add_last_status(env, "objectIsNull", object);

  node_api_set_property(env, object, NULL, value);

  add_last_status(env, "keyIsNull", object);

  node_api_set_property(env, object, key, NULL);

  add_last_status(env, "valueIsNull", object);

  return object;
}

static node_api_value
TestHasProperty(node_api_env env, node_api_callback_info info) {
  node_api_status status;
  node_api_value object, key;
  bool result;

  NODE_API_CALL(env, node_api_create_object(env, &object));

  NODE_API_CALL(env,
      node_api_create_string_utf8(env, "", NODE_API_AUTO_LENGTH, &key));

  status = node_api_has_property(NULL, object, key, &result);

  add_returned_status(env,
                      "envIsNull",
                      object,
                      "Invalid argument",
                      node_api_invalid_arg,
                      status);

  node_api_has_property(env, NULL, key, &result);

  add_last_status(env, "objectIsNull", object);

  node_api_has_property(env, object, NULL, &result);

  add_last_status(env, "keyIsNull", object);

  node_api_has_property(env, object, key, NULL);

  add_last_status(env, "resultIsNull", object);

  return object;
}

static node_api_value
TestGetProperty(node_api_env env, node_api_callback_info info) {
  node_api_status status;
  node_api_value object, key, result;

  NODE_API_CALL(env, node_api_create_object(env, &object));

  NODE_API_CALL(env,
      node_api_create_string_utf8(env, "", NODE_API_AUTO_LENGTH, &key));

  NODE_API_CALL(env, node_api_create_object(env, &result));

  status = node_api_get_property(NULL, object, key, &result);

  add_returned_status(env,
                      "envIsNull",
                      object,
                      "Invalid argument",
                      node_api_invalid_arg,
                      status);

  node_api_get_property(env, NULL, key, &result);

  add_last_status(env, "objectIsNull", object);

  node_api_get_property(env, object, NULL, &result);

  add_last_status(env, "keyIsNull", object);

  node_api_get_property(env, object, key, NULL);

  add_last_status(env, "resultIsNull", object);

  return object;
}

static node_api_value
TestFreeze(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  node_api_value object = args[0];
  NODE_API_CALL(env, node_api_object_freeze(env, object));

  return object;
}

static node_api_value TestSeal(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  node_api_value object = args[0];
  NODE_API_CALL(env, node_api_object_seal(env, object));

  return object;
}

// We create two type tags. They are basically 128-bit UUIDs.
static const node_api_type_tag type_tags[2] = {
  { 0xdaf987b3cc62481a, 0xb745b0497f299531 },
  { 0xbb7936c374084d9b, 0xa9548d0762eeedb9 }
};

static node_api_value
TypeTaggedInstance(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  uint32_t type_index;
  node_api_value instance, which_type;

  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, &which_type, NULL, NULL));
  NODE_API_CALL(env, node_api_get_value_uint32(env, which_type, &type_index));
  NODE_API_CALL(env, node_api_create_object(env, &instance));
  NODE_API_CALL(env,
      node_api_type_tag_object(env, instance, &type_tags[type_index]));

  return instance;
}

static node_api_value
CheckTypeTag(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  bool result;
  node_api_value argv[2], js_result;
  uint32_t type_index;

  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, argv, NULL, NULL));
  NODE_API_CALL(env, node_api_get_value_uint32(env, argv[0], &type_index));
  NODE_API_CALL(env, node_api_check_object_type_tag(env,
                                                    argv[1],
                                                    &type_tags[type_index],
                                                    &result));
  NODE_API_CALL(env, node_api_get_boolean(env, result, &js_result));

  return js_result;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("Get", Get),
    DECLARE_NODE_API_PROPERTY("GetNamed", GetNamed),
    DECLARE_NODE_API_PROPERTY("GetPropertyNames", GetPropertyNames),
    DECLARE_NODE_API_PROPERTY("GetSymbolNames", GetSymbolNames),
    DECLARE_NODE_API_PROPERTY("Set", Set),
    DECLARE_NODE_API_PROPERTY("SetNamed", SetNamed),
    DECLARE_NODE_API_PROPERTY("Has", Has),
    DECLARE_NODE_API_PROPERTY("HasNamed", HasNamed),
    DECLARE_NODE_API_PROPERTY("HasOwn", HasOwn),
    DECLARE_NODE_API_PROPERTY("Delete", Delete),
    DECLARE_NODE_API_PROPERTY("New", New),
    DECLARE_NODE_API_PROPERTY("Inflate", Inflate),
    DECLARE_NODE_API_PROPERTY("Wrap", Wrap),
    DECLARE_NODE_API_PROPERTY("Unwrap", Unwrap),
    DECLARE_NODE_API_PROPERTY("TestSetProperty", TestSetProperty),
    DECLARE_NODE_API_PROPERTY("TestHasProperty", TestHasProperty),
    DECLARE_NODE_API_PROPERTY("TypeTaggedInstance", TypeTaggedInstance),
    DECLARE_NODE_API_PROPERTY("CheckTypeTag", CheckTypeTag),
    DECLARE_NODE_API_PROPERTY("TestGetProperty", TestGetProperty),
    DECLARE_NODE_API_PROPERTY("TestFreeze", TestFreeze),
    DECLARE_NODE_API_PROPERTY("TestSeal", TestSeal),
  };

  init_test_null(env, exports);

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
