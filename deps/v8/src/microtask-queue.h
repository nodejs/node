// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MICROTASK_QUEUE_H_
#define V8_MICROTASK_QUEUE_H_

#include <stdint.h>
#include <memory>
#include <vector>

#include "include/v8-internal.h"  // For Address.
#include "include/v8.h"
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

  // Uses raw Address values because it's called via ExternalReference.
  // {raw_microtask} is a tagged Microtask pointer.
  // Returns a tagged Object pointer.
  static Address CallEnqueueMicrotask(Isolate* isolate,
                                      intptr_t microtask_queue_pointer,
                                      Address raw_microtask);

  void EnqueueMicrotask(Microtask microtask);

  // Returns -1 if the execution is terminating, otherwise, returns 0.
  // TODO(tzik): Update the implementation to return the number of processed
  // microtasks.
  int RunMicrotasks(Isolate* isolate);

  // Iterate all pending Microtasks in this queue as strong roots, so that
  // builtins can update the queue directly without the write barrier.
  void IterateMicrotasks(RootVisitor* visitor);

  // Microtasks scope depth represents nested scopes controlling microtasks
  // invocation, which happens when depth reaches zero.
  void IncrementMicrotasksScopeDepth() { ++microtasks_depth_; }
  void DecrementMicrotasksScopeDepth() { --microtasks_depth_; }
  int GetMicrotasksScopeDepth() const { return microtasks_depth_; }

  // Possibly nested microtasks suppression scopes prevent microtasks
  // from running.
  void IncrementMicrotasksSuppressions() { ++microtasks_suppressions_; }
  void DecrementMicrotasksSuppressions() { --microtasks_suppressions_; }
  bool HasMicrotasksSuppressions() const {
    return microtasks_suppressions_ != 0;
  }

#ifdef DEBUG
  // In debug we check that calls not intended to invoke microtasks are
  // still correctly wrapped with microtask scopes.
  void IncrementDebugMicrotasksScopeDepth() { ++debug_microtasks_depth_; }
  void DecrementDebugMicrotasksScopeDepth() { --debug_microtasks_depth_; }
  bool DebugMicrotasksScopeDepthIsZero() const {
    return debug_microtasks_depth_ == 0;
  }
#endif

  void AddMicrotasksCompletedCallback(MicrotasksCompletedCallback callback);
  void RemoveMicrotasksCompletedCallback(MicrotasksCompletedCallback callback);
  void FireMicrotasksCompletedCallback(Isolate* isolate) const;
  bool IsRunningMicrotasks() const { return is_running_microtasks_; }

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
  void OnCompleted(Isolate* isolate);

  MicrotaskQueue();
  void ResizeBuffer(intptr_t new_capacity);

  // A ring buffer to hold Microtask instances.
  // ring_buffer_[(start_ + i) % capacity_] contains |i|th Microtask for each
  // |i| in [0, size_).
  intptr_t size_ = 0;
  intptr_t capacity_ = 0;
  intptr_t start_ = 0;
  Address* ring_buffer_ = nullptr;

  // MicrotaskQueue instances form a doubly linked list loop, so that all
  // instances are reachable through |next_|.
  MicrotaskQueue* next_ = nullptr;
  MicrotaskQueue* prev_ = nullptr;

  int microtasks_depth_ = 0;
  int microtasks_suppressions_ = 0;
#ifdef DEBUG
  int debug_microtasks_depth_ = 0;
#endif

  bool is_running_microtasks_ = false;
  std::vector<MicrotasksCompletedCallback> microtasks_completed_callbacks_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_MICROTASK_QUEUE_H_
