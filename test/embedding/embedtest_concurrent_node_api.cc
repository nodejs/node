#include "embedtest_node_api.h"

#include <mutex>
#include <thread>

static const char* main_script =
    "globalThis.require = require('module').createRequire(process.execPath);\n"
    "require('vm').runInThisContext(process.argv[1]);";

// We can use multiple environments at the same time on their own threads.
extern "C" int32_t test_main_concurrent_node_api(int32_t argc, char* argv[]) {
  std::atomic<int32_t> global_count{0};
  std::atomic<int32_t> global_exit_code{0};
  CHECK(node_api_init_once_per_process(argc,
                                       argv,
                                       node_api_platform_no_flags,
                                       nullptr,
                                       nullptr,
                                       nullptr,
                                       nullptr));

  const size_t thread_count = 12;
  std::vector<std::thread> threads;
  threads.reserve(thread_count);
  for (size_t i = 0; i < thread_count; i++) {
    threads.emplace_back([&global_count, &global_exit_code] {
      int32_t exit_code = [&]() {
        node_api_env_options options;
        CHECK(node_api_create_env_options(&options));
        napi_env env;
        CHECK(node_api_create_env(
            options, nullptr, nullptr, main_script, NAPI_VERSION, &env));

        CHECK(node_api_open_env_scope(env));

        napi_value global, my_count;
        CHECK(napi_get_global(env, &global));
        CHECK(napi_get_named_property(env, global, "myCount", &my_count));
        int32_t count;
        CHECK(napi_get_value_int32(env, my_count, &count));
        global_count.fetch_add(count);

        CHECK(node_api_close_env_scope(env));
        CHECK(node_api_delete_env(env, nullptr));
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

  CHECK(node_api_uninit_once_per_process());

  fprintf(stdout, "%d\n", global_count.load());

  return 0;
}

// We can use multiple environments at the same thread.
// For each use we must open and close the environment scope.
extern "C" int32_t test_main_multi_env_node_api(int32_t argc, char* argv[]) {
  CHECK(node_api_init_once_per_process(argc,
                                       argv,
                                       node_api_platform_no_flags,
                                       nullptr,
                                       nullptr,
                                       nullptr,
                                       nullptr));

  const size_t env_count = 12;
  std::vector<napi_env> envs;
  envs.reserve(env_count);
  for (size_t i = 0; i < env_count; i++) {
    node_api_env_options options;
    CHECK(node_api_create_env_options(&options));
    napi_env env;
    CHECK(node_api_create_env(
        options, nullptr, nullptr, main_script, NAPI_VERSION, &env));
    envs.push_back(env);

    CHECK(node_api_open_env_scope(env));

    napi_value undefined, global, func;
    CHECK(napi_get_undefined(env, &undefined));
    CHECK(napi_get_global(env, &global));
    CHECK(napi_get_named_property(env, global, "incMyCount", &func));

    napi_valuetype func_type;
    CHECK(napi_typeof(env, func, &func_type));
    CHECK_TRUE(func_type == napi_function);
    CHECK(napi_call_function(env, undefined, func, 0, nullptr, nullptr));

    CHECK(node_api_close_env_scope(env));
  }

  bool more_work = false;
  do {
    more_work = false;
    for (napi_env env : envs) {
      CHECK(node_api_open_env_scope(env));

      bool has_more_work = false;
      bool had_run_once = false;
      CHECK(node_api_run_env_while(
          env,
          [](void* predicate_data) {
            bool* had_run_once = static_cast<bool*>(predicate_data);
            if (*had_run_once) {
              return false;
            }
            *had_run_once = true;
            return true;
          },
          &had_run_once,
          &has_more_work));
      more_work |= has_more_work;

      CHECK(node_api_close_env_scope(env));
    }
  } while (more_work);

  int32_t global_count = 0;
  for (size_t i = 0; i < env_count; i++) {
    napi_env env = envs[i];
    CHECK(node_api_open_env_scope(env));

    napi_value global, my_count;
    CHECK(napi_get_global(env, &global));
    CHECK(napi_get_named_property(env, global, "myCount", &my_count));

    napi_valuetype my_count_type;
    CHECK(napi_typeof(env, my_count, &my_count_type));
    CHECK_TRUE(my_count_type == napi_number);
    int32_t count;
    CHECK(napi_get_value_int32(env, my_count, &count));

    global_count += count;

    CHECK(node_api_close_env_scope(env));
    CHECK(node_api_delete_env(env, nullptr));
  }

  CHECK(node_api_uninit_once_per_process());

  fprintf(stdout, "%d\n", global_count);

  return 0;
}

// We can use the environment from different threads as long as only one thread
// at a time is using it.
extern "C" int32_t test_main_multi_thread_node_api(int32_t argc, char* argv[]) {
  CHECK(node_api_init_once_per_process(argc,
                                       argv,
                                       node_api_platform_no_flags,
                                       nullptr,
                                       nullptr,
                                       nullptr,
                                       nullptr));

  node_api_env_options options;
  CHECK(node_api_create_env_options(&options));
  napi_env env;
  CHECK(node_api_create_env(
      options, nullptr, nullptr, main_script, NAPI_VERSION, &env));

  std::mutex mutex;
  std::atomic<int32_t> result_count{0};
  std::atomic<int32_t> result_exit_code{0};
  const size_t thread_count = 5;
  std::vector<std::thread> threads;
  threads.reserve(thread_count);
  for (size_t i = 0; i < thread_count; i++) {
    threads.emplace_back([env, &result_count, &result_exit_code, &mutex] {
      int32_t exit_code = [&]() {
        std::scoped_lock lock(mutex);
        CHECK(node_api_open_env_scope(env));

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

        CHECK(node_api_close_env_scope(env));
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

  CHECK(node_api_delete_env(env, nullptr));
  CHECK(node_api_uninit_once_per_process());

  fprintf(stdout, "%d\n", result_count.load());

  return 0;
}
