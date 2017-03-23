#include <node_api.h>

void Test(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc;
  status = napi_get_cb_args_length(env, info, &argc);
  if (status != napi_ok) return;

  if (argc < 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[10];
  status = napi_get_cb_args(env, info, args, 10);
  if (status != napi_ok) return;

  napi_valuetype valuetype;
  status = napi_typeof(env, args[0], &valuetype);
  if (status != napi_ok) return;

  if (valuetype != napi_function) {
    napi_throw_type_error(env, "Wrong type of argments. Expects a function.");
    return;
  }

  napi_value function = args[0];
  napi_value* argv = args + 1;
  argc = argc - 1;

  napi_value global;
  status = napi_get_global(env, &global);
  if (status != napi_ok) return;

  napi_value result;
  status = napi_call_function(env, global, function, argc, argv, &result);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, result);
  if (status != napi_ok) return;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  napi_value fn;
  status =  napi_create_function(env, NULL, Test, NULL, &fn);
  if (status != napi_ok) return;

  status = napi_set_named_property(env, exports, "Test", fn);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
