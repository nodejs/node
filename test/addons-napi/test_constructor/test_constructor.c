#include <node_api.h>

static double value_ = 1;
napi_ref constructor_;

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

void New(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value jsthis;
  status = napi_get_cb_this(env, info, &jsthis);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, jsthis);
  if (status != napi_ok) return;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  napi_value number;
  status = napi_create_number(env, value_, &number);
  if (status != napi_ok) return;

  napi_property_descriptor properties[] = {
      { "echo", Echo, 0, 0, 0, napi_default, 0 },
      { "accessorValue", 0, GetValue, SetValue, 0, napi_default, 0},
      { "readwriteValue", 0, 0, 0, number, napi_default, 0 },
      { "readonlyValue", 0, 0, 0, number, napi_read_only, 0},
      { "hiddenValue", 0, 0, 0, number, napi_read_only | napi_dont_enum, 0},
  };

  napi_value cons;
  status = napi_define_class(env, "MyObject", New,
    NULL, sizeof(properties)/sizeof(*properties), properties, &cons);
  if (status != napi_ok) return;

  status = napi_set_named_property(env, module, "exports", cons);
  if (status != napi_ok) return;

  status = napi_create_reference(env, cons, 1, &constructor_);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
