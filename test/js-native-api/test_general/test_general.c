// we define NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED here to
// validate that it can be used as a form of test itself. It is
// not related to any of the other tests
// defined in the file
#define NODE_API_NO_EXTERNAL_BUFFERS_ALLOWED
#include <js_native_api.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "../common.h"
#include "../entry_point.h"

static napi_value testStrictEquals(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  bool bool_result;
  napi_value result;
  NODE_API_CALL(env, napi_strict_equals(env, args[0], args[1], &bool_result));
  NODE_API_CALL(env, napi_get_boolean(env, bool_result, &result));

  return result;
}

static napi_value testGetPrototype(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value result;
  NODE_API_CALL(env, napi_get_prototype(env, args[0], &result));

  return result;
}

static napi_value testGetVersion(napi_env env, napi_callback_info info) {
  uint32_t version;
  napi_value result;
  NODE_API_CALL(env, napi_get_version(env, &version));
  NODE_API_CALL(env, napi_create_uint32(env, version, &result));
  return result;
}

static napi_value doInstanceOf(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  bool instanceof;
  NODE_API_CALL(env, napi_instanceof(env, args[0], args[1], &instanceof));

  napi_value result;
  NODE_API_CALL(env, napi_get_boolean(env, instanceof, &result));

  return result;
}

static napi_value getNull(napi_env env, napi_callback_info info) {
  napi_value result;
  NODE_API_CALL(env, napi_get_null(env, &result));
  return result;
}

static napi_value getUndefined(napi_env env, napi_callback_info info) {
  napi_value result;
  NODE_API_CALL(env, napi_get_undefined(env, &result));
  return result;
}

static napi_value createNapiError(napi_env env, napi_callback_info info) {
  napi_value value;
  NODE_API_CALL(env, napi_create_string_utf8(env, "xyz", 3, &value));

  double double_value;
  napi_status status = napi_get_value_double(env, value, &double_value);

  NODE_API_ASSERT(env, status != napi_ok, "Failed to produce error condition");

  const napi_extended_error_info *error_info = 0;
  NODE_API_CALL(env, napi_get_last_error_info(env, &error_info));

  NODE_API_ASSERT(env, error_info->error_code == status,
      "Last error info code should match last status");
  NODE_API_ASSERT(env, error_info->error_message,
      "Last error info message should not be null");

  return NULL;
}

static napi_value testNapiErrorCleanup(napi_env env, napi_callback_info info) {
  const napi_extended_error_info *error_info = 0;
  NODE_API_CALL(env, napi_get_last_error_info(env, &error_info));

  napi_value result;
  bool is_ok = error_info->error_code == napi_ok;
  NODE_API_CALL(env, napi_get_boolean(env, is_ok, &result));

  return result;
}

static napi_value testNapiTypeof(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_valuetype argument_type;
  NODE_API_CALL(env, napi_typeof(env, args[0], &argument_type));

  napi_value result = NULL;
  if (argument_type == napi_number) {
    NODE_API_CALL(env, napi_create_string_utf8(
        env, "number", NAPI_AUTO_LENGTH, &result));
  } else if (argument_type == napi_string) {
    NODE_API_CALL(env, napi_create_string_utf8(
        env, "string", NAPI_AUTO_LENGTH, &result));
  } else if (argument_type == napi_function) {
    NODE_API_CALL(env, napi_create_string_utf8(
        env, "function", NAPI_AUTO_LENGTH, &result));
  } else if (argument_type == napi_object) {
    NODE_API_CALL(env, napi_create_string_utf8(
        env, "object", NAPI_AUTO_LENGTH, &result));
  } else if (argument_type == napi_boolean) {
    NODE_API_CALL(env, napi_create_string_utf8(
        env, "boolean", NAPI_AUTO_LENGTH, &result));
  } else if (argument_type == napi_undefined) {
    NODE_API_CALL(env, napi_create_string_utf8(
        env, "undefined", NAPI_AUTO_LENGTH, &result));
  } else if (argument_type == napi_symbol) {
    NODE_API_CALL(env, napi_create_string_utf8(
        env, "symbol", NAPI_AUTO_LENGTH, &result));
  } else if (argument_type == napi_null) {
    NODE_API_CALL(env, napi_create_string_utf8(
        env, "null", NAPI_AUTO_LENGTH, &result));
  }
  return result;
}

static bool deref_item_called = false;
static void deref_item(napi_env env, void* data, void* hint) {
  (void) hint;

  NODE_API_ASSERT_RETURN_VOID(env, data == &deref_item_called,
    "Finalize callback was called with the correct pointer");

  deref_item_called = true;
}

static napi_value deref_item_was_called(napi_env env, napi_callback_info info) {
  napi_value it_was_called;

  NODE_API_CALL(env, napi_get_boolean(env, deref_item_called, &it_was_called));

  return it_was_called;
}

static napi_value wrap_first_arg(napi_env env,
                                 napi_callback_info info,
                                 napi_finalize finalizer,
                                 void* data) {
  size_t argc = 1;
  napi_value to_wrap;

  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, &to_wrap, NULL, NULL));
  NODE_API_CALL(env, napi_wrap(env, to_wrap, data, finalizer, NULL, NULL));

  return to_wrap;
}

static napi_value wrap(napi_env env, napi_callback_info info) {
  deref_item_called = false;
  return wrap_first_arg(env, info, deref_item, &deref_item_called);
}

static napi_value unwrap(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value wrapped;
  void* data;

  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, &wrapped, NULL, NULL));
  NODE_API_CALL(env, napi_unwrap(env, wrapped, &data));

  return NULL;
}

static napi_value remove_wrap(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value wrapped;
  void* data;

  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, &wrapped, NULL, NULL));
  NODE_API_CALL(env, napi_remove_wrap(env, wrapped, &data));

  return NULL;
}

static bool finalize_called = false;
static void test_finalize(napi_env env, void* data, void* hint) {
  finalize_called = true;
}

static napi_value test_finalize_wrap(napi_env env, napi_callback_info info) {
  return wrap_first_arg(env, info, test_finalize, NULL);
}

static napi_value finalize_was_called(napi_env env, napi_callback_info info) {
  napi_value it_was_called;

  NODE_API_CALL(env, napi_get_boolean(env, finalize_called, &it_was_called));

  return it_was_called;
}

static napi_value testAdjustExternalMemory(napi_env env, napi_callback_info info) {
  napi_value result;
  int64_t adjustedValue;

  NODE_API_CALL(env, napi_adjust_external_memory(env, 1, &adjustedValue));
  NODE_API_CALL(env, napi_create_double(env, (double)adjustedValue, &result));

  return result;
}

static napi_value testNapiRun(napi_env env, napi_callback_info info) {
  napi_value script, result;
  size_t argc = 1;

  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, &script, NULL, NULL));

  NODE_API_CALL(env, napi_run_script(env, script, &result));

  return result;
}

static void finalizer_only_callback(napi_env env, void* data, void* hint) {
  napi_ref js_cb_ref = data;
  napi_value js_cb, undefined;
  NODE_API_CALL_RETURN_VOID(env, napi_get_reference_value(env, js_cb_ref, &js_cb));
  NODE_API_CALL_RETURN_VOID(env, napi_get_undefined(env, &undefined));
  NODE_API_CALL_RETURN_VOID(env,
      napi_call_function(env, undefined, js_cb, 0, NULL, NULL));
  NODE_API_CALL_RETURN_VOID(env, napi_delete_reference(env, js_cb_ref));
}

static napi_value add_finalizer_only(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2];
  napi_ref js_cb_ref;

  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  NODE_API_CALL(env, napi_create_reference(env, argv[1], 1, &js_cb_ref));
  NODE_API_CALL(env,
      napi_add_finalizer(
          env, argv[0], js_cb_ref, finalizer_only_callback, NULL, NULL));
  return NULL;
}

static const char* env_cleanup_finalizer_messages[] = {
  "simple wrap",
  "wrap, removeWrap",
  "first wrap",
  "second wrap"
};

static void cleanup_env_finalizer(napi_env env, void* data, void* hint) {
  (void) env;
  (void) hint;

  printf("finalize at env cleanup for %s\n",
      env_cleanup_finalizer_messages[(uintptr_t)data]);
}

static napi_value env_cleanup_wrap(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2];
  uint32_t value;
  uintptr_t ptr_value;
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));

  NODE_API_CALL(env, napi_get_value_uint32(env, argv[1], &value));

  ptr_value = value;
  return wrap_first_arg(env, info, cleanup_env_finalizer, (void*)ptr_value);
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
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
    DECLARE_NODE_API_PROPERTY("testAdjustExternalMemory", testAdjustExternalMemory)
  };

  NODE_API_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
