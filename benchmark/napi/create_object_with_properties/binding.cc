#include <assert.h>
#include <node_api.h>

// Creating with many options because complains are when ~20 properties
static void CreateTestProperties(napi_env env,
                                 napi_value names[20],
                                 napi_value values[20]) {
  napi_create_string_utf8(env, "foo", NAPI_AUTO_LENGTH, &names[0]);
  napi_create_string_utf8(env, "value1", NAPI_AUTO_LENGTH, &values[0]);
  napi_create_string_utf8(env, "alpha", NAPI_AUTO_LENGTH, &names[1]);
  napi_create_int32(env, 100, &values[1]);
  napi_create_string_utf8(env, "beta", NAPI_AUTO_LENGTH, &names[2]);
  napi_get_boolean(env, true, &values[2]);
  napi_create_string_utf8(env, "gamma", NAPI_AUTO_LENGTH, &names[3]);
  napi_create_double(env, 3.14159, &values[3]);
  napi_create_string_utf8(env, "delta", NAPI_AUTO_LENGTH, &names[4]);
  napi_create_int32(env, 42, &values[4]);
  napi_create_string_utf8(env, "epsilon", NAPI_AUTO_LENGTH, &names[5]);
  napi_create_string_utf8(env, "test", NAPI_AUTO_LENGTH, &values[5]);
  napi_create_string_utf8(env, "zeta", NAPI_AUTO_LENGTH, &names[6]);
  napi_create_string_utf8(env, "data", NAPI_AUTO_LENGTH, &values[6]);
  napi_create_string_utf8(env, "eta", NAPI_AUTO_LENGTH, &names[7]);
  napi_create_string_utf8(env, "info", NAPI_AUTO_LENGTH, &values[7]);
  napi_create_string_utf8(env, "theta", NAPI_AUTO_LENGTH, &names[8]);
  napi_create_string_utf8(env, "sample", NAPI_AUTO_LENGTH, &values[8]);
  napi_create_string_utf8(env, "iota", NAPI_AUTO_LENGTH, &names[9]);
  napi_create_double(env, 2.71828, &values[9]);
  napi_create_string_utf8(env, "kappa", NAPI_AUTO_LENGTH, &names[10]);
  napi_create_string_utf8(env, "benchmark", NAPI_AUTO_LENGTH, &values[10]);
  napi_create_string_utf8(env, "lambda", NAPI_AUTO_LENGTH, &names[11]);
  napi_create_string_utf8(env, "result", NAPI_AUTO_LENGTH, &values[11]);
  napi_create_string_utf8(env, "mu", NAPI_AUTO_LENGTH, &names[12]);
  napi_create_string_utf8(env, "output", NAPI_AUTO_LENGTH, &values[12]);
  napi_create_string_utf8(env, "nu", NAPI_AUTO_LENGTH, &names[13]);
  napi_get_boolean(env, false, &values[13]);
  napi_create_string_utf8(env, "xi", NAPI_AUTO_LENGTH, &names[14]);
  napi_create_int32(env, 7, &values[14]);
  napi_create_string_utf8(env, "omicron", NAPI_AUTO_LENGTH, &names[15]);
  napi_create_double(env, 1.618, &values[15]);
  napi_create_string_utf8(env, "pi", NAPI_AUTO_LENGTH, &names[16]);
  napi_create_string_utf8(env, "config", NAPI_AUTO_LENGTH, &values[16]);
  napi_create_string_utf8(env, "rho", NAPI_AUTO_LENGTH, &names[17]);
  napi_create_int32(env, 999, &values[17]);
  napi_create_string_utf8(env, "sigma", NAPI_AUTO_LENGTH, &names[18]);
  napi_create_double(env, 0.577, &values[18]);
  napi_create_string_utf8(env, "tau", NAPI_AUTO_LENGTH, &names[19]);
  napi_get_boolean(env, true, &values[19]);
}

static napi_value CreateObjectWithPropertiesNew(napi_env env,
                                                napi_callback_info info) {
  size_t argc = 4;
  napi_value args[4];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

  napi_value count_val = args[0];
  napi_value bench_obj = args[1];
  napi_value start_fn = args[2];
  napi_value end_fn = args[3];

  uint32_t count;
  napi_get_value_uint32(env, count_val, &count);

  napi_value names[20];
  napi_value values[20];
  napi_value null_prototype;

  napi_get_null(env, &null_prototype);
  CreateTestProperties(env, names, values);

  napi_call_function(env, bench_obj, start_fn, 0, nullptr, nullptr);

  for (uint32_t i = 0; i < count; i++) {
    napi_value obj;
    napi_create_object_with_properties(
        env, null_prototype, names, values, 20, &obj);
  }

  napi_call_function(env, bench_obj, end_fn, 1, &count_val, nullptr);

  return nullptr;
}

static napi_value CreateObjectWithPropertiesOld(napi_env env,
                                                napi_callback_info info) {
  size_t argc = 4;
  napi_value args[4];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

  napi_value count_val = args[0];
  napi_value bench_obj = args[1];
  napi_value start_fn = args[2];
  napi_value end_fn = args[3];

  uint32_t count;
  napi_get_value_uint32(env, count_val, &count);

  napi_value names[20];
  napi_value values[20];

  CreateTestProperties(env, names, values);

  napi_call_function(env, bench_obj, start_fn, 0, nullptr, nullptr);

  for (uint32_t i = 0; i < count; i++) {
    napi_value obj;
    napi_create_object(env, &obj);
    napi_set_property(env, obj, names[0], values[0]);
    napi_set_property(env, obj, names[1], values[1]);
    napi_set_property(env, obj, names[2], values[2]);
    napi_set_property(env, obj, names[3], values[3]);
    napi_set_property(env, obj, names[4], values[4]);
    napi_set_property(env, obj, names[5], values[5]);
    napi_set_property(env, obj, names[6], values[6]);
    napi_set_property(env, obj, names[7], values[7]);
    napi_set_property(env, obj, names[8], values[8]);
    napi_set_property(env, obj, names[9], values[9]);
    napi_set_property(env, obj, names[10], values[10]);
    napi_set_property(env, obj, names[11], values[11]);
    napi_set_property(env, obj, names[12], values[12]);
    napi_set_property(env, obj, names[13], values[13]);
    napi_set_property(env, obj, names[14], values[14]);
    napi_set_property(env, obj, names[15], values[15]);
    napi_set_property(env, obj, names[16], values[16]);
    napi_set_property(env, obj, names[17], values[17]);
    napi_set_property(env, obj, names[18], values[18]);
    napi_set_property(env, obj, names[19], values[19]);
  }

  napi_call_function(env, bench_obj, end_fn, 1, &count_val, nullptr);

  return nullptr;
}

NAPI_MODULE_INIT() {
  napi_property_descriptor desc[] = {
      {"createObjectWithPropertiesNew",
       0,
       CreateObjectWithPropertiesNew,
       0,
       0,
       0,
       napi_default,
       0},
      {"createObjectWithPropertiesOld",
       0,
       CreateObjectWithPropertiesOld,
       0,
       0,
       0,
       napi_default,
       0},
  };

  napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
  return exports;
}
