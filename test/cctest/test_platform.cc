#include "node_internals.h"
#include "libplatform/libplatform.h"

#include <string>
#include "gtest/gtest.h"
#include "node_test_fixture.h"

// This task increments the given run counter and reposts itself until the
// repost counter reaches zero.
class RepostingTask : public v8::Task {
 public:
  explicit RepostingTask(int repost_count,
                         int* run_count,
                         v8::Isolate* isolate,
                         node::NodePlatform* platform)
      : repost_count_(repost_count),
        run_count_(run_count),
        isolate_(isolate),
        platform_(platform) {}

  // v8::Task implementation
  void Run() final {
    ++*run_count_;
    if (repost_count_ > 0) {
      --repost_count_;
      std::shared_ptr<v8::TaskRunner> task_runner =
          platform_->GetForegroundTaskRunner(isolate_,
                                             v8::TaskPriority::kUserBlocking);
      task_runner->PostTask(std::make_unique<RepostingTask>(
          repost_count_, run_count_, isolate_, platform_));
    }
  }

 private:
  int repost_count_;
  int* run_count_;
  v8::Isolate* isolate_;
  node::NodePlatform* platform_;
};

class NoopTask : public v8::Task {
 public:
  void Run() final {}
};

class PostDelayedTaskAfterShutdownStartsTask : public v8::Task {
 public:
  PostDelayedTaskAfterShutdownStartsTask(node::NodePlatform* platform,
                                         uv_sem_t* task_started,
                                         uv_sem_t* post_task)
      : platform_(platform),
        task_started_(task_started),
        post_task_(post_task) {}

  void Run() final {
    uv_sem_post(task_started_);
    uv_sem_wait(post_task_);
    uv_sleep(50);
    platform_->PostDelayedTaskOnWorkerThreadImpl(v8::TaskPriority::kUserVisible,
                                                 std::make_unique<NoopTask>(),
                                                 1.0,
                                                 v8::SourceLocation());
  }

 private:
  node::NodePlatform* platform_;
  uv_sem_t* task_started_;
  uv_sem_t* post_task_;
};

static void ShutdownPlatform(void* arg) {
  static_cast<node::NodePlatform*>(arg)->Shutdown();
}

class PlatformTest : public EnvironmentTestFixture {};

TEST_F(PlatformTest, SkipNewTasksInFlushForegroundTasks) {
  v8::Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env {handle_scope, argv};
  int run_count = 0;
  std::shared_ptr<v8::TaskRunner> task_runner =
      platform->GetForegroundTaskRunner(isolate_,
                                        v8::TaskPriority::kUserBlocking);
  task_runner->PostTask(
      std::make_unique<RepostingTask>(2, &run_count, isolate_, platform.get()));
  EXPECT_TRUE(platform->FlushForegroundTasks(isolate_));
  EXPECT_EQ(1, run_count);
  EXPECT_TRUE(platform->FlushForegroundTasks(isolate_));
  EXPECT_EQ(2, run_count);
  EXPECT_TRUE(platform->FlushForegroundTasks(isolate_));
  EXPECT_EQ(3, run_count);
  EXPECT_FALSE(platform->FlushForegroundTasks(isolate_));
}

// Regression test: a worker thread posting a delayed task concurrently with
// platform shutdown must not call uv_async_send() on the scheduler's
// flush_tasks_ handle after Stop() has begun closing it. The two semaphores
// and the sleeps pin the ordering so the delayed task is posted only after
// Shutdown() has run, which is the window that used to abort with
// "Assertion failed: !(handle->flags & UV_HANDLE_CLOSING)". The test passes
// when shutdown completes without crashing.
TEST_F(NodeZeroIsolateTestFixture, DelayedWorkerTaskDuringPlatformShutdown) {
  node::NodePlatform test_platform(1, tracing_agent->GetTracingController());

  uv_sem_t task_started;
  uv_sem_t post_task;
  ASSERT_EQ(0, uv_sem_init(&task_started, 0));
  ASSERT_EQ(0, uv_sem_init(&post_task, 0));

  test_platform.PostTaskOnWorkerThreadImpl(
      v8::TaskPriority::kUserBlocking,
      std::make_unique<PostDelayedTaskAfterShutdownStartsTask>(
          &test_platform, &task_started, &post_task),
      v8::SourceLocation());

  uv_sem_wait(&task_started);

  uv_thread_t shutdown_thread;
  ASSERT_EQ(
      0, uv_thread_create(&shutdown_thread, ShutdownPlatform, &test_platform));
  uv_sleep(50);
  uv_sem_post(&post_task);
  ASSERT_EQ(0, uv_thread_join(&shutdown_thread));

  uv_sem_destroy(&post_task);
  uv_sem_destroy(&task_started);
}

// Tests the registration of an abstract `IsolatePlatformDelegate` instance as
// opposed to the more common `uv_loop_s*` version of `RegisterIsolate`.
TEST_F(NodeZeroIsolateTestFixture, IsolatePlatformDelegateTest) {
  // Allocate isolate
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = allocator.get();
  create_params.cpp_heap =
      v8::CppHeap::Create(platform.get(), v8::CppHeapCreateParams{{}})
          .release();
  auto isolate = v8::Isolate::Allocate();
  CHECK_NOT_NULL(isolate);

  // Register *first*, then initialize
  auto delegate = std::make_shared<node::PerIsolatePlatformData>(
    isolate,
    &current_loop);
  platform->RegisterIsolate(isolate, delegate.get());
  v8::Isolate::Initialize(isolate, create_params);

  // Try creating Context + IsolateData + Environment
  {
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);

    auto context = node::NewContext(isolate);
    CHECK(!context.IsEmpty());
    v8::Context::Scope context_scope(context);

    std::unique_ptr<node::IsolateData, decltype(&node::FreeIsolateData)>
      isolate_data{node::CreateIsolateData(isolate,
                                           &current_loop,
                                           platform.get()),
                   node::FreeIsolateData};
    CHECK(isolate_data);

    std::unique_ptr<node::Environment, decltype(&node::FreeEnvironment)>
      environment{node::CreateEnvironment(isolate_data.get(),
                                          context,
                                          {},
                                          {}),
                  node::FreeEnvironment};
    CHECK(environment);
  }

  // Graceful shutdown
  delegate->Shutdown();
  platform->DisposeIsolate(isolate);
}

TEST_F(PlatformTest, TracingControllerNullptr) {
  v8::TracingController* orig_controller = node::GetTracingController();
  node::SetTracingController(nullptr);
  EXPECT_EQ(node::GetTracingController(), nullptr);

  v8::Isolate::Scope isolate_scope(isolate_);
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env {handle_scope, argv};

  node::LoadEnvironment(*env, [&](const node::StartExecutionCallbackInfo& info)
                                  -> v8::MaybeLocal<v8::Value> {
    return v8::Null(isolate_);
  });

  node::SetTracingController(orig_controller);
  EXPECT_EQ(node::GetTracingController(), orig_controller);
}
