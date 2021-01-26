#include <js_native_api.h>
#include "../common.h"

static double value_ = 1;

static node_api_value GetValue(node_api_env env, node_api_callback_info info) {
  size_t argc = 0;
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, NULL, NULL, NULL));

  NODE_API_ASSERT(env, argc == 0, "Wrong number of arguments");

  node_api_value number;
  NODE_API_CALL(env, node_api_create_double(env, value_, &number));

  return number;
}

static node_api_value SetValue(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");

  NODE_API_CALL(env, node_api_get_value_double(env, args[0], &value_));

  return NULL;
}

static node_api_value Echo(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 1, "Wrong number of arguments");

  return args[0];
}

static node_api_value
HasNamedProperty(node_api_env env, node_api_callback_info info) {
  size_t argc = 2;
  node_api_value args[2];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc == 2, "Wrong number of arguments");

  // Extract the name of the property to check
  char buffer[128];
  size_t copied;
  NODE_API_CALL(env,
      node_api_get_value_string_utf8(
          env, args[1], buffer, sizeof(buffer), &copied));

  // do the check and create the boolean return value
  bool value;
  node_api_value result;
  NODE_API_CALL(env,
      node_api_has_named_property(env, args[0], buffer, &value));
  NODE_API_CALL(env, node_api_get_boolean(env, value, &result));

  return result;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_value number;
  NODE_API_CALL(env, node_api_create_double(env, value_, &number));

  node_api_value name_value;
  NODE_API_CALL(env, node_api_create_string_utf8(env,
                                                 "NameKeyValue",
                                                 NODE_API_AUTO_LENGTH,
                                                 &name_value));

  node_api_value symbol_description;
  node_api_value name_symbol;
  NODE_API_CALL(env, node_api_create_string_utf8(env,
                                                 "NameKeySymbol",
                                                 NODE_API_AUTO_LENGTH,
                                                 &symbol_description));
  NODE_API_CALL(env, node_api_create_symbol(env,
                                    symbol_description,
                                    &name_symbol));

  node_api_property_descriptor properties[] = {
    { "echo", 0, Echo, 0, 0, 0, node_api_enumerable, 0 },
    { "readwriteValue", 0, 0, 0, 0, number,
      node_api_enumerable | node_api_writable, 0 },
    { "readonlyValue", 0, 0, 0, 0, number, node_api_enumerable, 0},
    { "hiddenValue", 0, 0, 0, 0, number, node_api_default, 0},
    { NULL, name_value, 0, 0, 0, number, node_api_enumerable, 0},
    { NULL, name_symbol, 0, 0, 0, number, node_api_enumerable, 0},
    { "readwriteAccessor1", 0, 0, GetValue, SetValue, 0, node_api_default, 0},
    { "readwriteAccessor2", 0, 0, GetValue, SetValue, 0, node_api_writable, 0},
    { "readonlyAccessor1", 0, 0, GetValue, NULL, 0, node_api_default, 0},
    { "readonlyAccessor2", 0, 0, GetValue, NULL, 0, node_api_writable, 0},
    { "hasNamedProperty", 0, HasNamedProperty, 0, 0, 0, node_api_default, 0 },
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(properties) / sizeof(*properties), properties));

  return exports;
}
EXTERN_C_END
