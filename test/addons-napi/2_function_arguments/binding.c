#include <node_api.h>

void Add(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc;
  status = napi_get_cb_args_length(env, info, &argc);
  if (status != napi_ok) return;

  if (argc < 2) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[2];
  status = napi_get_cb_args(env, info, args, 2);
  if (status != napi_ok) return;

  napi_valuetype valuetype0;
  status = napi_typeof(env, args[0], &valuetype0);
  if (status != napi_ok) return;

  napi_valuetype valuetype1;
  status = napi_typeof(env, args[1], &valuetype1);
  if (status != napi_ok) return;

  if (valuetype0 != napi_number || valuetype1 != napi_number) {
    napi_throw_type_error(env, "Wrong arguments");
    return;
  }

  double value0;
  status = napi_get_value_double(env, args[0], &value0);
  if (status != napi_ok) return;

  double value1;
  status = napi_get_value_double(env, args[1], &value1);
  if (status != napi_ok) return;

  napi_value sum;
  status = napi_create_number(env, value0 + value1, &sum);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, sum);
  if (status != napi_ok) return;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;
  napi_property_descriptor addDescriptor = DECLARE_NAPI_METHOD("add", Add);
  status = napi_define_properties(env, exports, 1, &addDescriptor);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
