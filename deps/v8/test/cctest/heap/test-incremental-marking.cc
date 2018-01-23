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

#include "src/global-handles.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/spaces.h"
#include "src/objects-inl.h"
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
  MockPlatform() : task_(nullptr), old_platform_(i::V8::GetCurrentPlatform()) {
    // Now that it's completely constructed, make this the current platform.
    i::V8::SetPlatformForTesting(this);
  }
  virtual ~MockPlatform() {
    delete task_;
    i::V8::SetPlatformForTesting(old_platform_);
  }

  void CallOnForegroundThread(v8::Isolate* isolate, Task* task) override {
    task_ = task;
  }

  bool IdleTasksEnabled(v8::Isolate* isolate) override { return false; }

  bool PendingTask() { return task_ != nullptr; }

  void PerformTask() {
    Task* task = task_;
    task_ = nullptr;
    task->Run();
    delete task;
  }

 private:
  Task* task_;
  v8::Platform* old_platform_;
};

TEST(IncrementalMarkingUsingTasks) {
  if (!i::FLAG_incremental_marking) return;
  FLAG_stress_incremental_marking = false;
  CcTest::InitializeVM();
  MockPlatform platform;
  i::heap::SimulateFullSpace(CcTest::heap()->old_space());
  i::IncrementalMarking* marking = CcTest::heap()->incremental_marking();
  marking->Stop();
  marking->Start(i::GarbageCollectionReason::kTesting);
  CHECK(platform.PendingTask());
  while (platform.PendingTask()) {
    platform.PerformTask();
  }
  CHECK(marking->IsStopped());
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
