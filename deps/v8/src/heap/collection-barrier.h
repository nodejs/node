// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_COLLECTION_BARRIER_H_
#define V8_HEAP_COLLECTION_BARRIER_H_

#include <atomic>

#include "src/base/optional.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/base/platform/mutex.h"
#include "src/heap/local-heap.h"
#include "src/logging/counters.h"

namespace v8 {
namespace internal {

class Heap;

// This class stops and resumes all background threads waiting for GC.
class CollectionBarrier {
 public:
  explicit CollectionBarrier(Heap* heap) : heap_(heap) {}

  // Returns true when collection was requested.
  bool WasGCRequested();

  // Requests a GC from the main thread.
  void RequestGC();

  // Resumes all threads waiting for GC when tear down starts.
  void NotifyShutdownRequested();

  // Stops the TimeToCollection timer when starting the GC.
  void StopTimeToCollectionTimer();

  // Resumes threads waiting for collection.
  void ResumeThreadsAwaitingCollection();

  // This is the method use by background threads to request and wait for GC.
  bool AwaitCollectionBackground(LocalHeap* local_heap);

 private:
  // Activate stack guards and posting a task to perform the GC.
  void ActivateStackGuardAndPostTask();

  Heap* heap_;
  base::Mutex mutex_;
  base::ConditionVariable cv_wakeup_;
  base::ElapsedTimer timer_;
  std::atomic<bool> collection_requested_{false};
  bool block_for_collection_ = false;
  bool shutdown_requested_ = false;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_COLLECTION_BARRIER_H_
