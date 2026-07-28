#include <js_native_api.h>

#include "../common.h"
#include "test_null.h"

static napi_value SetProperty(napi_env env, napi_callback_info info) {
  napi_value return_value, object, key;

  NODE_API_CALL(env, napi_create_object(env, &return_value));
  NODE_API_CALL(env, napi_create_object(env, &object));
  NODE_API_CALL(env,
      napi_create_string_utf8(env, "someString", NAPI_AUTO_LENGTH, &key));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      napi_invalid_arg,
                      napi_set_property(NULL, object, key, object));

  napi_set_property(env, NULL, key, object);
  add_last_status(env, "objectIsNull", return_value);

  napi_set_property(env, object, NULL, object);
  add_last_status(env, "keyIsNull", return_value);

  napi_set_property(env, object, key, NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static napi_value GetProperty(napi_env env, napi_callback_info info) {
  napi_value return_value, object, key, prop;

  NODE_API_CALL(env, napi_create_object(env, &return_value));
  NODE_API_CALL(env, napi_create_object(env, &object));
  NODE_API_CALL(env,
      napi_create_string_utf8(env, "someString", NAPI_AUTO_LENGTH, &key));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      napi_invalid_arg,
                      napi_get_property(NULL, object, key, &prop));

  napi_get_property(env, NULL, key, &prop);
  add_last_status(env, "objectIsNull", return_value);

  napi_get_property(env, object, NULL, &prop);
  add_last_status(env, "keyIsNull", return_value);

  napi_get_property(env, object, key, NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static napi_value TestBoolValuedPropApi(napi_env env,
    napi_status (*api)(napi_env, napi_value, napi_value, bool*)) {
  napi_value return_value, object, key;
  bool result;

  NODE_API_CALL(env, napi_create_object(env, &return_value));
  NODE_API_CALL(env, napi_create_object(env, &object));
  NODE_API_CALL(env,
      napi_create_string_utf8(env, "someString", NAPI_AUTO_LENGTH, &key));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      napi_invalid_arg,
                      api(NULL, object, key, &result));

  api(env, NULL, key, &result);
  add_last_status(env, "objectIsNull", return_value);

  api(env, object, NULL, &result);
  add_last_status(env, "keyIsNull", return_value);

  api(env, object, key, NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static napi_value HasProperty(napi_env env, napi_callback_info info) {
  return TestBoolValuedPropApi(env, napi_has_property);
}

static napi_value HasOwnProperty(napi_env env, napi_callback_info info) {
  return TestBoolValuedPropApi(env, napi_has_own_property);
}

static napi_value DeleteProperty(napi_env env, napi_callback_info info) {
  return TestBoolValuedPropApi(env, napi_delete_property);
}

static napi_value SetNamedProperty(napi_env env, napi_callback_info info) {
  napi_value return_value, object;

  NODE_API_CALL(env, napi_create_object(env, &return_value));
  NODE_API_CALL(env, napi_create_object(env, &object));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      napi_invalid_arg,
                      napi_set_named_property(NULL, object, "key", object));

  napi_set_named_property(env, NULL, "key", object);
  add_last_status(env, "objectIsNull", return_value);

  napi_set_named_property(env, object, NULL, object);
  add_last_status(env, "keyIsNull", return_value);

  napi_set_named_property(env, object, "key", NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static napi_value GetNamedProperty(napi_env env, napi_callback_info info) {
  napi_value return_value, object, prop;

  NODE_API_CALL(env, napi_create_object(env, &return_value));
  NODE_API_CALL(env, napi_create_object(env, &object));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      napi_invalid_arg,
                      napi_get_named_property(NULL, object, "key", &prop));

  napi_get_named_property(env, NULL, "key", &prop);
  add_last_status(env, "objectIsNull", return_value);

  napi_get_named_property(env, object, NULL, &prop);
  add_last_status(env, "keyIsNull", return_value);

  napi_get_named_property(env, object, "key", NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static napi_value HasNamedProperty(napi_env env, napi_callback_info info) {
  napi_value return_value, object;
  bool result;

  NODE_API_CALL(env, napi_create_object(env, &return_value));
  NODE_API_CALL(env, napi_create_object(env, &object));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      napi_invalid_arg,
                      napi_has_named_property(NULL, object, "key", &result));

  napi_has_named_property(env, NULL, "key", &result);
  add_last_status(env, "objectIsNull", return_value);

  napi_has_named_property(env, object, NULL, &result);
  add_last_status(env, "keyIsNull", return_value);

  napi_has_named_property(env, object, "key", NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static napi_value SetElement(napi_env env, napi_callback_info info) {
  napi_value return_value, object;

  NODE_API_CALL(env, napi_create_object(env, &return_value));
  NODE_API_CALL(env, napi_create_object(env, &object));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      napi_invalid_arg,
                      napi_set_element(NULL, object, 0, object));

  napi_set_element(env, NULL, 0, object);
  add_last_status(env, "objectIsNull", return_value);

  napi_set_property(env, object, 0, NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static napi_value GetElement(napi_env env, napi_callback_info info) {
  napi_value return_value, object, prop;

  NODE_API_CALL(env, napi_create_object(env, &return_value));
  NODE_API_CALL(env, napi_create_object(env, &object));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      napi_invalid_arg,
                      napi_get_element(NULL, object, 0, &prop));

  napi_get_property(env, NULL, 0, &prop);
  add_last_status(env, "objectIsNull", return_value);

  napi_get_property(env, object, 0, NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static napi_value TestBoolValuedElementApi(napi_env env,
    napi_status (*api)(napi_env, napi_value, uint32_t, bool*)) {
  napi_value return_value, object;
  bool result;

  NODE_API_CALL(env, napi_create_object(env, &return_value));
  NODE_API_CALL(env, napi_create_object(env, &object));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      napi_invalid_arg,
                      api(NULL, object, 0, &result));

  api(env, NULL, 0, &result);
  add_last_status(env, "objectIsNull", return_value);

  api(env, object, 0, NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static napi_value HasElement(napi_env env, napi_callback_info info) {
  return TestBoolValuedElementApi(env, napi_has_element);
}

static napi_value DeleteElement(napi_env env, napi_callback_info info) {
  return TestBoolValuedElementApi(env, napi_delete_element);
}

static napi_value DefineProperties(napi_env env, napi_callback_info info) {
  napi_value object, return_value;

  napi_property_descriptor desc = {
    "prop", NULL, DefineProperties, NULL, NULL, NULL, napi_enumerable, NULL
  };

  NODE_API_CALL(env, napi_create_object(env, &object));
  NODE_API_CALL(env, napi_create_object(env, &return_value));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      napi_invalid_arg,
                      napi_define_properties(NULL, object, 1, &desc));

  napi_define_properties(env, NULL, 1, &desc);
  add_last_status(env, "objectIsNull", return_value);

  napi_define_properties(env, object, 1, NULL);
  add_last_status(env, "descriptorListIsNull", return_value);

  desc.utf8name = NULL;
  napi_define_properties(env, object, 1, NULL);
  add_last_status(env, "utf8nameIsNull", return_value);
  desc.utf8name = "prop";

  desc.method = NULL;
  napi_define_properties(env, object, 1, NULL);
  add_last_status(env, "methodIsNull", return_value);
  desc.method = DefineProperties;

  return return_value;
}

static napi_value GetPropertyNames(napi_env env, napi_callback_info info) {
  napi_value return_value, props;

  NODE_API_CALL(env, napi_create_object(env, &return_value));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      napi_invalid_arg,
                      napi_get_property_names(NULL, return_value, &props));

  napi_get_property_names(env, NULL, &props);
  add_last_status(env, "objectIsNull", return_value);

  napi_get_property_names(env, return_value, NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static napi_value GetAllPropertyNames(napi_env env, napi_callback_info info) {
  napi_value return_value, props;

  NODE_API_CALL(env, napi_create_object(env, &return_value));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      napi_invalid_arg,
                      napi_get_all_property_names(NULL,
                                                  return_value,
                                                  napi_key_own_only,
                                                  napi_key_writable,
                                                  napi_key_keep_numbers,
                                                  &props));

  napi_get_all_property_names(env,
                              NULL,
                              napi_key_own_only,
                              napi_key_writable,
                              napi_key_keep_numbers,
                              &props);
  add_last_status(env, "objectIsNull", return_value);

  napi_get_all_property_names(env,
                              return_value,
                              napi_key_own_only,
                              napi_key_writable,
                              napi_key_keep_numbers,
                              NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

static napi_value GetPrototype(napi_env env, napi_callback_info info) {
  napi_value return_value, proto;

  NODE_API_CALL(env, napi_create_object(env, &return_value));

  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      napi_invalid_arg,
                      napi_get_prototype(NULL, return_value, &proto));

  napi_get_prototype(env, NULL, &proto);
  add_last_status(env, "objectIsNull", return_value);

  napi_get_prototype(env, return_value, NULL);
  add_last_status(env, "valueIsNull", return_value);

  return return_value;
}

void init_test_null(napi_env env, napi_value exports) {
  napi_value test_null;

  const napi_property_descriptor test_null_props[] = {
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

  NODE_API_CALL_RETURN_VOID(env, napi_create_object(env, &test_null));
  NODE_API_CALL_RETURN_VOID(env, napi_define_properties(
      env, test_null, sizeof(test_null_props) / sizeof(*test_null_props),
      test_null_props));

  const napi_property_descriptor test_null_set = {
    "testNull", NULL, NULL, NULL, NULL, test_null, napi_enumerable, NULL
  };

  NODE_API_CALL_RETURN_VOID(env,
      napi_define_properties(env, exports, 1, &test_null_set));
}
