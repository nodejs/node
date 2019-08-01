// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/init/v8.h"

#include "src/heap/spaces.h"
#include "test/cctest/cctest.h"
#include "test/cctest/heap/heap-utils.h"

using v8::IdleTask;
using v8::Task;
using v8::Isolate;

namespace v8 {
namespace internal {
namespace heap {

class MockPlatformForUnmapper : public TestPlatform {
 public:
  MockPlatformForUnmapper()
      : task_(nullptr), old_platform_(i::V8::GetCurrentPlatform()) {
    // Now that it's completely constructed, make this the current platform.
    i::V8::SetPlatformForTesting(this);
  }
  ~MockPlatformForUnmapper() override {
    delete task_;
    i::V8::SetPlatformForTesting(old_platform_);
    for (auto& task : worker_tasks_) {
      old_platform_->CallOnWorkerThread(std::move(task));
    }
    worker_tasks_.clear();
  }

  void CallOnForegroundThread(v8::Isolate* isolate, Task* task) override {
    task_ = task;
  }

  void CallOnWorkerThread(std::unique_ptr<Task> task) override {
    worker_tasks_.push_back(std::move(task));
  }

  bool IdleTasksEnabled(v8::Isolate* isolate) override { return false; }

  int NumberOfWorkerThreads() override {
    return old_platform_->NumberOfWorkerThreads();
  }

 private:
  Task* task_;
  std::vector<std::unique_ptr<Task>> worker_tasks_;
  v8::Platform* old_platform_;
};

TEST(EagerUnmappingInCollectAllAvailableGarbage) {
  CcTest::InitializeVM();
  MockPlatformForUnmapper platform;
  Heap* heap = CcTest::heap();
  i::heap::SimulateFullSpace(heap->old_space());
  CcTest::CollectAllAvailableGarbage();
  CHECK_EQ(0, heap->memory_allocator()->unmapper()->NumberOfChunks());
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
