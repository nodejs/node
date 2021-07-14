// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_ARRAY_BUFFER_SWEEPER_H_
#define V8_HEAP_ARRAY_BUFFER_SWEEPER_H_

#include "src/base/platform/mutex.h"
#include "src/objects/js-array-buffer.h"
#include "src/tasks/cancelable-task.h"

namespace v8 {
namespace internal {

class ArrayBufferExtension;
class Heap;

// Singly linked-list of ArrayBufferExtensions that stores head and tail of the
// list to allow for concatenation of lists.
struct ArrayBufferList {
  ArrayBufferList() : head_(nullptr), tail_(nullptr), bytes_(0) {}

  ArrayBufferExtension* head_;
  ArrayBufferExtension* tail_;
  size_t bytes_;

  bool IsEmpty() {
    DCHECK_IMPLIES(head_, tail_);
    return head_ == nullptr;
  }

  size_t Bytes() { return bytes_; }
  size_t BytesSlow();

  void Reset() {
    head_ = tail_ = nullptr;
    bytes_ = 0;
  }

  void Append(ArrayBufferExtension* extension);
  void Append(ArrayBufferList* list);

  V8_EXPORT_PRIVATE bool Contains(ArrayBufferExtension* extension);
};

// The ArrayBufferSweeper iterates and deletes ArrayBufferExtensions
// concurrently to the application.
class ArrayBufferSweeper {
 public:
  explicit ArrayBufferSweeper(Heap* heap)
      : heap_(heap),
        sweeping_in_progress_(false),
        freed_bytes_(0),
        young_bytes_(0),
        old_bytes_(0) {}
  ~ArrayBufferSweeper() { ReleaseAll(); }

  void EnsureFinished();
  void RequestSweepYoung();
  void RequestSweepFull();

  // Track the given ArrayBufferExtension for the given JSArrayBuffer.
  void Append(JSArrayBuffer object, ArrayBufferExtension* extension);

  // Detaches an ArrayBufferExtension from a JSArrayBuffer.
  void Detach(JSArrayBuffer object, ArrayBufferExtension* extension);

  ArrayBufferList young() { return young_; }
  ArrayBufferList old() { return old_; }

  size_t YoungBytes();
  size_t OldBytes();

 private:
  enum class SweepingScope { kYoung, kFull };

  enum class SweepingState { kInProgress, kDone };

  struct SweepingJob {
    ArrayBufferSweeper* sweeper_;
    CancelableTaskManager::Id id_;
    std::atomic<SweepingState> state_;
    ArrayBufferList young_;
    ArrayBufferList old_;
    SweepingScope scope_;

    SweepingJob(ArrayBufferSweeper* sweeper, ArrayBufferList young,
                ArrayBufferList old, SweepingScope scope)
        : sweeper_(sweeper),
          id_(0),
          state_(SweepingState::kInProgress),
          young_(young),
          old_(old),
          scope_(scope) {}

    void Sweep();
    void SweepYoung();
    void SweepFull();
    ArrayBufferList SweepListFull(ArrayBufferList* list);
  };

  base::Optional<SweepingJob> job_;

  void Merge();
  void MergeBackExtensionsWhenSwept();

  void UpdateCountersForConcurrentlySweptExtensions();
  void IncrementExternalMemoryCounters(size_t bytes);
  void DecrementExternalMemoryCounters(size_t bytes);
  void IncrementFreedBytes(size_t bytes);

  void RequestSweep(SweepingScope sweeping_task);
  void Prepare(SweepingScope sweeping_task);

  ArrayBufferList SweepYoungGen();
  void SweepOldGen(ArrayBufferExtension* extension);

  void ReleaseAll();
  void ReleaseAll(ArrayBufferList* extension);

  Heap* const heap_;
  bool sweeping_in_progress_;
  base::Mutex sweeping_mutex_;
  base::ConditionVariable job_finished_;
  std::atomic<size_t> freed_bytes_;

  ArrayBufferList young_;
  ArrayBufferList old_;

  size_t young_bytes_;
  size_t old_bytes_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_ARRAY_BUFFER_SWEEPER_H_
