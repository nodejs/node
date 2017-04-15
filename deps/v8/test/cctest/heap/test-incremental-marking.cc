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
#include "test/cctest/heap/heap-utils.h"

using v8::IdleTask;
using v8::Task;
using v8::Isolate;

namespace v8 {
namespace internal {

class MockPlatform : public v8::Platform {
 public:
  explicit MockPlatform(v8::Platform* platform)
      : platform_(platform), task_(nullptr) {}
  virtual ~MockPlatform() { delete task_; }

  void CallOnBackgroundThread(Task* task,
                              ExpectedRuntime expected_runtime) override {
    platform_->CallOnBackgroundThread(task, expected_runtime);
  }

  void CallOnForegroundThread(v8::Isolate* isolate, Task* task) override {
    task_ = task;
  }

  void CallDelayedOnForegroundThread(v8::Isolate* isolate, Task* task,
                                     double delay_in_seconds) override {
    platform_->CallDelayedOnForegroundThread(isolate, task, delay_in_seconds);
  }

  double MonotonicallyIncreasingTime() override {
    return platform_->MonotonicallyIncreasingTime();
  }

  void CallIdleOnForegroundThread(v8::Isolate* isolate,
                                  IdleTask* task) override {
    platform_->CallIdleOnForegroundThread(isolate, task);
  }

  bool IdleTasksEnabled(v8::Isolate* isolate) override { return false; }

  bool PendingTask() { return task_ != nullptr; }

  void PerformTask() {
    Task* task = task_;
    task_ = nullptr;
    task->Run();
    delete task;
  }

  using Platform::AddTraceEvent;
  uint64_t AddTraceEvent(char phase, const uint8_t* categoryEnabledFlag,
                         const char* name, const char* scope, uint64_t id,
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
  Task* task_;
};

TEST(IncrementalMarkingUsingTasks) {
  if (!i::FLAG_incremental_marking) return;
  CcTest::InitializeVM();
  v8::Platform* old_platform = i::V8::GetCurrentPlatform();
  MockPlatform platform(old_platform);
  i::V8::SetPlatformForTesting(&platform);
  i::heap::SimulateFullSpace(CcTest::heap()->old_space());
  i::IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  marking->Stop();
  marking->Start(i::GarbageCollectionReason::kTesting);
  CHECK(platform.PendingTask());
  while (platform.PendingTask()) {
    platform.PerformTask();
  }
  CHECK(marking->IsStopped());
  i::V8::SetPlatformForTesting(old_platform);
}

}  // namespace internal
}  // namespace v8
