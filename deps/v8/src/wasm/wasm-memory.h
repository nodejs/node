// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_MEMORY_H_
#define V8_WASM_WASM_MEMORY_H_

#include <atomic>
#include <unordered_map>

#include "src/base/platform/mutex.h"
#include "src/flags.h"
#include "src/handles.h"
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
  // Use {kSoftLimit} if you can implement a fallback which needs less reserved
  // memory.
  enum ReservationLimit { kSoftLimit, kHardLimit };
  bool ReserveAddressSpace(size_t num_bytes, ReservationLimit limit);

  void RegisterAllocation(Isolate* isolate, void* allocation_base,
                          size_t allocation_length, void* buffer_start,
                          size_t buffer_length);

  struct AllocationData {
    void* allocation_base = nullptr;
    size_t allocation_length = 0;
    void* buffer_start = nullptr;
    size_t buffer_length = 0;

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

  // Decreases the amount of reserved address space.
  void ReleaseReservation(size_t num_bytes);

  // Removes an allocation from the tracker.
  AllocationData ReleaseAllocation(Isolate* isolate, const void* buffer_start);

  bool IsWasmMemory(const void* buffer_start);

  // Returns whether the given buffer is a Wasm memory with guard regions large
  // enough to safely use trap handlers.
  bool HasFullGuardRegions(const void* buffer_start);

  // Returns a pointer to a Wasm buffer's allocation data, or nullptr if the
  // buffer is not tracked.
  const AllocationData* FindAllocationData(const void* buffer_start);

  // Checks if a buffer points to a Wasm memory and if so does any necessary
  // work to reclaim the buffer. If this function returns false, the caller must
  // free the buffer manually.
  bool FreeMemoryIfIsWasmMemory(Isolate* isolate, const void* buffer_start);

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
  void AddAddressSpaceSample(Isolate* isolate);

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

  // Track Wasm memory allocation information. This is keyed by the start of the
  // buffer, rather than by the start of the allocation.
  std::unordered_map<const void*, AllocationData> allocations_;

  DISALLOW_COPY_AND_ASSIGN(WasmMemoryTracker);
};

// Attempts to allocate an array buffer with guard regions suitable for trap
// handling. If address space is not available, it will return a buffer with
// mini-guards that will require bounds checks.
MaybeHandle<JSArrayBuffer> NewArrayBuffer(
    Isolate*, size_t size, SharedFlag shared = SharedFlag::kNotShared);

Handle<JSArrayBuffer> SetupArrayBuffer(
    Isolate*, void* backing_store, size_t size, bool is_external,
    SharedFlag shared = SharedFlag::kNotShared);

void DetachMemoryBuffer(Isolate* isolate, Handle<JSArrayBuffer> buffer,
                        bool free_memory);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_MEMORY_H_
