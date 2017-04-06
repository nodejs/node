#include <node_api.h>

static double value_ = 1;

void GetValue(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc;
  status = napi_get_cb_args_length(env, info, &argc);
  if (status != napi_ok) return;

  if (argc != 0) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value number;
  status = napi_create_number(env, value_, &number);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, number);
  if (status != napi_ok) return;
}

void SetValue(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc;
  status = napi_get_cb_args_length(env, info, &argc);
  if (status != napi_ok) return;

  if (argc != 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value arg;
  status = napi_get_cb_args(env, info, &arg, 1);
  if (status != napi_ok) return;

  status = napi_get_value_double(env, arg, &value_);
  if (status != napi_ok) return;
}

void Echo(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc;
  status = napi_get_cb_args_length(env, info, &argc);
  if (status != napi_ok) return;

  if (argc != 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value arg;
  status = napi_get_cb_args(env, info, &arg, 1);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, arg);
  if (status != napi_ok) return;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  napi_value number;
  status = napi_create_number(env, value_, &number);
  if (status != napi_ok) return;

  napi_property_descriptor properties[] = {
      { "echo", Echo, 0, 0, 0, napi_enumerable, 0 },
      { "readwriteValue", 0, 0, 0, number, napi_enumerable | napi_writable, 0 },
      { "readonlyValue", 0, 0, 0, number, napi_enumerable, 0},
      { "hiddenValue", 0, 0, 0, number, napi_default, 0},
      { "readwriteAccessor1", 0, GetValue, SetValue, 0, napi_default, 0},
      { "readwriteAccessor2", 0, GetValue, SetValue, 0, napi_writable, 0},
      { "readonlyAccessor1", 0, GetValue, NULL, 0, napi_default, 0},
      { "readonlyAccessor2", 0, GetValue, NULL, 0, napi_writable, 0},
  };

  status = napi_define_properties(
    env, exports, sizeof(properties) / sizeof(*properties), properties);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
