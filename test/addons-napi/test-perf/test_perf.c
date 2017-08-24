#include <node_api.h>
#include <string.h>
#include "../common.h"

napi_value TestMark(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc == 1, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));

  NAPI_ASSERT(env, valuetype0 == napi_string,
    "Wrong type of argments. Expects a string as first argument.");

  char buffer[100];
  size_t buffer_size = 100;
  size_t copied;

  NAPI_CALL(env,
    napi_get_value_string_utf8(env, args[0], buffer, buffer_size, &copied));

  NAPI_CALL(env, napi_performance_mark(env, buffer));

  return napi_undefined;
}

napi_value TestMeasure(napi_env env, napi_callback_info info) {
  size_t argc = 3;
  napi_value args[3];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc == 3, "Wrong number of arguments");

  for (int n = 0; n < 3; n++) {
    napi_valuetype valuetype;
    NAPI_CALL(env, napi_typeof(env, args[n], &valuetype));
    NAPI_ASSERT(env, valuetype == napi_string,
      "Wrong type of argments. Expects a string as argument.");
    }

  char name[100];
  char start[100];
  char end[100];
  size_t copied;

  NAPI_CALL(env,
    napi_get_value_string_utf8(env, args[0], name, sizeof(name), &copied));
  NAPI_CALL(env,
    napi_get_value_string_utf8(env, args[0], start, sizeof(start), &copied));
  NAPI_CALL(env,
    napi_get_value_string_utf8(env, args[0], end, sizeof(end), &copied));

  NAPI_CALL(env, napi_performance_measure(env, name, start, end));

  return napi_undefined;
}

void Init(napi_env env, napi_value exports, napi_value module, void* priv) {
  napi_property_descriptor descriptors[] = {
    DECLARE_NAPI_PROPERTY("TestMark", TestMark),
    DECLARE_NAPI_PROPERTY("TestMeasure", TestMeasure)
  };

  NAPI_CALL_RETURN_VOID(env, napi_define_properties(
    env, exports, sizeof(descriptors) / sizeof(*descriptors), descriptors));
}

NAPI_MODULE(addon, Init)
