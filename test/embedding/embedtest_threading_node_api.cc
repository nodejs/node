#include "embedtest_node_api.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

// Tests that multiple runtimes can be run at the same time in their own
// threads. The test creates 12 threads and 12 runtimes. Each runtime runs in it
// own thread.
extern "C" int32_t test_main_threading_runtime_per_thread_node_api(
    int32_t argc, char* argv[]) {
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
        CHECK(InvokeNodeApi(runtime, [&](napi_env env) {
          napi_value global, my_count;
          NODE_API_CALL(napi_get_global(env, &global));
          NODE_API_CALL(
              napi_get_named_property(env, global, "myCount", &my_count));
          int32_t count;
          NODE_API_CALL(napi_get_value_int32(env, my_count, &count));
          global_count.fetch_add(count);
        }));

        CHECK(node_embedding_runtime_complete_event_loop(runtime));
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
extern "C" int32_t test_main_threading_several_runtimes_per_thread_node_api(
    int32_t argc, char* argv[]) {
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

    int32_t exit_code = 0;
    CHECK(InvokeNodeApi(runtime, [&](napi_env env) {
      napi_value undefined, global, func;
      NODE_API_CALL(napi_get_undefined(env, &undefined));
      NODE_API_CALL(napi_get_global(env, &global));
      NODE_API_CALL(napi_get_named_property(env, global, "incMyCount", &func));

      napi_valuetype func_type;
      NODE_API_CALL(napi_typeof(env, func, &func_type));
      CHECK_TRUE_RETURN_VOID(func_type == napi_function);
      NODE_API_CALL(
          napi_call_function(env, undefined, func, 0, nullptr, nullptr));
    }));
    CHECK(exit_code);
  }

  bool more_work = false;
  do {
    more_work = false;
    for (node_embedding_runtime runtime : runtimes) {
      bool has_more_work = false;
      CHECK(node_embedding_runtime_run_event_loop(
          runtime, node_embedding_event_loop_run_nowait, &has_more_work));
      more_work |= has_more_work;
    }
  } while (more_work);

  int32_t global_count = 0;
  for (node_embedding_runtime runtime : runtimes) {
    int32_t exit_code = 0;
    CHECK(InvokeNodeApi(runtime, [&](napi_env env) {
      napi_value global, my_count;
      NODE_API_CALL(napi_get_global(env, &global));
      NODE_API_CALL(napi_get_named_property(env, global, "myCount", &my_count));

      napi_valuetype my_count_type;
      NODE_API_CALL(napi_typeof(env, my_count, &my_count_type));
      CHECK_TRUE_RETURN_VOID(my_count_type == napi_number);
      int32_t count;
      NODE_API_CALL(napi_get_value_int32(env, my_count, &count));

      global_count += count;
    }));
    CHECK(exit_code);
    CHECK(node_embedding_runtime_complete_event_loop(runtime));
    CHECK(node_embedding_delete_runtime(runtime));
  }

  CHECK(node_embedding_delete_platform(platform));

  fprintf(stdout, "%d\n", global_count);

  return 0;
}

// Tests that a runtime can be invoked from different threads as long as only
// one thread uses it at a time.
extern "C" int32_t test_main_threading_runtime_in_several_threads_node_api(
    int32_t argc, char* argv[]) {
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

  // Use mutex to synchronize access to the runtime.
  std::mutex mutex;
  std::atomic<int32_t> result_count{0};
  std::atomic<int32_t> result_exit_code{0};
  const size_t thread_count = 5;
  std::vector<std::thread> threads;
  threads.reserve(thread_count);
  for (size_t i = 0; i < thread_count; i++) {
    threads.emplace_back([runtime, &result_count, &result_exit_code, &mutex] {
      std::scoped_lock lock(mutex);
      int32_t exit_code = InvokeNodeApi(runtime, [&](napi_env env) {
        napi_value undefined, global, func, my_count;
        NODE_API_CALL(napi_get_undefined(env, &undefined));
        NODE_API_CALL(napi_get_global(env, &global));
        NODE_API_CALL(
            napi_get_named_property(env, global, "incMyCount", &func));

        napi_valuetype func_type;
        NODE_API_CALL(napi_typeof(env, func, &func_type));
        CHECK_TRUE_RETURN_VOID(func_type == napi_function);
        NODE_API_CALL(
            napi_call_function(env, undefined, func, 0, nullptr, nullptr));

        NODE_API_CALL(
            napi_get_named_property(env, global, "myCount", &my_count));
        napi_valuetype count_type;
        NODE_API_CALL(napi_typeof(env, my_count, &count_type));
        CHECK_TRUE_RETURN_VOID(count_type == napi_number);
        int32_t count;
        NODE_API_CALL(napi_get_value_int32(env, my_count, &count));
        result_count.store(count);
      });
      if (exit_code != 0) {
        result_exit_code.store(exit_code);
      }
    });
  }

  for (size_t i = 0; i < thread_count; i++) {
    threads[i].join();
  }

  CHECK_EXIT_CODE(result_exit_code.load());

  CHECK(node_embedding_runtime_complete_event_loop(runtime));
  CHECK(node_embedding_delete_runtime(runtime));
  CHECK(node_embedding_delete_platform(platform));

  fprintf(stdout, "%d\n", result_count.load());

  return 0;
}

// Tests that a the runtime's event loop can be called from the UI thread
// event loop.
extern "C" int32_t test_main_threading_runtime_in_ui_thread_node_api(
    int32_t argc, char* argv[]) {
  // A simulation of the UI thread's event loop implemented as a dispatcher
  // queue. Note that it is a very simplistic implementation not suitable
  // for the real apps.
  class UIQueue {
   public:
    void PostTask(std::function<void()>&& task) {
      std::scoped_lock lock(mutex_);
      if (!is_finished_) {
        tasks_.push_back(std::move(task));
        wakeup_.notify_one();
      }
    }

    void Run() {
      for (;;) {
        std::function<void()> task;
        {
          std::unique_lock lock(mutex_);
          wakeup_.wait(lock, [&] { return is_finished_ || !tasks_.empty(); });
          if (is_finished_) break;
          task = std::move(tasks_.front());
          tasks_.pop_front();
        }
        task();
      }
    }

    void Stop() {
      std::scoped_lock lock(mutex_);
      if (!is_finished_) {
        is_finished_ = true;
        wakeup_.notify_one();
      }
    }

   private:
    std::mutex mutex_;
    std::condition_variable wakeup_;
    std::deque<std::function<void()>> tasks_;
    bool is_finished_{false};
  } ui_queue;

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

  // The callback will be invoked from the runtime's event loop observer thread.
  // It must schedule the work to the UI thread's event loop.
  node_embedding_runtime_on_event_loop_run_request(
      runtime,
      [](node_embedding_runtime runtime, void* data) {
        auto ui_queue = static_cast<UIQueue*>(data);
        ui_queue->PostTask([runtime, ui_queue]() {
          int32_t exit_code = 0;
          CHECK_RETURN_VOID(node_embedding_runtime_run_event_loop(
              runtime, node_embedding_event_loop_run_nowait, nullptr));

          // Check myCount and stop the processing when it reaches 5.
          exit_code = InvokeNodeApi(runtime, [&](napi_env env) {
            napi_value global, my_count;
            NODE_API_CALL(napi_get_global(env, &global));
            NODE_API_CALL(
                napi_get_named_property(env, global, "myCount", &my_count));
            napi_valuetype count_type;
            NODE_API_CALL(napi_typeof(env, my_count, &count_type));
            CHECK_TRUE_RETURN_VOID(count_type == napi_number);
            int32_t count;
            NODE_API_CALL(napi_get_value_int32(env, my_count, &count));
            if (count == 5) {
              CHECK_RETURN_VOID(
                  node_embedding_runtime_complete_event_loop(runtime));
              fprintf(stdout, "%d\n", count);
              ui_queue->Stop();
            }
          });
          CHECK_RETURN_VOID(exit_code);
        });
      },
      &ui_queue);

  CHECK(node_embedding_runtime_initialize(runtime, main_script));

  // The initial task starts the JS code that then will do the timer scheduling.
  // The timer supposed to be handled by the runtime's event loop.
  ui_queue.PostTask([runtime]() {
    int32_t exit_code = InvokeNodeApi(runtime, [&](napi_env env) {
      napi_value undefined, global, func;
      NODE_API_CALL(napi_get_undefined(env, &undefined));
      NODE_API_CALL(napi_get_global(env, &global));
      NODE_API_CALL(napi_get_named_property(env, global, "incMyCount", &func));

      napi_valuetype func_type;
      NODE_API_CALL(napi_typeof(env, func, &func_type));
      CHECK_TRUE_RETURN_VOID(func_type == napi_function);
      NODE_API_CALL(
          napi_call_function(env, undefined, func, 0, nullptr, nullptr));

      CHECK_RETURN_VOID(node_embedding_runtime_run_event_loop(
          runtime, node_embedding_event_loop_run_nowait, nullptr));
    });
    CHECK_RETURN_VOID(exit_code);
  });

  ui_queue.Run();

  CHECK(node_embedding_delete_runtime(runtime));
  CHECK(node_embedding_delete_platform(platform));

  return 0;
}
