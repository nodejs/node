#include <js_native_api.h>
#include <string.h>
#include <stdlib.h>
#include "../common.h"

static node_api_value Multiply(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 2, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_object,
      "Wrong type of arguments. Expects a typed array as first argument.");

  node_api_value input_array = args[0];
  bool is_typedarray;
  NODE_API_CALL(env, node_api_is_typedarray(env, input_array, &is_typedarray));

  NODE_API_ASSERT(env, is_typedarray,
      "Wrong type of arguments. Expects a typed array as first argument.");

  node_api_valuetype valuetype1;
  NODE_API_CALL(env, node_api_typeof(env, args[1], &valuetype1));

  NODE_API_ASSERT(env, valuetype1 == node_api_number,
      "Wrong type of arguments. Expects a number as second argument.");

  double multiplier;
  NODE_API_CALL(env, node_api_get_value_double(env, args[1], &multiplier));

  node_api_typedarray_type type;
  node_api_value input_buffer;
  size_t byte_offset;
  size_t i, length;
  NODE_API_CALL(env, node_api_get_typedarray_info(
      env, input_array, &type, &length, NULL, &input_buffer, &byte_offset));

  void* data;
  size_t byte_length;
  NODE_API_CALL(env, node_api_get_arraybuffer_info(
      env, input_buffer, &data, &byte_length));

  node_api_value output_buffer;
  void* output_ptr = NULL;
  NODE_API_CALL(env, node_api_create_arraybuffer(
      env, byte_length, &output_ptr, &output_buffer));

  node_api_value output_array;
  NODE_API_CALL(env, node_api_create_typedarray(
      env, type, length, output_buffer, byte_offset, &output_array));

  if (type == node_api_uint8_array) {
    uint8_t* input_bytes = (uint8_t*)(data) + byte_offset;
    uint8_t* output_bytes = (uint8_t*)(output_ptr);
    for (i = 0; i < length; i++) {
      output_bytes[i] = (uint8_t)(input_bytes[i] * multiplier);
    }
  } else if (type == node_api_float64_array) {
    double* input_doubles = (double*)((uint8_t*)(data) + byte_offset);
    double* output_doubles = (double*)(output_ptr);
    for (i = 0; i < length; i++) {
      output_doubles[i] = input_doubles[i] * multiplier;
    }
  } else {
    node_api_throw_error(env, NULL,
        "Typed array was of a type not expected by test.");
    return NULL;
  }

  return output_array;
}

static void FinalizeCallback(node_api_env env,
                             void* finalize_data,
                             void* finalize_hint)
{
  free(finalize_data);
}

static node_api_value External(node_api_env env, node_api_callback_info info) {
  const uint8_t nElem = 3;
  int8_t* externalData = malloc(nElem*sizeof(int8_t));
  externalData[0] = 0;
  externalData[1] = 1;
  externalData[2] = 2;

  node_api_value output_buffer;
  NODE_API_CALL(env, node_api_create_external_arraybuffer(
      env,
      externalData,
      nElem*sizeof(int8_t),
      FinalizeCallback,
      NULL,  // finalize_hint
      &output_buffer));

  node_api_value output_array;
  NODE_API_CALL(env, node_api_create_typedarray(env,
      node_api_int8_array,
      nElem,
      output_buffer,
      0,
      &output_array));

  return output_array;
}


static node_api_value
NullArrayBuffer(node_api_env env, node_api_callback_info info) {
  static void* data = NULL;
  node_api_value arraybuffer;
  NODE_API_CALL(env,
      node_api_create_external_arraybuffer(
          env, data, 0, NULL, NULL, &arraybuffer));
  return arraybuffer;
}

static node_api_value
CreateTypedArray(node_api_env env, node_api_callback_info info) {
  size_t argc = 4;
  node_api_value args[4];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 2 || argc == 4, "Wrong number of arguments");

  node_api_value input_array = args[0];
  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, input_array, &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_object,
      "Wrong type of arguments. Expects a typed array as first argument.");

  bool is_typedarray;
  NODE_API_CALL(env, node_api_is_typedarray(env, input_array, &is_typedarray));

  NODE_API_ASSERT(env, is_typedarray,
      "Wrong type of arguments. Expects a typed array as first argument.");

  node_api_valuetype valuetype1;
  node_api_value input_buffer = args[1];
  NODE_API_CALL(env, node_api_typeof(env, input_buffer, &valuetype1));

  NODE_API_ASSERT(env, valuetype1 == node_api_object,
      "Wrong type of arguments. Expects an array buffer as second argument.");

  bool is_arraybuffer;
  NODE_API_CALL(env,
      node_api_is_arraybuffer(env, input_buffer, &is_arraybuffer));

  NODE_API_ASSERT(env, is_arraybuffer,
      "Wrong type of arguments. Expects an array buffer as second argument.");

  node_api_typedarray_type type;
  node_api_value in_array_buffer;
  size_t byte_offset;
  size_t length;
  NODE_API_CALL(env, node_api_get_typedarray_info(
      env, input_array, &type, &length, NULL, &in_array_buffer, &byte_offset));

  if (argc == 4) {
    node_api_valuetype valuetype2;
    NODE_API_CALL(env, node_api_typeof(env, args[2], &valuetype2));

    NODE_API_ASSERT(env, valuetype2 == node_api_number,
        "Wrong type of arguments. Expects a number as third argument.");

    uint32_t uint32_length;
    NODE_API_CALL(env,
        node_api_get_value_uint32(env, args[2], &uint32_length));
    length = uint32_length;

    node_api_valuetype valuetype3;
    NODE_API_CALL(env, node_api_typeof(env, args[3], &valuetype3));

    NODE_API_ASSERT(env, valuetype3 == node_api_number,
        "Wrong type of arguments. Expects a number as third argument.");

    uint32_t uint32_byte_offset;
    NODE_API_CALL(env,
        node_api_get_value_uint32(env, args[3], &uint32_byte_offset));
    byte_offset = uint32_byte_offset;
  }

  node_api_value output_array;
  NODE_API_CALL(env, node_api_create_typedarray(
      env, type, length, input_buffer, byte_offset, &output_array));

  return output_array;
}

static node_api_value Detach(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));
  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments.");

  bool is_typedarray;
  NODE_API_CALL(env, node_api_is_typedarray(env, args[0], &is_typedarray));
  NODE_API_ASSERT(env, is_typedarray,
      "Wrong type of arguments. Expects a typedarray as first argument.");

  node_api_value arraybuffer;
  NODE_API_CALL(env,
      node_api_get_typedarray_info(
          env, args[0], NULL, NULL, NULL, &arraybuffer, NULL));
  NODE_API_CALL(env, node_api_detach_arraybuffer(env, arraybuffer));

  return NULL;
}

static node_api_value
IsDetached(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));
  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments.");

  node_api_value array_buffer = args[0];
  bool is_arraybuffer;
  NODE_API_CALL(env,
      node_api_is_arraybuffer(env, array_buffer, &is_arraybuffer));
  NODE_API_ASSERT(env, is_arraybuffer,
      "Wrong type of arguments. Expects an array buffer as first argument.");

  bool is_detached;
  NODE_API_CALL(env,
      node_api_is_detached_arraybuffer(env, array_buffer, &is_detached));

  node_api_value result;
  NODE_API_CALL(env, node_api_get_boolean(env, is_detached, &result));

  return result;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("Multiply", Multiply),
    DECLARE_NODE_API_PROPERTY("External", External),
    DECLARE_NODE_API_PROPERTY("NullArrayBuffer", NullArrayBuffer),
    DECLARE_NODE_API_PROPERTY("CreateTypedArray", CreateTypedArray),
    DECLARE_NODE_API_PROPERTY("Detach", Detach),
    DECLARE_NODE_API_PROPERTY("IsDetached", IsDetached),
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
