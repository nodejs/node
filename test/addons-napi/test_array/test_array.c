#include <node_api.h>
#include <string.h>

void Test(napi_env env, napi_callback_info info) {
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

  if (valuetype0 != napi_object) {
    napi_throw_type_error(
        env, "Wrong type of argments. Expects an array as first argument.");
    return;
  }

  napi_valuetype valuetype1;
  status = napi_typeof(env, args[1], &valuetype1);
  if (status != napi_ok) return;

  if (valuetype1 != napi_number) {
    napi_throw_type_error(
        env, "Wrong type of argments. Expects an integer as second argument.");
    return;
  }

  napi_value array = args[0];
  int index;
  status = napi_get_value_int32(env, args[1], &index);
  if (status != napi_ok) return;

  bool isarray;
  status = napi_is_array(env, array, &isarray);
  if (status != napi_ok) return;

  if (isarray) {
    uint32_t size;
    status = napi_get_array_length(env, array, &size);
    if (status != napi_ok) return;

    if (index >= (int)(size)) {
      napi_value str;
      status = napi_create_string_utf8(env, "Index out of bound!", -1, &str);
      if (status != napi_ok) return;

      status = napi_set_return_value(env, info, str);
      if (status != napi_ok) return;
    } else if (index < 0) {
      napi_throw_type_error(env, "Invalid index. Expects a positive integer.");
    } else {
      napi_value ret;
      status = napi_get_element(env, array, index, &ret);
      if (status != napi_ok) return;

      status = napi_set_return_value(env, info, ret);
      if (status != napi_ok) return;
    }
  }
}

void New(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc;
  status = napi_get_cb_args_length(env, info, &argc);
  if (status != napi_ok) return;

  if (argc < 1) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[1];
  status = napi_get_cb_args(env, info, args, 1);
  if (status != napi_ok) return;

  napi_valuetype valuetype;
  status = napi_typeof(env, args[0], &valuetype);
  if (status != napi_ok) return;

  if (valuetype != napi_object) {
    napi_throw_type_error(
        env, "Wrong type of argments. Expects an array as first argument.");
    return;
  }

  napi_value ret;
  status = napi_create_array(env, &ret);
  if (status != napi_ok) return;

  uint32_t i, length;
  status = napi_get_array_length(env, args[0], &length);
  if (status != napi_ok) return;

  for (i = 0; i < length; i++) {
    napi_value e;
    status = napi_get_element(env, args[0], i, &e);
    if (status != napi_ok) return;

    status = napi_set_element(env, ret, i, e);
    if (status != napi_ok) return;
  }

  status = napi_set_return_value(env, info, ret);
  if (status != napi_ok) return;
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  napi_property_descriptor descriptors[] = {
      DECLARE_NAPI_METHOD("Test", Test),
      DECLARE_NAPI_METHOD("New", New),
  };

  status = napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
