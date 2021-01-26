#include <js_native_api.h>
#include "../common.h"

static node_api_value
createDate(node_api_env env, node_api_callback_info info) {
  size_t argc = 1;
  node_api_value args[1];
  NODE_API_CALL(env, node_api_get_cb_info(env, info, &argc, args, NULL, NULL));

  NODE_API_ASSERT(env, argc >= 1, "Wrong number of arguments");

  node_api_valuetype valuetype0;
  NODE_API_CALL(env, node_api_typeof(env, args[0], &valuetype0));

  NODE_API_ASSERT(env, valuetype0 == node_api_number,
      "Wrong type of arguments. Expects a number as first argument.");

  double time;
  NODE_API_CALL(env, node_api_get_value_double(env, args[0], &time));

  node_api_value date;
  NODE_API_CALL(env, node_api_create_date(env, time, &date));

  return date;
}

static node_api_value isDate(node_api_env env, node_api_callback_info info) {
  node_api_value date, result;
  size_t argc = 1;
  bool is_date;

  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, &date, NULL, NULL));
  NODE_API_CALL(env, node_api_is_date(env, date, &is_date));
  NODE_API_CALL(env, node_api_get_boolean(env, is_date, &result));

  return result;
}

static node_api_value
getDateValue(node_api_env env, node_api_callback_info info) {
  node_api_value date, result;
  size_t argc = 1;
  double value;

  NODE_API_CALL(env,
      node_api_get_cb_info(env, info, &argc, &date, NULL, NULL));
  NODE_API_CALL(env, node_api_get_date_value(env, date, &value));
  NODE_API_CALL(env, node_api_create_double(env, value, &result));

  return result;
}

EXTERN_C_START
node_api_value Init(node_api_env env, node_api_value exports) {
  node_api_property_descriptor descriptors[] = {
    DECLARE_NODE_API_PROPERTY("createDate", createDate),
    DECLARE_NODE_API_PROPERTY("isDate", isDate),
    DECLARE_NODE_API_PROPERTY("getDateValue", getDateValue),
  };

  NODE_API_CALL(env, node_api_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
