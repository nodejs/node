#define NAPI_EXPERIMENTAL
#include <js_native_api.h>
#include <string.h>
#include "../common.h"
#include "../entry_point.h"

static napi_value TestIsSharedArrayBuffer(napi_env env,
                                          napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  bool is_sharedarraybuffer;
  NODE_API_CALL(
      env, node_api_is_sharedarraybuffer(env, args[0], &is_sharedarraybuffer));

  napi_value ret;
  NODE_API_CALL(env, napi_get_boolean(env, is_sharedarraybuffer, &ret));

  return ret;
}

static napi_value TestCreateSharedArrayBuffer(napi_env env,
                                              napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NODE_API_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(
      env,
      valuetype0 == napi_number,
      "Wrong type of arguments. Expects a number as first argument.");

  int32_t byte_length;
  NODE_API_CALL(env, napi_get_value_int32(env, args[0], &byte_length));

  NODE_API_ASSERT(env,
                  byte_length >= 0,
                  "Invalid byte length. Expects a non-negative integer.");

  napi_value ret;
  void* data;
  NODE_API_CALL(
      env, node_api_create_sharedarraybuffer(env, byte_length, &data, &ret));

  return ret;
}

static napi_value TestGetSharedArrayBufferInfo(napi_env env,
                                               napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  void* data;
  size_t byte_length;
  NODE_API_CALL(env,
                napi_get_arraybuffer_info(env, args[0], &data, &byte_length));

  napi_value ret;
  NODE_API_CALL(env, napi_create_uint32(env, byte_length, &ret));

  return ret;
}

static void WriteTestDataToBuffer(void* data, size_t byte_length) {
  if (byte_length > 0 && data != NULL) {
    uint8_t* bytes = (uint8_t*)data;
    for (size_t i = 0; i < byte_length; i++) {
      bytes[i] = i % 256;
    }
  }
}

static napi_value TestSharedArrayBufferData(napi_env env,
                                            napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  void* data;
  size_t byte_length;
  NODE_API_CALL(env,
                napi_get_arraybuffer_info(env, args[0], &data, &byte_length));

  WriteTestDataToBuffer(data, byte_length);

  // Return the same data pointer validity
  bool data_valid = (data != NULL) && (byte_length > 0);

  napi_value ret;
  NODE_API_CALL(env, napi_get_boolean(env, data_valid, &ret));

  return ret;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
      DECLARE_NODE_API_PROPERTY("TestIsSharedArrayBuffer",
                                TestIsSharedArrayBuffer),
      DECLARE_NODE_API_PROPERTY("TestCreateSharedArrayBuffer",
                                TestCreateSharedArrayBuffer),
      DECLARE_NODE_API_PROPERTY("TestGetSharedArrayBufferInfo",
                                TestGetSharedArrayBufferInfo),
      DECLARE_NODE_API_PROPERTY("TestSharedArrayBufferData",
                                TestSharedArrayBufferData),
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
