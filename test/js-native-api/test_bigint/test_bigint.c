#include <limits.h>
#include <inttypes.h>
#include <stdio.h>
#include <js_native_api.h>
#include "../common.h"

static node_api_value
IsLossless(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  bool is_signed;
  NODE_API_CALL(env, node_api_get_value_bool(env, args[1], &is_signed));

  bool lossless;

  if (is_signed) {
    int64_t input;
    NODE_API_CALL(env,
        node_api_get_value_bigint_int64(env, args[0], &input, &lossless));
  } else {
    uint64_t input;
    NODE_API_CALL(env,
        node_api_get_value_bigint_uint64(env, args[0], &input, &lossless));
  }

  node_api_value output;
  NODE_API_CALL(env, node_api_get_boolean(env, lossless, &output));

  return output;
}

static node_api_value
TestInt64(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_bigint,
      "Wrong type of arguments. Expects a bigint as first argument.");

  int64_t input;
  bool lossless;
  NODE_API_CALL(env,
      node_api_get_value_bigint_int64(env, args[0], &input, &lossless));

  node_api_value output;
  NODE_API_CALL(env, node_api_create_bigint_int64(env, input, &output));

  return output;
}

static node_api_value
TestUint64(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_bigint,
      "Wrong type of arguments. Expects a bigint as first argument.");

  uint64_t input;
  bool lossless;
  NODE_API_CALL(env, node_api_get_value_bigint_uint64(
        env, args[0], &input, &lossless));

  node_api_value output;
  NODE_API_CALL(env, node_api_create_bigint_uint64(env, input, &output));

  return output;
}

static node_api_value
TestWords(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_bigint,
      "Wrong type of arguments. Expects a bigint as first argument.");

  size_t expected_word_count;
  NODE_API_CALL(env, node_api_get_value_bigint_words(
        env, args[0], NULL, &expected_word_count, NULL));

  int sign_bit;
  size_t word_count = 10;
  uint64_t words[10];

  NODE_API_CALL(env, node_api_get_value_bigint_words(
        env, args[0], &sign_bit, &word_count, words));

  NODE_API_ASSERT(env, word_count == expected_word_count,
      "word counts do not match");

  node_api_value output;
  NODE_API_CALL(env,
      node_api_create_bigint_words(env, sign_bit, word_count, words, &output));

  return output;
}

// throws RangeError
static node_api_value
CreateTooBigBigInt(node_api_env env, node_api_callback_info info) {
  int sign_bit = 0;
  size_t word_count = SIZE_MAX;
  uint64_t words[10];

  node_api_value output;

  NODE_API_CALL(env, node_api_create_bigint_words(
        env, sign_bit, word_count, words, &output));

  return output;
}

// Test that we correctly forward exceptions from the engine.
static node_api_value
MakeBigIntWordsThrow(node_api_env env, node_api_callback_info info) {
  uint64_t words[10];
  node_api_value output;

  node_api_status status = node_api_create_bigint_words(env,
                                                        0,
                                                        INT_MAX,
                                                        words,
                                                        &output);
  if (status != node_api_pending_exception)
    node_api_throw_error(env,
                         NULL,
                         "Expected status `node_api_pending_exception`");

  return NULL;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("IsLossless", IsLossless),
    DECLARE_NODE_API_PROPERTY("TestInt64", TestInt64),
    DECLARE_NODE_API_PROPERTY("TestUint64", TestUint64),
    DECLARE_NODE_API_PROPERTY("TestWords", TestWords),
    DECLARE_NODE_API_PROPERTY("CreateTooBigBigInt", CreateTooBigBigInt),
    DECLARE_NODE_API_PROPERTY("MakeBigIntWordsThrow", MakeBigIntWordsThrow),
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
