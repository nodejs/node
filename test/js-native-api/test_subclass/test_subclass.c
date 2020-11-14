#include <stdlib.h>
#include <string.h>
#define NAPI_EXPERIMENTAL
#include <js_native_api.h>
#include "../common.h"

static napi_value NativeClass(napi_env env, napi_callback_info info) {
  return NULL;
}

static napi_value SuperMethod(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value param;
  int32_t value;
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &param, NULL, NULL));
  NAPI_CALL(env, napi_get_value_int32(env, param, &value));
  NAPI_CALL(env, napi_create_int32(env, ++value, &param));
  return param;
}

static napi_value SuperChainableMethod(napi_env env, napi_callback_info info) {
  size_t argc = 1, length;
  napi_value param;
  char* str;
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &param, NULL, NULL));
  NAPI_CALL(env, napi_get_value_string_utf8(env, param, NULL, 0, &length));
  str = malloc(length + 3);
  memset(str, 0, length + 3);
  NAPI_CALL(env,
            napi_get_value_string_utf8(env, param, str, length + 1, &length));
  str[length] = '-';
  str[length + 1] = '1';
  NAPI_CALL(env, napi_create_string_utf8(env, str, length + 2, &param));
  free(str);
  return param;
}

static napi_status InitNativeClass(napi_env env, napi_value* result) {
  napi_property_descriptor props[] = {
    DECLARE_NAPI_PROPERTY("superMethod", SuperMethod),
    DECLARE_NAPI_PROPERTY("chainableMethod", SuperChainableMethod),
    { "value", NULL, NULL, NULL, NULL, NULL, napi_enumerable, NULL },
  };

  STATUS_CALL(napi_create_uint32(env, 5, &props[2].value));

  return napi_define_class(env,
                           "NativeClass",
                           NAPI_AUTO_LENGTH,
                           NativeClass,
                           NULL,
                           sizeof(props) / sizeof(*props),
                           props,
                           result);
}

static napi_value NativeSubclass(napi_env env, napi_callback_info info) {
  return NULL;
}

static napi_value SubMethod(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value param;
  int32_t value;
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &param, NULL, NULL));
  NAPI_CALL(env, napi_get_value_int32(env, param, &value));
  NAPI_CALL(env, napi_create_int32(env, --value, &param));
  return param;
}

static napi_value SubChainableMethod(napi_env env, napi_callback_info info) {
  size_t argc = 1, length;
  char* str;
  napi_value param, super_fn, js_this;
  napi_ref super_ref;
  NAPI_CALL(env, napi_get_cb_info(env,
                                  info,
                                  &argc,
                                  &param,
                                  &js_this,
                                  (void**)(&super_ref)));

  // Chain up.
  NAPI_CALL(env, napi_get_reference_value(env, super_ref, &super_fn));
  NAPI_CALL(env, napi_call_function(env, js_this, super_fn, 1, &param, &param));

  // Prepend "1-"
  NAPI_CALL(env, napi_get_value_string_utf8(env, param, NULL, 0, &length));
  str = malloc(length + 3);
  memset(str, 0, length + 3);
  NAPI_CALL(env, napi_get_value_string_utf8(env,
                                            param,
                                            &str[2],
                                            length + 1,
                                            &length));
  str[0] = '1';
  str[1] = '-';
  NAPI_CALL(env, napi_create_string_utf8(env, str, length + 2, &param));
  free(str);
  return param;
}

static void DeleteSuperRef(napi_env env, void* data, void* hint) {
  NAPI_CALL_RETURN_VOID(env, napi_delete_reference(env, (napi_ref)data));
}

static napi_value ProcessSuperParams(napi_env env, napi_callback_info info) {
  napi_value ar;
  NAPI_CALL(env, napi_create_array(env, &ar));
  return ar;
}

static napi_status
InitNativeSubclass(napi_env env, napi_value super, napi_value* result) {
  napi_ref super_ref;
  napi_value super_fn;

  napi_property_descriptor props[] = {
    DECLARE_NAPI_PROPERTY(NULL, SubMethod),
    DECLARE_NAPI_PROPERTY("chainableMethod", SubChainableMethod),
  };

  // Intentionally use a string-valued `napi_value` instead of a literal string.
  STATUS_CALL(napi_create_string_utf8(env,
                                      "subMethod",
                                      NAPI_AUTO_LENGTH,
                                      &props[0].name));

  // Save a reference to the chain-up method that the subclass'
  // `chainableMethod` will use.
  STATUS_CALL(napi_get_named_property(env, super, "prototype", &super_fn));
  STATUS_CALL(napi_get_named_property(env,
                                      super_fn,
                                      "chainableMethod",
                                      &super_fn));
  STATUS_CALL(napi_create_reference(env, super_fn, 1, &super_ref));
  props[1].data = super_ref;

  STATUS_CALL(napi_define_subclass(env,
                                   super,
                                   "NativeSubclass",
                                   NAPI_AUTO_LENGTH,
                                   NativeSubclass,
                                   ProcessSuperParams,
                                   NULL,
                                   sizeof(props) / sizeof(*props),
                                   props,
                                   result));

  // Make sure the reference gets destroyed along with the constructor.
  return napi_add_finalizer(env,
                            *result,
                            super_ref,
                            DeleteSuperRef,
                            NULL,
                            NULL);
}

static napi_value DefineSubclass(napi_env env, napi_callback_info info) {
  napi_value result;
  size_t argc = 1;

  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &result, NULL, NULL));
  NAPI_CALL(env, InitNativeSubclass(env, result, &result));

  return result;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor props[] = {
    { "NativeClass", NULL, NULL, NULL, NULL, NULL, napi_enumerable, NULL },
    { "NativeSubclass", NULL, NULL, NULL, NULL, NULL, napi_enumerable, NULL },
    DECLARE_NAPI_PROPERTY("defineSubclass", DefineSubclass),
  };

  NAPI_CALL(env, InitNativeClass(env, &props[0].value));

  NAPI_CALL(env, InitNativeSubclass(env, props[0].value, &props[1].value));

  NAPI_CALL(env, napi_define_properties(env,
                                        exports,
                                        sizeof(props) / sizeof(*props),
                                        props));
  return exports;
}
EXTERN_C_END
