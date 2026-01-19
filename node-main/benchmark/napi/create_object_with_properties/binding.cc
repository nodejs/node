#include <assert.h>
#include <node_api.h>
#include <string>

struct BenchmarkParams {
  napi_value count_val;
  napi_value bench_obj;
  napi_value start_fn;
  napi_value end_fn;
  uint32_t count;
};

static BenchmarkParams ParseBenchmarkArgs(napi_env env,
                                          const napi_callback_info info) {
  BenchmarkParams params;
  size_t argc = 4;
  napi_value args[4];
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

  params.count_val = args[0];
  params.bench_obj = args[1];
  params.start_fn = args[2];
  params.end_fn = args[3];

  napi_get_value_uint32(env, params.count_val, &params.count);
  return params;
}

static napi_value global_names[20];
static napi_value global_values[20];
static bool global_properties_initialized = false;

// Creating with many options because complains are when ~20 properties
static void InitializeTestProperties(napi_env env) {
  if (global_properties_initialized) return;

  for (int i = 0; i < 20; i++) {
    std::string name = "foo" + std::to_string(i);
    napi_create_string_utf8(
        env, name.c_str(), NAPI_AUTO_LENGTH, &global_names[i]);
    napi_create_string_utf8(
        env, name.c_str(), NAPI_AUTO_LENGTH, &global_values[i]);
  }
  global_properties_initialized = true;
}

static napi_value CreateObjectWithPropertiesNew(napi_env env,
                                                napi_callback_info info) {
  BenchmarkParams params = ParseBenchmarkArgs(env, info);

  InitializeTestProperties(env);

  napi_value null_prototype;
  napi_get_null(env, &null_prototype);

  napi_call_function(
      env, params.bench_obj, params.start_fn, 0, nullptr, nullptr);

  for (uint32_t i = 0; i < params.count; i++) {
    napi_value obj;
    napi_create_object_with_properties(
        env, null_prototype, global_names, global_values, 20, &obj);
  }

  napi_call_function(
      env, params.bench_obj, params.end_fn, 1, &params.count_val, nullptr);

  return nullptr;
}

static napi_value CreateObjectWithPropertiesOld(napi_env env,
                                                napi_callback_info info) {
  BenchmarkParams params = ParseBenchmarkArgs(env, info);

  InitializeTestProperties(env);

  napi_call_function(
      env, params.bench_obj, params.start_fn, 0, nullptr, nullptr);

  for (uint32_t i = 0; i < params.count; i++) {
    napi_value obj;
    napi_create_object(env, &obj);
    for (int j = 0; j < 20; j++) {
      napi_set_property(env, obj, global_names[j], global_values[j]);
    }
  }

  napi_call_function(
      env, params.bench_obj, params.end_fn, 1, &params.count_val, nullptr);

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
