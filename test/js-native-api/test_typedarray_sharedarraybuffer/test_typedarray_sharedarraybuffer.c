// Verify napi_create_typedarray() accepts SharedArrayBuffer-backed views
// without changing its existing error handling.

#include <js_native_api.h>
#include "../common.h"
#include "../entry_point.h"

static napi_value CreateTypedArray(napi_env env, napi_callback_info info) {
  size_t argc = 4;
  napi_value args[4];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 2 || argc == 4, "Wrong number of arguments");

  bool is_typedarray;
  NODE_API_CALL(env, napi_is_typedarray(env, args[0], &is_typedarray));
  NODE_API_ASSERT(env,
                  is_typedarray,
                  "Wrong type of arguments. Expects a typed array as first "
                  "argument.");

  napi_typedarray_type type;
  size_t length;
  size_t byte_offset;
  NODE_API_CALL(env,
                napi_get_typedarray_info(
                    env, args[0], &type, &length, NULL, NULL, &byte_offset));

  if (argc == 4) {
    uint32_t uint32_length;
    NODE_API_CALL(env, napi_get_value_uint32(env, args[2], &uint32_length));
    length = uint32_length;

    uint32_t uint32_byte_offset;
    NODE_API_CALL(env,
                  napi_get_value_uint32(env, args[3], &uint32_byte_offset));
    byte_offset = uint32_byte_offset;
  }

  napi_value typedarray;
  NODE_API_CALL(env,
                napi_create_typedarray(
                    env, type, length, args[1], byte_offset, &typedarray));

  return typedarray;
}

static napi_value GetArrayBuffer(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");

  napi_value arraybuffer;
  NODE_API_CALL(env,
                napi_get_typedarray_info(
                    env, args[0], NULL, NULL, NULL, &arraybuffer, NULL));

  return arraybuffer;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
      DECLARE_NODE_API_PROPERTY("CreateTypedArray", CreateTypedArray),
      DECLARE_NODE_API_PROPERTY("GetArrayBuffer", GetArrayBuffer),
  };

  NODE_API_CALL(
      env,
      napi_define_properties(env,
                             exports,
                             sizeof(descriptors) / sizeof(*descriptors),
                             descriptors));

  return exports;
}
EXTERN_C_END
