#include <assert.h>
#define NAPI_EXPERIMENTAL
#include <node_api.h>

#define NAPI_CALL(call)                                                        \
  do {                                                                         \
    napi_status status = call;                                                 \
    assert(status == napi_ok && #call " failed");                              \
  } while (0);

#define EXPORT_FUNC(env, exports, name, func)                                  \
  do {                                                                         \
    napi_value js_func;                                                        \
    NAPI_CALL(napi_create_function(                                            \
        (env), (name), NAPI_AUTO_LENGTH, (func), NULL, &js_func));             \
    NAPI_CALL(napi_set_named_property((env), (exports), (name), js_func));     \
  } while (0);

const char* one_byte_string = "The Quick Brown Fox Jumped Over The Lazy Dog.";
const char16_t* two_byte_string =
    u"The Quick Brown Fox Jumped Over The Lazy Dog.";

#define DECLARE_BINDING(CapName, lowercase_name, var_name)                     \
  static napi_value CreateString##CapName(napi_env env,                        \
                                          napi_callback_info info) {           \
    size_t argc = 4;                                                           \
    napi_value argv[4];                                                        \
    uint32_t n;                                                                \
    uint32_t index;                                                            \
    napi_handle_scope scope;                                                   \
    napi_value js_string;                                                      \
                                                                               \
    NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));           \
    NAPI_CALL(napi_get_value_uint32(env, argv[0], &n));                        \
    NAPI_CALL(napi_open_handle_scope(env, &scope));                            \
    NAPI_CALL(napi_call_function(env, argv[1], argv[2], 0, NULL, NULL));       \
    for (index = 0; index < n; index++) {                                      \
      NAPI_CALL(napi_create_string_##lowercase_name(                           \
          env, (var_name), NAPI_AUTO_LENGTH, &js_string));                     \
    }                                                                          \
    NAPI_CALL(napi_call_function(env, argv[1], argv[3], 1, &argv[0], NULL));   \
    NAPI_CALL(napi_close_handle_scope(env, scope));                            \
                                                                               \
    return NULL;                                                               \
  }

DECLARE_BINDING(Latin1, latin1, one_byte_string)
DECLARE_BINDING(Utf8, utf8, one_byte_string)
DECLARE_BINDING(Utf16, utf16, two_byte_string)

NAPI_MODULE_INIT() {
  EXPORT_FUNC(env, exports, "createStringLatin1", CreateStringLatin1);
  EXPORT_FUNC(env, exports, "createStringUtf8", CreateStringUtf8);
  EXPORT_FUNC(env, exports, "createStringUtf16", CreateStringUtf16);
  return exports;
}
