#include <js_native_api.h>

#include "../common.h"
#include "test_null.h"

static node_api_value
SetProperty(node_api_env env, node_api_callback_info info) {
  node_api_value return_value, object, key;

  NODE_API_CALL(env, node_api_create_object(env, &return_value));
  NODE_API_CALL(env, node_api_create_object(env, &object));
  NODE_API_CALL(env,
      node_api_create_string_utf8(
          env, "someString", NODE_API_AUTO_LENGTH, &key));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      node_api_invalid_arg,
                      node_api_set_property(NULL, object, key, object));

  node_api_set_property(env, NULL, key, object);
  add_last_status(env, "objectIsNull", return_value);

  node_api_set_property(env, object, NULL, object);
  add_last_status(env, "keyIsNull", return_value);

  node_api_set_property(env, object, key, NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static node_api_value
GetProperty(node_api_env env, node_api_callback_info info) {
  node_api_value return_value, object, key, prop;

  NODE_API_CALL(env, node_api_create_object(env, &return_value));
  NODE_API_CALL(env, node_api_create_object(env, &object));
  NODE_API_CALL(env,
      node_api_create_string_utf8(
          env, "someString", NODE_API_AUTO_LENGTH, &key));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      node_api_invalid_arg,
                      node_api_get_property(NULL, object, key, &prop));

  node_api_get_property(env, NULL, key, &prop);
  add_last_status(env, "objectIsNull", return_value);

  node_api_get_property(env, object, NULL, &prop);
  add_last_status(env, "keyIsNull", return_value);

  node_api_get_property(env, object, key, NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static node_api_value
TestBoolValuedPropApi(node_api_env env,
                      node_api_status (*api)(node_api_env,
                                             node_api_value,
                                             node_api_value,
                                             bool*)) {
  node_api_value return_value, object, key;
  bool result;

  NODE_API_CALL(env, node_api_create_object(env, &return_value));
  NODE_API_CALL(env, node_api_create_object(env, &object));
  NODE_API_CALL(env,
      node_api_create_string_utf8(
          env, "someString", NODE_API_AUTO_LENGTH, &key));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      node_api_invalid_arg,
                      api(NULL, object, key, &result));

  api(env, NULL, key, &result);
  add_last_status(env, "objectIsNull", return_value);

  api(env, object, NULL, &result);
  add_last_status(env, "keyIsNull", return_value);

  api(env, object, key, NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static node_api_value
HasProperty(node_api_env env, node_api_callback_info info) {
  return TestBoolValuedPropApi(env, node_api_has_property);
}

static node_api_value
HasOwnProperty(node_api_env env, node_api_callback_info info) {
  return TestBoolValuedPropApi(env, node_api_has_own_property);
}

static node_api_value
DeleteProperty(node_api_env env, node_api_callback_info info) {
  return TestBoolValuedPropApi(env, node_api_delete_property);
}

static node_api_value
SetNamedProperty(node_api_env env, node_api_callback_info info) {
  node_api_value return_value, object;

  NODE_API_CALL(env, node_api_create_object(env, &return_value));
  NODE_API_CALL(env, node_api_create_object(env, &object));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      node_api_invalid_arg,
                      node_api_set_named_property(
                          NULL, object, "key", object));

  node_api_set_named_property(env, NULL, "key", object);
  add_last_status(env, "objectIsNull", return_value);

  node_api_set_named_property(env, object, NULL, object);
  add_last_status(env, "keyIsNull", return_value);

  node_api_set_named_property(env, object, "key", NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static node_api_value
GetNamedProperty(node_api_env env, node_api_callback_info info) {
  node_api_value return_value, object, prop;

  NODE_API_CALL(env, node_api_create_object(env, &return_value));
  NODE_API_CALL(env, node_api_create_object(env, &object));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      node_api_invalid_arg,
                      node_api_get_named_property(NULL, object, "key", &prop));

  node_api_get_named_property(env, NULL, "key", &prop);
  add_last_status(env, "objectIsNull", return_value);

  node_api_get_named_property(env, object, NULL, &prop);
  add_last_status(env, "keyIsNull", return_value);

  node_api_get_named_property(env, object, "key", NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static node_api_value
HasNamedProperty(node_api_env env, node_api_callback_info info) {
  node_api_value return_value, object;
  bool result;

  NODE_API_CALL(env, node_api_create_object(env, &return_value));
  NODE_API_CALL(env, node_api_create_object(env, &object));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      node_api_invalid_arg,
                      node_api_has_named_property(
                          NULL, object, "key", &result));

  node_api_has_named_property(env, NULL, "key", &result);
  add_last_status(env, "objectIsNull", return_value);

  node_api_has_named_property(env, object, NULL, &result);
  add_last_status(env, "keyIsNull", return_value);

  node_api_has_named_property(env, object, "key", NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static node_api_value
SetElement(node_api_env env, node_api_callback_info info) {
  node_api_value return_value, object;

  NODE_API_CALL(env, node_api_create_object(env, &return_value));
  NODE_API_CALL(env, node_api_create_object(env, &object));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      node_api_invalid_arg,
                      node_api_set_element(NULL, object, 0, object));

  node_api_set_element(env, NULL, 0, object);
  add_last_status(env, "objectIsNull", return_value);

  node_api_set_property(env, object, 0, NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static node_api_value
GetElement(node_api_env env, node_api_callback_info info) {
  node_api_value return_value, object, prop;

  NODE_API_CALL(env, node_api_create_object(env, &return_value));
  NODE_API_CALL(env, node_api_create_object(env, &object));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      node_api_invalid_arg,
                      node_api_get_element(NULL, object, 0, &prop));

  node_api_get_property(env, NULL, 0, &prop);
  add_last_status(env, "objectIsNull", return_value);

  node_api_get_property(env, object, 0, NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static node_api_value
TestBoolValuedElementApi(
    node_api_env env, node_api_status (*api)(node_api_env,
                                             node_api_value,
                                             uint32_t,
                                             bool*)) {
  node_api_value return_value, object;
  bool result;

  NODE_API_CALL(env, node_api_create_object(env, &return_value));
  NODE_API_CALL(env, node_api_create_object(env, &object));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      node_api_invalid_arg,
                      api(NULL, object, 0, &result));

  api(env, NULL, 0, &result);
  add_last_status(env, "objectIsNull", return_value);

  api(env, object, 0, NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static node_api_value
HasElement(node_api_env env, node_api_callback_info info) {
  return TestBoolValuedElementApi(env, node_api_has_element);
}

static node_api_value
DeleteElement(node_api_env env, node_api_callback_info info) {
  return TestBoolValuedElementApi(env, node_api_delete_element);
}

static node_api_value
DefineProperties(node_api_env env, node_api_callback_info info) {
  node_api_value object, return_value;

  node_api_property_descriptor desc = {
    "prop", NULL, DefineProperties, NULL, NULL, NULL, node_api_enumerable, NULL
  };

  NODE_API_CALL(env, node_api_create_object(env, &object));
  NODE_API_CALL(env, node_api_create_object(env, &return_value));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      node_api_invalid_arg,
                      node_api_define_properties(NULL, object, 1, &desc));

  node_api_define_properties(env, NULL, 1, &desc);
  add_last_status(env, "objectIsNull", return_value);

  node_api_define_properties(env, object, 1, NULL);
  add_last_status(env, "descriptorListIsNull", return_value);

  desc.utf8name = NULL;
  node_api_define_properties(env, object, 1, NULL);
  add_last_status(env, "utf8nameIsNull", return_value);
  desc.utf8name = "prop";

  desc.method = NULL;
  node_api_define_properties(env, object, 1, NULL);
  add_last_status(env, "methodIsNull", return_value);
  desc.method = DefineProperties;

  return return_value;
}

static node_api_value
GetPropertyNames(node_api_env env, node_api_callback_info info) {
  node_api_value return_value, props;

  NODE_API_CALL(env, node_api_create_object(env, &return_value));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      node_api_invalid_arg,
                      node_api_get_property_names(NULL, return_value, &props));

  node_api_get_property_names(env, NULL, &props);
  add_last_status(env, "objectIsNull", return_value);

  node_api_get_property_names(env, return_value, NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static node_api_value
GetAllPropertyNames(node_api_env env, node_api_callback_info info) {
  node_api_value return_value, props;

  NODE_API_CALL(env, node_api_create_object(env, &return_value));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      node_api_invalid_arg,
                      node_api_get_all_property_names(NULL,
                                                     return_value,
                                                     node_api_key_own_only,
                                                     node_api_key_writable,
                                                     node_api_key_keep_numbers,
                                                     &props));

  node_api_get_all_property_names(env,
                                  NULL,
                                  node_api_key_own_only,
                                  node_api_key_writable,
                                  node_api_key_keep_numbers,
                                  &props);
  add_last_status(env, "objectIsNull", return_value);

  node_api_get_all_property_names(env,
                                  return_value,
                                  node_api_key_own_only,
                                  node_api_key_writable,
                                  node_api_key_keep_numbers,
                                  NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static node_api_value
GetPrototype(node_api_env env, node_api_callback_info info) {
  node_api_value return_value, proto;

  NODE_API_CALL(env, node_api_create_object(env, &return_value));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      node_api_invalid_arg,
                      node_api_get_prototype(NULL, return_value, &proto));

  node_api_get_prototype(env, NULL, &proto);
  add_last_status(env, "objectIsNull", return_value);

  node_api_get_prototype(env, return_value, NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

void init_test_null(node_api_env env, node_api_value exports) {
  node_api_value test_null;

  const node_api_property_descriptor test_null_props[] = {
    DECLARE_NODE_API_PROPERTY("setProperty", SetProperty),
    DECLARE_NODE_API_PROPERTY("getProperty", GetProperty),
    DECLARE_NODE_API_PROPERTY("hasProperty", HasProperty),
    DECLARE_NODE_API_PROPERTY("hasOwnProperty", HasOwnProperty),
    DECLARE_NODE_API_PROPERTY("deleteProperty", DeleteProperty),
    DECLARE_NODE_API_PROPERTY("setNamedProperty", SetNamedProperty),
    DECLARE_NODE_API_PROPERTY("getNamedProperty", GetNamedProperty),
    DECLARE_NODE_API_PROPERTY("hasNamedProperty", HasNamedProperty),
    DECLARE_NODE_API_PROPERTY("setElement", SetElement),
    DECLARE_NODE_API_PROPERTY("getElement", GetElement),
    DECLARE_NODE_API_PROPERTY("hasElement", HasElement),
    DECLARE_NODE_API_PROPERTY("deleteElement", DeleteElement),
    DECLARE_NODE_API_PROPERTY("defineProperties", DefineProperties),
    DECLARE_NODE_API_PROPERTY("getPropertyNames", GetPropertyNames),
    DECLARE_NODE_API_PROPERTY("getAllPropertyNames", GetAllPropertyNames),
    DECLARE_NODE_API_PROPERTY("getPrototype", GetPrototype),
  };

  NODE_API_CALL_RETURN_VOID(env, node_api_create_object(env, &test_null));
  NODE_API_CALL_RETURN_VOID(env, node_api_define_properties(
      env, test_null, sizeof(test_null_props) / sizeof(*test_null_props),
      test_null_props));

  const node_api_property_descriptor test_null_set = {
    "testNull", NULL, NULL, NULL, NULL, test_null, node_api_enumerable, NULL
  };

  NODE_API_CALL_RETURN_VOID(env,
      node_api_define_properties(env, exports, 1, &test_null_set));
}
