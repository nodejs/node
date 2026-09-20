#include <js_native_api.h>
#include "../common.h"
#include "../entry_point.h"

// Showcase for the Node-API JS v-table.
//
// This is an ordinary js-native-api addon. The `NODE_API_MODULE_USE_VTABLE`
// define in binding.gyp turns every `napi_*` call below into a thin wrapper
// that dispatches through the `js_vtable` the runtime injected into
// `napi_env`, instead of binding to runtime-exported symbols. The source is
// unchanged from a classic addon.
//
// The functions exercised here (napi_create_string_utf8, napi_get_cb_info,
// napi_get_value_double, napi_create_double, napi_define_properties) all live
// in the JS v-table.

static napi_value Hello(napi_env env, napi_callback_info info) {
  napi_value result;
  NODE_API_CALL(
      env, napi_create_string_utf8(env, "world", NAPI_AUTO_LENGTH, &result));
  return result;
}

// Round-trips a number through the v-table: returns its argument doubled.
static napi_value Double(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value argv[1];
  NODE_API_CALL(env, napi_get_cb_info(env, info, &argc, argv, NULL, NULL));

  double value;
  NODE_API_CALL(env, napi_get_value_double(env, argv[0], &value));

  napi_value result;
  NODE_API_CALL(env, napi_create_double(env, value * 2, &result));
  return result;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor props[] = {
      DECLARE_NODE_API_PROPERTY("hello", Hello),
      DECLARE_NODE_API_PROPERTY("double", Double),
  };
  NODE_API_CALL(env,
                napi_define_properties(
                    env, exports, sizeof(props) / sizeof(*props), props));
  return exports;
}
EXTERN_C_END
