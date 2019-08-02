// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_MEMORY_H_
#define V8_WASM_WASM_MEMORY_H_

#include <atomic>
#include <unordered_map>
#include <unordered_set>

#include "src/base/platform/mutex.h"
#include "src/flags/flags.h"
#include "src/handles/handles.h"
#include "src/objects/js-array-buffer.h"

namespace v8 {
namespace internal {
namespace wasm {

// The {WasmMemoryTracker} tracks reservations and allocations for wasm memory
// and wasm code. There is an upper limit on the total reserved memory which is
// checked by this class. Allocations are stored so we can look them up when an
// array buffer dies and figure out the reservation and allocation bounds for
// that buffer.
class WasmMemoryTracker {
 public:
  WasmMemoryTracker() = default;
  V8_EXPORT_PRIVATE ~WasmMemoryTracker();

  // ReserveAddressSpace attempts to increase the reserved address space counter
  // by {num_bytes}. Returns true if successful (meaning it is okay to go ahead
  // and reserve {num_bytes} bytes), false otherwise.
  bool ReserveAddressSpace(size_t num_bytes);

  void RegisterAllocation(Isolate* isolate, void* allocation_base,
                          size_t allocation_length, void* buffer_start,
                          size_t buffer_length);

  struct SharedMemoryObjectState {
    Handle<WasmMemoryObject> memory_object;
    Isolate* isolate;

    SharedMemoryObjectState() = default;
    SharedMemoryObjectState(Handle<WasmMemoryObject> memory_object,
                            Isolate* isolate)
        : memory_object(memory_object), isolate(isolate) {}
  };

  struct AllocationData {
    void* allocation_base = nullptr;
    size_t allocation_length = 0;
    void* buffer_start = nullptr;
    size_t buffer_length = 0;
    bool is_shared = false;
    // Wasm memories are growable by default, this will be false only when
    // shared with an asmjs module.
    bool is_growable = true;

    // Track Wasm Memory instances across isolates, this is populated on
    // PostMessage using persistent handles for memory objects.
    std::vector<WasmMemoryTracker::SharedMemoryObjectState>
        memory_object_vector;

   private:
    AllocationData() = default;
    AllocationData(void* allocation_base, size_t allocation_length,
                   void* buffer_start, size_t buffer_length)
        : allocation_base(allocation_base),
          allocation_length(allocation_length),
          buffer_start(buffer_start),
          buffer_length(buffer_length) {
      DCHECK_LE(reinterpret_cast<uintptr_t>(allocation_base),
                reinterpret_cast<uintptr_t>(buffer_start));
      DCHECK_GE(
          reinterpret_cast<uintptr_t>(allocation_base) + allocation_length,
          reinterpret_cast<uintptr_t>(buffer_start));
      DCHECK_GE(
          reinterpret_cast<uintptr_t>(allocation_base) + allocation_length,
          reinterpret_cast<uintptr_t>(buffer_start) + buffer_length);
    }

    friend WasmMemoryTracker;
  };

  // Allow tests to allocate a backing store the same way as we do it for
  // WebAssembly memory. This is used in unit tests for trap handler to
  // generate the same signals/exceptions for invalid memory accesses as
  // we would get with WebAssembly memory.
  V8_EXPORT_PRIVATE void* TryAllocateBackingStoreForTesting(
      Heap* heap, size_t size, void** allocation_base,
      size_t* allocation_length);

  // Free memory allocated with TryAllocateBackingStoreForTesting.
  V8_EXPORT_PRIVATE void FreeBackingStoreForTesting(base::AddressRegion memory,
                                                    void* buffer_start);

  // Decreases the amount of reserved address space.
  void ReleaseReservation(size_t num_bytes);

  V8_EXPORT_PRIVATE bool IsWasmMemory(const void* buffer_start);

  bool IsWasmSharedMemory(const void* buffer_start);

  // Returns a pointer to a Wasm buffer's allocation data, or nullptr if the
  // buffer is not tracked.
  V8_EXPORT_PRIVATE const AllocationData* FindAllocationData(
      const void* buffer_start);

  // Free Memory allocated by the Wasm memory tracker
  bool FreeWasmMemory(Isolate* isolate, const void* buffer_start);

  void MarkWasmMemoryNotGrowable(Handle<JSArrayBuffer> buffer);

  bool IsWasmMemoryGrowable(Handle<JSArrayBuffer> buffer);

  // When WebAssembly.Memory is transferred over PostMessage, register the
  // allocation as shared and track the memory objects that will need
  // updating if memory is resized.
  void RegisterWasmMemoryAsShared(Handle<WasmMemoryObject> object,
                                  Isolate* isolate);

  // This method is called when the underlying backing store is grown, but
  // instances that share the backing_store have not yet been updated.
  void SetPendingUpdateOnGrow(Handle<JSArrayBuffer> old_buffer,
                              size_t new_size);

  // Interrupt handler for GROW_SHARED_MEMORY interrupt. Update memory objects
  // and instances that share the memory objects  after a Grow call.
  void UpdateSharedMemoryInstances(Isolate* isolate);

  // Due to timing of when buffers are garbage collected, vs. when isolate
  // object handles are destroyed, it is possible to leak global handles. To
  // avoid this, cleanup any global handles on isolate destruction if any exist.
  void DeleteSharedMemoryObjectsOnIsolate(Isolate* isolate);

  // Allocation results are reported to UMA
  //
  // See wasm_memory_allocation_result in counters.h
  enum class AllocationStatus {
    kSuccess,  // Succeeded on the first try

    kSuccessAfterRetry,  // Succeeded after garbage collection

    kAddressSpaceLimitReachedFailure,  // Failed because Wasm is at its address
                                       // space limit

    kOtherFailure  // Failed for an unknown reason
  };

 private:
  // Helper methods to free memory only if not shared by other isolates, memory
  // objects.
  void FreeMemoryIfNotShared_Locked(Isolate* isolate,
                                    const void* backing_store);
  bool CanFreeSharedMemory_Locked(const void* backing_store);
  void RemoveSharedBufferState_Locked(Isolate* isolate,
                                      const void* backing_store);

  // Registers the allocation as shared, and tracks all the memory objects
  // associates with this allocation across isolates.
  void RegisterSharedWasmMemory_Locked(Handle<WasmMemoryObject> object,
                                       Isolate* isolate);

  // Map the new size after grow to the buffer backing store, so that instances
  // and memory objects that share the WebAssembly.Memory across isolates can
  // be updated..
  void AddBufferToGrowMap_Locked(Handle<JSArrayBuffer> old_buffer,
                                 size_t new_size);

  // Trigger a GROW_SHARED_MEMORY interrupt on all the isolates that have memory
  // objects that share this buffer.
  void TriggerSharedGrowInterruptOnAllIsolates_Locked(
      Handle<JSArrayBuffer> old_buffer);

  // When isolates hit a stack check, update the memory objects associated with
  // that isolate.
  void UpdateSharedMemoryStateOnInterrupt_Locked(Isolate* isolate,
                                                 void* backing_store,
                                                 size_t new_size);

  // Check if all the isolates that share a backing_store have hit a stack
  // check. If a stack check is hit, and the backing store is pending grow,
  // this isolate will have updated memory objects.
  bool AreAllIsolatesUpdated_Locked(const void* backing_store);

  // If a grow call is made to a buffer with a pending grow, and all the
  // isolates that share this buffer have not hit a StackCheck, clear the set of
  // already updated instances so they can be updated with the new size on the
  // most recent grow call.
  void ClearUpdatedInstancesOnPendingGrow_Locked(const void* backing_store);

  // Helper functions to update memory objects on grow, and maintain state for
  // which isolates hit a stack check.
  void UpdateMemoryObjectsForIsolate_Locked(Isolate* isolate,
                                            void* backing_store,
                                            size_t new_size);
  bool MemoryObjectsNeedUpdate_Locked(Isolate* isolate,
                                      const void* backing_store);

  // Destroy global handles to memory objects, and remove backing store from
  // isolates_per_buffer on Free.
  void DestroyMemoryObjectsAndRemoveIsolateEntry_Locked(
      Isolate* isolate, const void* backing_store);
  void DestroyMemoryObjectsAndRemoveIsolateEntry_Locked(
      const void* backing_store);

  void RemoveIsolateFromBackingStore_Locked(Isolate* isolate,
                                            const void* backing_store);

  // Removes an allocation from the tracker.
  AllocationData ReleaseAllocation_Locked(Isolate* isolate,
                                          const void* buffer_start);

  // Clients use a two-part process. First they "reserve" the address space,
  // which signifies an intent to actually allocate it. This determines whether
  // doing the allocation would put us over our limit. Once there is a
  // reservation, clients can do the allocation and register the result.
  //
  // We should always have:
  // allocated_address_space_ <= reserved_address_space_ <= kAddressSpaceLimit
  std::atomic<size_t> reserved_address_space_{0};

  // Used to protect access to the allocated address space counter and
  // allocation map. This is needed because Wasm memories can be freed on
  // another thread by the ArrayBufferTracker.
  base::Mutex mutex_;

  size_t allocated_address_space_ = 0;

  //////////////////////////////////////////////////////////////////////////////
  // Protected by {mutex_}:

  // Track Wasm memory allocation information. This is keyed by the start of the
  // buffer, rather than by the start of the allocation.
  std::unordered_map<const void*, AllocationData> allocations_;

  // Maps each buffer to the isolates that share the backing store.
  std::unordered_map<const void*, std::unordered_set<Isolate*>>
      isolates_per_buffer_;

  // Maps which isolates have had a grow interrupt handled on the buffer. This
  // is maintained to ensure that the instances are updated with the right size
  // on Grow.
  std::unordered_map<const void*, std::unordered_set<Isolate*>>
      isolates_updated_on_grow_;

  // Maps backing stores(void*) to the size of the underlying memory in
  // (size_t). An entry to this map is made on a grow call to the corresponding
  // backing store. On consecutive grow calls to the same backing store,
  // the size entry is updated. This entry is made right after the mprotect
  // call to change the protections on a backing_store, so the memory objects
  // have not been updated yet. The backing store entry in this map is erased
  // when all the memory objects, or instances that share this backing store
  // have their bounds updated.
  std::unordered_map<void*, size_t> grow_update_map_;

  // End of fields protected by {mutex_}.
  //////////////////////////////////////////////////////////////////////////////

  DISALLOW_COPY_AND_ASSIGN(WasmMemoryTracker);
};

// Attempts to allocate an array buffer with guard regions suitable for trap
// handling. If address space is not available, it will return a buffer with
// mini-guards that will require bounds checks.
V8_EXPORT_PRIVATE MaybeHandle<JSArrayBuffer> NewArrayBuffer(Isolate*,
                                                            size_t size);

// Attempts to allocate a SharedArrayBuffer with guard regions suitable for
// trap handling. If address space is not available, it will try to reserve
// up to the maximum for that memory. If all else fails, it will return a
// buffer with mini-guards of initial size.
V8_EXPORT_PRIVATE MaybeHandle<JSArrayBuffer> NewSharedArrayBuffer(
    Isolate*, size_t initial_size, size_t max_size);

Handle<JSArrayBuffer> SetupArrayBuffer(
    Isolate*, void* backing_store, size_t size, bool is_external,
    SharedFlag shared = SharedFlag::kNotShared);

V8_EXPORT_PRIVATE void DetachMemoryBuffer(Isolate* isolate,
                                          Handle<JSArrayBuffer> buffer,
                                          bool free_memory);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_MEMORY_H_
