// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler-dispatcher/compiler-dispatcher.h"

#include <sstream>

#include "include/v8-platform.h"
#include "src/api-inl.h"
#include "src/ast/ast-value-factory.h"
#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/base/platform/semaphore.h"
#include "src/base/template-utils.h"
#include "src/compiler.h"
#include "src/flags.h"
#include "src/handles.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parsing.h"
#include "src/v8.h"
#include "src/zone/zone-list-inl.h"
#include "test/unittests/test-helpers.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    TestWithNativeContext::SetUpTestCase();
  }

  static void TearDownTestCase() {
    TestWithNativeContext::TearDownTestCase();
    CompilerDispatcherTestFlags::RestoreFlags();
  }

  static base::Optional<CompilerDispatcher::JobId> EnqueueUnoptimizedCompileJob(
      CompilerDispatcher* dispatcher, Isolate* isolate,
      Handle<SharedFunctionInfo> shared) {
    std::unique_ptr<ParseInfo> outer_parse_info =
        test::OuterParseInfoForShared(isolate, shared);
    AstValueFactory* ast_value_factory =
        outer_parse_info->GetOrCreateAstValueFactory();
    AstNodeFactory ast_node_factory(ast_value_factory,
                                    outer_parse_info->zone());

    const AstRawString* function_name =
        ast_value_factory->GetOneByteString("f");
    DeclarationScope* script_scope = new (outer_parse_info->zone())
        DeclarationScope(outer_parse_info->zone(), ast_value_factory);
    DeclarationScope* function_scope =
        new (outer_parse_info->zone()) DeclarationScope(
            outer_parse_info->zone(), script_scope, FUNCTION_SCOPE);
    function_scope->set_start_position(shared->StartPosition());
    function_scope->set_end_position(shared->EndPosition());
    std::vector<void*> pointer_buffer;
    ScopedPtrList<Statement> statements(&pointer_buffer);
    const FunctionLiteral* function_literal =
        ast_node_factory.NewFunctionLiteral(
            function_name, function_scope, statements, -1, -1, -1,
            FunctionLiteral::kNoDuplicateParameters,
            FunctionLiteral::kAnonymousExpression,
            FunctionLiteral::kShouldEagerCompile, shared->StartPosition(), true,
            shared->FunctionLiteralId(isolate), nullptr);

    return dispatcher->Enqueue(outer_parse_info.get(), function_name,
                               function_literal);
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
    base::MutexGuard lock(&mutex_);
    EXPECT_TRUE(foreground_tasks_.empty());
    EXPECT_TRUE(worker_tasks_.empty());
    EXPECT_TRUE(idle_task_ == nullptr);
  }

  int NumberOfWorkerThreads() override { return 1; }

  std::shared_ptr<TaskRunner> GetForegroundTaskRunner(
      v8::Isolate* isolate) override {
    return std::make_shared<MockForegroundTaskRunner>(this);
  }

  void CallOnWorkerThread(std::unique_ptr<Task> task) override {
    base::MutexGuard lock(&mutex_);
    worker_tasks_.push_back(std::move(task));
  }

  void CallDelayedOnWorkerThread(std::unique_ptr<Task> task,
                                 double delay_in_seconds) override {
    UNREACHABLE();
  }

  void CallOnForegroundThread(v8::Isolate* isolate, Task* task) override {
    base::MutexGuard lock(&mutex_);
    foreground_tasks_.push_back(std::unique_ptr<Task>(task));
  }

  void CallDelayedOnForegroundThread(v8::Isolate* isolate, Task* task,
                                     double delay_in_seconds) override {
    UNREACHABLE();
  }

  void CallIdleOnForegroundThread(v8::Isolate* isolate,
                                  IdleTask* task) override {
    base::MutexGuard lock(&mutex_);
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
      base::MutexGuard lock(&mutex_);
      task = idle_task_;
      ASSERT_TRUE(idle_task_ != nullptr);
      idle_task_ = nullptr;
    }
    task->Run(deadline_in_seconds);
    delete task;
  }

  bool IdleTaskPending() {
    base::MutexGuard lock(&mutex_);
    return idle_task_;
  }

  bool WorkerTasksPending() {
    base::MutexGuard lock(&mutex_);
    return !worker_tasks_.empty();
  }

  bool ForegroundTasksPending() {
    base::MutexGuard lock(&mutex_);
    return !foreground_tasks_.empty();
  }

  void RunWorkerTasksAndBlock(Platform* platform) {
    std::vector<std::unique_ptr<Task>> tasks;
    {
      base::MutexGuard lock(&mutex_);
      tasks.swap(worker_tasks_);
    }
    platform->CallOnWorkerThread(
        base::make_unique<TaskWrapper>(this, std::move(tasks), true));
    sem_.Wait();
  }

  void RunWorkerTasks(Platform* platform) {
    std::vector<std::unique_ptr<Task>> tasks;
    {
      base::MutexGuard lock(&mutex_);
      tasks.swap(worker_tasks_);
    }
    platform->CallOnWorkerThread(
        base::make_unique<TaskWrapper>(this, std::move(tasks), false));
  }

  void RunForegroundTasks() {
    std::vector<std::unique_ptr<Task>> tasks;
    {
      base::MutexGuard lock(&mutex_);
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
      base::MutexGuard lock(&mutex_);
      tasks.swap(worker_tasks_);
    }
  }

  void ClearForegroundTasks() {
    std::vector<std::unique_ptr<Task>> tasks;
    {
      base::MutexGuard lock(&mutex_);
      tasks.swap(foreground_tasks_);
    }
  }

  void ClearIdleTask() {
    base::MutexGuard lock(&mutex_);
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
    ~TaskWrapper() override = default;

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

  class MockForegroundTaskRunner final : public TaskRunner {
   public:
    explicit MockForegroundTaskRunner(MockPlatform* platform)
        : platform_(platform) {}

    void PostTask(std::unique_ptr<v8::Task> task) override {
      base::MutexGuard lock(&platform_->mutex_);
      platform_->foreground_tasks_.push_back(std::move(task));
    }

    void PostDelayedTask(std::unique_ptr<Task> task,
                         double delay_in_seconds) override {
      UNREACHABLE();
    }

    void PostIdleTask(std::unique_ptr<IdleTask> task) override {
      DCHECK(IdleTasksEnabled());
      base::MutexGuard lock(&platform_->mutex_);
      ASSERT_TRUE(platform_->idle_task_ == nullptr);
      platform_->idle_task_ = task.release();
    }

    bool IdleTasksEnabled() override { return true; }

   private:
    MockPlatform* platform_;
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
  dispatcher.AbortAll();
}

TEST_F(CompilerDispatcherTest, IsEnqueued) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  Handle<SharedFunctionInfo> shared =
      test::CreateSharedFunctionInfo(i_isolate(), nullptr);
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_FALSE(dispatcher.IsEnqueued(shared));

  base::Optional<CompilerDispatcher::JobId> job_id =
      EnqueueUnoptimizedCompileJob(&dispatcher, i_isolate(), shared);

  ASSERT_TRUE(job_id);
  ASSERT_TRUE(dispatcher.IsEnqueued(*job_id));
  ASSERT_FALSE(dispatcher.IsEnqueued(shared));  // SFI not yet registered.

  dispatcher.RegisterSharedFunctionInfo(*job_id, *shared);
  ASSERT_TRUE(dispatcher.IsEnqueued(*job_id));
  ASSERT_TRUE(dispatcher.IsEnqueued(shared));

  dispatcher.AbortAll();
  ASSERT_FALSE(dispatcher.IsEnqueued(*job_id));
  ASSERT_FALSE(dispatcher.IsEnqueued(shared));

  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(platform.WorkerTasksPending());
  platform.ClearWorkerTasks();
}

TEST_F(CompilerDispatcherTest, FinishNow) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  Handle<SharedFunctionInfo> shared =
      test::CreateSharedFunctionInfo(i_isolate(), nullptr);
  ASSERT_FALSE(shared->is_compiled());

  base::Optional<CompilerDispatcher::JobId> job_id =
      EnqueueUnoptimizedCompileJob(&dispatcher, i_isolate(), shared);
  dispatcher.RegisterSharedFunctionInfo(*job_id, *shared);

  ASSERT_TRUE(dispatcher.FinishNow(shared));
  // Finishing removes the SFI from the queue.
  ASSERT_FALSE(dispatcher.IsEnqueued(*job_id));
  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_TRUE(shared->is_compiled());

  platform.ClearWorkerTasks();
  ASSERT_FALSE(platform.IdleTaskPending());
  dispatcher.AbortAll();
}

TEST_F(CompilerDispatcherTest, CompileAndFinalize) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  Handle<SharedFunctionInfo> shared =
      test::CreateSharedFunctionInfo(i_isolate(), nullptr);
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());

  base::Optional<CompilerDispatcher::JobId> job_id =
      EnqueueUnoptimizedCompileJob(&dispatcher, i_isolate(), shared);
  ASSERT_TRUE(platform.WorkerTasksPending());

  // Run compile steps.
  platform.RunWorkerTasksAndBlock(V8::GetCurrentPlatform());

  // Since we haven't yet registered the SFI for the job, it should still be
  // enqueued and waiting.
  ASSERT_TRUE(dispatcher.IsEnqueued(*job_id));
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());

  // Register SFI, which should schedule another idle task to finalize the
  // compilation.
  dispatcher.RegisterSharedFunctionInfo(*job_id, *shared);
  ASSERT_TRUE(platform.IdleTaskPending());
  platform.RunIdleTask(1000.0, 0.0);

  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_TRUE(shared->is_compiled());
  ASSERT_FALSE(platform.WorkerTasksPending());
  ASSERT_FALSE(platform.IdleTaskPending());
  dispatcher.AbortAll();
}

TEST_F(CompilerDispatcherTest, IdleTaskNoIdleTime) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  Handle<SharedFunctionInfo> shared =
      test::CreateSharedFunctionInfo(i_isolate(), nullptr);
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());

  base::Optional<CompilerDispatcher::JobId> job_id =
      EnqueueUnoptimizedCompileJob(&dispatcher, i_isolate(), shared);
  dispatcher.RegisterSharedFunctionInfo(*job_id, *shared);

  // Run compile steps.
  platform.RunWorkerTasksAndBlock(V8::GetCurrentPlatform());

  // Job should be ready to finalize.
  ASSERT_EQ(dispatcher.jobs_.size(), 1u);
  ASSERT_TRUE(dispatcher.jobs_.begin()->second->has_run);
  ASSERT_TRUE(platform.IdleTaskPending());

  // Grant no idle time and have time advance beyond it in one step.
  platform.RunIdleTask(0.0, 1.0);

  ASSERT_TRUE(dispatcher.IsEnqueued(shared));
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_TRUE(platform.IdleTaskPending());

  // Job should be ready to finalize.
  ASSERT_EQ(dispatcher.jobs_.size(), 1u);
  ASSERT_TRUE(dispatcher.jobs_.begin()->second->has_run);

  // Now grant a lot of idle time and freeze time.
  platform.RunIdleTask(1000.0, 0.0);

  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_TRUE(shared->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_FALSE(platform.WorkerTasksPending());
  dispatcher.AbortAll();
}

TEST_F(CompilerDispatcherTest, IdleTaskSmallIdleTime) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  Handle<SharedFunctionInfo> shared_1 =
      test::CreateSharedFunctionInfo(i_isolate(), nullptr);
  ASSERT_FALSE(shared_1->is_compiled());
  Handle<SharedFunctionInfo> shared_2 =
      test::CreateSharedFunctionInfo(i_isolate(), nullptr);
  ASSERT_FALSE(shared_2->is_compiled());

  base::Optional<CompilerDispatcher::JobId> job_id_1 =
      EnqueueUnoptimizedCompileJob(&dispatcher, i_isolate(), shared_1);
  base::Optional<CompilerDispatcher::JobId> job_id_2 =
      EnqueueUnoptimizedCompileJob(&dispatcher, i_isolate(), shared_2);

  dispatcher.RegisterSharedFunctionInfo(*job_id_1, *shared_1);
  dispatcher.RegisterSharedFunctionInfo(*job_id_2, *shared_2);

  // Run compile steps.
  platform.RunWorkerTasksAndBlock(V8::GetCurrentPlatform());

  // Both jobs should be ready to finalize.
  ASSERT_EQ(dispatcher.jobs_.size(), 2u);
  ASSERT_TRUE(dispatcher.jobs_.begin()->second->has_run);
  ASSERT_TRUE((++dispatcher.jobs_.begin())->second->has_run);
  ASSERT_TRUE(platform.IdleTaskPending());

  // Grant a small anount of idle time and have time advance beyond it in one
  // step.
  platform.RunIdleTask(2.0, 1.0);

  // Only one of the jobs should be finalized.
  ASSERT_EQ(dispatcher.jobs_.size(), 1u);
  ASSERT_TRUE(dispatcher.jobs_.begin()->second->has_run);
  ASSERT_NE(dispatcher.IsEnqueued(shared_1), dispatcher.IsEnqueued(shared_2));
  ASSERT_NE(shared_1->is_compiled(), shared_2->is_compiled());
  ASSERT_TRUE(platform.IdleTaskPending());

  // Now grant a lot of idle time and freeze time.
  platform.RunIdleTask(1000.0, 0.0);

  ASSERT_FALSE(dispatcher.IsEnqueued(shared_1) ||
               dispatcher.IsEnqueued(shared_2));
  ASSERT_TRUE(shared_1->is_compiled() && shared_2->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_FALSE(platform.WorkerTasksPending());
  dispatcher.AbortAll();
}

TEST_F(CompilerDispatcherTest, IdleTaskException) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, 50);

  std::string raw_script("(x) { var a = ");
  for (int i = 0; i < 1000; i++) {
    // Alternate + and - to avoid n-ary operation nodes.
    raw_script += "'x' + 'x' - ";
  }
  raw_script += " 'x'; };";
  test::ScriptResource* script =
      new test::ScriptResource(raw_script.c_str(), strlen(raw_script.c_str()));
  Handle<SharedFunctionInfo> shared =
      test::CreateSharedFunctionInfo(i_isolate(), script);
  ASSERT_FALSE(shared->is_compiled());

  base::Optional<CompilerDispatcher::JobId> job_id =
      EnqueueUnoptimizedCompileJob(&dispatcher, i_isolate(), shared);
  dispatcher.RegisterSharedFunctionInfo(*job_id, *shared);

  // Run compile steps and finalize.
  platform.RunWorkerTasksAndBlock(V8::GetCurrentPlatform());
  platform.RunIdleTask(1000.0, 0.0);

  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_FALSE(i_isolate()->has_pending_exception());
  platform.ClearWorkerTasks();
  dispatcher.AbortAll();
}

TEST_F(CompilerDispatcherTest, FinishNowWithWorkerTask) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  Handle<SharedFunctionInfo> shared =
      test::CreateSharedFunctionInfo(i_isolate(), nullptr);
  ASSERT_FALSE(shared->is_compiled());

  base::Optional<CompilerDispatcher::JobId> job_id =
      EnqueueUnoptimizedCompileJob(&dispatcher, i_isolate(), shared);
  dispatcher.RegisterSharedFunctionInfo(*job_id, *shared);

  ASSERT_EQ(dispatcher.jobs_.size(), 1u);
  ASSERT_FALSE(dispatcher.jobs_.begin()->second->has_run);

  ASSERT_TRUE(dispatcher.IsEnqueued(shared));
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_EQ(dispatcher.jobs_.size(), 1u);
  ASSERT_FALSE(dispatcher.jobs_.begin()->second->has_run);
  ASSERT_TRUE(platform.WorkerTasksPending());

  // This does not block, but races with the FinishNow() call below.
  platform.RunWorkerTasks(V8::GetCurrentPlatform());

  ASSERT_TRUE(dispatcher.FinishNow(shared));
  // Finishing removes the SFI from the queue.
  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_TRUE(shared->is_compiled());
  if (platform.IdleTaskPending()) platform.ClearIdleTask();
  ASSERT_FALSE(platform.WorkerTasksPending());
  dispatcher.AbortAll();
}

TEST_F(CompilerDispatcherTest, IdleTaskMultipleJobs) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  Handle<SharedFunctionInfo> shared_1 =
      test::CreateSharedFunctionInfo(i_isolate(), nullptr);
  ASSERT_FALSE(shared_1->is_compiled());
  Handle<SharedFunctionInfo> shared_2 =
      test::CreateSharedFunctionInfo(i_isolate(), nullptr);
  ASSERT_FALSE(shared_2->is_compiled());

  base::Optional<CompilerDispatcher::JobId> job_id_1 =
      EnqueueUnoptimizedCompileJob(&dispatcher, i_isolate(), shared_1);
  base::Optional<CompilerDispatcher::JobId> job_id_2 =
      EnqueueUnoptimizedCompileJob(&dispatcher, i_isolate(), shared_2);

  dispatcher.RegisterSharedFunctionInfo(*job_id_1, *shared_1);
  dispatcher.RegisterSharedFunctionInfo(*job_id_2, *shared_2);

  ASSERT_TRUE(dispatcher.IsEnqueued(shared_1));
  ASSERT_TRUE(dispatcher.IsEnqueued(shared_2));

  // Run compile steps and finalize.
  platform.RunWorkerTasksAndBlock(V8::GetCurrentPlatform());
  platform.RunIdleTask(1000.0, 0.0);

  ASSERT_FALSE(dispatcher.IsEnqueued(shared_1));
  ASSERT_FALSE(dispatcher.IsEnqueued(shared_2));
  ASSERT_TRUE(shared_1->is_compiled());
  ASSERT_TRUE(shared_2->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_FALSE(platform.WorkerTasksPending());
  dispatcher.AbortAll();
}

TEST_F(CompilerDispatcherTest, FinishNowException) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, 50);

  std::string raw_script("(x) { var a = ");
  for (int i = 0; i < 1000; i++) {
    // Alternate + and - to avoid n-ary operation nodes.
    raw_script += "'x' + 'x' - ";
  }
  raw_script += " 'x'; };";
  test::ScriptResource* script =
      new test::ScriptResource(raw_script.c_str(), strlen(raw_script.c_str()));
  Handle<SharedFunctionInfo> shared =
      test::CreateSharedFunctionInfo(i_isolate(), script);
  ASSERT_FALSE(shared->is_compiled());

  base::Optional<CompilerDispatcher::JobId> job_id =
      EnqueueUnoptimizedCompileJob(&dispatcher, i_isolate(), shared);
  dispatcher.RegisterSharedFunctionInfo(*job_id, *shared);

  ASSERT_FALSE(dispatcher.FinishNow(shared));

  ASSERT_FALSE(dispatcher.IsEnqueued(shared));
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_TRUE(i_isolate()->has_pending_exception());

  i_isolate()->clear_pending_exception();
  ASSERT_FALSE(platform.IdleTaskPending());
  platform.ClearWorkerTasks();
  dispatcher.AbortAll();
}

TEST_F(CompilerDispatcherTest, AbortJobNotStarted) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  Handle<SharedFunctionInfo> shared =
      test::CreateSharedFunctionInfo(i_isolate(), nullptr);
  ASSERT_FALSE(shared->is_compiled());

  base::Optional<CompilerDispatcher::JobId> job_id =
      EnqueueUnoptimizedCompileJob(&dispatcher, i_isolate(), shared);

  ASSERT_EQ(dispatcher.jobs_.size(), 1u);
  ASSERT_FALSE(dispatcher.jobs_.begin()->second->has_run);

  ASSERT_TRUE(dispatcher.IsEnqueued(*job_id));
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_EQ(dispatcher.jobs_.size(), 1u);
  ASSERT_FALSE(dispatcher.jobs_.begin()->second->has_run);
  ASSERT_TRUE(platform.WorkerTasksPending());

  dispatcher.AbortJob(*job_id);

  // Aborting removes the job from the queue.
  ASSERT_FALSE(dispatcher.IsEnqueued(*job_id));
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());
  platform.ClearWorkerTasks();
  dispatcher.AbortAll();
}

TEST_F(CompilerDispatcherTest, AbortJobAlreadyStarted) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  Handle<SharedFunctionInfo> shared =
      test::CreateSharedFunctionInfo(i_isolate(), nullptr);
  ASSERT_FALSE(shared->is_compiled());

  base::Optional<CompilerDispatcher::JobId> job_id =
      EnqueueUnoptimizedCompileJob(&dispatcher, i_isolate(), shared);

  ASSERT_EQ(dispatcher.jobs_.size(), 1u);
  ASSERT_FALSE(dispatcher.jobs_.begin()->second->has_run);

  ASSERT_TRUE(dispatcher.IsEnqueued(*job_id));
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_EQ(dispatcher.jobs_.size(), 1u);
  ASSERT_FALSE(dispatcher.jobs_.begin()->second->has_run);
  ASSERT_TRUE(platform.WorkerTasksPending());

  // Have dispatcher block on the background thread when running the job.
  {
    base::LockGuard<base::Mutex> lock(&dispatcher.mutex_);
    dispatcher.block_for_testing_.SetValue(true);
  }

  // Start background thread and wait until it is about to run the job.
  platform.RunWorkerTasks(V8::GetCurrentPlatform());
  while (dispatcher.block_for_testing_.Value()) {
  }

  // Now abort while dispatcher is in the middle of running the job.
  dispatcher.AbortJob(*job_id);

  // Unblock background thread, and wait for job to complete.
  {
    base::LockGuard<base::Mutex> lock(&dispatcher.mutex_);
    dispatcher.main_thread_blocking_on_job_ =
        dispatcher.jobs_.begin()->second.get();
    dispatcher.semaphore_for_testing_.Signal();
    while (dispatcher.main_thread_blocking_on_job_ != nullptr) {
      dispatcher.main_thread_blocking_signal_.Wait(&dispatcher.mutex_);
    }
  }

  // Job should have finished running and then been aborted.
  ASSERT_TRUE(dispatcher.IsEnqueued(*job_id));
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_EQ(dispatcher.jobs_.size(), 1u);
  ASSERT_TRUE(dispatcher.jobs_.begin()->second->has_run);
  ASSERT_TRUE(dispatcher.jobs_.begin()->second->aborted);
  ASSERT_FALSE(platform.WorkerTasksPending());
  ASSERT_TRUE(platform.IdleTaskPending());

  // Runt the pending idle task
  platform.RunIdleTask(1000.0, 0.0);

  // Aborting removes the SFI from the queue.
  ASSERT_FALSE(dispatcher.IsEnqueued(*job_id));
  ASSERT_FALSE(shared->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_FALSE(platform.WorkerTasksPending());
  dispatcher.AbortAll();
}

TEST_F(CompilerDispatcherTest, CompileLazyFinishesDispatcherJob) {
  // Use the real dispatcher so that CompileLazy checks the same one for
  // enqueued functions.
  CompilerDispatcher* dispatcher = i_isolate()->compiler_dispatcher();

  const char raw_script[] = "function lazy() { return 42; }; lazy;";
  test::ScriptResource* script =
      new test::ScriptResource(raw_script, strlen(raw_script));
  Handle<JSFunction> f = RunJS<JSFunction>(script);
  Handle<SharedFunctionInfo> shared(f->shared(), i_isolate());
  ASSERT_FALSE(shared->is_compiled());

  base::Optional<CompilerDispatcher::JobId> job_id =
      EnqueueUnoptimizedCompileJob(dispatcher, i_isolate(), shared);
  dispatcher->RegisterSharedFunctionInfo(*job_id, *shared);

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

  const char raw_source_2[] = "function lazy2() { return 42; }; lazy2;";
  test::ScriptResource* source_2 =
      new test::ScriptResource(raw_source_2, strlen(raw_source_2));
  Handle<JSFunction> lazy2 = RunJS<JSFunction>(source_2);
  Handle<SharedFunctionInfo> shared_2(lazy2->shared(), i_isolate());
  ASSERT_FALSE(shared_2->is_compiled());

  const char raw_source_1[] = "function lazy1() { return lazy2(); }; lazy1;";
  test::ScriptResource* source_1 =
      new test::ScriptResource(raw_source_1, strlen(raw_source_1));
  Handle<JSFunction> lazy1 = RunJS<JSFunction>(source_1);
  Handle<SharedFunctionInfo> shared_1(lazy1->shared(), i_isolate());
  ASSERT_FALSE(shared_1->is_compiled());

  base::Optional<CompilerDispatcher::JobId> job_id_1 =
      EnqueueUnoptimizedCompileJob(dispatcher, i_isolate(), shared_1);
  dispatcher->RegisterSharedFunctionInfo(*job_id_1, *shared_1);

  base::Optional<CompilerDispatcher::JobId> job_id_2 =
      EnqueueUnoptimizedCompileJob(dispatcher, i_isolate(), shared_2);
  dispatcher->RegisterSharedFunctionInfo(*job_id_2, *shared_2);

  ASSERT_TRUE(dispatcher->IsEnqueued(shared_1));
  ASSERT_TRUE(dispatcher->IsEnqueued(shared_2));

  RunJS("lazy1();");
  ASSERT_TRUE(shared_1->is_compiled());
  ASSERT_TRUE(shared_2->is_compiled());
  ASSERT_FALSE(dispatcher->IsEnqueued(shared_1));
  ASSERT_FALSE(dispatcher->IsEnqueued(shared_2));
}

TEST_F(CompilerDispatcherTest, CompileMultipleOnBackgroundThread) {
  MockPlatform platform;
  CompilerDispatcher dispatcher(i_isolate(), &platform, FLAG_stack_size);

  Handle<SharedFunctionInfo> shared_1 =
      test::CreateSharedFunctionInfo(i_isolate(), nullptr);
  ASSERT_FALSE(shared_1->is_compiled());

  Handle<SharedFunctionInfo> shared_2 =
      test::CreateSharedFunctionInfo(i_isolate(), nullptr);
  ASSERT_FALSE(shared_2->is_compiled());

  base::Optional<CompilerDispatcher::JobId> job_id_1 =
      EnqueueUnoptimizedCompileJob(&dispatcher, i_isolate(), shared_1);
  dispatcher.RegisterSharedFunctionInfo(*job_id_1, *shared_1);

  base::Optional<CompilerDispatcher::JobId> job_id_2 =
      EnqueueUnoptimizedCompileJob(&dispatcher, i_isolate(), shared_2);
  dispatcher.RegisterSharedFunctionInfo(*job_id_2, *shared_2);

  ASSERT_EQ(dispatcher.jobs_.size(), 2u);
  ASSERT_FALSE(dispatcher.jobs_.begin()->second->has_run);
  ASSERT_FALSE((++dispatcher.jobs_.begin())->second->has_run);

  ASSERT_TRUE(dispatcher.IsEnqueued(shared_1));
  ASSERT_TRUE(dispatcher.IsEnqueued(shared_2));
  ASSERT_FALSE(shared_1->is_compiled());
  ASSERT_FALSE(shared_2->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());
  ASSERT_TRUE(platform.WorkerTasksPending());

  platform.RunWorkerTasksAndBlock(V8::GetCurrentPlatform());

  ASSERT_TRUE(platform.IdleTaskPending());
  ASSERT_FALSE(platform.WorkerTasksPending());
  ASSERT_EQ(dispatcher.jobs_.size(), 2u);
  ASSERT_TRUE(dispatcher.jobs_.begin()->second->has_run);
  ASSERT_TRUE((++dispatcher.jobs_.begin())->second->has_run);

  // Now grant a lot of idle time and freeze time.
  platform.RunIdleTask(1000.0, 0.0);

  ASSERT_FALSE(dispatcher.IsEnqueued(shared_1));
  ASSERT_FALSE(dispatcher.IsEnqueued(shared_2));
  ASSERT_TRUE(shared_1->is_compiled());
  ASSERT_TRUE(shared_2->is_compiled());
  ASSERT_FALSE(platform.IdleTaskPending());
  dispatcher.AbortAll();
}

}  // namespace internal
}  // namespace v8
