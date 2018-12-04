// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "src/heap/heap-inl.h"
#include "src/objects-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-memory.h"
#include "src/wasm/wasm-module.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {

constexpr size_t kNegativeGuardSize = 1u << 31;  // 2GiB

void AddAllocationStatusSample(Isolate* isolate,
                               WasmMemoryTracker::AllocationStatus status) {
  isolate->counters()->wasm_memory_allocation_result()->AddSample(
      static_cast<int>(status));
}

void* TryAllocateBackingStore(WasmMemoryTracker* memory_tracker, Heap* heap,
                              size_t size, void** allocation_base,
                              size_t* allocation_length) {
  using AllocationStatus = WasmMemoryTracker::AllocationStatus;
#if V8_TARGET_ARCH_64_BIT
  bool require_full_guard_regions = true;
#else
  bool require_full_guard_regions = false;
#endif
  // Let the WasmMemoryTracker know we are going to reserve a bunch of
  // address space.
  // Try up to three times; getting rid of dead JSArrayBuffer allocations might
  // require two GCs because the first GC maybe incremental and may have
  // floating garbage.
  static constexpr int kAllocationRetries = 2;
  bool did_retry = false;
  for (int trial = 0;; ++trial) {
    // For guard regions, we always allocate the largest possible offset into
    // the heap, so the addressable memory after the guard page can be made
    // inaccessible.
    //
    // To protect against 32-bit integer overflow issues, we also protect the
    // 2GiB before the valid part of the memory buffer.
    // TODO(7881): do not use static_cast<uint32_t>() here
    *allocation_length =
        require_full_guard_regions
            ? RoundUp(kWasmMaxHeapOffset + kNegativeGuardSize, CommitPageSize())
            : RoundUp(base::bits::RoundUpToPowerOfTwo32(
                          static_cast<uint32_t>(size)),
                      kWasmPageSize);
    DCHECK_GE(*allocation_length, size);
    DCHECK_GE(*allocation_length, kWasmPageSize);

    auto limit = require_full_guard_regions ? WasmMemoryTracker::kSoftLimit
                                            : WasmMemoryTracker::kHardLimit;
    if (memory_tracker->ReserveAddressSpace(*allocation_length, limit)) break;

    did_retry = true;
    // After first and second GC: retry.
    if (trial == kAllocationRetries) {
      // If we fail to allocate guard regions and the fallback is enabled, then
      // retry without full guard regions.
      if (require_full_guard_regions && FLAG_wasm_trap_handler_fallback) {
        require_full_guard_regions = false;
        --trial;  // one more try.
        continue;
      }

      // We are over the address space limit. Fail.
      //
      // When running under the correctness fuzzer (i.e.
      // --abort-on-stack-or-string-length-overflow is preset), we crash
      // instead so it is not incorrectly reported as a correctness
      // violation. See https://crbug.com/828293#c4
      if (FLAG_abort_on_stack_or_string_length_overflow) {
        FATAL("could not allocate wasm memory");
      }
      AddAllocationStatusSample(
          heap->isolate(), AllocationStatus::kAddressSpaceLimitReachedFailure);
      return nullptr;
    }
    // Collect garbage and retry.
    heap->MemoryPressureNotification(MemoryPressureLevel::kCritical, true);
  }

  // The Reserve makes the whole region inaccessible by default.
  DCHECK_NULL(*allocation_base);
  for (int trial = 0;; ++trial) {
    *allocation_base =
        AllocatePages(GetPlatformPageAllocator(), nullptr, *allocation_length,
                      kWasmPageSize, PageAllocator::kNoAccess);
    if (*allocation_base != nullptr) break;
    if (trial == kAllocationRetries) {
      memory_tracker->ReleaseReservation(*allocation_length);
      AddAllocationStatusSample(heap->isolate(),
                                AllocationStatus::kOtherFailure);
      return nullptr;
    }
    heap->MemoryPressureNotification(MemoryPressureLevel::kCritical, true);
  }
  byte* memory = reinterpret_cast<byte*>(*allocation_base);
  if (require_full_guard_regions) {
    memory += kNegativeGuardSize;
  }

  // Make the part we care about accessible.
  if (size > 0) {
    bool result =
        SetPermissions(GetPlatformPageAllocator(), memory,
                       RoundUp(size, kWasmPageSize), PageAllocator::kReadWrite);
    // SetPermissions commits the extra memory, which may put us over the
    // process memory limit. If so, report this as an OOM.
    if (!result) {
      V8::FatalProcessOutOfMemory(nullptr, "TryAllocateBackingStore");
    }
  }

  memory_tracker->RegisterAllocation(heap->isolate(), *allocation_base,
                                     *allocation_length, memory, size);
  AddAllocationStatusSample(heap->isolate(),
                            did_retry ? AllocationStatus::kSuccessAfterRetry
                                      : AllocationStatus::kSuccess);
  return memory;
}

#if V8_TARGET_ARCH_MIPS64
// MIPS64 has a user space of 2^40 bytes on most processors,
// address space limits needs to be smaller.
constexpr size_t kAddressSpaceSoftLimit = 0x2100000000L;  // 132 GiB
constexpr size_t kAddressSpaceHardLimit = 0x4000000000L;  // 256 GiB
#elif V8_TARGET_ARCH_64_BIT
constexpr size_t kAddressSpaceSoftLimit = 0x6000000000L;   // 384 GiB
constexpr size_t kAddressSpaceHardLimit = 0x10100000000L;  // 1 TiB + 4 GiB
#else
constexpr size_t kAddressSpaceSoftLimit = 0x90000000;  // 2 GiB + 256 MiB
constexpr size_t kAddressSpaceHardLimit = 0xC0000000;  // 3 GiB
#endif

}  // namespace

WasmMemoryTracker::~WasmMemoryTracker() {
  // All reserved address space should be released before the allocation tracker
  // is destroyed.
  DCHECK_EQ(reserved_address_space_, 0u);
  DCHECK_EQ(allocated_address_space_, 0u);
}

bool WasmMemoryTracker::ReserveAddressSpace(size_t num_bytes,
                                            ReservationLimit limit) {
  size_t reservation_limit =
      limit == kSoftLimit ? kAddressSpaceSoftLimit : kAddressSpaceHardLimit;
  while (true) {
    size_t old_count = reserved_address_space_.load();
    if (old_count > reservation_limit) return false;
    if (reservation_limit - old_count < num_bytes) return false;
    if (reserved_address_space_.compare_exchange_weak(old_count,
                                                      old_count + num_bytes)) {
      return true;
    }
  }
}

void WasmMemoryTracker::ReleaseReservation(size_t num_bytes) {
  size_t const old_reserved = reserved_address_space_.fetch_sub(num_bytes);
  USE(old_reserved);
  DCHECK_LE(num_bytes, old_reserved);
}

void WasmMemoryTracker::RegisterAllocation(Isolate* isolate,
                                           void* allocation_base,
                                           size_t allocation_length,
                                           void* buffer_start,
                                           size_t buffer_length) {
  base::LockGuard<base::Mutex> scope_lock(&mutex_);

  allocated_address_space_ += allocation_length;
  AddAddressSpaceSample(isolate);

  allocations_.emplace(buffer_start,
                       AllocationData{allocation_base, allocation_length,
                                      buffer_start, buffer_length});
}

WasmMemoryTracker::AllocationData WasmMemoryTracker::ReleaseAllocation(
    Isolate* isolate, const void* buffer_start) {
  base::LockGuard<base::Mutex> scope_lock(&mutex_);

  auto find_result = allocations_.find(buffer_start);
  CHECK_NE(find_result, allocations_.end());

  if (find_result != allocations_.end()) {
    size_t num_bytes = find_result->second.allocation_length;
    DCHECK_LE(num_bytes, reserved_address_space_);
    DCHECK_LE(num_bytes, allocated_address_space_);
    reserved_address_space_ -= num_bytes;
    allocated_address_space_ -= num_bytes;
    // ReleaseAllocation might be called with a nullptr as isolate if the
    // embedder is releasing the allocation and not a specific isolate. This
    // happens if the allocation was shared between multiple isolates (threads).
    if (isolate) AddAddressSpaceSample(isolate);

    AllocationData allocation_data = find_result->second;
    allocations_.erase(find_result);
    return allocation_data;
  }
  UNREACHABLE();
}

const WasmMemoryTracker::AllocationData* WasmMemoryTracker::FindAllocationData(
    const void* buffer_start) {
  base::LockGuard<base::Mutex> scope_lock(&mutex_);
  const auto& result = allocations_.find(buffer_start);
  if (result != allocations_.end()) {
    return &result->second;
  }
  return nullptr;
}

bool WasmMemoryTracker::IsWasmMemory(const void* buffer_start) {
  base::LockGuard<base::Mutex> scope_lock(&mutex_);
  return allocations_.find(buffer_start) != allocations_.end();
}

bool WasmMemoryTracker::HasFullGuardRegions(const void* buffer_start) {
  base::LockGuard<base::Mutex> scope_lock(&mutex_);
  const auto allocation = allocations_.find(buffer_start);

  if (allocation == allocations_.end()) {
    return false;
  }

  Address start = reinterpret_cast<Address>(buffer_start);
  Address limit =
      reinterpret_cast<Address>(allocation->second.allocation_base) +
      allocation->second.allocation_length;
  return start + kWasmMaxHeapOffset < limit;
}

bool WasmMemoryTracker::FreeMemoryIfIsWasmMemory(Isolate* isolate,
                                                 const void* buffer_start) {
  if (IsWasmMemory(buffer_start)) {
    const AllocationData allocation = ReleaseAllocation(isolate, buffer_start);
    CHECK(FreePages(GetPlatformPageAllocator(), allocation.allocation_base,
                    allocation.allocation_length));
    return true;
  }
  return false;
}

void WasmMemoryTracker::AddAddressSpaceSample(Isolate* isolate) {
  // Report address space usage in MiB so the full range fits in an int on all
  // platforms.
  isolate->counters()->wasm_address_space_usage_mb()->AddSample(
      static_cast<int>(allocated_address_space_ >> 20));
}

Handle<JSArrayBuffer> SetupArrayBuffer(Isolate* isolate, void* backing_store,
                                       size_t size, bool is_external,
                                       SharedFlag shared) {
  Handle<JSArrayBuffer> buffer =
      isolate->factory()->NewJSArrayBuffer(shared, TENURED);
  constexpr bool is_wasm_memory = true;
  JSArrayBuffer::Setup(buffer, isolate, is_external, backing_store, size,
                       shared, is_wasm_memory);
  buffer->set_is_neuterable(false);
  buffer->set_is_growable(true);
  return buffer;
}

MaybeHandle<JSArrayBuffer> NewArrayBuffer(Isolate* isolate, size_t size,
                                          SharedFlag shared) {
  // Enforce engine-limited maximum allocation size.
  if (size > kV8MaxWasmMemoryBytes) return {};
  // Enforce flag-limited maximum allocation size.
  if (size > (FLAG_wasm_max_mem_pages * uint64_t{kWasmPageSize})) return {};

  WasmMemoryTracker* memory_tracker = isolate->wasm_engine()->memory_tracker();

  // Set by TryAllocateBackingStore or GetEmptyBackingStore
  void* allocation_base = nullptr;
  size_t allocation_length = 0;

  void* memory = TryAllocateBackingStore(memory_tracker, isolate->heap(), size,
                                         &allocation_base, &allocation_length);
  if (memory == nullptr) return {};

#if DEBUG
  // Double check the API allocator actually zero-initialized the memory.
  const byte* bytes = reinterpret_cast<const byte*>(memory);
  for (size_t i = 0; i < size; ++i) {
    DCHECK_EQ(0, bytes[i]);
  }
#endif

  reinterpret_cast<v8::Isolate*>(isolate)
      ->AdjustAmountOfExternalAllocatedMemory(size);

  constexpr bool is_external = false;
  return SetupArrayBuffer(isolate, memory, size, is_external, shared);
}

void DetachMemoryBuffer(Isolate* isolate, Handle<JSArrayBuffer> buffer,
                        bool free_memory) {
  if (buffer->is_shared()) return;  // Detaching shared buffers is impossible.
  DCHECK(!buffer->is_neuterable());

  const bool is_external = buffer->is_external();
  DCHECK(!buffer->is_neuterable());
  if (!is_external) {
    buffer->set_is_external(true);
    isolate->heap()->UnregisterArrayBuffer(*buffer);
    if (free_memory) {
      // We need to free the memory before neutering the buffer because
      // FreeBackingStore reads buffer->allocation_base(), which is nulled out
      // by Neuter. This means there is a dangling pointer until we neuter the
      // buffer. Since there is no way for the user to directly call
      // FreeBackingStore, we can ensure this is safe.
      buffer->FreeBackingStoreFromMainThread();
    }
  }

  DCHECK(buffer->is_external());
  buffer->set_is_wasm_memory(false);
  buffer->set_is_neuterable(true);
  buffer->Neuter();
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
