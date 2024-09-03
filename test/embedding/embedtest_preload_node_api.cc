#include "embedtest_node_api.h"

#include <mutex>
#include <thread>

static const char* main_script =
    "globalThis.require = require('module').createRequire(process.execPath);\n"
    "require('vm').runInThisContext(process.argv[1]);";

// Test the preload callback being called.
extern "C" int32_t test_main_preload_node_api(int32_t argc, char* argv[]) {
  CHECK(node_api_initialize_platform(argc,
                                     argv,
                                     node_api_platform_no_flags,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr));

  node_api_env_options options;
  CHECK(node_api_create_env_options(&options));
  CHECK(node_api_env_options_set_preload_callback(
      options,
      [](napi_env env,
         napi_value /*process*/,
         napi_value /*require*/,
         void* /*cb_data*/) {
        napi_value global, value;
        napi_get_global(env, &global);
        napi_create_int32(env, 42, &value);
        napi_set_named_property(env, global, "preloadValue", value);
      },
      nullptr));
  napi_env env;
  CHECK(node_api_create_env(
      options, nullptr, nullptr, main_script, NAPI_VERSION, &env));

  CHECK(node_api_delete_env(env, nullptr));

  CHECK(node_api_dispose_platform());

  return 0;
}
