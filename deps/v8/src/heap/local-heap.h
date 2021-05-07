// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_LOCAL_HEAP_H_
#define V8_HEAP_LOCAL_HEAP_H_

#include <atomic>
#include <memory>

#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/common/assert-scope.h"
#include "src/execution/isolate.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/concurrent-allocator.h"

namespace v8 {
namespace internal {

class Heap;
class Safepoint;
class LocalHandles;

// LocalHeap is used by the GC to track all threads with heap access in order to
// stop them before performing a collection. LocalHeaps can be either Parked or
// Running and are in Parked mode when initialized.
//   Running: Thread is allowed to access the heap but needs to give the GC the
//            chance to run regularly by manually invoking Safepoint(). The
//            thread can be parked using ParkedScope.
//   Parked:  Heap access is not allowed, so the GC will not stop this thread
//            for a collection. Useful when threads do not need heap access for
//            some time or for blocking operations like locking a mutex.
class V8_EXPORT_PRIVATE LocalHeap {
 public:
  using GCEpilogueCallback = void(void* data);

  explicit LocalHeap(
      Heap* heap, ThreadKind kind,
      std::unique_ptr<PersistentHandles> persistent_handles = nullptr);
  ~LocalHeap();

  // Invoked by main thread to signal this thread that it needs to halt in a
  // safepoint.
  void RequestSafepoint();

  // Frequently invoked by local thread to check whether safepoint was requested
  // from the main thread.
  void Safepoint() {
    DCHECK(AllowSafepoints::IsAllowed());

    if (IsSafepointRequested()) {
      ClearSafepointRequested();
      EnterSafepoint();
    }
  }

  LocalHandles* handles() { return handles_.get(); }

  template <typename T>
  Handle<T> NewPersistentHandle(T object) {
    if (!persistent_handles_) {
      EnsurePersistentHandles();
    }
    return persistent_handles_->NewHandle(object);
  }

  template <typename T>
  Handle<T> NewPersistentHandle(Handle<T> object) {
    return NewPersistentHandle(*object);
  }

  template <typename T>
  MaybeHandle<T> NewPersistentMaybeHandle(MaybeHandle<T> maybe_handle) {
    Handle<T> handle;
    if (maybe_handle.ToHandle(&handle)) {
      return NewPersistentHandle(handle);
    }
    return kNullMaybeHandle;
  }

  void AttachPersistentHandles(
      std::unique_ptr<PersistentHandles> persistent_handles);
  std::unique_ptr<PersistentHandles> DetachPersistentHandles();
#ifdef DEBUG
  bool ContainsPersistentHandle(Address* location);
  bool ContainsLocalHandle(Address* location);
  bool IsHandleDereferenceAllowed();
#endif

  bool IsParked();

  Heap* heap() { return heap_; }

  MarkingBarrier* marking_barrier() { return marking_barrier_.get(); }
  ConcurrentAllocator* old_space_allocator() { return &old_space_allocator_; }

  // Mark/Unmark linear allocation areas black. Used for black allocation.
  void MarkLinearAllocationAreaBlack();
  void UnmarkLinearAllocationArea();

  // Give up linear allocation areas. Used for mark-compact GC.
  void FreeLinearAllocationArea();

  // Create filler object in linear allocation areas. Verifying requires
  // iterable heap.
  void MakeLinearAllocationAreaIterable();

  // Fetches a pointer to the local heap from the thread local storage.
  // It is intended to be used in handle and write barrier code where it is
  // difficult to get a pointer to the current instance of local heap otherwise.
  // The result may be a nullptr if there is no local heap instance associated
  // with the current thread.
  static LocalHeap* Current();

#ifdef DEBUG
  void VerifyCurrent();
#endif

  // Allocate an uninitialized object.
  V8_WARN_UNUSED_RESULT inline AllocationResult AllocateRaw(
      int size_in_bytes, AllocationType allocation,
      AllocationOrigin origin = AllocationOrigin::kRuntime,
      AllocationAlignment alignment = kWordAligned);

  // Allocates an uninitialized object and crashes when object
  // cannot be allocated.
  V8_WARN_UNUSED_RESULT inline Address AllocateRawOrFail(
      int size_in_bytes, AllocationType allocation,
      AllocationOrigin origin = AllocationOrigin::kRuntime,
      AllocationAlignment alignment = kWordAligned);

  bool is_main_thread() const { return is_main_thread_; }

  // Requests GC and blocks until the collection finishes.
  void PerformCollection();

  // Adds a callback that is invoked with the given |data| after each GC.
  // The callback is invoked on the main thread before any background thread
  // resumes. The callback must not allocate or make any other calls that
  // can trigger GC.
  void AddGCEpilogueCallback(GCEpilogueCallback* callback, void* data);
  void RemoveGCEpilogueCallback(GCEpilogueCallback* callback, void* data);

 private:
  enum class ThreadState {
    // Threads in this state need to be stopped in a safepoint.
    Running,
    // Thread was parked, which means that the thread is not allowed to access
    // or manipulate the heap in any way.
    Parked,
    // Thread was stopped in a safepoint.
    Safepoint
  };

  // Slow path of allocation that performs GC and then retries allocation in
  // loop.
  Address PerformCollectionAndAllocateAgain(int object_size,
                                            AllocationType type,
                                            AllocationOrigin origin,
                                            AllocationAlignment alignment);

  void Park();
  void Unpark();
  void EnsureParkedBeforeDestruction();

  void EnsurePersistentHandles();

  V8_INLINE bool IsSafepointRequested() {
    return safepoint_requested_.load(std::memory_order_relaxed);
  }
  void ClearSafepointRequested();

  void EnterSafepoint();

  void InvokeGCEpilogueCallbacksInSafepoint();

  Heap* heap_;
  bool is_main_thread_;

  base::Mutex state_mutex_;
  base::ConditionVariable state_change_;
  ThreadState state_;

  std::atomic<bool> safepoint_requested_;

  bool allocation_failed_;

  LocalHeap* prev_;
  LocalHeap* next_;

  std::unique_ptr<LocalHandles> handles_;
  std::unique_ptr<PersistentHandles> persistent_handles_;
  std::unique_ptr<MarkingBarrier> marking_barrier_;

  std::vector<std::pair<GCEpilogueCallback*, void*>> gc_epilogue_callbacks_;

  ConcurrentAllocator old_space_allocator_;

  friend class Heap;
  friend class GlobalSafepoint;
  friend class ParkedScope;
  friend class UnparkedScope;
  friend class ConcurrentAllocator;
  friend class Isolate;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_LOCAL_HEAP_H_
