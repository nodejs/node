#include <node_api.h>
#include "../common.h"

napi_value testStrictEquals(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  bool bool_result;
  napi_value result;
  NAPI_CALL(env, napi_strict_equals(env, args[0], args[1], &bool_result));
  NAPI_CALL(env, napi_get_boolean(env, bool_result, &result));

  return result;
}

napi_value testGetPrototype(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_value result;
  NAPI_CALL(env, napi_get_prototype(env, args[0], &result));

  return result;
}

napi_value testGetVersion(napi_env env, napi_callback_info info) {
  uint32_t version;
  napi_value result;
  NAPI_CALL(env, napi_get_version(env, &version));
  NAPI_CALL(env, napi_create_uint32(env, version, &result));
  return result;
}

napi_value testGetNodeVersion(napi_env env, napi_callback_info info) {
  const napi_node_version* node_version;
  napi_value result, major, minor, patch, release;
  NAPI_CALL(env, napi_get_node_version(env, &node_version));
  NAPI_CALL(env, napi_create_uint32(env, node_version->major, &major));
  NAPI_CALL(env, napi_create_uint32(env, node_version->minor, &minor));
  NAPI_CALL(env, napi_create_uint32(env, node_version->patch, &patch));
  NAPI_CALL(env, napi_create_string_utf8(env,
                                         node_version->release,
                                         (size_t)-1,
                                         &release));
  NAPI_CALL(env, napi_create_array_with_length(env, 4, &result));
  NAPI_CALL(env, napi_set_element(env, result, 0, major));
  NAPI_CALL(env, napi_set_element(env, result, 1, minor));
  NAPI_CALL(env, napi_set_element(env, result, 2, patch));
  NAPI_CALL(env, napi_set_element(env, result, 3, release));
  return result;
}

napi_value doInstanceOf(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  bool instanceof;
  NAPI_CALL(env, napi_instanceof(env, args[0], args[1], &instanceof));

  napi_value result;
  NAPI_CALL(env, napi_get_boolean(env, instanceof, &result));

  return result;
}

napi_value getNull(napi_env env, napi_callback_info info) {
  napi_value result;
  NAPI_CALL(env, napi_get_null(env, &result));
  return result;
}

napi_value getUndefined(napi_env env, napi_callback_info info) {
  napi_value result;
  NAPI_CALL(env, napi_get_undefined(env, &result));
  return result;
}

napi_value createNapiError(napi_env env, napi_callback_info info) {
  napi_value value;
  NAPI_CALL(env, napi_create_string_utf8(env, "xyz", 3, &value));

  double double_value;
  napi_status status = napi_get_value_double(env, value, &double_value);

  NAPI_ASSERT(env, status != napi_ok, "Failed to produce error condition");

  const napi_extended_error_info *error_info = 0;
  NAPI_CALL(env, napi_get_last_error_info(env, &error_info));

  NAPI_ASSERT(env, error_info->error_code == status,
    "Last error info code should match last status");
  NAPI_ASSERT(env, error_info->error_message,
    "Last error info message should not be null");

  return NULL;
}

napi_value testNapiErrorCleanup(napi_env env, napi_callback_info info) {
  const napi_extended_error_info *error_info = 0;
  NAPI_CALL(env, napi_get_last_error_info(env, &error_info));

  napi_value result;
  bool is_ok = error_info->error_code == napi_ok;
  NAPI_CALL(env, napi_get_boolean(env, is_ok, &result));

  return result;
}

napi_value testNapiTypeof(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  napi_valuetype argument_type;
  NAPI_CALL(env, napi_typeof(env, args[0], &argument_type));

  napi_value result = NULL;
  if (argument_type == napi_number) {
    NAPI_CALL(env, napi_create_string_utf8(env, "number", -1, &result));
  } else if (argument_type == napi_string) {
    NAPI_CALL(env, napi_create_string_utf8(env, "string", -1, &result));
  } else if (argument_type == napi_function) {
    NAPI_CALL(env, napi_create_string_utf8(env, "function", -1, &result));
  } else if (argument_type == napi_object) {
    NAPI_CALL(env, napi_create_string_utf8(env, "object", -1, &result));
  } else if (argument_type == napi_boolean) {
    NAPI_CALL(env, napi_create_string_utf8(env, "boolean", -1, &result));
  } else if (argument_type == napi_undefined) {
    NAPI_CALL(env, napi_create_string_utf8(env, "undefined", -1, &result));
  } else if (argument_type == napi_symbol) {
    NAPI_CALL(env, napi_create_string_utf8(env, "symbol", -1, &result));
  } else if (argument_type == napi_null) {
    NAPI_CALL(env, napi_create_string_utf8(env, "null", -1, &result));
  }
  return result;
}

static bool deref_item_called = false;
static void deref_item(napi_env env, void* data, void* hint) {
  (void) hint;

  deref_item_called = true;
  NAPI_CALL_RETURN_VOID(env, napi_delete_reference(env, (napi_ref)data));
}

napi_value deref_item_was_called(napi_env env, napi_callback_info info) {
  napi_value it_was_called;

  NAPI_CALL(env, napi_get_boolean(env, deref_item_called, &it_was_called));

  return it_was_called;
}

napi_value wrap(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value argv[2];
  napi_ref payload;

  deref_item_called = false;

  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));
  NAPI_CALL(env, napi_create_reference(env, argv[1], 1, &payload));
  NAPI_CALL(env, napi_wrap(env, argv[0], payload, deref_item, NULL, NULL));

  return NULL;
}

napi_value remove_wrap(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value wrapped;
  void* data;

  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &wrapped, NULL, NULL));
  NAPI_CALL(env, napi_remove_wrap(env, wrapped, &data));
  if (data != NULL) {
    NAPI_CALL(env, napi_delete_reference(env, (napi_ref)data));
  }

  return NULL;
}

static bool finalize_called = false;
static void test_finalize(napi_env env, void* data, void* hint) {
  finalize_called = true;
}

napi_value test_finalize_wrap(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value js_object;

  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &js_object, NULL, NULL));
  NAPI_CALL(env, napi_wrap(env, js_object, NULL, test_finalize, NULL, NULL));

  return NULL;
}

napi_value finalize_was_called(napi_env env, napi_callback_info info) {
  napi_value it_was_called;

  NAPI_CALL(env, napi_get_boolean(env, finalize_called, &it_was_called));

  return it_was_called;
}

napi_value testAdjustExternalMemory(napi_env env, napi_callback_info info) {
  napi_value result;
  int64_t adjustedValue;

  NAPI_CALL(env, napi_adjust_external_memory(env, 1, &adjustedValue));
  NAPI_CALL(env, napi_create_double(env, adjustedValue, &result));

  return result;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("testStrictEquals", testStrictEquals),
    DECLARE_NAPI_PROPERTY("testGetPrototype", testGetPrototype),
    DECLARE_NAPI_PROPERTY("testGetVersion", testGetVersion),
    DECLARE_NAPI_PROPERTY("testGetNodeVersion", testGetNodeVersion),
    DECLARE_NAPI_PROPERTY("doInstanceOf", doInstanceOf),
    DECLARE_NAPI_PROPERTY("getUndefined", getUndefined),
    DECLARE_NAPI_PROPERTY("getNull", getNull),
    DECLARE_NAPI_PROPERTY("createNapiError", createNapiError),
    DECLARE_NAPI_PROPERTY("testNapiErrorCleanup", testNapiErrorCleanup),
    DECLARE_NAPI_PROPERTY("testNapiTypeof", testNapiTypeof),
    DECLARE_NAPI_PROPERTY("wrap", wrap),
    DECLARE_NAPI_PROPERTY("removeWrap", remove_wrap),
    DECLARE_NAPI_PROPERTY("testFinalizeWrap", test_finalize_wrap),
    DECLARE_NAPI_PROPERTY("finalizeWasCalled", finalize_was_called),
    DECLARE_NAPI_PROPERTY("derefItemWasCalled", deref_item_was_called),
    DECLARE_NAPI_PROPERTY("testAdjustExternalMemory", testAdjustExternalMemory)
  };

  NAPI_CALL_RETURN_VOID(env, napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));
}

NAPI_MODULE(addon, Init)
