// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "src/heap/heap.h"
#include "src/heap/memory-allocator.h"
#include "src/init/v8.h"
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
  ~MockPlatformForUnmapper() override {
    for (auto& task : worker_tasks_) {
      CcTest::default_platform()->CallOnWorkerThread(std::move(task));
    }
    worker_tasks_.clear();
  }

  void CallOnWorkerThread(std::unique_ptr<Task> task) override {
    worker_tasks_.push_back(std::move(task));
  }

  bool IdleTasksEnabled(v8::Isolate* isolate) override { return false; }

 private:
  std::vector<std::unique_ptr<Task>> worker_tasks_;
};

UNINITIALIZED_TEST(EagerUnmappingInCollectAllAvailableGarbage) {
  v8_flags.stress_concurrent_allocation = false;  // For SimulateFullSpace.
  MockPlatformForUnmapper platform;
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);

  {
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = CcTest::NewContext(isolate);
    v8::Context::Scope context_scope(context);
    Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
    Heap* heap = i_isolate->heap();
    i::heap::SimulateFullSpace(heap->old_space());
    CcTest::CollectAllAvailableGarbage(i_isolate);
    CHECK_EQ(0, heap->memory_allocator()->unmapper()->NumberOfChunks());
  }
  isolate->Dispose();
}

}  // namespace heap
}  // namespace internal
}  // namespace v8
