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
  const size_t thread_count = 12;
  std::vector<std::thread> threads;
  threads.reserve(thread_count);
  std::atomic<int32_t> global_count{0};
  std::atomic<node_embedding_exit_code> global_exit_code{};

  CHECK_EXIT_CODE(
      node_embedding_create_platform(argc, argv, nullptr, nullptr, &platform));
  if (!platform) {
    return node_embedding_exit_code_ok;  // early return
  }

  for (size_t i = 0; i < thread_count; i++) {
    threads.emplace_back([platform, &global_count, &global_exit_code] {
      node_embedding_exit_code exit_code = [&]() {
        CHECK_EXIT_CODE(RunRuntime(
            platform,
            [&](node_embedding_platform platform,
                node_embedding_runtime runtime) {
              // Inspector can be associated with only one runtime in the
              // process.
              CHECK_EXIT_CODE(node_embedding_runtime_set_flags(
                  runtime,
                  node_embedding_runtime_default_flags |
                      node_embedding_runtime_no_create_inspector));
              CHECK_EXIT_CODE(node_embedding_runtime_on_start_execution(
                  runtime,
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
            [&](node_embedding_runtime runtime, napi_env env) {
              napi_value global, my_count;
              NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
              NODE_API_CALL_RETURN_VOID(
                  napi_get_named_property(env, global, "myCount", &my_count));
              int32_t count;
              NODE_API_CALL_RETURN_VOID(
                  napi_get_value_int32(env, my_count, &count));
              global_count.fetch_add(count);
            }));
        return node_embedding_exit_code_ok;
      }();
      if (exit_code != node_embedding_exit_code_ok) {
        global_exit_code.store(exit_code);
      }
    });
  }

  for (size_t i = 0; i < thread_count; i++) {
    threads[i].join();
  }

  CHECK_EXIT_CODE(global_exit_code.load());

  CHECK_EXIT_CODE(node_embedding_delete_platform(platform));

  fprintf(stdout, "%d\n", global_count.load());

  return node_embedding_exit_code_ok;
}

// Tests that multiple runtimes can run in the same thread.
// The runtime scope must be opened and closed for each use.
// There are 12 runtimes that share the same main thread.
extern "C" int32_t test_main_threading_several_runtimes_per_thread_node_api(
    int32_t argc, char* argv[]) {
  node_embedding_platform platform;
  const size_t runtime_count = 12;
  std::vector<node_embedding_runtime> runtimes;
  runtimes.reserve(runtime_count);
  bool more_work = false;
  int32_t global_count = 0;

  CHECK_EXIT_CODE(
      node_embedding_create_platform(argc, argv, nullptr, nullptr, &platform));
  if (!platform) {
    return node_embedding_exit_code_ok;  // early return
  }

  for (size_t i = 0; i < runtime_count; i++) {
    node_embedding_runtime runtime;
    CHECK_EXIT_CODE(CreateRuntime(
        platform,
        [&](node_embedding_platform platform, node_embedding_runtime runtime) {
          // Inspector can be associated with only one runtime in the process.
          CHECK_EXIT_CODE(node_embedding_runtime_set_flags(
              runtime,
              node_embedding_runtime_default_flags |
                  node_embedding_runtime_no_create_inspector));
          CHECK_EXIT_CODE(node_embedding_runtime_on_start_execution(
              runtime,
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
        &runtime));
    runtimes.push_back(runtime);

    CHECK_EXIT_CODE(
        RunNodeApi(runtime, [&](node_embedding_runtime runtime, napi_env env) {
          napi_value undefined, global, func;
          NODE_API_CALL_RETURN_VOID(napi_get_undefined(env, &undefined));
          NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
          NODE_API_CALL_RETURN_VOID(
              napi_get_named_property(env, global, "incMyCount", &func));

          napi_valuetype func_type;
          NODE_API_CALL_RETURN_VOID(napi_typeof(env, func, &func_type));
          NODE_API_ASSERT_RETURN_VOID(func_type == napi_function);
          NODE_API_CALL_RETURN_VOID(
              napi_call_function(env, undefined, func, 0, nullptr, nullptr));
        }));
  }

  do {
    more_work = false;
    for (node_embedding_runtime runtime : runtimes) {
      bool has_more_work = false;
      CHECK_EXIT_CODE(node_embedding_run_event_loop(
          runtime, node_embedding_event_loop_run_nowait, &has_more_work));
      more_work |= has_more_work;
    }
  } while (more_work);

  for (node_embedding_runtime runtime : runtimes) {
    CHECK_EXIT_CODE(
        RunNodeApi(runtime, [&](node_embedding_runtime runtime, napi_env env) {
          napi_value global, my_count;
          NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
          NODE_API_CALL_RETURN_VOID(
              napi_get_named_property(env, global, "myCount", &my_count));

          napi_valuetype my_count_type;
          NODE_API_CALL_RETURN_VOID(napi_typeof(env, my_count, &my_count_type));
          NODE_API_ASSERT_RETURN_VOID(my_count_type == napi_number);
          int32_t count;
          NODE_API_CALL_RETURN_VOID(
              napi_get_value_int32(env, my_count, &count));

          global_count += count;
        }));
    CHECK_EXIT_CODE(node_embedding_complete_event_loop(runtime));
    CHECK_EXIT_CODE(node_embedding_delete_runtime(runtime));
  }

  CHECK_EXIT_CODE(node_embedding_delete_platform(platform));

  fprintf(stdout, "%d\n", global_count);

  return node_embedding_exit_code_ok;
}

// Tests that a runtime can be invoked from different threads as long as only
// one thread uses it at a time.
extern "C" int32_t test_main_threading_runtime_in_several_threads_node_api(
    int32_t argc, char* argv[]) {
  node_embedding_platform platform;

  // Use mutex to synchronize access to the runtime.
  std::mutex mutex;
  std::atomic<int32_t> result_count{0};
  std::atomic<node_embedding_exit_code> result_exit_code{
      node_embedding_exit_code_ok};
  const size_t thread_count = 5;
  std::vector<std::thread> threads;
  threads.reserve(thread_count);

  CHECK_EXIT_CODE(
      node_embedding_create_platform(argc, argv, nullptr, nullptr, &platform));
  if (!platform) {
    return node_embedding_exit_code_ok;  // early return
  }

  node_embedding_runtime runtime;
  CHECK_EXIT_CODE(CreateRuntime(
      platform,
      [&](node_embedding_platform platform, node_embedding_runtime runtime) {
        CHECK_EXIT_CODE(node_embedding_runtime_on_start_execution(
            runtime,
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
      &runtime));

  for (size_t i = 0; i < thread_count; i++) {
    threads.emplace_back([runtime, &result_count, &result_exit_code, &mutex] {
      std::scoped_lock lock(mutex);
      node_embedding_exit_code exit_code = RunNodeApi(
          runtime, [&](node_embedding_runtime runtime, napi_env env) {
            napi_value undefined, global, func, my_count;
            NODE_API_CALL_RETURN_VOID(napi_get_undefined(env, &undefined));
            NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
            NODE_API_CALL_RETURN_VOID(
                napi_get_named_property(env, global, "incMyCount", &func));

            napi_valuetype func_type;
            NODE_API_CALL_RETURN_VOID(napi_typeof(env, func, &func_type));
            NODE_API_ASSERT_RETURN_VOID(func_type == napi_function);
            NODE_API_CALL_RETURN_VOID(
                napi_call_function(env, undefined, func, 0, nullptr, nullptr));

            NODE_API_CALL_RETURN_VOID(
                napi_get_named_property(env, global, "myCount", &my_count));
            napi_valuetype count_type;
            NODE_API_CALL_RETURN_VOID(napi_typeof(env, my_count, &count_type));
            NODE_API_ASSERT_RETURN_VOID(count_type == napi_number);
            int32_t count;
            NODE_API_CALL_RETURN_VOID(
                napi_get_value_int32(env, my_count, &count));
            result_count.store(count);
          });
      if (exit_code != node_embedding_exit_code_ok) {
        result_exit_code.store(exit_code);
      }
    });
  }

  for (size_t i = 0; i < thread_count; i++) {
    threads[i].join();
  }

  CHECK_EXIT_CODE(result_exit_code.load());

  CHECK_EXIT_CODE(node_embedding_complete_event_loop(runtime));
  CHECK_EXIT_CODE(node_embedding_delete_runtime(runtime));
  CHECK_EXIT_CODE(node_embedding_delete_platform(platform));

  fprintf(stdout, "%d\n", result_count.load());

  return node_embedding_exit_code_ok;
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
  CHECK_EXIT_CODE(
      node_embedding_create_platform(argc, argv, nullptr, nullptr, &platform));
  if (!platform) {
    return node_embedding_exit_code_ok;  // early return
  }

  node_embedding_runtime runtime;
  CHECK_EXIT_CODE(CreateRuntime(
      platform,
      [&](node_embedding_platform platform, node_embedding_runtime runtime) {
        // The callback will be invoked from the runtime's event loop observer
        // thread. It must schedule the work to the UI thread's event loop.
        CHECK_EXIT_CODE(node_embedding_on_wake_up_event_loop(
            runtime,
            [](void* data, node_embedding_runtime runtime) {
              auto ui_queue = static_cast<UIQueue*>(data);
              ui_queue->PostTask([runtime, ui_queue]() {
                CHECK_EXIT_CODE_RETURN_VOID(node_embedding_run_event_loop(
                    runtime, node_embedding_event_loop_run_nowait, nullptr));

                // Check myCount and stop the processing when it reaches 5.
                CHECK_EXIT_CODE_RETURN_VOID(RunNodeApi(
                    runtime, [&](node_embedding_runtime runtime, napi_env env) {
                      napi_value global, my_count;
                      NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
                      NODE_API_CALL_RETURN_VOID(napi_get_named_property(
                          env, global, "myCount", &my_count));
                      napi_valuetype count_type;
                      NODE_API_CALL_RETURN_VOID(
                          napi_typeof(env, my_count, &count_type));
                      NODE_API_ASSERT_RETURN_VOID(count_type == napi_number);
                      int32_t count;
                      NODE_API_CALL_RETURN_VOID(
                          napi_get_value_int32(env, my_count, &count));
                      if (count == 5) {
                        node_embedding_complete_event_loop(runtime);
                        fprintf(stdout, "%d\n", count);
                        ui_queue->Stop();
                      }
                    }));
              });
            },
            &ui_queue));

        CHECK_EXIT_CODE(node_embedding_runtime_on_start_execution(
            runtime,
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
      &runtime));

  // The initial task starts the JS code that then will do the timer
  // scheduling. The timer supposed to be handled by the runtime's event loop.
  ui_queue.PostTask([runtime]() {
    node_embedding_exit_code exit_code =
        RunNodeApi(runtime, [&](node_embedding_runtime runtime, napi_env env) {
          napi_value undefined, global, func;
          NODE_API_CALL_RETURN_VOID(napi_get_undefined(env, &undefined));
          NODE_API_CALL_RETURN_VOID(napi_get_global(env, &global));
          NODE_API_CALL_RETURN_VOID(
              napi_get_named_property(env, global, "incMyCount", &func));

          napi_valuetype func_type;
          NODE_API_CALL_RETURN_VOID(napi_typeof(env, func, &func_type));
          NODE_API_ASSERT_RETURN_VOID(func_type == napi_function);
          NODE_API_CALL_RETURN_VOID(
              napi_call_function(env, undefined, func, 0, nullptr, nullptr));

          node_embedding_run_event_loop(
              runtime, node_embedding_event_loop_run_nowait, nullptr);
        });
    CHECK_EXIT_CODE_RETURN_VOID(exit_code);
  });

  ui_queue.Run();

  CHECK_EXIT_CODE(node_embedding_delete_runtime(runtime));
  CHECK_EXIT_CODE(node_embedding_delete_platform(platform));

  return node_embedding_exit_code_ok;
}
