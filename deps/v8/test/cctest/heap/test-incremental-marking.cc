// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/heap/safepoint.h"

#ifdef __linux__
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <utility>

#include "src/init/v8.h"

#include "src/handles/global-handles.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/spaces.h"
#include "src/objects/objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

using v8::IdleTask;
using v8::Task;
using v8::Isolate;

namespace v8 {
namespace internal {
namespace heap {

class MockPlatform : public TestPlatform {
 public:
  MockPlatform()
      : taskrunner_(new MockTaskRunner()),
        old_platform_(i::V8::GetCurrentPlatform()) {
    // Now that it's completely constructed, make this the current platform.
    i::V8::SetPlatformForTesting(this);
  }
  ~MockPlatform() override {
    i::V8::SetPlatformForTesting(old_platform_);
    for (auto& task : worker_tasks_) {
      old_platform_->CallOnWorkerThread(std::move(task));
    }
    worker_tasks_.clear();
  }

  std::shared_ptr<v8::TaskRunner> GetForegroundTaskRunner(
      v8::Isolate* isolate) override {
    return taskrunner_;
  }

  void CallOnWorkerThread(std::unique_ptr<Task> task) override {
    worker_tasks_.push_back(std::move(task));
  }

  bool IdleTasksEnabled(v8::Isolate* isolate) override { return false; }

  bool PendingTask() { return taskrunner_->PendingTask(); }

  void PerformTask() { taskrunner_->PerformTask(); }

 private:
  class MockTaskRunner : public v8::TaskRunner {
   public:
    void PostTask(std::unique_ptr<v8::Task> task) override {
      task_ = std::move(task);
    }

    void PostDelayedTask(std::unique_ptr<Task> task,
                         double delay_in_seconds) override {
      task_ = std::move(task);
    }

    void PostIdleTask(std::unique_ptr<IdleTask> task) override {
      UNREACHABLE();
    }

    bool IdleTasksEnabled() override { return false; }

    bool PendingTask() { return task_ != nullptr; }

    void PerformTask() {
      std::unique_ptr<Task> task = std::move(task_);
      task->Run();
    }

   private:
    std::unique_ptr<Task> task_;
  };

  std::shared_ptr<MockTaskRunner> taskrunner_;
  std::vector<std::unique_ptr<Task>> worker_tasks_;
  v8::Platform* old_platform_;
};

TEST(IncrementalMarkingUsingTasks) {
  if (!i::FLAG_incremental_marking) return;
  FLAG_stress_concurrent_allocation = false;  // For SimulateFullSpace.
  FLAG_stress_incremental_marking = false;
  CcTest::InitializeVM();
  MockPlatform platform;
  i::heap::SimulateFullSpace(CcTest::heap()->old_space());
  i::IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  marking->Stop();
  {
    SafepointScope scope(CcTest::heap());
    marking->Start(i::GarbageCollectionReason::kTesting);
  }
  CHECK(platform.PendingTask());
  while (platform.PendingTask()) {
    platform.PerformTask();
  }
  CHECK(marking->IsStopped());
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
