// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_COLLECTION_BARRIER_H_
#define V8_HEAP_COLLECTION_BARRIER_H_

#include <atomic>

#include "src/base/optional.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/base/platform/mutex.h"
#include "src/logging/counters.h"

namespace v8 {
namespace internal {

class Heap;

// This class stops and resumes all background threads waiting for GC.
class CollectionBarrier {
  Heap* heap_;
  base::Mutex mutex_;
  base::ConditionVariable cond_;
  base::ElapsedTimer timer_;

  enum class RequestState {
    // Default state, no collection requested and tear down wasn't initated
    // yet.
    kDefault,

    // Collection was already requested
    kCollectionRequested,

    // Collection was already started
    kCollectionStarted,

    // This state is reached after isolate starts to shut down. The main
    // thread can't perform any GCs anymore, so all allocations need to be
    // allowed from here on until background thread finishes.
    kShutdown,
  };

  // The current state.
  std::atomic<RequestState> state_;

  // Request GC by activating stack guards and posting a task to perform the
  // GC.
  void ActivateStackGuardAndPostTask();

  // Returns true when state was successfully updated from kDefault to
  // kCollection.
  bool FirstCollectionRequest() {
    RequestState expected = RequestState::kDefault;
    return state_.compare_exchange_strong(expected,
                                          RequestState::kCollectionRequested);
  }

  // Sets state back to kDefault - invoked at end of GC.
  void ClearCollectionRequested() {
    RequestState old_state =
        state_.exchange(RequestState::kDefault, std::memory_order_relaxed);
    USE(old_state);
    DCHECK_EQ(old_state, RequestState::kCollectionStarted);
  }

 public:
  explicit CollectionBarrier(Heap* heap)
      : heap_(heap), state_(RequestState::kDefault) {}

  // Checks whether any background thread requested GC.
  bool CollectionRequested() {
    return state_.load(std::memory_order_relaxed) ==
           RequestState::kCollectionRequested;
  }

  void StopTimeToCollectionTimer();
  void BlockUntilCollected();

  // Resumes threads waiting for collection.
  void ResumeThreadsAwaitingCollection();

  // Sets current state to kShutdown.
  void ShutdownRequested();

  // This is the method use by background threads to request and wait for GC.
  void AwaitCollectionBackground();
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_COLLECTION_BARRIER_H_
