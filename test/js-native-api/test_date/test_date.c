#include <js_native_api.h>
#include "../common.h"

static napi_value createDate(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc >= 1, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NAPI_ASSERT(env, valuetype0 == napi_number,
              "Wrong type of arguments. Expects a number as first argument.");

  double time;
  NAPI_CALL(env, napi_get_value_double(env, args[0], &time));

  napi_value date;
  NAPI_CALL(env, napi_create_date(env, time, &date));

  return date;
}

static napi_value isDate(napi_env env, napi_callback_info info) {
  napi_value date, result;
  size_t argc = 1;
  bool is_date;

  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &date, NULL, NULL));
  NAPI_CALL(env, napi_is_date(env, date, &is_date));
  NAPI_CALL(env, napi_get_boolean(env, is_date, &result));

  return result;
}

static napi_value getDateValue(napi_env env, napi_callback_info info) {
  napi_value date, result;
  size_t argc = 1;
  double value;

  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, &date, NULL, NULL));
  NAPI_CALL(env, napi_get_date_value(env, date, &value));
  NAPI_CALL(env, napi_create_double(env, value, &result));

  return result;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("createDate", createDate),
    DECLARE_NAPI_PROPERTY("isDate", isDate),
    DECLARE_NAPI_PROPERTY("getDateValue", getDateValue),
  };

  NAPI_CALL(env, napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));

  return exports;
}
EXTERN_C_END
