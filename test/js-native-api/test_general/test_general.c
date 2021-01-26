#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <js_native_api.h>
#include "../common.h"

static node_api_value
testStrictEquals(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  bool bool_result;
  node_api_value result;
  NODE_API_CALL(env,
      node_api_strict_equals(env, args[0], args[1], &bool_result));
  NODE_API_CALL(env, node_api_get_boolean(env, bool_result, &result));

  return result;
}

static node_api_value
testGetPrototype(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  node_api_value result;
  NODE_API_CALL(env, node_api_get_prototype(env, args[0], &result));

  return result;
}

static node_api_value
testGetVersion(node_api_env env, node_api_callback_info info) {
  uint32_t version;
  node_api_value result;
  NODE_API_CALL(env, node_api_get_version(env, &version));
  NODE_API_CALL(env, node_api_create_uint32(env, version, &result));
  return result;
}

static node_api_value
doInstanceOf(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  bool instanceof;
  NODE_API_CALL(env, node_api_instanceof(env, args[0], args[1], &instanceof));

  node_api_value result;
  NODE_API_CALL(env, node_api_get_boolean(env, instanceof, &result));

  return result;
}

static node_api_value
getNull(node_api_env env, node_api_callback_info info) {
  node_api_value result;
  NODE_API_CALL(env, node_api_get_null(env, &result));
  return result;
}

static node_api_value
getUndefined(node_api_env env, node_api_callback_info info) {
  node_api_value result;
  NODE_API_CALL(env, node_api_get_undefined(env, &result));
  return result;
}

static node_api_value
createNapiError(node_api_env env, node_api_callback_info info) {
  node_api_value value;
  NODE_API_CALL(env, node_api_create_string_utf8(env, "xyz", 3, &value));

  double double_value;
  node_api_status status =
      node_api_get_value_double(env, value, &double_value);

  NODE_API_ASSERT(env, status != node_api_ok,
      "Failed to produce error condition");

  const node_api_extended_error_info *error_info = 0;
  NODE_API_CALL(env, node_api_get_last_error_info(env, &error_info));

  NODE_API_ASSERT(env, error_info->error_code == status,
      "Last error info code should match last status");
  NODE_API_ASSERT(env, error_info->error_message,
      "Last error info message should not be null");

  return NULL;
}

static node_api_value
testNapiErrorCleanup(node_api_env env, node_api_callback_info info) {
  const node_api_extended_error_info *error_info = 0;
  NODE_API_CALL(env, node_api_get_last_error_info(env, &error_info));

  node_api_value result;
  bool is_ok = error_info->error_code == node_api_ok;
  NODE_API_CALL(env, node_api_get_boolean(env, is_ok, &result));

  return result;
}

static node_api_value
testNapiTypeof(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  node_api_valuetype argument_type;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &argument_type));

  node_api_value result = NULL;
  if (argument_type == node_api_number) {
    NODE_API_CALL(env, node_api_create_string_utf8(
        env, "number", NODE_API_AUTO_LENGTH, &result));
  } else if (argument_type == node_api_string) {
    NODE_API_CALL(env, node_api_create_string_utf8(
        env, "string", NODE_API_AUTO_LENGTH, &result));
  } else if (argument_type == node_api_function) {
    NODE_API_CALL(env, node_api_create_string_utf8(
        env, "function", NODE_API_AUTO_LENGTH, &result));
  } else if (argument_type == node_api_object) {
    NODE_API_CALL(env, node_api_create_string_utf8(
        env, "object", NODE_API_AUTO_LENGTH, &result));
  } else if (argument_type == node_api_boolean) {
    NODE_API_CALL(env, node_api_create_string_utf8(
        env, "boolean", NODE_API_AUTO_LENGTH, &result));
  } else if (argument_type == node_api_undefined) {
    NODE_API_CALL(env, node_api_create_string_utf8(
        env, "undefined", NODE_API_AUTO_LENGTH, &result));
  } else if (argument_type == node_api_symbol) {
    NODE_API_CALL(env, node_api_create_string_utf8(
        env, "symbol", NODE_API_AUTO_LENGTH, &result));
  } else if (argument_type == node_api_null) {
    NODE_API_CALL(env, node_api_create_string_utf8(
        env, "null", NODE_API_AUTO_LENGTH, &result));
  }
  return result;
}

static bool deref_item_called = false;
static void deref_item(node_api_env env, void* data, void* hint) {
  (void) hint;

  NODE_API_ASSERT_RETURN_VOID(env, data == &deref_item_called,
    "Finalize callback was called with the correct pointer");

  deref_item_called = true;
}

static node_api_value
deref_item_was_called(node_api_env env, node_api_callback_info info) {
  node_api_value it_was_called;

  NODE_API_CALL(env,
      node_api_get_boolean(env, deref_item_called, &it_was_called));

  return it_was_called;
}

static node_api_value wrap_first_arg(node_api_env env,
                                     node_api_callback_info info,
                                     node_api_finalize finalizer,
                                     void* data) {
  size_t argc = 1;
  node_api_value to_wrap;

  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, &to_wrap, NULL, NULL));
  NODE_API_CALL(env, node_api_wrap(env, to_wrap, data, finalizer, NULL, NULL));

  return to_wrap;
}

static node_api_value wrap(node_api_env env, node_api_callback_info info) {
  deref_item_called = false;
  return wrap_first_arg(env, info, deref_item, &deref_item_called);
}

static node_api_value unwrap(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value wrapped;
  void* data;

  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, &wrapped, NULL, NULL));
  NODE_API_CALL(env, node_api_unwrap(env, wrapped, &data));

  return NULL;
}

static node_api_value
remove_wrap(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value wrapped;
  void* data;

  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, &wrapped, NULL, NULL));
  NODE_API_CALL(env, node_api_remove_wrap(env, wrapped, &data));

  return NULL;
}

static bool finalize_called = false;
static void test_finalize(node_api_env env, void* data, void* hint) {
  finalize_called = true;
}

static node_api_value
test_finalize_wrap(node_api_env env, node_api_callback_info info) {
  return wrap_first_arg(env, info, test_finalize, NULL);
}

static node_api_value
finalize_was_called(node_api_env env, node_api_callback_info info) {
  node_api_value it_was_called;

  NODE_API_CALL(env,
      node_api_get_boolean(env, finalize_called, &it_was_called));

  return it_was_called;
}

static node_api_value
testAdjustExternalMemory(node_api_env env, node_api_callback_info info) {
  node_api_value result;
  int64_t adjustedValue;

  NODE_API_CALL(env, node_api_adjust_external_memory(env, 1, &adjustedValue));
  NODE_API_CALL(env,
      node_api_create_double(env, (double)adjustedValue, &result));

  return result;
}

static node_api_value
testNapiRun(node_api_env env, node_api_callback_info info) {
  node_api_value script, result;
  size_t argc = 1;

  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, &script, NULL, NULL));

  NODE_API_CALL(env, node_api_run_script(env, script, &result));

  return result;
}

static void finalizer_only_callback(node_api_env env, void* data, void* hint) {
  node_api_ref js_cb_ref = data;
  node_api_value js_cb, undefined;
  NODE_API_CALL_RETURN_VOID(env,
      node_api_get_reference_value(env, js_cb_ref, &js_cb));
  NODE_API_CALL_RETURN_VOID(env, node_api_get_undefined(env, &undefined));
  NODE_API_CALL_RETURN_VOID(env,
      node_api_call_function(env, undefined, js_cb, 0, NULL, NULL));
  NODE_API_CALL_RETURN_VOID(env, node_api_delete_reference(env, js_cb_ref));
}

static node_api_value
add_finalizer_only(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value argv[2];
  node_api_ref js_cb_ref;

  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, argv, NULL, NULL));
  NODE_API_CALL(env, node_api_create_reference(env, argv[1], 1, &js_cb_ref));
  NODE_API_CALL(env,
      node_api_add_finalizer(env,
                         argv[0],
                         js_cb_ref,
                         finalizer_only_callback,
                         NULL,
                         NULL));
  return NULL;
}

static const char* env_cleanup_finalizer_messages[] = {
  "simple wrap",
  "wrap, removeWrap",
  "first wrap",
  "second wrap"
};

static void cleanup_env_finalizer(node_api_env env, void* data, void* hint) {
  (void) env;
  (void) hint;

  printf("finalize at env cleanup for %s\n",
      env_cleanup_finalizer_messages[(uintptr_t)data]);
}

static node_api_value
env_cleanup_wrap(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value argv[2];
  uint32_t value;
  uintptr_t ptr_value;
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, argv, NULL, NULL));

  NODE_API_CALL(env, node_api_get_value_uint32(env, argv[1], &value));

  ptr_value = value;
  return wrap_first_arg(env, info, cleanup_env_finalizer, (void*)ptr_value);
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("testStrictEquals", testStrictEquals),
    DECLARE_NODE_API_PROPERTY("testGetPrototype", testGetPrototype),
    DECLARE_NODE_API_PROPERTY("testGetVersion", testGetVersion),
    DECLARE_NODE_API_PROPERTY("testNapiRun", testNapiRun),
    DECLARE_NODE_API_PROPERTY("doInstanceOf", doInstanceOf),
    DECLARE_NODE_API_PROPERTY("getUndefined", getUndefined),
    DECLARE_NODE_API_PROPERTY("getNull", getNull),
    DECLARE_NODE_API_PROPERTY("createNapiError", createNapiError),
    DECLARE_NODE_API_PROPERTY("testNapiErrorCleanup", testNapiErrorCleanup),
    DECLARE_NODE_API_PROPERTY("testNapiTypeof", testNapiTypeof),
    DECLARE_NODE_API_PROPERTY("wrap", wrap),
    DECLARE_NODE_API_PROPERTY("envCleanupWrap", env_cleanup_wrap),
    DECLARE_NODE_API_PROPERTY("unwrap", unwrap),
    DECLARE_NODE_API_PROPERTY("removeWrap", remove_wrap),
    DECLARE_NODE_API_PROPERTY("addFinalizerOnly", add_finalizer_only),
    DECLARE_NODE_API_PROPERTY("testFinalizeWrap", test_finalize_wrap),
    DECLARE_NODE_API_PROPERTY("finalizeWasCalled", finalize_was_called),
    DECLARE_NODE_API_PROPERTY("derefItemWasCalled", deref_item_was_called),
    DECLARE_NODE_API_PROPERTY(
        "testAdjustExternalMemory", testAdjustExternalMemory)
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
