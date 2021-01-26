#include <assert.h>
#define NODE_API_EXPERIMENTAL
#include <node_api.h>

#define NODE_API_CALL(call)                           \
  do {                                                \
    node_api_status status = call;                    \
    assert(status == node_api_ok && #call " failed"); \
  } while (0);

#define EXPORT_FUNC(env, exports, name, func)           \
  do {                                                  \
    node_api_value js_func;                             \
    NODE_API_CALL(node_api_create_function((env),       \
                                  (name),               \
                                  NODE_API_AUTO_LENGTH, \
                                  (func),               \
                                  NULL,                 \
                                  &js_func));           \
    NODE_API_CALL(node_api_set_named_property((env),    \
                                     (exports),         \
                                     (name),            \
                                     js_func));         \
  } while (0);

static const node_api_type_tag tag = {
  0xe7ecbcd5954842f6, 0x9e75161c9bf27282
};

static node_api_value
TagObject(node_api_env env, node_api_callback_info info) {
  size_t argc = 4;
  node_api_value argv[4];
  uint32_t n;
  uint32_t index;
  node_api_handle_scope scope;

  NODE_API_CALL(node_api_get_cb_info(env, info, &argc, argv, NULL, NULL));
  NODE_API_CALL(node_api_get_value_uint32(env, argv[0], &n));
  NODE_API_CALL(node_api_open_handle_scope(env, &scope));
  node_api_value objects[n];
  for (index = 0; index < n; index++) {
    NODE_API_CALL(node_api_create_object(env, &objects[index]));
  }

  // Time the object tag creation.
  NODE_API_CALL(node_api_call_function(env, argv[1], argv[2], 0, NULL, NULL));
  for (index = 0; index < n; index++) {
    NODE_API_CALL(node_api_type_tag_object(env, objects[index], &tag));
  }
  NODE_API_CALL(node_api_call_function(
      env, argv[1], argv[3], 1, &argv[0], NULL));

  NODE_API_CALL(node_api_close_handle_scope(env, scope));
  return NULL;
}

static node_api_value
CheckObjectTag(node_api_env env, node_api_callback_info info) {
  size_t argc = 4;
  node_api_value argv[4];
  uint32_t n;
  uint32_t index;
  bool is_of_type;

  NODE_API_CALL(node_api_get_cb_info(env, info, &argc, argv, NULL, NULL));
  NODE_API_CALL(node_api_get_value_uint32(env, argv[0], &n));
  node_api_value object;
  NODE_API_CALL(node_api_create_object(env, &object));
  NODE_API_CALL(node_api_type_tag_object(env, object, &tag));

  // Time the object tag checking.
  NODE_API_CALL(node_api_call_function(env, argv[1], argv[2], 0, NULL, NULL));
  for (index = 0; index < n; index++) {
    NODE_API_CALL(node_api_check_object_type_tag(
        env, object, &tag, &is_of_type));
    assert(is_of_type && " type mismatch");
  }
  NODE_API_CALL(node_api_call_function(
      env, argv[1], argv[3], 1, &argv[0], NULL));

  return NULL;
}

NODE_API_MODULE_INIT() {
  EXPORT_FUNC(env, exports, "tagObject", TagObject);
  EXPORT_FUNC(env, exports, "checkObjectTag", CheckObjectTag);
  return exports;
}
