// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_MEMORY_H_
#define V8_WASM_WASM_MEMORY_H_

#include <unordered_map>

#include "src/base/platform/mutex.h"
#include "src/flags.h"
#include "src/handles.h"
#include "src/objects/js-array.h"

namespace v8 {
namespace internal {
namespace wasm {

class WasmMemoryTracker {
 public:
  WasmMemoryTracker() {}
  ~WasmMemoryTracker();

  // ReserveAddressSpace attempts to increase the reserved address space counter
  // to determine whether there is enough headroom to allocate another guarded
  // Wasm memory. Returns true if successful (meaning it is okay to go ahead and
  // allocate the buffer), false otherwise.
  bool ReserveAddressSpace(size_t num_bytes);

  void RegisterAllocation(void* allocation_base, size_t allocation_length,
                          void* buffer_start, size_t buffer_length);

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

  // Decreases the amount of reserved address space
  void ReleaseReservation(size_t num_bytes);

  // Removes an allocation from the tracker
  AllocationData ReleaseAllocation(const void* buffer_start);

  bool IsWasmMemory(const void* buffer_start);

  // Returns a pointer to a Wasm buffer's allocation data, or nullptr if the
  // buffer is not tracked.
  const AllocationData* FindAllocationData(const void* buffer_start);

  // Empty WebAssembly memories are all backed by a shared inaccessible
  // reservation. This method creates this store or returns the existing one if
  // already created.
  void* GetEmptyBackingStore(void** allocation_base, size_t* allocation_length,
                             Heap* heap);

  bool IsEmptyBackingStore(const void* buffer_start) const;

  // Checks if a buffer points to a Wasm memory and if so does any necessary
  // work to reclaim the buffer. If this function returns false, the caller must
  // free the buffer manually.
  bool FreeMemoryIfIsWasmMemory(const void* buffer_start);

 private:
  AllocationData InternalReleaseAllocation(const void* buffer_start);

  // Clients use a two-part process. First they "reserve" the address space,
  // which signifies an intent to actually allocate it. This determines whether
  // doing the allocation would put us over our limit. Once there is a
  // reservation, clients can do the allocation and register the result.
  //
  // We should always have:
  // allocated_address_space_ <= reserved_address_space_ <= kAddressSpaceLimit
  std::atomic_size_t reserved_address_space_{0};

  // Used to protect access to the allocated address space counter and
  // allocation map. This is needed because Wasm memories can be freed on
  // another thread by the ArrayBufferTracker.
  base::Mutex mutex_;

  size_t allocated_address_space_{0};

  // Track Wasm memory allocation information. This is keyed by the start of the
  // buffer, rather than by the start of the allocation.
  std::unordered_map<const void*, AllocationData> allocations_;

  // Empty backing stores still need to be backed by mapped pages when using
  // trap handlers. Because this could eat up address space quickly, we keep a
  // shared backing store here.
  AllocationData empty_backing_store_;

  DISALLOW_COPY_AND_ASSIGN(WasmMemoryTracker);
};

MaybeHandle<JSArrayBuffer> NewArrayBuffer(
    Isolate*, size_t size, bool require_guard_regions,
    SharedFlag shared = SharedFlag::kNotShared);

Handle<JSArrayBuffer> SetupArrayBuffer(
    Isolate*, void* backing_store, size_t size, bool is_external,
    SharedFlag shared = SharedFlag::kNotShared);

void DetachMemoryBuffer(Isolate* isolate, Handle<JSArrayBuffer> buffer,
                        bool free_memory);

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_MEMORY_H_
