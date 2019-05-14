#include <js_native_api.h>
#include "../common.h"

static double value_ = 1;
static double static_value_ = 10;

static napi_value GetValue(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, NULL, NULL, NULL));

  NAPI_ASSERT(env, argc == 0, "Wrong number of arguments");

  napi_value number;
  NAPI_CALL(env, napi_create_double(env, value_, &number));

  return number;
}

static napi_value SetValue(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc == 1, "Wrong number of arguments");

  NAPI_CALL(env, napi_get_value_double(env, args[0], &value_));

  return NULL;
}

static napi_value Echo(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc == 1, "Wrong number of arguments");

  return args[0];
}

static napi_value New(napi_env env, napi_callback_info info) {
  napi_value _this;
  NAPI_CALL(env, napi_get_cb_info(env, info, NULL, NULL, &_this, NULL));

  return _this;
}

static napi_value GetStaticValue(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, NULL, NULL, NULL));

  NAPI_ASSERT(env, argc == 0, "Wrong number of arguments");

  napi_value number;
  NAPI_CALL(env, napi_create_double(env, static_value_, &number));

  return number;
}


static napi_value NewExtra(napi_env env, napi_callback_info info) {
  napi_value _this;
  NAPI_CALL(env, napi_get_cb_info(env, info, NULL, NULL, &_this, NULL));

  return _this;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_value number, cons;
  NAPI_CALL(env, napi_create_double(env, value_, &number));

  NAPI_CALL(env, napi_define_class(
      env, "MyObject_Extra", 8, NewExtra, NULL, 0, NULL, &cons));

  napi_property_descriptor properties[] = {
    { "echo", 0, Echo, 0, 0, 0, napi_enumerable, 0 },
    { "readwriteValue", 0, 0, 0, 0, number, napi_enumerable | napi_writable, 0 },
    { "readonlyValue", 0, 0, 0, 0, number, napi_enumerable, 0},
    { "hiddenValue", 0, 0, 0, 0, number, napi_default, 0},
    { "readwriteAccessor1", 0, 0, GetValue, SetValue, 0, napi_default, 0},
    { "readwriteAccessor2", 0, 0, GetValue, SetValue, 0, napi_writable, 0},
    { "readonlyAccessor1", 0, 0, GetValue, NULL, 0, napi_default, 0},
    { "readonlyAccessor2", 0, 0, GetValue, NULL, 0, napi_writable, 0},
    { "staticReadonlyAccessor1", 0, 0, GetStaticValue, NULL, 0,
        napi_default | napi_static, 0},
    { "constructorName", 0, 0, 0, 0, cons, napi_enumerable | napi_static, 0 },
  };

  NAPI_CALL(env, napi_define_class(env, "MyObject", NAPI_AUTO_LENGTH, New,
      NULL, sizeof(properties)/sizeof(*properties), properties, &cons));

  return cons;
}
EXTERN_C_END
