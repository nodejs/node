#include <js_native_api.h>

#include "../common.h"
#include "test_null.h"

#define DECLARE_TEST(charset, str_arg)                                    \
  static napi_value                                                       \
  test_create_##charset(napi_env env, napi_callback_info info) {          \
    napi_value return_value, result;                                      \
    NODE_API_CALL(env, napi_create_object(env, &return_value));           \
                                                                          \
    add_returned_status(env,                                              \
                        "envIsNull",                                      \
                        return_value,                                     \
                        "Invalid argument",                               \
                        napi_invalid_arg,                                 \
                        napi_create_string_##charset(NULL,                \
                                                     (str_arg),           \
                                                     NAPI_AUTO_LENGTH,    \
                                                     &result));           \
                                                                          \
    napi_create_string_##charset(env, NULL, NAPI_AUTO_LENGTH, &result);   \
    add_last_status(env, "stringIsNullNonZeroLength", return_value);      \
                                                                          \
    napi_create_string_##charset(env, NULL, 0, &result);                  \
    add_last_status(env, "stringIsNullZeroLength", return_value);         \
                                                                          \
    napi_create_string_##charset(env, (str_arg), NAPI_AUTO_LENGTH, NULL); \
    add_last_status(env, "resultIsNull", return_value);                   \
                                                                          \
    return return_value;                                                  \
  }

static const char16_t something[] = {
  (char16_t)'s',
  (char16_t)'o',
  (char16_t)'m',
  (char16_t)'e',
  (char16_t)'t',
  (char16_t)'h',
  (char16_t)'i',
  (char16_t)'n',
  (char16_t)'g',
  (char16_t)'\0'
};

DECLARE_TEST(utf8, "something")
DECLARE_TEST(latin1, "something")
DECLARE_TEST(utf16, something)

void init_test_null(napi_env env, napi_value exports) {
  napi_value test_null;

  const napi_property_descriptor test_null_props[] = {
    DECLARE_NODE_API_PROPERTY("test_create_utf8", test_create_utf8),
    DECLARE_NODE_API_PROPERTY("test_create_latin1", test_create_latin1),
    DECLARE_NODE_API_PROPERTY("test_create_utf16", test_create_utf16),
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
