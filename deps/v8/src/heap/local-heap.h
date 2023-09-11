// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_LOCAL_HEAP_H_
#define V8_HEAP_LOCAL_HEAP_H_

#include <atomic>
#include <memory>

#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/base/platform/condition-variable.h"
#include "src/base/platform/mutex.h"
#include "src/common/assert-scope.h"
#include "src/execution/isolate.h"
#include "src/handles/global-handles.h"
#include "src/handles/persistent-handles.h"
#include "src/heap/concurrent-allocator.h"
#include "src/heap/gc-callbacks.h"
namespace v8 {
namespace internal {

class Heap;
class LocalHandles;
class MarkingBarrier;
class MemoryChunk;
class Safepoint;

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
  using GCEpilogueCallback = void(LocalIsolate*, GCType, GCCallbackFlags,
                                  void*);

  explicit LocalHeap(
      Heap* heap, ThreadKind kind,
      std::unique_ptr<PersistentHandles> persistent_handles = nullptr);
  ~LocalHeap();

  // Frequently invoked by local thread to check whether safepoint was requested
  // from the main thread.
  void Safepoint() {
    DCHECK(AllowSafepoints::IsAllowed());
    ThreadState current = state_.load_relaxed();

    if (V8_UNLIKELY(current.IsRunningWithSlowPathFlag())) {
      SafepointSlowPath();
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
  bool HasPersistentHandles() { return !!persistent_handles_; }
  bool ContainsPersistentHandle(Address* location);
  bool ContainsLocalHandle(Address* location);
  bool IsHandleDereferenceAllowed();
#endif

  bool IsParked();
  bool IsRunning();

  Heap* heap() { return heap_; }
  Heap* AsHeap() { return heap(); }

  MarkingBarrier* marking_barrier() { return marking_barrier_.get(); }
  ConcurrentAllocator* old_space_allocator() {
    return old_space_allocator_.get();
  }
  ConcurrentAllocator* code_space_allocator() {
    return code_space_allocator_.get();
  }
  ConcurrentAllocator* shared_old_space_allocator() {
    return shared_old_space_allocator_.get();
  }

  void RegisterCodeObject(Handle<Code> code) {
    heap()->RegisterCodeObject(code);
  }

  // Mark/Unmark linear allocation areas black. Used for black allocation.
  void MarkLinearAllocationAreaBlack();
  void UnmarkLinearAllocationArea();

  // Mark/Unmark linear allocation areas in shared heap black. Used for black
  // allocation.
  void MarkSharedLinearAllocationAreaBlack();
  void UnmarkSharedLinearAllocationArea();

  // Give up linear allocation areas. Used for mark-compact GC.
  void FreeLinearAllocationArea();

  // Free all shared LABs. Used by the shared mark-compact GC.
  void FreeSharedLinearAllocationArea();

  // Create filler object in linear allocation areas. Verifying requires
  // iterable heap.
  void MakeLinearAllocationAreaIterable();

  // Makes the shared LAB iterable.
  void MakeSharedLinearAllocationAreaIterable();

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
      AllocationAlignment alignment = kTaggedAligned);

  // Allocates an uninitialized object and crashes when object
  // cannot be allocated.
  V8_WARN_UNUSED_RESULT inline Address AllocateRawOrFail(
      int size_in_bytes, AllocationType allocation,
      AllocationOrigin origin = AllocationOrigin::kRuntime,
      AllocationAlignment alignment = kTaggedAligned);

  void NotifyObjectSizeChange(
      HeapObject object, int old_size, int new_size,
      ClearRecordedSlots clear_recorded_slots,
      UpdateInvalidatedObjectSize update_invalidated_object_size =
          UpdateInvalidatedObjectSize::kYes);

  bool is_main_thread() const { return is_main_thread_; }
  bool deserialization_complete() const {
    return heap_->deserialization_complete();
  }
  ReadOnlySpace* read_only_space() { return heap_->read_only_space(); }

  // Adds a callback that is invoked with the given |data| after each GC.
  // The callback is invoked on the main thread before any background thread
  // resumes. The callback must not allocate or make any other calls that
  // can trigger GC.
  void AddGCEpilogueCallback(GCEpilogueCallback* callback, void* data,
                             GCType gc_type = static_cast<v8::GCType>(
                                 GCType::kGCTypeMarkSweepCompact |
                                 GCType::kGCTypeScavenge |
                                 GCType::kGCTypeMinorMarkCompact));
  void RemoveGCEpilogueCallback(GCEpilogueCallback* callback, void* data);

  // Weakens StrongDescriptorArray objects into regular DescriptorArray objects.
  void WeakenDescriptorArrays(
      GlobalHandleVector<DescriptorArray> strong_descriptor_arrays);

  // Used to make SetupMainThread() available to unit tests.
  void SetUpMainThreadForTesting();

 private:
  using ParkedBit = base::BitField8<bool, 0, 1>;
  using SafepointRequestedBit = ParkedBit::Next<bool, 1>;
  using CollectionRequestedBit = SafepointRequestedBit::Next<bool, 1>;

  class ThreadState final {
   public:
    static constexpr ThreadState Parked() {
      return ThreadState(ParkedBit::kMask);
    }
    static constexpr ThreadState Running() { return ThreadState(0); }

    constexpr bool IsRunning() const { return !ParkedBit::decode(raw_state_); }

    constexpr ThreadState SetRunning() const V8_WARN_UNUSED_RESULT {
      return ThreadState(raw_state_ & ~ParkedBit::kMask);
    }

    constexpr bool IsParked() const { return ParkedBit::decode(raw_state_); }

    constexpr ThreadState SetParked() const V8_WARN_UNUSED_RESULT {
      return ThreadState(ParkedBit::kMask | raw_state_);
    }

    constexpr bool IsSafepointRequested() const {
      return SafepointRequestedBit::decode(raw_state_);
    }

    constexpr bool IsCollectionRequested() const {
      return CollectionRequestedBit::decode(raw_state_);
    }

    constexpr bool IsRunningWithSlowPathFlag() const {
      return IsRunning() && (raw_state_ & (SafepointRequestedBit::kMask |
                                           CollectionRequestedBit::kMask));
    }

   private:
    constexpr explicit ThreadState(uint8_t value) : raw_state_(value) {}

    constexpr uint8_t raw() const { return raw_state_; }

    uint8_t raw_state_;

    friend class LocalHeap;
  };

  class AtomicThreadState final {
   public:
    constexpr explicit AtomicThreadState(ThreadState state)
        : raw_state_(state.raw()) {}

    bool CompareExchangeStrong(ThreadState& expected, ThreadState updated) {
      return raw_state_.compare_exchange_strong(expected.raw_state_,
                                                updated.raw());
    }

    bool CompareExchangeWeak(ThreadState& expected, ThreadState updated) {
      return raw_state_.compare_exchange_weak(expected.raw_state_,
                                              updated.raw());
    }

    ThreadState SetParked() {
      return ThreadState(raw_state_.fetch_or(ParkedBit::kMask));
    }

    ThreadState SetSafepointRequested() {
      return ThreadState(raw_state_.fetch_or(SafepointRequestedBit::kMask));
    }

    ThreadState ClearSafepointRequested() {
      return ThreadState(raw_state_.fetch_and(~SafepointRequestedBit::kMask));
    }

    ThreadState SetCollectionRequested() {
      return ThreadState(raw_state_.fetch_or(CollectionRequestedBit::kMask));
    }

    ThreadState ClearCollectionRequested() {
      return ThreadState(raw_state_.fetch_and(~CollectionRequestedBit::kMask));
    }

    ThreadState load_relaxed() const {
      return ThreadState(raw_state_.load(std::memory_order_relaxed));
    }

   private:
    std::atomic<uint8_t> raw_state_;
  };

  // Slow path of allocation that performs GC and then retries allocation in
  // loop.
  Address PerformCollectionAndAllocateAgain(int object_size,
                                            AllocationType type,
                                            AllocationOrigin origin,
                                            AllocationAlignment alignment);

  void Park() {
    DCHECK(AllowSafepoints::IsAllowed());
    ThreadState expected = ThreadState::Running();
    if (!state_.CompareExchangeWeak(expected, ThreadState::Parked())) {
      ParkSlowPath();
    }
  }

  void Unpark() {
    DCHECK(AllowSafepoints::IsAllowed());
    ThreadState expected = ThreadState::Parked();
    if (!state_.CompareExchangeWeak(expected, ThreadState::Running())) {
      UnparkSlowPath();
    }
  }

  void ParkSlowPath();
  void UnparkSlowPath();
  void EnsureParkedBeforeDestruction();
  void SafepointSlowPath();
  void SleepInSafepoint();
  void SleepInUnpark();

  void EnsurePersistentHandles();

  void InvokeGCEpilogueCallbacksInSafepoint(GCType gc_type,
                                            GCCallbackFlags flags);

  void SetUpMainThread();
  void SetUp();
  void SetUpSharedMarking();

  Heap* heap_;
  bool is_main_thread_;

  AtomicThreadState state_;

  bool allocation_failed_;
  bool main_thread_parked_;

  LocalHeap* prev_;
  LocalHeap* next_;

  std::unordered_set<MemoryChunk*> unprotected_memory_chunks_;
  uintptr_t code_page_collection_memory_modification_scope_depth_{0};

  std::unique_ptr<LocalHandles> handles_;
  std::unique_ptr<PersistentHandles> persistent_handles_;
  std::unique_ptr<MarkingBarrier> marking_barrier_;

  GCCallbacks<LocalIsolate, DisallowGarbageCollection> gc_epilogue_callbacks_;

  std::unique_ptr<ConcurrentAllocator> old_space_allocator_;
  std::unique_ptr<ConcurrentAllocator> code_space_allocator_;
  std::unique_ptr<ConcurrentAllocator> shared_old_space_allocator_;

  MarkingBarrier* saved_marking_barrier_ = nullptr;

  friend class CollectionBarrier;
  friend class ConcurrentAllocator;
  friend class GlobalSafepoint;
  friend class Heap;
  friend class Isolate;
  friend class IsolateSafepoint;
  friend class IsolateSafepointScope;
  friend class ParkedScope;
  friend class UnparkedScope;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_LOCAL_HEAP_H_
