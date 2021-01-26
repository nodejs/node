#include <limits.h>  // INT_MAX
#include <string.h>
#include <js_native_api.h>
#include "../common.h"

static node_api_value
TestLatin1(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype));

  NODE_API_ASSERT(env, valuetype == node_api_string,
      "Wrong type of argment. Expects a string.");

  char buffer[128];
  size_t buffer_size = 128;
  size_t copied;

  NODE_API_CALL(env,
      node_api_get_value_string_latin1(
          env, args[0], buffer, buffer_size, &copied));

  node_api_value output;
  NODE_API_CALL(env,
      node_api_create_string_latin1(env, buffer, copied, &output));

  return output;
}

static node_api_value TestUtf8(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype));

  NODE_API_ASSERT(env, valuetype == node_api_string,
      "Wrong type of argment. Expects a string.");

  char buffer[128];
  size_t buffer_size = 128;
  size_t copied;

  NODE_API_CALL(env,
      node_api_get_value_string_utf8(
          env, args[0], buffer, buffer_size, &copied));

  node_api_value output;
  NODE_API_CALL(env,
      node_api_create_string_utf8(env, buffer, copied, &output));

  return output;
}

static node_api_value
TestUtf16(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype));

  NODE_API_ASSERT(env, valuetype == node_api_string,
      "Wrong type of argment. Expects a string.");

  char16_t buffer[128];
  size_t buffer_size = 128;
  size_t copied;

  NODE_API_CALL(env,
      node_api_get_value_string_utf16(
          env, args[0], buffer, buffer_size, &copied));

  node_api_value output;
  NODE_API_CALL(env,
      node_api_create_string_utf16(env, buffer, copied, &output));

  return output;
}

static node_api_value
TestLatin1Insufficient(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype));

  NODE_API_ASSERT(env, valuetype == node_api_string,
      "Wrong type of argment. Expects a string.");

  char buffer[4];
  size_t buffer_size = 4;
  size_t copied;

  NODE_API_CALL(env,
      node_api_get_value_string_latin1(
          env, args[0], buffer, buffer_size, &copied));

  node_api_value output;
  NODE_API_CALL(env,
      node_api_create_string_latin1(env, buffer, copied, &output));

  return output;
}

static node_api_value
TestUtf8Insufficient(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype));

  NODE_API_ASSERT(env, valuetype == node_api_string,
      "Wrong type of argment. Expects a string.");

  char buffer[4];
  size_t buffer_size = 4;
  size_t copied;

  NODE_API_CALL(env,
      node_api_get_value_string_utf8(
          env, args[0], buffer, buffer_size, &copied));

  node_api_value output;
  NODE_API_CALL(env,
      node_api_create_string_utf8(env, buffer, copied, &output));

  return output;
}

static node_api_value
TestUtf16Insufficient(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype));

  NODE_API_ASSERT(env, valuetype == node_api_string,
      "Wrong type of argment. Expects a string.");

  char16_t buffer[4];
  size_t buffer_size = 4;
  size_t copied;

  NODE_API_CALL(env,
      node_api_get_value_string_utf16(
          env, args[0], buffer, buffer_size, &copied));

  node_api_value output;
  NODE_API_CALL(env,
      node_api_create_string_utf16(env, buffer, copied, &output));

  return output;
}

static node_api_value
Utf16Length(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype));

  NODE_API_ASSERT(env, valuetype == node_api_string,
      "Wrong type of argment. Expects a string.");

  size_t length;
  NODE_API_CALL(env,
      node_api_get_value_string_utf16(env, args[0], NULL, 0, &length));

  node_api_value output;
  NODE_API_CALL(env, node_api_create_uint32(env, (uint32_t)length, &output));

  return output;
}

static node_api_value
Utf8Length(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype));

  NODE_API_ASSERT(env, valuetype == node_api_string,
      "Wrong type of argment. Expects a string.");

  size_t length;
  NODE_API_CALL(env,
      node_api_get_value_string_utf8(env, args[0], NULL, 0, &length));

  node_api_value output;
  NODE_API_CALL(env, node_api_create_uint32(env, (uint32_t)length, &output));

  return output;
}

static node_api_value
TestLargeUtf8(node_api_env env, node_api_callback_info info) {
  node_api_value output;
  if (SIZE_MAX > INT_MAX) {
    NODE_API_CALL(env,
        node_api_create_string_utf8(env, "", ((size_t)INT_MAX) + 1, &output));
  } else {
    // just throw the expected error as there is nothing to test
    // in this case since we can't overflow
    NODE_API_CALL(env, node_api_throw_error(env, NULL, "Invalid argument"));
  }

  return output;
}

static node_api_value
TestLargeLatin1(node_api_env env, node_api_callback_info info) {
  node_api_value output;
  if (SIZE_MAX > INT_MAX) {
    NODE_API_CALL(env,
        node_api_create_string_latin1(
            env, "", ((size_t)INT_MAX) + 1, &output));
  } else {
    // just throw the expected error as there is nothing to test
    // in this case since we can't overflow
    NODE_API_CALL(env, node_api_throw_error(env, NULL, "Invalid argument"));
  }

  return output;
}

static node_api_value
TestLargeUtf16(node_api_env env, node_api_callback_info info) {
  node_api_value output;
  if (SIZE_MAX > INT_MAX) {
    NODE_API_CALL(env,
        node_api_create_string_utf16(
            env, ((const char16_t*)""), ((size_t)INT_MAX) + 1, &output));
  } else {
    // just throw the expected error as there is nothing to test
    // in this case since we can't overflow
    NODE_API_CALL(env, node_api_throw_error(env, NULL, "Invalid argument"));
  }

  return output;
}

static node_api_value
TestMemoryCorruption(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");

  char buf[10] = { 0 };
  NODE_API_CALL(env,
      node_api_get_value_string_utf8(env, args[0], buf, 0, NULL));

  char zero[10] = { 0 };
  if (memcmp(buf, zero, sizeof(buf)) != 0) {
    NODE_API_CALL(env, node_api_throw_error(env, NULL, "Buffer overwritten"));
  }

  return NULL;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor properties[] = {
    DECLARE_NODE_API_PROPERTY("TestLatin1", TestLatin1),
    DECLARE_NODE_API_PROPERTY("TestLatin1Insufficient",
        TestLatin1Insufficient),
    DECLARE_NODE_API_PROPERTY("TestUtf8", TestUtf8),
    DECLARE_NODE_API_PROPERTY("TestUtf8Insufficient", TestUtf8Insufficient),
    DECLARE_NODE_API_PROPERTY("TestUtf16", TestUtf16),
    DECLARE_NODE_API_PROPERTY("TestUtf16Insufficient", TestUtf16Insufficient),
    DECLARE_NODE_API_PROPERTY("Utf16Length", Utf16Length),
    DECLARE_NODE_API_PROPERTY("Utf8Length", Utf8Length),
    DECLARE_NODE_API_PROPERTY("TestLargeUtf8", TestLargeUtf8),
    DECLARE_NODE_API_PROPERTY("TestLargeLatin1", TestLargeLatin1),
    DECLARE_NODE_API_PROPERTY("TestLargeUtf16", TestLargeUtf16),
    DECLARE_NODE_API_PROPERTY("TestMemoryCorruption", TestMemoryCorruption),
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}
EXTERN_C_END
