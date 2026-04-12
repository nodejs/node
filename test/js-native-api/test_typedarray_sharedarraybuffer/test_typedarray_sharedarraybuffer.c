// Verify napi_create_typedarray() accepts SharedArrayBuffer-backed views
// without changing its existing error handling.

#include <js_native_api.h>
#include "../common.h"
#include "../entry_point.h"

static napi_value CreateTypedArray(napi_env env,
                                   napi_callback_info info,
                                   napi_typedarray_type type) {
  size_t argc = 3;
  napi_value args[3];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 3, "Wrong number of arguments");

  uint32_t byte_offset;
  NODE_API_CALL(env, napi_get_value_uint32(env, args[1], &byte_offset));

  uint32_t length;
  NODE_API_CALL(env, napi_get_value_uint32(env, args[2], &length));

  napi_value typedarray;
  NODE_API_CALL(env,
                napi_create_typedarray(
                    env, type, length, args[0], byte_offset, &typedarray));

  return typedarray;
}

static napi_value CreateUint8Array(napi_env env, napi_callback_info info) {
  return CreateTypedArray(env, info, napi_uint8_array);
}

static napi_value CreateUint16Array(napi_env env, napi_callback_info info) {
  return CreateTypedArray(env, info, napi_uint16_array);
}

static napi_value CreateInt32Array(napi_env env, napi_callback_info info) {
  return CreateTypedArray(env, info, napi_int32_array);
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
      DECLARE_NODE_API_PROPERTY("CreateUint8Array", CreateUint8Array),
      DECLARE_NODE_API_PROPERTY("CreateUint16Array", CreateUint16Array),
      DECLARE_NODE_API_PROPERTY("CreateInt32Array", CreateInt32Array),
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
