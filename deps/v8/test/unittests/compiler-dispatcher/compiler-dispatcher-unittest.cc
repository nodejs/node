// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler-dispatcher/compiler-dispatcher.h"

#include <sstream>

#include "include/v8-platform.h"
#include "src/api.h"
#include "src/ast/ast-value-factory.h"
#include "src/base/platform/semaphore.h"
#include "src/base/template-utils.h"
#include "src/compiler-dispatcher/compiler-dispatcher-job.h"
#include "src/compiler-dispatcher/compiler-dispatcher-tracer.h"
#include "src/compiler-dispatcher/unoptimized-compile-job.h"
#include "src/compiler.h"
#include "src/flags.h"
#include "src/handles.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parsing.h"
#include "src/v8.h"
#include "test/unittests/test-helpers.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// V8 is smart enough to know something was already compiled and return compiled
// code straight away. We need a unique name for each test function so that V8
// returns an empty SharedFunctionInfo.
#define _STR(x) #x
#define STR(x) _STR(x)
#define _SCRIPT(fn, a, b, c) a fn b fn c
#define SCRIPT(a, b, c) _SCRIPT("f" STR(__LINE__), a, b, c)
#define TEST_SCRIPT() \
  "function f" STR(__LINE__) "(x, y) { return x * y }; f" STR(__LINE__) ";"

namespace v8 {
namespace internal {

class CompilerDispatcherTestFlags {
 public:
  static void SetFlagsForTest() {
    CHECK_NULL(save_flags_);
    save_flags_ = new SaveFlags();
    FLAG_single_threaded = true;
    FlagList::EnforceFlagImplications();
    FLAG_compiler_dispatcher = true;
  }

  static void RestoreFlags() {
    CHECK_NOT_NULL(save_flags_);
    delete save_flags_;
    save_flags_ = nullptr;
  }

 private:
  static SaveFlags* save_flags_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CompilerDispatcherTestFlags);
};

SaveFlags* CompilerDispatcherTestFlags::save_flags_ = nullptr;

class CompilerDispatcherTest : public TestWithNativeContext {
 public:
  CompilerDispatcherTest() = default;
  ~CompilerDispatcherTest() override = default;

  static void SetUpTestCase() {
    CompilerDispatcherTestFlags::SetFlagsForTest();
    TestWithNativeContext ::SetUpTestCase();
  }

  static void TearDownTestCase() {
    TestWithNativeContext ::TearDownTestCase();
    CompilerDispatcherTestFlags::RestoreFlags();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CompilerDispatcherTest);
};

namespace {

class MockPlatform : public v8::Platform {
 public:
  MockPlatform()
      : time_(0.0),
        time_step_(0.0),
        idle_task_(nullptr),
        sem_(0),
        tracing_controller_(V8::GetCurrentPlatform()->GetTracingController()) {}
  ~MockPlatform() override {
    base::LockGuard<base::Mutex> lock(&mutex_);
    EXPECT_TRUE(foreground_tasks_.empty());
    EXPECT_TRUE(worker_tasks_.empty());
    EXPECT_TRUE(idle_task_ == nullptr);
  }

  int NumberOfWorkerThreads() override { return 1; }

  std::shared_ptr<TaskRunner> GetForegroundTaskRunner(
      v8::Isolate* isolate) override {
    constexpr bool is_foreground_task_runner = true;
    return std::make_shared<MockTaskRunner>(this, is_foreground_task_runner);
  }

  std::shared_ptr<TaskRunner> GetWorkerThreadsTaskRunner(
      v8::Isolate* isolate) override {
    constexpr bool is_foreground_task_runner = false;
    return std::make_shared<MockTaskRunner>(this, is_foreground_task_runner);
  }

  void CallOnWorkerThread(std::unique_ptr<Task> task) override {
    base::LockGuard<base::Mutex> lock(&mutex_);
    worker_tasks_.push_back(std::move(task));
  }

  void CallOnForegroundThread(v8::Isolate* isolate, Task* task) override {
    base::LockGuard<base::Mutex> lock(&mutex_);
    foreground_tasks_.push_back(std::unique_ptr<Task>(task));
  }

  void CallDelayedOnForegroundThread(v8::Isolate* isolate, Task* task,
                                     double delay_in_seconds) override {
    UNREACHABLE();
  }

  void CallIdleOnForegroundThread(v8::Isolate* isolate,
                                  IdleTask* task) override {
    base::LockGuard<base::Mutex> lock(&mutex_);
    ASSERT_TRUE(idle_task_ == nullptr);
    idle_task_ = task;
  }

  bool IdleTasksEnabled(v8::Isolate* isolate) override { return true; }

  double MonotonicallyIncreasingTime() override {
    time_ += time_step_;
    return time_;
  }

  double CurrentClockTimeMillis() override {
    return time_ * base::Time::kMillisecondsPerSecond;
  }

  v8::TracingController* GetTracingController() override {
    return tracing_controller_;
  }

  void RunIdleTask(double deadline_in_seconds, double time_step) {
    time_step_ = time_step;
    IdleTask* task;
    {
      base::LockGuard<base::Mutex> lock(&mutex_);
      task = idle_task_;
      ASSERT_TRUE(idle_task_ != nullptr);
      idle_task_ = nullptr;
    }
    task->Run(deadline_in_seconds);
    delete task;
  }

  bool IdleTaskPending() {
    base::LockGuard<base::Mutex> lock(&mutex_);
    return idle_task_;
  }

  bool WorkerTasksPending() {
    base::LockGuard<base::Mutex> lock(&mutex_);
    return !worker_tasks_.empty();
  }

  bool ForegroundTasksPending() {
    base::LockGuard<base::Mutex> lock(&mutex_);
    return !foreground_tasks_.empty();
  }

  void RunWorkerTasksAndBlock(Platform* platform) {
    std::vector<std::unique_ptr<Task>> tasks;
    {
      base::LockGuard<base::Mutex> lock(&mutex_);
      tasks.swap(worker_tasks_);
    }
    platform->CallOnWorkerThread(
        base::make_unique<TaskWrapper>(this, std::move(tasks), true));
    sem_.Wait();
  }

  void RunWorkerTasks(Platform* platform) {
    std::vector<std::unique_ptr<Task>> tasks;
    {
      base::LockGuard<base::Mutex> lock(&mutex_);
      tasks.swap(worker_tasks_);
    }
    platform->CallOnWorkerThread(
        base::make_unique<TaskWrapper>(this, std::move(tasks), false));
  }

  void RunForegroundTasks() {
    std::vector<std::unique_ptr<Task>> tasks;
    {
      base::LockGuard<base::Mutex> lock(&mutex_);
      tasks.swap(foreground_tasks_);
    }
    for (auto& task : tasks) {
      task->Run();
      // Reset |task| before running the next one.
      task.reset();
    }
  }

  void ClearWorkerTasks() {
    std::vector<std::unique_ptr<Task>> tasks;
    {
      base::LockGuard<base::Mutex> lock(&mutex_);
      tasks.swap(worker_tasks_);
    }
  }

  void ClearForegroundTasks() {
    std::vector<std::unique_ptr<Task>> tasks;
    {
      base::LockGuard<base::Mutex> lock(&mutex_);
      tasks.swap(foreground_tasks_);
    }
  }

  void ClearIdleTask() {
    base::LockGuard<base::Mutex> lock(&mutex_);
    ASSERT_TRUE(idle_task_ != nullptr);
    delete idle_task_;
    idle_task_ = nullptr;
  }

 private:
  class TaskWrapper : public Task {
   public:
    TaskWrapper(MockPlatform* platform,
                std::vector<std::unique_ptr<Task>> tasks, bool signal)
        : platform_(platform), tasks_(std::move(tasks)), signal_(signal) {}
    ~TaskWrapper() = default;

    void Run() override {
      for (auto& task : tasks_) {
        task->Run();
        // Reset |task| before running the next one.
        task.reset();
      }
      if (signal_) platform_->sem_.Signal();
    }

   private:
    MockPlatform* platform_;
    std::vector<std::unique_ptr<Task>> tasks_;
    bool signal_;

    DISALLOW_COPY_AND_ASSIGN(TaskWrapper);
  };

  class MockTaskRunner final : public TaskRunner {
   public:
    MockTaskRunner(MockPlatform* platform, bool is_foreground_task_runner)
        : platform_(platform),
          is_foreground_task_runner_(is_foreground_task_runner) {}

    void PostTask(std::unique_ptr<v8::Task> task) override {
      base::LockGuard<base::Mutex> lock(&platform_->mutex_);
      if (is_foreground_task_runner_) {
        platform_->foreground_tasks_.push_back(std::move(task));
      } else {
        platform_->worker_tasks_.push_back(std::move(task));
      }
    }

    void PostDelayedTask(std::unique_ptr<Task> task,
                         double delay_in_seconds) override {
      UNREACHABLE();
    };

    void PostIdleTask(std::unique_ptr<IdleTask> task) override {
      DCHECK(IdleTasksEnabled());
      base::LockGuard<base::Mutex> lock(&platform_->mutex_);
      ASSERT_TRUE(platform_->idle_task_ == nullptr);
      platform_->idle_task_ = task.release();
    }

    bool IdleTasksEnabled() override {
      // Idle tasks are enabled only in the foreground task runner.
      return is_foreground_task_runner_;
    };

   private:
    MockPlatform* platform_;
    bool is_foreground_task_runner_;
  };

  double time_;
  double time_step_;

  // Protects all *_tasks_.
  base::Mutex mutex_;

  IdleTask* idle_task_;
  std::vector<std::unique_ptr<Task>> worker_tasks_;
  std::vector<std::unique_ptr<Task>> foreground_tasks_;

  base::Semaphore sem_;

  v8::TracingController* tracing_controller_;

  DISALLOW_COPY_AND_ASSIGN(MockPlatform);
};

}  // namespace

TEST_F(CompilerDispatcherTest, Construct) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);
}

TEST_F(CompilerDispatcherTest, IsEnqueued) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  const char script[] = TEST_SCRIPT();
  Handle<JSFunction> f = RunJS<JSFunction>(script);

  Handle<SharedFunctionInfo> shared(f->shared(), i_isolate());

  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_TRUE(dispatcher.Enqueue(shared));
  ASSERT_TRUE(dispatcher.IsEnqueued(shared));
  dispatcher.AbortAll(BlockingBehavior::kBlock);
  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_TRUE(platform.IdleTaskPending());
  platform.ClearIdleTask();
}

TEST_F(CompilerDispatcherTest, FinishNow) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  const char script[] = TEST_SCRIPT();
  Handle<JSFunction> f = RunJS<JSFunction>(script);
  Handle<SharedFunctionInfo> shared(f->shared(), i_isolate());

  ASSERT_FALSE(shared->is_compiled());
  ASSERT_TRUE(dispatcher.Enqueue(shared));
  ASSERT_TRUE(dispatcher.FinishNow(shared));
  // Finishing removes the SFI from the queue.
  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_TRUE(shared->is_compiled());
  ASSERT_TRUE(platform.IdleTaskPending());
  platform.ClearIdleTask();
}

TEST_F(CompilerDispatcherTest, FinishAllNow) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  constexpr int num_funcs = 2;
  Handle<JSFunction> f[num_funcs];
  Handle<SharedFunctionInfo> shared[num_funcs];

  for (int i = 0; i < num_funcs; ++i) {
    std::stringstream ss;
    ss << 'f' << STR(__LINE__) << '_' << i;
    std::string func_name = ss.str();
    std::string script("function f" + func_name + "(x, y) { return x * y }; f" +
                       func_name + ";");
    f[i] = RunJS<JSFunction>(script.c_str());
    shared[i] = Handle<SharedFunctionInfo>(f[i]->shared(), i_isolate());
    ASSERT_FALSE(shared[i]->is_compiled());
    ASSERT_TRUE(dispatcher.Enqueue(shared[i]));
  }
  dispatcher.FinishAllNow();
  for (int i = 0; i < num_funcs; ++i) {
    // Finishing removes the SFI from the queue.
    ASSERT_FALSE(dispatcher.IsEnqueued(shared[i]));
    ASSERT_TRUE(shared[i]->is_compiled());
  }
  platform.ClearIdleTask();
  platform.ClearWorkerTasks();
}

TEST_F(CompilerDispatcherTest, IdleTask) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  const char script[] = TEST_SCRIPT();
  Handle<JSFunction> f = RunJS<JSFunction>(script);
  Handle<SharedFunctionInfo> shared(f->shared(), i_isolate());

  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(dispatcher.Enqueue(shared));
  ASSERT_TRUE(platform.IdleTaskPending());

  // Since time doesn't progress on the MockPlatform, this is enough idle time
  // to finish compiling the function.
  platform.RunIdleTask(1000.0, 0.0);

  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_TRUE(shared->is_compiled());
}

TEST_F(CompilerDispatcherTest, IdleTaskSmallIdleTime) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  const char script[] = TEST_SCRIPT();
  Handle<JSFunction> f = RunJS<JSFunction>(script);
  Handle<SharedFunctionInfo> shared(f->shared(), i_isolate());

  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(dispatcher.Enqueue(shared));
  ASSERT_TRUE(platform.IdleTaskPending());

  // The job should be scheduled for the main thread.
  ASSERT_EQ(dispatcher.jobs_.size(), 1u);
  ASSERT_EQ(UnoptimizedCompileJob::Status::kInitial,
            dispatcher.jobs_.begin()->second->status());

  // Only grant a little idle time and have time advance beyond it in one step.
  platform.RunIdleTask(2.0, 1.0);

  ASSERT_TRUE(dispatcher.IsEnqueued(shared));
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_TRUE(platform.IdleTaskPending());

  // The job should be still scheduled for the main thread, but ready for
  // parsing.
  ASSERT_EQ(dispatcher.jobs_.size(), 1u);
  ASSERT_EQ(UnoptimizedCompileJob::Status::kPrepared,
            dispatcher.jobs_.begin()->second->status());

  // Now grant a lot of idle time and freeze time.
  platform.RunIdleTask(1000.0, 0.0);

  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_TRUE(shared->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());
}

TEST_F(CompilerDispatcherTest, IdleTaskException) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, 50);

  std::string func_name("f" STR(__LINE__));
  std::string script("function " + func_name + "(x) { var a = ");
  for (int i = 0; i < 500; i++) {
    // Alternate + and - to avoid n-ary operation nodes.
    script += "'x' + 'x' - ";
  }
  script += " 'x'; }; " + func_name + ";";
  Handle<JSFunction> f = RunJS<JSFunction>(script.c_str());
  Handle<SharedFunctionInfo> shared(f->shared(), i_isolate());

  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(dispatcher.Enqueue(shared));
  ASSERT_TRUE(platform.IdleTaskPending());

  // Since time doesn't progress on the MockPlatform, this is enough idle time
  // to finish compiling the function.
  platform.RunIdleTask(1000.0, 0.0);

  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_FALSE(i_isolate()->has_pending_exception());
}

TEST_F(CompilerDispatcherTest, CompileOnBackgroundThread) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  const char script[] = TEST_SCRIPT();
  Handle<JSFunction> f = RunJS<JSFunction>(script);
  Handle<SharedFunctionInfo> shared(f->shared(), i_isolate());

  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(dispatcher.Enqueue(shared));
  ASSERT_TRUE(platform.IdleTaskPending());

  ASSERT_EQ(dispatcher.jobs_.size(), 1u);
  ASSERT_EQ(UnoptimizedCompileJob::Status::kInitial,
            dispatcher.jobs_.begin()->second->status());

  // Make compiling super expensive, and advance job as much as possible on the
  // foreground thread.
  dispatcher.tracer_->RecordCompile(50000.0, 1);
  platform.RunIdleTask(10.0, 0.0);
  ASSERT_EQ(UnoptimizedCompileJob::Status::kPrepared,
            dispatcher.jobs_.begin()->second->status());

  ASSERT_TRUE(dispatcher.IsEnqueued(shared));
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(platform.WorkerTasksPending());

  platform.RunWorkerTasksAndBlock(V8::GetCurrentPlatform());

  ASSERT_TRUE(platform.IdleTaskPending());
  ASSERT_FALSE(platform.WorkerTasksPending());
  ASSERT_EQ(UnoptimizedCompileJob::Status::kCompiled,
            dispatcher.jobs_.begin()->second->status());

  // Now grant a lot of idle time and freeze time.
  platform.RunIdleTask(1000.0, 0.0);

  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_TRUE(shared->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());
}

TEST_F(CompilerDispatcherTest, FinishNowWithWorkerTask) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  const char script[] = TEST_SCRIPT();
  Handle<JSFunction> f = RunJS<JSFunction>(script);
  Handle<SharedFunctionInfo> shared(f->shared(), i_isolate());

  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(dispatcher.Enqueue(shared));
  ASSERT_TRUE(platform.IdleTaskPending());

  ASSERT_EQ(dispatcher.jobs_.size(), 1u);
  ASSERT_EQ(UnoptimizedCompileJob::Status::kInitial,
            dispatcher.jobs_.begin()->second->status());

  // Make compiling super expensive, and advance job as much as possible on the
  // foreground thread.
  dispatcher.tracer_->RecordCompile(50000.0, 1);
  platform.RunIdleTask(10.0, 0.0);
  ASSERT_EQ(UnoptimizedCompileJob::Status::kPrepared,
            dispatcher.jobs_.begin()->second->status());

  ASSERT_TRUE(dispatcher.IsEnqueued(shared));
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(platform.WorkerTasksPending());

  // This does not block, but races with the FinishNow() call below.
  platform.RunWorkerTasks(V8::GetCurrentPlatform());

  ASSERT_TRUE(dispatcher.FinishNow(shared));
  // Finishing removes the SFI from the queue.
  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_TRUE(shared->is_compiled());
  if (platform.IdleTaskPending()) platform.ClearIdleTask();
  ASSERT_FALSE(platform.WorkerTasksPending());
}

TEST_F(CompilerDispatcherTest, IdleTaskMultipleJobs) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  const char script1[] = TEST_SCRIPT();
  Handle<JSFunction> f1 = RunJS<JSFunction>(script1);
  Handle<SharedFunctionInfo> shared1(f1->shared(), i_isolate());

  const char script2[] = TEST_SCRIPT();
  Handle<JSFunction> f2 = RunJS<JSFunction>(script2);
  Handle<SharedFunctionInfo> shared2(f2->shared(), i_isolate());

  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(dispatcher.Enqueue(shared1));
  ASSERT_TRUE(dispatcher.Enqueue(shared2));
  ASSERT_TRUE(platform.IdleTaskPending());

  // Since time doesn't progress on the MockPlatform, this is enough idle time
  // to finish compiling the function.
  platform.RunIdleTask(1000.0, 0.0);

  ASSERT_FALSE(dispatcher.IsEnqueued(shared1));
  ASSERT_FALSE(dispatcher.IsEnqueued(shared2));
  ASSERT_TRUE(shared1->is_compiled());
  ASSERT_TRUE(shared2->is_compiled());
}

TEST_F(CompilerDispatcherTest, FinishNowException) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, 50);

  std::string func_name("f" STR(__LINE__));
  std::string script("function " + func_name + "(x) { var a = ");
  for (int i = 0; i < 500; i++) {
    // Alternate + and - to avoid n-ary operation nodes.
    script += "'x' + 'x' - ";
  }
  script += " 'x'; }; " + func_name + ";";
  Handle<JSFunction> f = RunJS<JSFunction>(script.c_str());
  Handle<SharedFunctionInfo> shared(f->shared(), i_isolate());

  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(dispatcher.Enqueue(shared));
  ASSERT_TRUE(platform.IdleTaskPending());

  ASSERT_FALSE(dispatcher.FinishNow(shared));

  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_TRUE(i_isolate()->has_pending_exception());

  i_isolate()->clear_pending_exception();
  platform.ClearIdleTask();
}

TEST_F(CompilerDispatcherTest, AsyncAbortAllPendingWorkerTask) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  const char script[] = TEST_SCRIPT();
  Handle<JSFunction> f = RunJS<JSFunction>(script);
  Handle<SharedFunctionInfo> shared(f->shared(), i_isolate());

  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(dispatcher.Enqueue(shared));
  ASSERT_TRUE(platform.IdleTaskPending());

  ASSERT_EQ(dispatcher.jobs_.size(), 1u);
  ASSERT_EQ(UnoptimizedCompileJob::Status::kInitial,
            dispatcher.jobs_.begin()->second->status());

  // Make compiling super expensive, and advance job as much as possible on the
  // foreground thread.
  dispatcher.tracer_->RecordCompile(50000.0, 1);
  platform.RunIdleTask(10.0, 0.0);
  ASSERT_EQ(UnoptimizedCompileJob::Status::kPrepared,
            dispatcher.jobs_.begin()->second->status());

  ASSERT_TRUE(dispatcher.IsEnqueued(shared));
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(platform.WorkerTasksPending());

  // The background task hasn't yet started, so we can just cancel it.
  dispatcher.AbortAll(BlockingBehavior::kDontBlock);
  ASSERT_FALSE(platform.ForegroundTasksPending());

  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_FALSE(shared->is_compiled());

  platform.RunWorkerTasksAndBlock(V8::GetCurrentPlatform());

  if (platform.IdleTaskPending()) platform.ClearIdleTask();
  ASSERT_FALSE(platform.WorkerTasksPending());
  ASSERT_FALSE(platform.ForegroundTasksPending());
}

TEST_F(CompilerDispatcherTest, AsyncAbortAllRunningWorkerTask) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  const char script1[] = TEST_SCRIPT();
  Handle<JSFunction> f1 = RunJS<JSFunction>(script1);
  Handle<SharedFunctionInfo> shared1(f1->shared(), i_isolate());

  const char script2[] = TEST_SCRIPT();
  Handle<JSFunction> f2 = RunJS<JSFunction>(script2);
  Handle<SharedFunctionInfo> shared2(f2->shared(), i_isolate());

  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(dispatcher.Enqueue(shared1));
  ASSERT_TRUE(platform.IdleTaskPending());

  ASSERT_EQ(dispatcher.jobs_.size(), 1u);
  ASSERT_EQ(UnoptimizedCompileJob::Status::kInitial,
            dispatcher.jobs_.begin()->second->status());

  // Make compiling super expensive, and advance job as much as possible on the
  // foreground thread.
  dispatcher.tracer_->RecordCompile(50000.0, 1);
  platform.RunIdleTask(10.0, 0.0);
  ASSERT_EQ(UnoptimizedCompileJob::Status::kPrepared,
            dispatcher.jobs_.begin()->second->status());

  ASSERT_TRUE(dispatcher.IsEnqueued(shared1));
  ASSERT_FALSE(shared1->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(platform.WorkerTasksPending());

  // Kick off background tasks and freeze them.
  dispatcher.block_for_testing_.SetValue(true);
  platform.RunWorkerTasks(V8::GetCurrentPlatform());

  // Busy loop until the background task started running.
  while (dispatcher.block_for_testing_.Value()) {
  }
  dispatcher.AbortAll(BlockingBehavior::kDontBlock);
  ASSERT_TRUE(platform.ForegroundTasksPending());

  // We can't schedule new tasks while we're aborting.
  ASSERT_FALSE(dispatcher.Enqueue(shared2));

  // Run the first AbortTask. Since the background job is still pending, it
  // can't do anything.
  platform.RunForegroundTasks();
  {
    base::LockGuard<base::Mutex> lock(&dispatcher.mutex_);
    ASSERT_TRUE(dispatcher.abort_);
  }

  // Release background task.
  dispatcher.semaphore_for_testing_.Signal();

  // Busy loop until the background task scheduled another AbortTask task.
  while (!platform.ForegroundTasksPending()) {
  }

  platform.RunForegroundTasks();
  ASSERT_TRUE(dispatcher.jobs_.empty());
  {
    base::LockGuard<base::Mutex> lock(&dispatcher.mutex_);
    ASSERT_FALSE(dispatcher.abort_);
  }

  ASSERT_TRUE(platform.IdleTaskPending());
  platform.RunIdleTask(5.0, 1.0);
  ASSERT_FALSE(platform.WorkerTasksPending());
  ASSERT_FALSE(platform.ForegroundTasksPending());

  // Now it's possible to enqueue new functions again.
  ASSERT_TRUE(dispatcher.Enqueue(shared2));
  ASSERT_TRUE(platform.IdleTaskPending());
  ASSERT_FALSE(platform.WorkerTasksPending());
  ASSERT_FALSE(platform.ForegroundTasksPending());
  platform.ClearIdleTask();
}

TEST_F(CompilerDispatcherTest, FinishNowDuringAbortAll) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  const char script[] = TEST_SCRIPT();
  Handle<JSFunction> f = RunJS<JSFunction>(script);
  Handle<SharedFunctionInfo> shared(f->shared(), i_isolate());

  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(dispatcher.Enqueue(shared));
  ASSERT_TRUE(platform.IdleTaskPending());

  ASSERT_EQ(dispatcher.jobs_.size(), 1u);
  ASSERT_EQ(UnoptimizedCompileJob::Status::kInitial,
            dispatcher.jobs_.begin()->second->status());

  // Make compiling super expensive, and advance job as much as possible on the
  // foreground thread.
  dispatcher.tracer_->RecordCompile(50000.0, 1);
  platform.RunIdleTask(10.0, 0.0);
  ASSERT_EQ(UnoptimizedCompileJob::Status::kPrepared,
            dispatcher.jobs_.begin()->second->status());

  ASSERT_TRUE(dispatcher.IsEnqueued(shared));
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(platform.WorkerTasksPending());

  // Kick off background tasks and freeze them.
  dispatcher.block_for_testing_.SetValue(true);
  platform.RunWorkerTasks(V8::GetCurrentPlatform());

  // Busy loop until the background task started running.
  while (dispatcher.block_for_testing_.Value()) {
  }
  dispatcher.AbortAll(BlockingBehavior::kDontBlock);
  ASSERT_TRUE(platform.ForegroundTasksPending());

  // Run the first AbortTask. Since the background job is still pending, it
  // can't do anything.
  platform.RunForegroundTasks();
  {
    base::LockGuard<base::Mutex> lock(&dispatcher.mutex_);
    ASSERT_TRUE(dispatcher.abort_);
  }

  // While the background thread holds on to a job, it is still enqueud.
  ASSERT_TRUE(dispatcher.IsEnqueued(shared));

  // Release background task.
  dispatcher.semaphore_for_testing_.Signal();

  // Force the compilation to finish, even while aborting.
  ASSERT_TRUE(dispatcher.FinishNow(shared));
  ASSERT_TRUE(dispatcher.jobs_.empty());

  // Busy wait for the background task to finish.
  for (;;) {
    base::LockGuard<base::Mutex> lock(&dispatcher.mutex_);
    if (dispatcher.num_worker_tasks_ == 0) {
      break;
    }
  }

  ASSERT_TRUE(platform.ForegroundTasksPending());
  ASSERT_TRUE(platform.IdleTaskPending());
  ASSERT_FALSE(platform.WorkerTasksPending());

  platform.RunForegroundTasks();
  {
    base::LockGuard<base::Mutex> lock(&dispatcher.mutex_);
    ASSERT_FALSE(dispatcher.abort_);
  }

  platform.ClearForegroundTasks();
  platform.ClearIdleTask();
}

TEST_F(CompilerDispatcherTest, MemoryPressure) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  const char script[] = TEST_SCRIPT();
  Handle<JSFunction> f = RunJS<JSFunction>(script);
  Handle<SharedFunctionInfo> shared(f->shared(), i_isolate());

  // Can't enqueue tasks under memory pressure.
  dispatcher.MemoryPressureNotification(v8::MemoryPressureLevel::kCritical,
                                        true);
  ASSERT_FALSE(dispatcher.Enqueue(shared));

  dispatcher.MemoryPressureNotification(v8::MemoryPressureLevel::kNone, true);
  ASSERT_TRUE(dispatcher.Enqueue(shared));

  // Memory pressure cancels current jobs.
  dispatcher.MemoryPressureNotification(v8::MemoryPressureLevel::kCritical,
                                        true);
  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  platform.ClearIdleTask();
}

namespace {

class PressureNotificationTask : public CancelableTask {
 public:
  PressureNotificationTask(Isolate* isolate, CompilerDispatcher* dispatcher,
                           base::Semaphore* sem)
      : CancelableTask(isolate), dispatcher_(dispatcher), sem_(sem) {}
  ~PressureNotificationTask() override {}

  void RunInternal() override {
    dispatcher_->MemoryPressureNotification(v8::MemoryPressureLevel::kCritical,
                                            false);
    sem_->Signal();
  }

 private:
  CompilerDispatcher* dispatcher_;
  base::Semaphore* sem_;

  DISALLOW_COPY_AND_ASSIGN(PressureNotificationTask);
};

}  // namespace

TEST_F(CompilerDispatcherTest, MemoryPressureFromBackground) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  const char script[] = TEST_SCRIPT();
  Handle<JSFunction> f = RunJS<JSFunction>(script);
  Handle<SharedFunctionInfo> shared(f->shared(), i_isolate());

  ASSERT_TRUE(dispatcher.Enqueue(shared));
  base::Semaphore sem(0);
  V8::GetCurrentPlatform()->CallOnWorkerThread(
      base::make_unique<PressureNotificationTask>(i_isolate(), &dispatcher,
                                                  &sem));

  sem.Wait();

  // A memory pressure task is pending, and running it will cancel the job.
  ASSERT_TRUE(platform.ForegroundTasksPending());
  ASSERT_TRUE(dispatcher.IsEnqueued(shared));
  platform.RunForegroundTasks();
  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_FALSE(shared->is_compiled());

  // Since the AbortAll() call is made from a task, AbortAll thinks that there
  // is at least one task running, and fires of an AbortTask to be safe.
  ASSERT_TRUE(platform.ForegroundTasksPending());
  platform.RunForegroundTasks();
  ASSERT_FALSE(platform.ForegroundTasksPending());

  platform.ClearIdleTask();
}

TEST_F(CompilerDispatcherTest, EnqueueJob) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);
  const char script[] = TEST_SCRIPT();
  Handle<JSFunction> f = RunJS<JSFunction>(script);
  Handle<SharedFunctionInfo> shared(f->shared(), i_isolate());
  std::unique_ptr<CompilerDispatcherJob> job(
      new UnoptimizedCompileJob(i_isolate(), dispatcher.tracer_.get(), shared,
                                dispatcher.max_stack_size_));
  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  dispatcher.Enqueue(std::move(job));
  ASSERT_TRUE(dispatcher.IsEnqueued(shared));

  ASSERT_TRUE(platform.IdleTaskPending());
  platform.ClearIdleTask();
  ASSERT_FALSE(platform.WorkerTasksPending());
}

TEST_F(CompilerDispatcherTest, EnqueueAndStep) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  const char script[] = TEST_SCRIPT();
  Handle<JSFunction> f = RunJS<JSFunction>(script);
  Handle<SharedFunctionInfo> shared(f->shared(), i_isolate());

  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_TRUE(dispatcher.EnqueueAndStep(shared));
  ASSERT_TRUE(dispatcher.IsEnqueued(shared));

  ASSERT_EQ(UnoptimizedCompileJob::Status::kPrepared,
            dispatcher.jobs_.begin()->second->status());

  ASSERT_TRUE(platform.IdleTaskPending());
  platform.ClearIdleTask();
  ASSERT_TRUE(platform.WorkerTasksPending());
  platform.ClearWorkerTasks();
}

TEST_F(CompilerDispatcherTest, CompileLazyFinishesDispatcherJob) {
  // Use the real dispatcher so that CompileLazy checks the same one for
  // enqueued functions.
  CompilerDispatcher* dispatcher = i_isolate()->compiler_dispatcher();

  const char script[] = "function lazy() { return 42; }; lazy;";
  Handle<JSFunction> f = RunJS<JSFunction>(script);
  Handle<SharedFunctionInfo> shared(f->shared(), i_isolate());

  ASSERT_FALSE(shared->is_compiled());
  ASSERT_FALSE(dispatcher->IsEnqueued(shared));
  ASSERT_TRUE(dispatcher->Enqueue(shared));
  ASSERT_TRUE(dispatcher->IsEnqueued(shared));

  // Now force the function to run and ensure CompileLazy finished and dequeues
  // it from the dispatcher.
  RunJS("lazy();");
  ASSERT_TRUE(shared->is_compiled());
  ASSERT_FALSE(dispatcher->IsEnqueued(shared));
}

TEST_F(CompilerDispatcherTest, CompileLazy2FinishesDispatcherJob) {
  // Use the real dispatcher so that CompileLazy checks the same one for
  // enqueued functions.
  CompilerDispatcher* dispatcher = i_isolate()->compiler_dispatcher();

  const char source2[] = "function lazy2() { return 42; }; lazy2;";
  Handle<JSFunction> lazy2 = RunJS<JSFunction>(source2);
  Handle<SharedFunctionInfo> shared2(lazy2->shared(), i_isolate());
  ASSERT_FALSE(shared2->is_compiled());

  const char source1[] = "function lazy1() { return lazy2(); }; lazy1;";
  Handle<JSFunction> lazy1 = RunJS<JSFunction>(source1);
  Handle<SharedFunctionInfo> shared1(lazy1->shared(), i_isolate());
  ASSERT_FALSE(shared1->is_compiled());

  ASSERT_TRUE(dispatcher->Enqueue(shared1));
  ASSERT_TRUE(dispatcher->Enqueue(shared2));

  RunJS("lazy1();");
  ASSERT_TRUE(shared1->is_compiled());
  ASSERT_TRUE(shared2->is_compiled());
  ASSERT_FALSE(dispatcher->IsEnqueued(shared1));
  ASSERT_FALSE(dispatcher->IsEnqueued(shared2));
}

TEST_F(CompilerDispatcherTest, EnqueueAndStepTwice) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  const char script[] = TEST_SCRIPT();
  Handle<JSFunction> f = RunJS<JSFunction>(script);
  Handle<SharedFunctionInfo> shared(f->shared(), i_isolate());

  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_TRUE(dispatcher.EnqueueAndStep(shared));
  ASSERT_TRUE(dispatcher.IsEnqueued(shared));
  ASSERT_EQ(UnoptimizedCompileJob::Status::kPrepared,
            dispatcher.jobs_.begin()->second->status());

  // EnqueueAndStep of the same function again (shouldn't step the job.
  ASSERT_TRUE(dispatcher.EnqueueAndStep(shared));
  ASSERT_EQ(UnoptimizedCompileJob::Status::kPrepared,
            dispatcher.jobs_.begin()->second->status());

  ASSERT_TRUE(platform.IdleTaskPending());
  ASSERT_TRUE(platform.WorkerTasksPending());
  platform.ClearIdleTask();
  platform.ClearWorkerTasks();
}

TEST_F(CompilerDispatcherTest, CompileMultipleOnBackgroundThread) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  const char script1[] = TEST_SCRIPT();
  Handle<JSFunction> f1 = RunJS<JSFunction>(script1);
  Handle<SharedFunctionInfo> shared1(f1->shared(), i_isolate());
  const char script2[] = TEST_SCRIPT();
  Handle<JSFunction> f2 = RunJS<JSFunction>(script2);
  Handle<SharedFunctionInfo> shared2(f2->shared(), i_isolate());

  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(dispatcher.Enqueue(shared1));
  ASSERT_TRUE(dispatcher.Enqueue(shared2));
  ASSERT_TRUE(platform.IdleTaskPending());

  ASSERT_EQ(dispatcher.jobs_.size(), 2u);
  ASSERT_EQ(UnoptimizedCompileJob::Status::kInitial,
            dispatcher.jobs_.begin()->second->status());
  ASSERT_EQ(UnoptimizedCompileJob::Status::kInitial,
            (++dispatcher.jobs_.begin())->second->status());

  // Make compiling super expensive, and advance job as much as possible on the
  // foreground thread.
  dispatcher.tracer_->RecordCompile(50000.0, 1);
  platform.RunIdleTask(10.0, 0.0);
  ASSERT_EQ(dispatcher.jobs_.size(), 2u);
  ASSERT_EQ(UnoptimizedCompileJob::Status::kPrepared,
            dispatcher.jobs_.begin()->second->status());
  ASSERT_EQ(UnoptimizedCompileJob::Status::kPrepared,
            (++dispatcher.jobs_.begin())->second->status());

  ASSERT_TRUE(dispatcher.IsEnqueued(shared1));
  ASSERT_TRUE(dispatcher.IsEnqueued(shared2));
  ASSERT_FALSE(shared1->is_compiled());
  ASSERT_FALSE(shared2->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(platform.WorkerTasksPending());

  platform.RunWorkerTasksAndBlock(V8::GetCurrentPlatform());

  ASSERT_TRUE(platform.IdleTaskPending());
  ASSERT_FALSE(platform.WorkerTasksPending());
  ASSERT_EQ(dispatcher.jobs_.size(), 2u);
  ASSERT_EQ(UnoptimizedCompileJob::Status::kCompiled,
            dispatcher.jobs_.begin()->second->status());
  ASSERT_EQ(UnoptimizedCompileJob::Status::kCompiled,
            (++dispatcher.jobs_.begin())->second->status());

  // Now grant a lot of idle time and freeze time.
  platform.RunIdleTask(1000.0, 0.0);

  ASSERT_FALSE(dispatcher.IsEnqueued(shared1));
  ASSERT_FALSE(dispatcher.IsEnqueued(shared2));
  ASSERT_TRUE(shared1->is_compiled());
  ASSERT_TRUE(shared2->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());
}

#undef _STR
#undef STR
#undef _SCRIPT
#undef SCRIPT
#undef TEST_SCRIPT

}  // namespace internal
}  // namespace v8
