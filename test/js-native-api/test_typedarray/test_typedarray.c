#include <js_native_api.h>
#include <string.h>
#include "../common.h"

static napi_value Multiply(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc == 2, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NAPI_ASSERT(env, valuetype0 == napi_object,
      "Wrong type of arguments. Expects a typed array as first argument.");

  napi_value input_array = args[0];
  bool is_typedarray;
  NAPI_CALL(env, napi_is_typedarray(env, input_array, &is_typedarray));

  NAPI_ASSERT(env, is_typedarray,
      "Wrong type of arguments. Expects a typed array as first argument.");

  napi_valuetype valuetype1;
  NAPI_CALL(env, napi_typeof(env, args[1], &valuetype1));

  NAPI_ASSERT(env, valuetype1 == napi_number,
      "Wrong type of arguments. Expects a number as second argument.");

  double multiplier;
  NAPI_CALL(env, napi_get_value_double(env, args[1], &multiplier));

  napi_typedarray_type type;
  napi_value input_buffer;
  size_t byte_offset;
  size_t i, length;
  NAPI_CALL(env, napi_get_typedarray_info(
      env, input_array, &type, &length, NULL, &input_buffer, &byte_offset));

  void* data;
  size_t byte_length;
  NAPI_CALL(env, napi_get_arraybuffer_info(
      env, input_buffer, &data, &byte_length));

  napi_value output_buffer;
  void* output_ptr = NULL;
  NAPI_CALL(env, napi_create_arraybuffer(
      env, byte_length, &output_ptr, &output_buffer));

  napi_value output_array;
  NAPI_CALL(env, napi_create_typedarray(
      env, type, length, output_buffer, byte_offset, &output_array));

  if (type == napi_uint8_array) {
    uint8_t* input_bytes = (uint8_t*)(data) + byte_offset;
    uint8_t* output_bytes = (uint8_t*)(output_ptr);
    for (i = 0; i < length; i++) {
      output_bytes[i] = (uint8_t)(input_bytes[i] * multiplier);
    }
  } else if (type == napi_float64_array) {
    double* input_doubles = (double*)((uint8_t*)(data) + byte_offset);
    double* output_doubles = (double*)(output_ptr);
    for (i = 0; i < length; i++) {
      output_doubles[i] = input_doubles[i] * multiplier;
    }
  } else {
    napi_throw_error(env, NULL,
        "Typed array was of a type not expected by test.");
    return NULL;
  }

  return output_array;
}

static napi_value External(napi_env env, napi_callback_info info) {
  static int8_t externalData[] = {0, 1, 2};

  napi_value output_buffer;
  NAPI_CALL(env, napi_create_external_arraybuffer(
      env,
      externalData,
      sizeof(externalData),
      NULL,  // finalize_callback
      NULL,  // finalize_hint
      &output_buffer));

  napi_value output_array;
  NAPI_CALL(env, napi_create_typedarray(env,
      napi_int8_array,
      sizeof(externalData) / sizeof(int8_t),
      output_buffer,
      0,
      &output_array));

  return output_array;
}

static napi_value CreateTypedArray(napi_env env, napi_callback_info info) {
  size_t argc = 4;
  napi_value args[4];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc == 2 || argc == 4, "Wrong number of arguments");

  napi_value input_array = args[0];
  napi_valuetype valuetype0;
  NAPI_CALL(env, napi_typeof(env, input_array, &valuetype0));

  NAPI_ASSERT(env, valuetype0 == napi_object,
      "Wrong type of arguments. Expects a typed array as first argument.");

  bool is_typedarray;
  NAPI_CALL(env, napi_is_typedarray(env, input_array, &is_typedarray));

  NAPI_ASSERT(env, is_typedarray,
      "Wrong type of arguments. Expects a typed array as first argument.");

  napi_valuetype valuetype1;
  napi_value input_buffer = args[1];
  NAPI_CALL(env, napi_typeof(env, input_buffer, &valuetype1));

  NAPI_ASSERT(env, valuetype1 == napi_object,
      "Wrong type of arguments. Expects an array buffer as second argument.");

  bool is_arraybuffer;
  NAPI_CALL(env, napi_is_arraybuffer(env, input_buffer, &is_arraybuffer));

  NAPI_ASSERT(env, is_arraybuffer,
      "Wrong type of arguments. Expects an array buffer as second argument.");

  napi_typedarray_type type;
  napi_value in_array_buffer;
  size_t byte_offset;
  size_t length;
  NAPI_CALL(env, napi_get_typedarray_info(
      env, input_array, &type, &length, NULL, &in_array_buffer, &byte_offset));

  if (argc == 4) {
    napi_valuetype valuetype2;
    NAPI_CALL(env, napi_typeof(env, args[2], &valuetype2));

    NAPI_ASSERT(env, valuetype2 == napi_number,
        "Wrong type of arguments. Expects a number as third argument.");

    uint32_t uint32_length;
    NAPI_CALL(env, napi_get_value_uint32(env, args[2], &uint32_length));
    length = uint32_length;

    napi_valuetype valuetype3;
    NAPI_CALL(env, napi_typeof(env, args[3], &valuetype3));

    NAPI_ASSERT(env, valuetype3 == napi_number,
        "Wrong type of arguments. Expects a number as third argument.");

    uint32_t uint32_byte_offset;
    NAPI_CALL(env, napi_get_value_uint32(env, args[3], &uint32_byte_offset));
    byte_offset = uint32_byte_offset;
  }

  napi_value output_array;
  NAPI_CALL(env, napi_create_typedarray(
      env, type, length, input_buffer, byte_offset, &output_array));

  return output_array;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("Multiply", Multiply),
    DECLARE_NAPI_PROPERTY("External", External),
    DECLARE_NAPI_PROPERTY("CreateTypedArray", CreateTypedArray),
  };

  NAPI_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
