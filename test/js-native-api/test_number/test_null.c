#include <js_native_api.h>

#include "../common.h"

// Unifies the way the macros declare values.
typedef double double_t;

#define BINDING_FOR_CREATE(initial_capital, lowercase)                         \
  static napi_value Create##initial_capital(napi_env env,                      \
                                            napi_callback_info info) {         \
    napi_value return_value, call_result;                                      \
    lowercase##_t value = 42;                                                  \
    NODE_API_CALL(env, napi_create_object(env, &return_value));                \
    add_returned_status(env,                                                   \
                        "envIsNull",                                           \
                        return_value,                                          \
                        "Invalid argument",                                    \
                        napi_invalid_arg,                                      \
                        napi_create_##lowercase(NULL, value, &call_result));   \
    napi_create_##lowercase(env, value, NULL);                                 \
    add_last_status(env, "resultIsNull", return_value);                        \
    return return_value;                                                       \
  }

#define BINDING_FOR_GET_VALUE(initial_capital, lowercase)                      \
  static napi_value GetValue##initial_capital(napi_env env,                    \
                                              napi_callback_info info) {       \
    napi_value return_value, call_result;                                      \
    lowercase##_t value = 42;                                                  \
    NODE_API_CALL(env, napi_create_object(env, &return_value));                \
    NODE_API_CALL(env, napi_create_##lowercase(env, value, &call_result));     \
    add_returned_status(                                                       \
        env,                                                                   \
        "envIsNull",                                                           \
        return_value,                                                          \
        "Invalid argument",                                                    \
        napi_invalid_arg,                                                      \
        napi_get_value_##lowercase(NULL, call_result, &value));                \
    napi_get_value_##lowercase(env, NULL, &value);                             \
    add_last_status(env, "valueIsNull", return_value);                         \
    napi_get_value_##lowercase(env, call_result, NULL);                        \
    add_last_status(env, "resultIsNull", return_value);                        \
    return return_value;                                                       \
  }

BINDING_FOR_CREATE(Double, double)
BINDING_FOR_CREATE(Int32, int32)
BINDING_FOR_CREATE(Uint32, uint32)
BINDING_FOR_CREATE(Int64, int64)
BINDING_FOR_GET_VALUE(Double, double)
BINDING_FOR_GET_VALUE(Int32, int32)
BINDING_FOR_GET_VALUE(Uint32, uint32)
BINDING_FOR_GET_VALUE(Int64, int64)

void init_test_null(napi_env env, napi_value exports) {
  const napi_property_descriptor test_null_props[] = {
      DECLARE_NODE_API_PROPERTY("createDouble", CreateDouble),
      DECLARE_NODE_API_PROPERTY("createInt32", CreateInt32),
      DECLARE_NODE_API_PROPERTY("createUint32", CreateUint32),
      DECLARE_NODE_API_PROPERTY("createInt64", CreateInt64),
      DECLARE_NODE_API_PROPERTY("getValueDouble", GetValueDouble),
      DECLARE_NODE_API_PROPERTY("getValueInt32", GetValueInt32),
      DECLARE_NODE_API_PROPERTY("getValueUint32", GetValueUint32),
      DECLARE_NODE_API_PROPERTY("getValueInt64", GetValueInt64),
  };
  napi_value test_null;

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
