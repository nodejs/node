#include <assert.h>
#define NAPI_EXPERIMENTAL
#include <node_api.h>

#define NAPI_CALL(call)                           \
  do {                                            \
    napi_status status = call;                    \
    assert(status == napi_ok && #call " failed"); \
  } while (0);

#define EXPORT_FUNC(env, exports, name, func)       \
  do {                                              \
    napi_value js_func;                             \
    NAPI_CALL(napi_create_function((env),           \
                                  (name),           \
                                  NAPI_AUTO_LENGTH, \
                                  (func),           \
                                  NULL,             \
                                  &js_func));       \
    NAPI_CALL(napi_set_named_property((env),        \
                                     (exports),     \
                                     (name),        \
                                     js_func));     \
  } while (0);

static const napi_type_tag tag = {
  0xe7ecbcd5954842f6, 0x9e75161c9bf27282
};

static napi_value TagObject(napi_env env, napi_callback_info info) {
  size_t argc = 4;
  napi_value argv[4];
  uint32_t n;
  uint32_t index;
  napi_handle_scope scope;

  NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  NAPI_CALL(napi_get_value_uint32(env, argv[0], &n));
  NAPI_CALL(napi_open_handle_scope(env, &scope));
  napi_value objects[n];
  for (index = 0; index < n; index++) {
    NAPI_CALL(napi_create_object(env, &objects[index]));
  }

  // Time the object tag creation.
  NAPI_CALL(napi_call_function(env, argv[1], argv[2], 0, NULL, NULL));
  for (index = 0; index < n; index++) {
    NAPI_CALL(napi_type_tag_object(env, objects[index], &tag));
  }
  NAPI_CALL(napi_call_function(env, argv[1], argv[3], 1, &argv[0], NULL));

  NAPI_CALL(napi_close_handle_scope(env, scope));
  return NULL;
}

static napi_value CheckObjectTag(napi_env env, napi_callback_info info) {
  size_t argc = 4;
  napi_value argv[4];
  uint32_t n;
  uint32_t index;
  bool is_of_type;

  NAPI_CALL(napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  NAPI_CALL(napi_get_value_uint32(env, argv[0], &n));
  napi_value object;
  NAPI_CALL(napi_create_object(env, &object));
  NAPI_CALL(napi_type_tag_object(env, object, &tag));

  // Time the object tag checking.
  NAPI_CALL(napi_call_function(env, argv[1], argv[2], 0, NULL, NULL));
  for (index = 0; index < n; index++) {
    NAPI_CALL(napi_check_object_type_tag(env, object, &tag, &is_of_type));
    assert(is_of_type && " type mismatch");
  }
  NAPI_CALL(napi_call_function(env, argv[1], argv[3], 1, &argv[0], NULL));

  return NULL;
}

NAPI_MODULE_INIT() {
  EXPORT_FUNC(env, exports, "tagObject", TagObject);
  EXPORT_FUNC(env, exports, "checkObjectTag", CheckObjectTag);
  return exports;
}
