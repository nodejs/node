// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <unordered_map>

#include "include/v8-platform.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/time.h"
#include "src/base/template-utils.h"
#include "src/base/utils/random-number-generator.h"
#include "src/d8-platforms.h"

namespace v8 {

class PredictablePlatform : public Platform {
 public:
  explicit PredictablePlatform(std::unique_ptr<Platform> platform)
      : platform_(std::move(platform)) {
    DCHECK_NOT_NULL(platform_);
  }

  PageAllocator* GetPageAllocator() override {
    return platform_->GetPageAllocator();
  }

  void OnCriticalMemoryPressure() override {
    platform_->OnCriticalMemoryPressure();
  }

  bool OnCriticalMemoryPressure(size_t length) override {
    return platform_->OnCriticalMemoryPressure(length);
  }

  std::shared_ptr<TaskRunner> GetForegroundTaskRunner(
      v8::Isolate* isolate) override {
    return platform_->GetForegroundTaskRunner(isolate);
  }

  int NumberOfWorkerThreads() override { return 0; }

  void CallOnWorkerThread(std::unique_ptr<Task> task) override {
    // It's not defined when background tasks are being executed, so we can just
    // execute them right away.
    task->Run();
  }

  void CallDelayedOnWorkerThread(std::unique_ptr<Task> task,
                                 double delay_in_seconds) override {
    // Never run delayed tasks.
  }

  void CallOnForegroundThread(v8::Isolate* isolate, Task* task) override {
    // This is a deprecated function and should not be called anymore.
    UNREACHABLE();
  }

  void CallDelayedOnForegroundThread(v8::Isolate* isolate, Task* task,
                                     double delay_in_seconds) override {
    // This is a deprecated function and should not be called anymore.
    UNREACHABLE();
  }

  void CallIdleOnForegroundThread(Isolate* isolate, IdleTask* task) override {
    UNREACHABLE();
  }

  bool IdleTasksEnabled(Isolate* isolate) override { return false; }

  double MonotonicallyIncreasingTime() override {
    return synthetic_time_in_sec_ += 0.00001;
  }

  double CurrentClockTimeMillis() override {
    return MonotonicallyIncreasingTime() * base::Time::kMillisecondsPerSecond;
  }

  v8::TracingController* GetTracingController() override {
    return platform_->GetTracingController();
  }

  Platform* platform() const { return platform_.get(); }

 private:
  double synthetic_time_in_sec_ = 0.0;
  std::unique_ptr<Platform> platform_;

  DISALLOW_COPY_AND_ASSIGN(PredictablePlatform);
};

std::unique_ptr<Platform> MakePredictablePlatform(
    std::unique_ptr<Platform> platform) {
  return base::make_unique<PredictablePlatform>(std::move(platform));
}

class DelayedTasksPlatform : public Platform {
 public:
  explicit DelayedTasksPlatform(std::unique_ptr<Platform> platform)
      : platform_(std::move(platform)) {
    DCHECK_NOT_NULL(platform_);
  }

  explicit DelayedTasksPlatform(std::unique_ptr<Platform> platform,
                                int64_t random_seed)
      : platform_(std::move(platform)), rng_(random_seed) {
    DCHECK_NOT_NULL(platform_);
  }

  ~DelayedTasksPlatform() {
    // When the platform shuts down, all task runners must be freed.
    DCHECK_EQ(0, delayed_task_runners_.size());
  }

  PageAllocator* GetPageAllocator() override {
    return platform_->GetPageAllocator();
  }

  void OnCriticalMemoryPressure() override {
    platform_->OnCriticalMemoryPressure();
  }

  bool OnCriticalMemoryPressure(size_t length) override {
    return platform_->OnCriticalMemoryPressure(length);
  }

  std::shared_ptr<TaskRunner> GetForegroundTaskRunner(
      v8::Isolate* isolate) override {
    std::shared_ptr<TaskRunner> runner =
        platform_->GetForegroundTaskRunner(isolate);

    base::MutexGuard lock_guard(&mutex_);
    // Check if we can re-materialize the weak ptr in our map.
    std::weak_ptr<DelayedTaskRunner>& weak_delayed_runner =
        delayed_task_runners_[runner.get()];
    std::shared_ptr<DelayedTaskRunner> delayed_runner =
        weak_delayed_runner.lock();

    if (!delayed_runner) {
      // Create a new {DelayedTaskRunner} and keep a weak reference in our map.
      delayed_runner.reset(new DelayedTaskRunner(runner, this),
                           DelayedTaskRunnerDeleter{});
      weak_delayed_runner = delayed_runner;
    }

    return std::move(delayed_runner);
  }

  int NumberOfWorkerThreads() override {
    return platform_->NumberOfWorkerThreads();
  }

  void CallOnWorkerThread(std::unique_ptr<Task> task) override {
    platform_->CallOnWorkerThread(MakeDelayedTask(std::move(task)));
  }

  void CallDelayedOnWorkerThread(std::unique_ptr<Task> task,
                                 double delay_in_seconds) override {
    platform_->CallDelayedOnWorkerThread(MakeDelayedTask(std::move(task)),
                                         delay_in_seconds);
  }

  void CallOnForegroundThread(v8::Isolate* isolate, Task* task) override {
    // This is a deprecated function and should not be called anymore.
    UNREACHABLE();
  }

  void CallDelayedOnForegroundThread(v8::Isolate* isolate, Task* task,
                                     double delay_in_seconds) override {
    // This is a deprecated function and should not be called anymore.
    UNREACHABLE();
  }

  void CallIdleOnForegroundThread(Isolate* isolate, IdleTask* task) override {
    // This is a deprecated function and should not be called anymore.
    UNREACHABLE();
  }

  bool IdleTasksEnabled(Isolate* isolate) override {
    return platform_->IdleTasksEnabled(isolate);
  }

  double MonotonicallyIncreasingTime() override {
    return platform_->MonotonicallyIncreasingTime();
  }

  double CurrentClockTimeMillis() override {
    return platform_->CurrentClockTimeMillis();
  }

  v8::TracingController* GetTracingController() override {
    return platform_->GetTracingController();
  }

 private:
  class DelayedTaskRunnerDeleter;
  class DelayedTaskRunner final : public TaskRunner {
   public:
    DelayedTaskRunner(std::shared_ptr<TaskRunner> task_runner,
                      DelayedTasksPlatform* platform)
        : task_runner_(task_runner), platform_(platform) {}

    void PostTask(std::unique_ptr<Task> task) final {
      task_runner_->PostTask(platform_->MakeDelayedTask(std::move(task)));
    }

    void PostDelayedTask(std::unique_ptr<Task> task,
                         double delay_in_seconds) final {
      task_runner_->PostDelayedTask(platform_->MakeDelayedTask(std::move(task)),
                                    delay_in_seconds);
    }

    void PostIdleTask(std::unique_ptr<IdleTask> task) final {
      task_runner_->PostIdleTask(
          platform_->MakeDelayedIdleTask(std::move(task)));
    }

    bool IdleTasksEnabled() final { return task_runner_->IdleTasksEnabled(); }

   private:
    friend class DelayedTaskRunnerDeleter;
    std::shared_ptr<TaskRunner> task_runner_;
    DelayedTasksPlatform* platform_;
  };

  class DelayedTaskRunnerDeleter {
   public:
    void operator()(DelayedTaskRunner* runner) const {
      TaskRunner* original_runner = runner->task_runner_.get();
      base::MutexGuard lock_guard(&runner->platform_->mutex_);
      auto& delayed_task_runners = runner->platform_->delayed_task_runners_;
      DCHECK_EQ(1, delayed_task_runners.count(original_runner));
      delayed_task_runners.erase(original_runner);
    }
  };

  class DelayedTask : public Task {
   public:
    DelayedTask(std::unique_ptr<Task> task, int32_t delay_ms)
        : task_(std::move(task)), delay_ms_(delay_ms) {}
    void Run() final {
      base::OS::Sleep(base::TimeDelta::FromMicroseconds(delay_ms_));
      task_->Run();
    }

   private:
    std::unique_ptr<Task> task_;
    int32_t delay_ms_;
  };

  class DelayedIdleTask : public IdleTask {
   public:
    DelayedIdleTask(std::unique_ptr<IdleTask> task, int32_t delay_ms)
        : task_(std::move(task)), delay_ms_(delay_ms) {}
    void Run(double deadline_in_seconds) final {
      base::OS::Sleep(base::TimeDelta::FromMicroseconds(delay_ms_));
      task_->Run(deadline_in_seconds);
    }

   private:
    std::unique_ptr<IdleTask> task_;
    int32_t delay_ms_;
  };

  std::unique_ptr<Platform> platform_;

  // The Mutex protects the RNG, which is used by foreground and background
  // threads, and the {delayed_task_runners_} map might be accessed concurrently
  // by the shared_ptr destructor.
  base::Mutex mutex_;
  base::RandomNumberGenerator rng_;
  std::unordered_map<TaskRunner*, std::weak_ptr<DelayedTaskRunner>>
      delayed_task_runners_;

  int32_t GetRandomDelayInMilliseconds() {
    base::MutexGuard lock_guard(&mutex_);
    double delay_fraction = rng_.NextDouble();
    // Sleep up to 100ms (100000us). Square {delay_fraction} to shift
    // distribution towards shorter sleeps.
    return 1e5 * (delay_fraction * delay_fraction);
  }

  std::unique_ptr<Task> MakeDelayedTask(std::unique_ptr<Task> task) {
    return base::make_unique<DelayedTask>(std::move(task),
                                          GetRandomDelayInMilliseconds());
  }

  std::unique_ptr<IdleTask> MakeDelayedIdleTask(
      std::unique_ptr<IdleTask> task) {
    return base::make_unique<DelayedIdleTask>(std::move(task),
                                              GetRandomDelayInMilliseconds());
  }

  DISALLOW_COPY_AND_ASSIGN(DelayedTasksPlatform);
};

std::unique_ptr<Platform> MakeDelayedTasksPlatform(
    std::unique_ptr<Platform> platform, int64_t random_seed) {
  if (random_seed) {
    return base::make_unique<DelayedTasksPlatform>(std::move(platform),
                                                   random_seed);
  }
  return base::make_unique<DelayedTasksPlatform>(std::move(platform));
}

}  // namespace v8
