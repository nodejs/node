#include <node_api.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static node_api_value
CallWithString(node_api_env env, node_api_callback_info info) {
  node_api_status status;

  size_t argc = 1;
  node_api_value args[1];
  status = node_api_get_cb_info(env, info, &argc, args, NULL, NULL);
  assert(status == node_api_ok);

  node_api_valuetype types[1];
  status = node_api_typeof(env, args[0], types);
  assert(status == node_api_ok);

  assert(types[0] == node_api_string);
  if (types[0] == node_api_string) {
    size_t len = 0;
    // Get the length
    status = node_api_get_value_string_utf8(env, args[0], NULL, 0, &len);
    assert(status == node_api_ok);
    char* buf = (char*)malloc(len + 1);
    status = node_api_get_value_string_utf8(env, args[0], buf, len + 1, &len);
    assert(status == node_api_ok);
    free(buf);
  }

  return NULL;
}

static node_api_value
CallWithArray(node_api_env env, node_api_callback_info info) {
  node_api_status status;

  size_t argc = 1;
  node_api_value args[1];
  status = node_api_get_cb_info(env, info, &argc, args, NULL, NULL);
  assert(status == node_api_ok);

  node_api_value array = args[0];
  bool is_array = false;
  status = node_api_is_array(env, array, &is_array);
  assert(status == node_api_ok);

  assert(is_array);
  if (is_array) {
    uint32_t length;
    status = node_api_get_array_length(env, array, &length);
    assert(status == node_api_ok);

    uint32_t i;
    for (i = 0; i < length; ++i) {
      node_api_value v;
      status = node_api_get_element(env, array, i, &v);
      assert(status == node_api_ok);
    }
  }

  return NULL;
}

static node_api_value
CallWithNumber(node_api_env env, node_api_callback_info info) {
  node_api_status status;

  size_t argc = 1;
  node_api_value args[1];
  status = node_api_get_cb_info(env, info, &argc, args, NULL, NULL);
  assert(status == node_api_ok);

  node_api_valuetype types[1];
  status = node_api_typeof(env, args[0], types);
  assert(status == node_api_ok);

  assert(types[0] == node_api_number);
  if (types[0] == node_api_number) {
    double value = 0.0;
    status = node_api_get_value_double(env, args[0], &value);
    assert(status == node_api_ok);
  }

  return NULL;
}

static node_api_value
CallWithObject(node_api_env env, node_api_callback_info info) {
  node_api_status status;

  size_t argc = 1;
  node_api_value args[1];
  status = node_api_get_cb_info(env, info, &argc, args, NULL, NULL);
  assert(status == node_api_ok);

  node_api_valuetype types[1];
  status = node_api_typeof(env, args[0], types);
  assert(status == node_api_ok);

  assert(argc == 1 && types[0] == node_api_object);
  if (argc == 1 && types[0] == node_api_object) {
    node_api_value value;

    status = node_api_get_named_property(env, args[0], "map", &value);
    assert(status == node_api_ok);

    status = node_api_get_named_property(env, args[0], "operand", &value);
    assert(status == node_api_ok);

    status = node_api_get_named_property(env, args[0], "data", &value);
    assert(status == node_api_ok);

    status = node_api_get_named_property(env, args[0], "reduce", &value);
    assert(status == node_api_ok);
  }

  return NULL;
}

static node_api_value
CallWithTypedarray(node_api_env env, node_api_callback_info info) {
  node_api_status status;

  size_t argc = 1;
  node_api_value args[1];
  status = node_api_get_cb_info(env, info, &argc, args, NULL, NULL);
  assert(status == node_api_ok);

  bool is_typedarray = false;
  status = node_api_is_typedarray(env, args[0], &is_typedarray);
  assert(status == node_api_ok);

  assert(is_typedarray);
  if (is_typedarray) {
    node_api_typedarray_type type;
    node_api_value input_buffer;
    size_t byte_offset = 0;
    size_t length = 0;
    status = node_api_get_typedarray_info(env, args[0], &type, &length,
        NULL, &input_buffer, &byte_offset);
    assert(status == node_api_ok);
    assert(length > 0);

    void* data = NULL;
    size_t byte_length = 0;
    status = node_api_get_arraybuffer_info(env,
        input_buffer, &data, &byte_length);
    assert(status == node_api_ok);

    uint32_t* input_integers = (uint32_t*)((uint8_t*)(data) + byte_offset);
    assert(input_integers);
  }

  return NULL;
}

static node_api_value
CallWithArguments(node_api_env env, node_api_callback_info info) {
  node_api_status status;

  size_t argc = 1;
  node_api_value args[1000];
  // Get the length
  status = node_api_get_cb_info(env, info, &argc, NULL, NULL, NULL);
  assert(status == node_api_ok);

  status = node_api_get_cb_info(env, info, &argc, args, NULL, NULL);
  assert(status == node_api_ok);
  assert(argc <= 1000);

  node_api_valuetype types[1];
  status = node_api_typeof(env, args[0], types);
  assert(status == node_api_ok);

  assert(argc > 1 && types[0] == node_api_number);
  if (argc > 1 && types[0] == node_api_number) {
    uint32_t loop = 0;
    status = node_api_get_value_uint32(env, args[0], &loop);
    assert(status == node_api_ok);

    uint32_t i;
    for (i = 1; i < loop; ++i) {
      assert(i < argc);
      status = node_api_typeof(env, args[i], types);
      assert(status == node_api_ok);
      assert(types[0] == node_api_number);

      uint32_t value = 0;
      status = node_api_get_value_uint32(env, args[i], &value);
      assert(status == node_api_ok);
    }
  }

  return NULL;
}


#define EXPORT_FUNC(env, exports, name, func)           \
  do {                                                  \
    node_api_status status;                             \
    node_api_value js_func;                             \
    status = node_api_create_function((env),            \
                                  (name),               \
                                  NODE_API_AUTO_LENGTH, \
                                  (func),               \
                                  NULL,                 \
                                  &js_func);            \
    assert(status == node_api_ok);                      \
    status = node_api_set_named_property((env),         \
                                     (exports),         \
                                     (name),            \
                                     js_func);          \
    assert(status == node_api_ok);                      \
  } while (0);


NODE_API_MODULE_INIT() {
  EXPORT_FUNC(env, exports, "callWithString", CallWithString);
  EXPORT_FUNC(env, exports, "callWithLongString", CallWithString);

  EXPORT_FUNC(env, exports, "callWithArray", CallWithArray);
  EXPORT_FUNC(env, exports, "callWithLargeArray", CallWithArray);
  EXPORT_FUNC(env, exports, "callWithHugeArray", CallWithArray);

  EXPORT_FUNC(env, exports, "callWithNumber", CallWithNumber);

  EXPORT_FUNC(env, exports, "callWithObject", CallWithObject);
  EXPORT_FUNC(env, exports, "callWithTypedarray", CallWithTypedarray);

  EXPORT_FUNC(env, exports, "callWith10Numbers", CallWithArguments);
  EXPORT_FUNC(env, exports, "callWith100Numbers", CallWithArguments);
  EXPORT_FUNC(env, exports, "callWith1000Numbers", CallWithArguments);

  return exports;
}
