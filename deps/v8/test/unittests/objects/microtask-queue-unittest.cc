// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/microtask-queue-inl.h"

#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {

void NoopCallback(void*) {}

class MicrotaskQueueTest : public TestWithIsolate {
 public:
  Handle<Microtask> NewMicrotask() {
    MicrotaskCallback callback = &NoopCallback;
    void* data = nullptr;
    return factory()->NewCallbackTask(
        factory()->NewForeign(reinterpret_cast<Address>(callback)),
        factory()->NewForeign(reinterpret_cast<Address>(data)));
  }
};

TEST_F(MicrotaskQueueTest, EnqueueMicrotask) {
  Handle<MicrotaskQueue> microtask_queue = factory()->NewMicrotaskQueue();
  Handle<Microtask> microtask = NewMicrotask();

  EXPECT_EQ(0, microtask_queue->pending_microtask_count());
  MicrotaskQueue::EnqueueMicrotask(isolate(), microtask_queue, microtask);
  EXPECT_EQ(1, microtask_queue->pending_microtask_count());
  ASSERT_LE(1, microtask_queue->queue()->length());
  EXPECT_EQ(*microtask, microtask_queue->queue()->get(0));

  std::vector<Handle<Microtask>> microtasks;
  microtasks.push_back(microtask);

  // Queue microtasks until the reallocation happens.
  int queue_capacity = microtask_queue->queue()->length();
  for (int i = 0; i < queue_capacity; ++i) {
    microtask = NewMicrotask();
    MicrotaskQueue::EnqueueMicrotask(isolate(), microtask_queue, microtask);
    microtasks.push_back(microtask);
  }

  int num_tasks = static_cast<int>(microtasks.size());
  EXPECT_EQ(num_tasks, microtask_queue->pending_microtask_count());
  ASSERT_LE(num_tasks, microtask_queue->queue()->length());
  for (int i = 0; i < num_tasks; ++i) {
    EXPECT_EQ(*microtasks[i], microtask_queue->queue()->get(i));
  }
}

}  // namespace internal
}  // namespace v8
