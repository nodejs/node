#include <js_native_api.h>

#include "../common.h"
#include "test_null.h"

static int some_data = 0;

static napi_value TestConstructor(napi_env env, napi_callback_info info) {
  return NULL;
}

static napi_value TestDefineClass(napi_env env, napi_callback_info info) {
  napi_value return_value, cons;

  const napi_property_descriptor prop =
      DECLARE_NODE_API_PROPERTY("testConstructor", TestConstructor);

  NODE_API_CALL(env, napi_create_object(env, &return_value));
  add_returned_status(env,
                      "envIsNull",
                      return_value,
                      "Invalid argument",
                      napi_invalid_arg,
                      napi_define_class(NULL,
                                        "TestClass",
                                        NAPI_AUTO_LENGTH,
                                        TestConstructor,
                                        &some_data,
                                        1,
                                        &prop,
                                        &cons));

  napi_define_class(env,
                    NULL,
                    NAPI_AUTO_LENGTH,
                    TestConstructor,
                    &some_data,
                    1,
                    &prop,
                    &cons);
  add_last_status(env, "nameIsNull", return_value);

  napi_define_class(
      env, "TestClass", 0, TestConstructor, &some_data, 1, &prop, &cons);
  add_last_status(env, "lengthIsZero", return_value);

  napi_define_class(
      env, "TestClass", NAPI_AUTO_LENGTH, NULL, &some_data, 1, &prop, &cons);
  add_last_status(env, "nativeSideIsNull", return_value);

  napi_define_class(env,
                    "TestClass",
                    NAPI_AUTO_LENGTH,
                    TestConstructor,
                    NULL,
                    1,
                    &prop,
                    &cons);
  add_last_status(env, "dataIsNull", return_value);

  napi_define_class(env,
                    "TestClass",
                    NAPI_AUTO_LENGTH,
                    TestConstructor,
                    &some_data,
                    0,
                    &prop,
                    &cons);
  add_last_status(env, "propsLengthIsZero", return_value);

  napi_define_class(env,
                    "TestClass",
                    NAPI_AUTO_LENGTH,
                    TestConstructor,
                    &some_data,
                    1,
                    NULL,
                    &cons);
  add_last_status(env, "propsIsNull", return_value);

  napi_define_class(env,
                    "TestClass",
                    NAPI_AUTO_LENGTH,
                    TestConstructor,
                    &some_data,
                    1,
                    &prop,
                    NULL);
  add_last_status(env, "resultIsNull", return_value);

  return return_value;
}

void init_test_null(napi_env env, napi_value exports) {
  napi_value test_null;

  const napi_property_descriptor test_null_props[] = {
      DECLARE_NODE_API_PROPERTY("testDefineClass", TestDefineClass),
  };

  NODE_API_CALL_RETURN_VOID(env, napi_create_object(env, &test_null));
  NODE_API_CALL_RETURN_VOID(
      env,
      napi_define_properties(env,
                             test_null,
                             sizeof(test_null_props) / sizeof(*test_null_props),
                             test_null_props));

  NODE_API_CALL_RETURN_VOID(
      env, napi_set_named_property(env, exports, "testNull", test_null));
}
