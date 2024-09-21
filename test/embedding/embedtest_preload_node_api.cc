#include "embedtest_node_api.h"

#include <mutex>
#include <thread>

// Tests that the same preload callback is called from the main thread and from
// the worker thread.
extern "C" int32_t test_main_preload_node_api(int32_t argc, char* argv[]) {
  CHECK_EXIT_CODE(RunMain(
      argc,
      argv,
      nullptr,
      [&](node_embedding_platform platform,
          node_embedding_runtime_config runtime_config) {
        CHECK_EXIT_CODE(node_embedding_runtime_on_preload(
            runtime_config,
            [](void* /*cb_data*/,
               node_embedding_runtime runtime,
               napi_env env,
               napi_value /*process*/,
               napi_value /*require*/
            ) {
              napi_value global, value;
              NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
              NODE_API_CALL_RETURN_VOID(napi_create_int32(env, 42, &value));
              NODE_API_CALL_RETURN_VOID(
                  napi_set_named_property(env, global, "preloadValue", value));
            },
            nullptr));
        CHECK_EXIT_CODE(node_embedding_runtime_on_start_execution(
            runtime_config,
            [](void* cb_data,
               node_embedding_runtime runtime,
               napi_env env,
               napi_value process,
               napi_value require,
               napi_value run_cjs) -> napi_value {
              napi_value script, undefined, result;
              NODE_API_CALL(napi_create_string_utf8(
                  env, main_script, NAPI_AUTO_LENGTH, &script));
              NODE_API_CALL(napi_get_undefined(env, &undefined));
              NODE_API_CALL(napi_call_function(
                  env, undefined, run_cjs, 1, &script, &result));
              return result;
            },
            nullptr));

        return node_embedding_exit_code_ok;
      },
      nullptr));

  return node_embedding_exit_code_ok;
}
