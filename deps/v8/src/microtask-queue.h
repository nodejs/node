// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MICROTASK_QUEUE_H_
#define V8_MICROTASK_QUEUE_H_

#include <stdint.h>
#include <memory>

#include "src/base/macros.h"

namespace v8 {
namespace internal {

class Isolate;
class Microtask;
class Object;
class RootVisitor;

class V8_EXPORT_PRIVATE MicrotaskQueue {
 public:
  static void SetUpDefaultMicrotaskQueue(Isolate* isolate);
  static std::unique_ptr<MicrotaskQueue> New(Isolate* isolate);

  ~MicrotaskQueue();

  static Object* CallEnqueueMicrotask(Isolate* isolate,
                                      intptr_t microtask_queue_pointer,
                                      Microtask* microtask);

  void EnqueueMicrotask(Microtask* microtask);

  // Returns -1 if the execution is terminating, otherwise, returns 0.
  // TODO(tzik): Update the implementation to return the number of processed
  // microtasks.
  int RunMicrotasks(Isolate* isolate);

  // Iterate all pending Microtasks in this queue as strong roots, so that
  // builtins can update the queue directly without the write barrier.
  void IterateMicrotasks(RootVisitor* visitor);

  intptr_t capacity() const { return capacity_; }
  intptr_t size() const { return size_; }
  intptr_t start() const { return start_; }

  MicrotaskQueue* next() const { return next_; }
  MicrotaskQueue* prev() const { return prev_; }

  static const size_t kRingBufferOffset;
  static const size_t kCapacityOffset;
  static const size_t kSizeOffset;
  static const size_t kStartOffset;

  static const intptr_t kMinimumCapacity;

 private:
  MicrotaskQueue();
  void ResizeBuffer(intptr_t new_capacity);

  // MicrotaskQueue instances form a doubly linked list loop, so that all
  // instances are reachable through |next_|.
  MicrotaskQueue* next_ = nullptr;
  MicrotaskQueue* prev_ = nullptr;

  // A ring buffer to hold Microtask instances.
  // ring_buffer_[(start_ + i) % capacity_] contains |i|th Microtask for each
  // |i| in [0, size_).
  Object** ring_buffer_ = nullptr;
  intptr_t capacity_ = 0;
  intptr_t size_ = 0;
  intptr_t start_ = 0;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_MICROTASK_QUEUE_H_
