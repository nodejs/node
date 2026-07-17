// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_COLLECTION_BARRIER_H_
#define V8_HEAP_COLLECTION_BARRIER_H_

#include <atomic>

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
  class V8_EXPORT_PRIVATE CollectionRequest {
   public:
    // Sets `kind` flag and returns true if a flag of equal or higher priority
    // was previously set.
    bool Set(RequestedGCKind kind);
    // Clears `kind` and any lower priority flag and returns true if `kind` was
    // previously set.
    bool Clear(RequestedGCKind kind);
    // Clear all flags.
    void ClearAll();
    // Returns whether `kind` flag was set.
    bool Has(RequestedGCKind kind) const;
    // Returns the highest priority flag if any, or std::nullopt.
    std::optional<RequestedGCKind> Get() const;
    // Returns true if any flag is set.
    bool HasAny() const;

   private:
    std::atomic<uint8_t> state_{0};
  };

  CollectionBarrier(
      Heap* heap, std::shared_ptr<v8::TaskRunner> foreground_task_runner);

  // Returns the requested collection kind if any.
  std::optional<RequestedGCKind> RequestedGC();

  // Requests a GC from the main thread. Returns whether GC was successfully
  // requested. Requesting a GC can fail when isolate shutdown was already
  // initiated.
  bool TryRequestGC(RequestedGCKind kind);

  // Resumes all threads waiting for GC when tear down starts.
  void NotifyShutdownRequested();

  // Stops the TimeToCollection timer when starting the GC.
  void StopTimeToCollectionTimer(RequestedGCKind kind);

  // Resumes threads waiting for collection of `kind`.
  void ResumeThreadsAwaitingCollection(RequestedGCKind kind);

  // Cancels collection if one was requested and resumes threads waiting for GC.
  void CancelCollectionAndResumeThreads();

  // This is the method use by background threads to request and wait for GC of
  // `kind`. Returns whether a GC was performed.
  bool AwaitCollectionBackground(LocalHeap* local_heap, RequestedGCKind kind);

 private:
  Heap* heap_;
  base::Mutex mutex_;
  base::ConditionVariable cv_wakeup_;
  base::ElapsedTimer major_timer_;
  base::ElapsedTimer last_resort_timer_;

  base::ElapsedTimer& GetTimerForCollectionRequest(RequestedGCKind kind);

  // Flag that main thread checks whether a GC was requested from the background
  // thread.
  CollectionRequest collection_request_;

  // Set to true when a GC was performed, false in case it was canceled because
  // the main thread parked itself without running the GC.
  bool collection_performed_ = false;

  // Will be set as soon as Isolate starts tear down.
  bool shutdown_requested_ = false;

  // Used to post tasks on the main thread.
  std::shared_ptr<v8::TaskRunner> foreground_task_runner_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_COLLECTION_BARRIER_H_
