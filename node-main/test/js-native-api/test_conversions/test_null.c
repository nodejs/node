#include <js_native_api.h>

#include "../common.h"
#include "test_null.h"

#define GEN_NULL_CHECK_BINDING(binding_name, output_type, api)            \
  static napi_value binding_name(napi_env env, napi_callback_info info) { \
    napi_value return_value;                                              \
    output_type result;                                                   \
    NODE_API_CALL(env, napi_create_object(env, &return_value));           \
    add_returned_status(env,                                              \
                        "envIsNull",                                      \
                        return_value,                                     \
                        "Invalid argument",                               \
                        napi_invalid_arg,                                 \
                        api(NULL, return_value, &result));                \
    api(env, NULL, &result);                                              \
    add_last_status(env, "valueIsNull", return_value);                    \
    api(env, return_value, NULL);                                         \
    add_last_status(env, "resultIsNull", return_value);                   \
    api(env, return_value, &result);                                      \
    add_last_status(env, "inputTypeCheck", return_value);                 \
    return return_value;                                                  \
  }

GEN_NULL_CHECK_BINDING(GetValueBool, bool, napi_get_value_bool)
GEN_NULL_CHECK_BINDING(GetValueInt32, int32_t, napi_get_value_int32)
GEN_NULL_CHECK_BINDING(GetValueUint32, uint32_t, napi_get_value_uint32)
GEN_NULL_CHECK_BINDING(GetValueInt64, int64_t, napi_get_value_int64)
GEN_NULL_CHECK_BINDING(GetValueDouble, double, napi_get_value_double)
GEN_NULL_CHECK_BINDING(CoerceToBool, napi_value, napi_coerce_to_bool)
GEN_NULL_CHECK_BINDING(CoerceToObject, napi_value, napi_coerce_to_object)
GEN_NULL_CHECK_BINDING(CoerceToString, napi_value, napi_coerce_to_string)

#define GEN_NULL_CHECK_STRING_BINDING(binding_name, arg_type, api)         \
  static napi_value binding_name(napi_env env, napi_callback_info info) {  \
    napi_value return_value;                                               \
    NODE_API_CALL(env, napi_create_object(env, &return_value));            \
    arg_type buf1[4];                                                      \
    size_t length1 = 3;                                                    \
    add_returned_status(env,                                               \
                        "envIsNull",                                       \
                        return_value,                                      \
                        "Invalid argument",                                \
                        napi_invalid_arg,                                  \
                        api(NULL, return_value, buf1, length1, &length1)); \
    arg_type buf2[4];                                                      \
    size_t length2 = 3;                                                    \
    api(env, NULL, buf2, length2, &length2);                               \
    add_last_status(env, "valueIsNull", return_value);                     \
    api(env, return_value, NULL, 3, NULL);                                 \
    add_last_status(env, "wrongTypeIn", return_value);                     \
    napi_value string;                                                     \
    NODE_API_CALL(env,                                                     \
              napi_create_string_utf8(env,                                 \
                                      "Something",                         \
                                      NAPI_AUTO_LENGTH,                    \
                                      &string));                           \
    api(env, string, NULL, 3, NULL);                                       \
    add_last_status(env, "bufAndOutLengthIsNull", return_value);           \
    return return_value;                                                   \
  }

GEN_NULL_CHECK_STRING_BINDING(GetValueStringUtf8,
                              char,
                              napi_get_value_string_utf8)
GEN_NULL_CHECK_STRING_BINDING(GetValueStringLatin1,
                              char,
                              napi_get_value_string_latin1)
GEN_NULL_CHECK_STRING_BINDING(GetValueStringUtf16,
                              char16_t,
                              napi_get_value_string_utf16)

void init_test_null(napi_env env, napi_value exports) {
  napi_value test_null;

  const napi_property_descriptor test_null_props[] = {
    DECLARE_NODE_API_PROPERTY("getValueBool", GetValueBool),
    DECLARE_NODE_API_PROPERTY("getValueInt32", GetValueInt32),
    DECLARE_NODE_API_PROPERTY("getValueUint32", GetValueUint32),
    DECLARE_NODE_API_PROPERTY("getValueInt64", GetValueInt64),
    DECLARE_NODE_API_PROPERTY("getValueDouble", GetValueDouble),
    DECLARE_NODE_API_PROPERTY("coerceToBool", CoerceToBool),
    DECLARE_NODE_API_PROPERTY("coerceToObject", CoerceToObject),
    DECLARE_NODE_API_PROPERTY("coerceToString", CoerceToString),
    DECLARE_NODE_API_PROPERTY("getValueStringUtf8", GetValueStringUtf8),
    DECLARE_NODE_API_PROPERTY("getValueStringLatin1", GetValueStringLatin1),
    DECLARE_NODE_API_PROPERTY("getValueStringUtf16", GetValueStringUtf16),
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
