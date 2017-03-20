#include <node_api.h>

void Get(napi_env env, napi_callback_info info) {
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
        env, "Wrong type of argments. Expects an object as first argument.");
    return;
  }

  napi_valuetype valuetype1;
  status = napi_typeof(env, args[1], &valuetype1);
  if (status != napi_ok) return;

  if (valuetype1 != napi_string && valuetype1 != napi_symbol) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects a string or symbol as second.");
    return;
  }

  napi_value object = args[0];
  napi_value output;
  status = napi_get_property(env, object, args[1], &output);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, output);
  if (status != napi_ok) return;
}

void Set(napi_env env, napi_callback_info info) {
  napi_status status;

  size_t argc;
  status = napi_get_cb_args_length(env, info, &argc);
  if (status != napi_ok) return;

  if (argc < 3) {
    napi_throw_type_error(env, "Wrong number of arguments");
    return;
  }

  napi_value args[3];
  status = napi_get_cb_args(env, info, args, 3);
  if (status != napi_ok) return;

  napi_valuetype valuetype0;
  status = napi_typeof(env, args[0], &valuetype0);
  if (status != napi_ok) return;

  if (valuetype0 != napi_object) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects an object as first argument.");
    return;
  }

  napi_valuetype valuetype1;
  status = napi_typeof(env, args[1], &valuetype1);
  if (status != napi_ok) return;

  if (valuetype1 != napi_string && valuetype1 != napi_symbol) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects a string or symbol as second.");
    return;
  }

  napi_value object = args[0];
  status = napi_set_property(env, object, args[1], args[2]);
  if (status != napi_ok) return;

  napi_value valuetrue;
  status = napi_get_boolean(env, true, &valuetrue);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, valuetrue);
  if (status != napi_ok) return;
}

void Has(napi_env env, napi_callback_info info) {
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
        env, "Wrong type of argments. Expects an object as first argument.");
    return;
  }

  napi_valuetype valuetype1;
  status = napi_typeof(env, args[1], &valuetype1);
  if (status != napi_ok) return;

  if (valuetype1 != napi_string && valuetype1 != napi_symbol) {
    napi_throw_type_error(env,
        "Wrong type of argments. Expects a string or symbol as second.");
    return;
  }

  napi_value obj = args[0];
  bool has_property;
  status = napi_has_property(env, obj, args[1], &has_property);
  if (status != napi_ok) return;

  napi_value ret;
  status = napi_get_boolean(env, has_property, &ret);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, ret);
  if (status != napi_ok) return;
}

void New(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value ret;
  status = napi_create_object(env, &ret);

  napi_value num;
  status = napi_create_number(env, 987654321, &num);
  if (status != napi_ok) return;

  status = napi_set_named_property(env, ret, "test_number", num);
  if (status != napi_ok) return;

  napi_value str;
  status = napi_create_string_utf8(env, "test string", -1, &str);
  if (status != napi_ok) return;

  status = napi_set_named_property(env, ret, "test_string", str);
  if (status != napi_ok) return;

  status = napi_set_return_value(env, info, ret);
  if (status != napi_ok) return;
}

void Inflate(napi_env env, napi_callback_info info) {
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
        env, "Wrong type of argments. Expects an object as first argument.");
    return;
  }

  napi_value obj = args[0];

  napi_value propertynames;
  status = napi_get_property_names(env, obj, &propertynames);
  if (status != napi_ok) return;

  uint32_t i, length;
  status = napi_get_array_length(env, propertynames, &length);
  if (status != napi_ok) return;

  for (i = 0; i < length; i++) {
    napi_value property_str;
    status = napi_get_element(env, propertynames, i, &property_str);
    if (status != napi_ok) return;

    napi_value value;
    status = napi_get_property(env, obj, property_str, &value);
    if (status != napi_ok) return;

    double double_val;
    status = napi_get_value_double(env, value, &double_val);
    if (status != napi_ok) return;

    status = napi_create_number(env, double_val + 1, &value);
    if (status != napi_ok) return;

    status = napi_set_property(env, obj, property_str, value);
    if (status != napi_ok) return;
  }
  status = napi_set_return_value(env, info, obj);
}

#define DECLARE_NAPI_METHOD(name, func)                          \
  { name, func, 0, 0, 0, napi_default, 0 }

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_status status;

  napi_property_descriptor descriptors[] = {
      DECLARE_NAPI_METHOD("Get", Get),
      DECLARE_NAPI_METHOD("Set", Set),
      DECLARE_NAPI_METHOD("Has", Has),
      DECLARE_NAPI_METHOD("New", New),
      DECLARE_NAPI_METHOD("Inflate", Inflate),
  };

  status = napi_define_properties(
      env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors);
  if (status != napi_ok) return;
}

NAPI_MODULE(addon, Init)
