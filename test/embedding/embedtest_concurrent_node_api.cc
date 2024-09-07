#include "embedtest_node_api.h"

#include <mutex>
#include <thread>

static const char* main_script =
    "globalThis.require = require('module').createRequire(process.execPath);\n"
    "require('vm').runInThisContext(process.argv[1]);";

// Tests that multiple runtimes can be run at the same time in their own
// threads. The test creates 12 threads and 12 runtimes. Each runtime runs in it
// own thread.
extern "C" int32_t test_main_concurrent_node_api(int32_t argc, char* argv[]) {
  node_embedding_platform platform;
  CHECK(node_embedding_create_platform(1, &platform));
  CHECK(node_embedding_platform_set_args(platform, argc, argv));
  bool early_return = false;
  CHECK(node_embedding_platform_initialize(platform, &early_return));
  if (early_return) {
    return 0;
  }

  std::atomic<int32_t> global_count{0};
  std::atomic<int32_t> global_exit_code{0};

  const size_t thread_count = 12;
  std::vector<std::thread> threads;
  threads.reserve(thread_count);
  for (size_t i = 0; i < thread_count; i++) {
    threads.emplace_back([platform, &global_count, &global_exit_code] {
      int32_t exit_code = [&]() {
        node_embedding_runtime runtime;
        CHECK(node_embedding_create_runtime(platform, &runtime));
        // Inspector can be associated with only one runtime in the process.
        CHECK(node_embedding_runtime_set_flags(
            runtime,
            node_embedding_runtime_default_flags |
                node_embedding_runtime_no_create_inspector));
        CHECK(
            node_embedding_runtime_set_node_api_version(runtime, NAPI_VERSION));
        CHECK(node_embedding_runtime_initialize(runtime, main_script));
        napi_env env;
        CHECK(node_embedding_runtime_get_node_api_env(runtime, &env));

        CHECK(node_embedding_runtime_open_scope(runtime));

        napi_value global, my_count;
        CHECK(napi_get_global(env, &global));
        CHECK(napi_get_named_property(env, global, "myCount", &my_count));
        int32_t count;
        CHECK(napi_get_value_int32(env, my_count, &count));
        global_count.fetch_add(count);

        CHECK(node_embedding_runtime_close_scope(runtime));
        CHECK(node_embedding_delete_runtime(runtime));
        return 0;
      }();
      if (exit_code != 0) {
        global_exit_code.store(exit_code);
      }
    });
  }

  for (size_t i = 0; i < thread_count; i++) {
    threads[i].join();
  }

  CHECK_EXIT_CODE(global_exit_code.load());

  CHECK(node_embedding_delete_platform(platform));

  fprintf(stdout, "%d\n", global_count.load());

  return 0;
}

// Tests that multiple runtimes can run in the same thread.
// The runtime scope must be opened and closed for each use.
// There are 12 runtimes that share the same main thread.
extern "C" int32_t test_main_multi_env_node_api(int32_t argc, char* argv[]) {
  node_embedding_platform platform;
  CHECK(node_embedding_create_platform(1, &platform));
  CHECK(node_embedding_platform_set_args(platform, argc, argv));
  bool early_return = false;
  CHECK(node_embedding_platform_initialize(platform, &early_return));
  if (early_return) {
    return 0;
  }

  const size_t runtime_count = 12;
  std::vector<node_embedding_runtime> runtimes;
  runtimes.reserve(runtime_count);
  for (size_t i = 0; i < runtime_count; i++) {
    node_embedding_runtime runtime;
    CHECK(node_embedding_create_runtime(platform, &runtime));
    // Inspector can be associated with only one runtime in the process.
    CHECK(node_embedding_runtime_set_flags(
        runtime,
        node_embedding_runtime_default_flags |
            node_embedding_runtime_no_create_inspector));
    CHECK(node_embedding_runtime_set_node_api_version(runtime, NAPI_VERSION));
    CHECK(node_embedding_runtime_initialize(runtime, main_script));
    runtimes.push_back(runtime);

    napi_env env;
    CHECK(node_embedding_runtime_get_node_api_env(runtime, &env));
    CHECK(node_embedding_runtime_open_scope(runtime));

    napi_value undefined, global, func;
    CHECK(napi_get_undefined(env, &undefined));
    CHECK(napi_get_global(env, &global));
    CHECK(napi_get_named_property(env, global, "incMyCount", &func));

    napi_valuetype func_type;
    CHECK(napi_typeof(env, func, &func_type));
    CHECK_TRUE(func_type == napi_function);
    CHECK(napi_call_function(env, undefined, func, 0, nullptr, nullptr));

    CHECK(node_embedding_runtime_close_scope(runtime));
  }

  bool more_work = false;
  do {
    more_work = false;
    for (node_embedding_runtime runtime : runtimes) {
      napi_env env;
      CHECK(node_embedding_runtime_get_node_api_env(runtime, &env));
      CHECK(node_embedding_runtime_open_scope(runtime));

      bool has_more_work = false;
      bool had_run_once = false;
      CHECK(node_embedding_runtime_run_event_loop_while(
          runtime,
          [](void* predicate_data, bool has_work) {
            bool* had_run_once = static_cast<bool*>(predicate_data);
            if (*had_run_once) {
              return false;
            }
            *had_run_once = true;
            return true;
          },
          &had_run_once,
          node_embedding_event_loop_run_nowait,
          &has_more_work));
      more_work |= has_more_work;

      CHECK(node_embedding_runtime_close_scope(runtime));
    }
  } while (more_work);

  int32_t global_count = 0;
  for (node_embedding_runtime runtime : runtimes) {
    napi_env env;
    CHECK(node_embedding_runtime_get_node_api_env(runtime, &env));
    CHECK(node_embedding_runtime_open_scope(runtime));

    napi_value global, my_count;
    CHECK(napi_get_global(env, &global));
    CHECK(napi_get_named_property(env, global, "myCount", &my_count));

    napi_valuetype my_count_type;
    CHECK(napi_typeof(env, my_count, &my_count_type));
    CHECK_TRUE(my_count_type == napi_number);
    int32_t count;
    CHECK(napi_get_value_int32(env, my_count, &count));

    global_count += count;

    CHECK(node_embedding_runtime_close_scope(runtime));
    CHECK(node_embedding_delete_runtime(runtime));
  }

  CHECK(node_embedding_delete_platform(platform));

  fprintf(stdout, "%d\n", global_count);

  return 0;
}

// Tests that a runtime can be invoked from different threads as long as only
// one thread uses it at a time.
extern "C" int32_t test_main_multi_thread_node_api(int32_t argc, char* argv[]) {
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
  CHECK(node_embedding_runtime_initialize(runtime, main_script));
  napi_env env;
  CHECK(node_embedding_runtime_get_node_api_env(runtime, &env));

  // Use mutex to synchronize access to the runtime.
  std::mutex mutex;
  std::atomic<int32_t> result_count{0};
  std::atomic<int32_t> result_exit_code{0};
  const size_t thread_count = 5;
  std::vector<std::thread> threads;
  threads.reserve(thread_count);
  for (size_t i = 0; i < thread_count; i++) {
    threads.emplace_back([runtime, env, &result_count, &result_exit_code, &mutex] {
      int32_t exit_code = [&]() {
        std::scoped_lock lock(mutex);
        CHECK(node_embedding_runtime_open_scope(runtime));

        napi_value undefined, global, func, my_count;
        CHECK(napi_get_undefined(env, &undefined));
        CHECK(napi_get_global(env, &global));
        CHECK(napi_get_named_property(env, global, "incMyCount", &func));

        napi_valuetype func_type;
        CHECK(napi_typeof(env, func, &func_type));
        CHECK_TRUE(func_type == napi_function);
        CHECK(napi_call_function(env, undefined, func, 0, nullptr, nullptr));

        CHECK(napi_get_named_property(env, global, "myCount", &my_count));
        napi_valuetype count_type;
        CHECK(napi_typeof(env, my_count, &count_type));
        CHECK_TRUE(count_type == napi_number);
        int32_t count;
        CHECK(napi_get_value_int32(env, my_count, &count));
        result_count.store(count);

        CHECK(node_embedding_runtime_close_scope(runtime));
        return 0;
      }();
      if (exit_code != 0) {
        result_exit_code.store(exit_code);
      }
    });
  }

  for (size_t i = 0; i < thread_count; i++) {
    threads[i].join();
  }

  CHECK_EXIT_CODE(result_exit_code.load());

  CHECK(node_embedding_delete_runtime(runtime));
  CHECK(node_embedding_delete_platform(platform));

  fprintf(stdout, "%d\n", result_count.load());

  return 0;
}
