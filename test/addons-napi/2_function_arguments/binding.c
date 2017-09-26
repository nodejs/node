#include <node_api.h>
#include "../common.h"

napi_value Add(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2];
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc >= 2, "Wrong number of arguments");

  napi_valuetype valuetype0;
  NAPI_CALL(env, napi_typeof(env, args[0], &valuetype0));

  napi_valuetype valuetype1;
  NAPI_CALL(env, napi_typeof(env, args[1], &valuetype1));

  NAPI_ASSERT(env, valuetype0 == napi_number && valuetype1 == napi_number,
      "Wrong argument type. Numbers expected.");

  double value0;
  NAPI_CALL(env, napi_get_value_double(env, args[0], &value0));

  double value1;
  NAPI_CALL(env, napi_get_value_double(env, args[1], &value1));

  napi_value sum;
  NAPI_CALL(env, napi_create_double(env, value0 + value1, &sum));

  return sum;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor desc = DECLARE_NAPI_PROPERTY("add", Add);
  NAPI_CALL(env, napi_define_properties(env, exports, 1, &desc));
  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
