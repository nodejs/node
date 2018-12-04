// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/microtask-queue.h"

#include "src/objects/microtask-queue-inl.h"

namespace v8 {
namespace internal {

// DCHECK requires this for taking the reference of it.
constexpr int MicrotaskQueue::kMinimumQueueCapacity;

// static
void MicrotaskQueue::EnqueueMicrotask(Isolate* isolate,
                                      Handle<MicrotaskQueue> microtask_queue,
                                      Handle<Microtask> microtask) {
  Handle<FixedArray> queue(microtask_queue->queue(), isolate);
  int num_tasks = microtask_queue->pending_microtask_count();
  DCHECK_LE(num_tasks, queue->length());
  if (num_tasks == queue->length()) {
    queue = isolate->factory()->CopyFixedArrayAndGrow(
        queue, std::max(num_tasks, kMinimumQueueCapacity));
    microtask_queue->set_queue(*queue);
  }
  DCHECK_LE(kMinimumQueueCapacity, queue->length());
  DCHECK_LT(num_tasks, queue->length());
  DCHECK(queue->get(num_tasks)->IsUndefined(isolate));
  queue->set(num_tasks, *microtask);
  microtask_queue->set_pending_microtask_count(num_tasks + 1);
}

// static
void MicrotaskQueue::RunMicrotasks(Handle<MicrotaskQueue> microtask_queue) {
  UNIMPLEMENTED();
}

}  // namespace internal
}  // namespace v8
