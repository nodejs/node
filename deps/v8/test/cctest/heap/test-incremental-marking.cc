// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#ifdef __linux__
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <utility>

#include "src/v8.h"

#include "src/full-codegen/full-codegen.h"
#include "src/global-handles.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/utils-inl.h"


using v8::IdleTask;
using v8::Task;
using v8::Isolate;

namespace v8 {
namespace internal {

class MockPlatform : public v8::Platform {
 public:
  explicit MockPlatform(v8::Platform* platform)
      : platform_(platform), idle_task_(nullptr), delayed_task_(nullptr) {}
  virtual ~MockPlatform() {
    delete idle_task_;
    delete delayed_task_;
  }

  void CallOnBackgroundThread(Task* task,
                              ExpectedRuntime expected_runtime) override {
    platform_->CallOnBackgroundThread(task, expected_runtime);
  }

  void CallOnForegroundThread(v8::Isolate* isolate, Task* task) override {
    platform_->CallOnForegroundThread(isolate, task);
  }

  void CallDelayedOnForegroundThread(v8::Isolate* isolate, Task* task,
                                     double delay_in_seconds) override {
    if (delayed_task_ != nullptr) {
      delete delayed_task_;
    }
    delayed_task_ = task;
  }

  double MonotonicallyIncreasingTime() override {
    return platform_->MonotonicallyIncreasingTime();
  }

  void CallIdleOnForegroundThread(v8::Isolate* isolate,
                                  IdleTask* task) override {
    CHECK(nullptr == idle_task_);
    idle_task_ = task;
  }

  bool IdleTasksEnabled(v8::Isolate* isolate) override { return true; }

  bool PendingIdleTask() { return idle_task_ != nullptr; }

  void PerformIdleTask(double idle_time_in_seconds) {
    IdleTask* task = idle_task_;
    idle_task_ = nullptr;
    task->Run(MonotonicallyIncreasingTime() + idle_time_in_seconds);
    delete task;
  }

  bool PendingDelayedTask() { return delayed_task_ != nullptr; }

  void PerformDelayedTask() {
    Task* task = delayed_task_;
    delayed_task_ = nullptr;
    task->Run();
    delete task;
  }

  uint64_t AddTraceEvent(char phase, const uint8_t* categoryEnabledFlag,
                         const char* name, uint64_t id,
                         uint64_t bind_id, int numArgs, const char** argNames,
                         const uint8_t* argTypes, const uint64_t* argValues,
                         unsigned int flags) override {
    return 0;
  }

  void UpdateTraceEventDuration(const uint8_t* categoryEnabledFlag,
                                const char* name, uint64_t handle) override {}

  const uint8_t* GetCategoryGroupEnabled(const char* name) override {
    static uint8_t no = 0;
    return &no;
  }

  const char* GetCategoryGroupName(
      const uint8_t* categoryEnabledFlag) override {
    static const char* dummy = "dummy";
    return dummy;
  }

 private:
  v8::Platform* platform_;
  IdleTask* idle_task_;
  Task* delayed_task_;
};


TEST(IncrementalMarkingUsingIdleTasks) {
  if (!i::FLAG_incremental_marking) return;
  CcTest::InitializeVM();
  v8::Platform* old_platform = i::V8::GetCurrentPlatform();
  MockPlatform platform(old_platform);
  i::V8::SetPlatformForTesting(&platform);
  SimulateFullSpace(CcTest::heap()->old_space());
  i::IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  marking->Stop();
  marking->Start();
  CHECK(platform.PendingIdleTask());
  const double kLongIdleTimeInSeconds = 1;
  const double kShortIdleTimeInSeconds = 0.010;
  const int kShortStepCount = 10;
  for (int i = 0; i < kShortStepCount && platform.PendingIdleTask(); i++) {
    platform.PerformIdleTask(kShortIdleTimeInSeconds);
  }
  while (platform.PendingIdleTask()) {
    platform.PerformIdleTask(kLongIdleTimeInSeconds);
  }
  CHECK(marking->IsStopped());
  i::V8::SetPlatformForTesting(old_platform);
}


TEST(IncrementalMarkingUsingIdleTasksAfterGC) {
  if (!i::FLAG_incremental_marking) return;
  CcTest::InitializeVM();
  v8::Platform* old_platform = i::V8::GetCurrentPlatform();
  MockPlatform platform(old_platform);
  i::V8::SetPlatformForTesting(&platform);
  SimulateFullSpace(CcTest::heap()->old_space());
  CcTest::heap()->CollectAllGarbage();
  i::IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  marking->Stop();
  marking->Start();
  CHECK(platform.PendingIdleTask());
  const double kLongIdleTimeInSeconds = 1;
  const double kShortIdleTimeInSeconds = 0.010;
  const int kShortStepCount = 10;
  for (int i = 0; i < kShortStepCount && platform.PendingIdleTask(); i++) {
    platform.PerformIdleTask(kShortIdleTimeInSeconds);
  }
  while (platform.PendingIdleTask()) {
    platform.PerformIdleTask(kLongIdleTimeInSeconds);
  }
  CHECK(marking->IsStopped());
  i::V8::SetPlatformForTesting(old_platform);
}


TEST(IncrementalMarkingUsingDelayedTasks) {
  if (!i::FLAG_incremental_marking) return;
  CcTest::InitializeVM();
  v8::Platform* old_platform = i::V8::GetCurrentPlatform();
  MockPlatform platform(old_platform);
  i::V8::SetPlatformForTesting(&platform);
  SimulateFullSpace(CcTest::heap()->old_space());
  i::IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  marking->Stop();
  marking->Start();
  CHECK(platform.PendingIdleTask());
  // The delayed task should be a no-op if the idle task makes progress.
  const int kIgnoredDelayedTaskStepCount = 1000;
  for (int i = 0; i < kIgnoredDelayedTaskStepCount; i++) {
    // Dummy idle task progress.
    marking->incremental_marking_job()->NotifyIdleTaskProgress();
    CHECK(platform.PendingDelayedTask());
    platform.PerformDelayedTask();
  }
  // Once we stop notifying idle task progress, the delayed tasks
  // should finish marking.
  while (!marking->IsStopped() && platform.PendingDelayedTask()) {
    platform.PerformDelayedTask();
  }
  // There could be pending delayed task from memory reducer after GC finishes.
  CHECK(marking->IsStopped());
  i::V8::SetPlatformForTesting(old_platform);
}

}  // namespace internal
}  // namespace v8
