#include "embedtest_node_api.h"

#include <mutex>
#include <thread>

// Tests that the same preload callback is called from the main thread and from
// the worker thread.
extern "C" int32_t test_main_preload_node_api(int32_t argc, char* argv[]) {
  node_embedding_platform platform;
  CHECK(node_embedding_create_platform(NODE_EMBEDDING_VERSION, &platform));
  CHECK(node_embedding_platform_set_args(platform, argc, argv));
  bool early_return = false;
  CHECK(node_embedding_platform_initialize(platform, &early_return));
  if (early_return) {
    return 0;
  }

  node_embedding_runtime runtime;
  CHECK(node_embedding_create_runtime(platform, &runtime));
  CHECK(node_embedding_runtime_set_node_api_version(runtime, NAPI_VERSION));
  CHECK(node_embedding_runtime_on_preload(
      runtime,
      [](node_embedding_runtime runtime,
         void* /*cb_data*/,
         napi_env env,
         napi_value /*process*/,
         napi_value /*require*/
      ) {
        napi_value global, value;
        napi_get_global(env, &global);
        napi_create_int32(env, 42, &value);
        napi_set_named_property(env, global, "preloadValue", value);
      },
      nullptr));
  CHECK(node_embedding_runtime_initialize(runtime, main_script));
  CHECK(node_embedding_runtime_complete_event_loop(runtime));
  CHECK(node_embedding_delete_runtime(runtime));
  CHECK(node_embedding_delete_platform(platform));

  return 0;
}
