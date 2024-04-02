#define NAPI_VERSION 9
#include <js_native_api.h>
#include "../common.h"
#include "../entry_point.h"

static double value_ = 1;

static napi_value GetValue(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, NULL, NULL, NULL));

  NODE_API_ASSERT(env, argc == 0, "Wrong number of arguments");

  napi_value number;
  NODE_API_CALL(env, napi_create_double(env, value_, &number));

  return number;
}

static napi_value SetValue(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");

  NODE_API_CALL(env, napi_get_value_double(env, args[0], &value_));

  return NULL;
}

static napi_value Echo(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");

  return args[0];
}

static napi_value HasNamedProperty(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 2, "Wrong number of arguments");

  // Extract the name of the property to check
  char buffer[128];
  size_t copied;
  NODE_API_CALL(env,
      napi_get_value_string_utf8(env, args[1], buffer, sizeof(buffer), &copied));

  // do the check and create the boolean return value
  bool value;
  napi_value result;
  NODE_API_CALL(env, napi_has_named_property(env, args[0], buffer, &value));
  NODE_API_CALL(env, napi_get_boolean(env, value, &result));

  return result;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_value number;
  NODE_API_CALL(env, napi_create_double(env, value_, &number));

  napi_value name_value;
  NODE_API_CALL(env,
      napi_create_string_utf8(
          env, "NameKeyValue", NAPI_AUTO_LENGTH, &name_value));

  napi_value symbol_description;
  napi_value name_symbol;
  NODE_API_CALL(env,
      napi_create_string_utf8(
          env, "NameKeySymbol", NAPI_AUTO_LENGTH, &symbol_description));
  NODE_API_CALL(env,
      napi_create_symbol(env, symbol_description, &name_symbol));

  napi_value name_symbol_descriptionless;
  NODE_API_CALL(env,
      napi_create_symbol(env, NULL, &name_symbol_descriptionless));

  napi_value name_symbol_for;
  NODE_API_CALL(env, node_api_symbol_for(env,
                                         "NameKeySymbolFor",
                                         NAPI_AUTO_LENGTH,
                                         &name_symbol_for));

  napi_property_descriptor properties[] = {
    { "echo", 0, Echo, 0, 0, 0, napi_enumerable, 0 },
    { "readwriteValue", 0, 0, 0, 0, number, napi_enumerable | napi_writable, 0 },
    { "readonlyValue", 0, 0, 0, 0, number, napi_enumerable, 0},
    { "hiddenValue", 0, 0, 0, 0, number, napi_default, 0},
    { NULL, name_value, 0, 0, 0, number, napi_enumerable, 0},
    { NULL, name_symbol, 0, 0, 0, number, napi_enumerable, 0},
    { NULL, name_symbol_descriptionless, 0, 0, 0, number, napi_enumerable, 0},
    { NULL, name_symbol_for, 0, 0, 0, number, napi_enumerable, 0},
    { "readwriteAccessor1", 0, 0, GetValue, SetValue, 0, napi_default, 0},
    { "readwriteAccessor2", 0, 0, GetValue, SetValue, 0, napi_writable, 0},
    { "readonlyAccessor1", 0, 0, GetValue, NULL, 0, napi_default, 0},
    { "readonlyAccessor2", 0, 0, GetValue, NULL, 0, napi_writable, 0},
    { "hasNamedProperty", 0, HasNamedProperty, 0, 0, 0, napi_default, 0 },
  };

  NODE_API_CALL(env, napi_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}
EXTERN_C_END
