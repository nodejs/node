// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-memory.h"
#include "src/objects-inl.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"

namespace v8 {
namespace internal {
namespace wasm {

namespace {
void* TryAllocateBackingStore(WasmMemoryTracker* memory_tracker, Heap* heap,
                              size_t size, bool require_guard_regions,
                              void** allocation_base,
                              size_t* allocation_length) {
#if V8_TARGET_ARCH_32_BIT
  DCHECK(!require_guard_regions);
#endif
  // We always allocate the largest possible offset into the heap, so the
  // addressable memory after the guard page can be made inaccessible.
  *allocation_length =
      require_guard_regions
          ? RoundUp(kWasmMaxHeapOffset, CommitPageSize())
          : RoundUp(
                base::bits::RoundUpToPowerOfTwo32(static_cast<uint32_t>(size)),
                kWasmPageSize);
  DCHECK_GE(*allocation_length, size);
  DCHECK_GE(*allocation_length, kWasmPageSize);

  // Let the WasmMemoryTracker know we are going to reserve a bunch of
  // address space.
  // Try up to three times; getting rid of dead JSArrayBuffer allocations might
  // require two GCs.
  // TODO(gc): Fix this to only require one GC (crbug.com/v8/7621).
  for (int trial = 0;; ++trial) {
    if (memory_tracker->ReserveAddressSpace(*allocation_length)) break;
    // Collect garbage and retry.
    heap->MemoryPressureNotification(MemoryPressureLevel::kCritical, true);
    // After first and second GC: retry.
    if (trial < 2) continue;
    // We are over the address space limit. Fail.
    //
    // When running under the correctness fuzzer (i.e.
    // --abort-on-stack-or-string-length-overflow is preset), we crash instead
    // so it is not incorrectly reported as a correctness violation. See
    // https://crbug.com/828293#c4
    if (FLAG_abort_on_stack_or_string_length_overflow) {
      FATAL("could not allocate wasm memory");
    }
    return nullptr;
  }

  // The Reserve makes the whole region inaccessible by default.
  *allocation_base = AllocatePages(nullptr, *allocation_length, kWasmPageSize,
                                   PageAllocator::kNoAccess);
  if (*allocation_base == nullptr) {
    memory_tracker->ReleaseReservation(*allocation_length);
    return nullptr;
  }
  void* memory = *allocation_base;

  // Make the part we care about accessible.
  if (size > 0) {
    bool result = SetPermissions(memory, RoundUp(size, kWasmPageSize),
                                 PageAllocator::kReadWrite);
    // SetPermissions commits the extra memory, which may put us over the
    // process memory limit. If so, report this as an OOM.
    if (!result) {
      V8::FatalProcessOutOfMemory(nullptr, "TryAllocateBackingStore");
    }
  }

  memory_tracker->RegisterAllocation(*allocation_base, *allocation_length,
                                     memory, size);
  return memory;
}
}  // namespace

WasmMemoryTracker::~WasmMemoryTracker() {
  if (empty_backing_store_.allocation_base != nullptr) {
    CHECK(FreePages(empty_backing_store_.allocation_base,
                    empty_backing_store_.allocation_length));
    InternalReleaseAllocation(empty_backing_store_.buffer_start);
  }

  // All reserved address space should be released before the allocation tracker
  // is destroyed.
  DCHECK_EQ(reserved_address_space_, 0u);
  DCHECK_EQ(allocated_address_space_, 0u);
}

bool WasmMemoryTracker::ReserveAddressSpace(size_t num_bytes) {
// Address space reservations are currently only meaningful using guard
// regions, which is currently only supported on 64-bit systems. On other
// platforms, we always fall back on bounds checks.
#if V8_TARGET_ARCH_64_BIT
  constexpr size_t kAddressSpaceLimit = 0x10000000000L;  // 1 TiB
#else
  constexpr size_t kAddressSpaceLimit = 0x80000000;  // 2 GiB
#endif

  size_t const old_count = reserved_address_space_.fetch_add(num_bytes);
  DCHECK_GE(old_count + num_bytes, old_count);
  if (old_count + num_bytes <= kAddressSpaceLimit) {
    return true;
  }
  reserved_address_space_ -= num_bytes;
  return false;
}

void WasmMemoryTracker::ReleaseReservation(size_t num_bytes) {
  size_t const old_reserved = reserved_address_space_.fetch_sub(num_bytes);
  USE(old_reserved);
  DCHECK_LE(num_bytes, old_reserved);
  DCHECK_GE(old_reserved - num_bytes, allocated_address_space_);
}

void WasmMemoryTracker::RegisterAllocation(void* allocation_base,
                                           size_t allocation_length,
                                           void* buffer_start,
                                           size_t buffer_length) {
  // Make sure the caller has reserved the address space before registering the
  // allocation.
  DCHECK_LE(allocated_address_space_ + allocation_length,
            reserved_address_space_);

  base::LockGuard<base::Mutex> scope_lock(&mutex_);

  allocated_address_space_ += allocation_length;

  allocations_.emplace(buffer_start,
                       AllocationData{allocation_base, allocation_length,
                                      buffer_start, buffer_length});
}

WasmMemoryTracker::AllocationData WasmMemoryTracker::ReleaseAllocation(
    const void* buffer_start) {
  if (IsEmptyBackingStore(buffer_start)) {
    return AllocationData();
  }
  return InternalReleaseAllocation(buffer_start);
}

WasmMemoryTracker::AllocationData WasmMemoryTracker::InternalReleaseAllocation(
    const void* buffer_start) {
  base::LockGuard<base::Mutex> scope_lock(&mutex_);

  auto find_result = allocations_.find(buffer_start);
  CHECK_NE(find_result, allocations_.end());

  if (find_result != allocations_.end()) {
    size_t num_bytes = find_result->second.allocation_length;
    DCHECK_LE(num_bytes, reserved_address_space_);
    DCHECK_LE(num_bytes, allocated_address_space_);
    reserved_address_space_ -= num_bytes;
    allocated_address_space_ -= num_bytes;

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

void* WasmMemoryTracker::GetEmptyBackingStore(void** allocation_base,
                                              size_t* allocation_length,
                                              Heap* heap) {
  if (empty_backing_store_.allocation_base == nullptr) {
    constexpr size_t buffer_length = 0;
    const bool require_guard_regions = trap_handler::IsTrapHandlerEnabled();
    void* local_allocation_base;
    size_t local_allocation_length;
    void* buffer_start = TryAllocateBackingStore(
        this, heap, buffer_length, require_guard_regions,
        &local_allocation_base, &local_allocation_length);

    empty_backing_store_ =
        AllocationData(local_allocation_base, local_allocation_length,
                       buffer_start, buffer_length);
  }
  *allocation_base = empty_backing_store_.allocation_base;
  *allocation_length = empty_backing_store_.allocation_length;
  return empty_backing_store_.buffer_start;
}

bool WasmMemoryTracker::IsEmptyBackingStore(const void* buffer_start) const {
  return buffer_start == empty_backing_store_.buffer_start;
}

bool WasmMemoryTracker::FreeMemoryIfIsWasmMemory(const void* buffer_start) {
  if (IsEmptyBackingStore(buffer_start)) {
    // We don't need to do anything for the empty backing store, because this
    // will be freed when WasmMemoryTracker shuts down. Return true so callers
    // will not try to free the buffer on their own.
    return true;
  }
  if (IsWasmMemory(buffer_start)) {
    const AllocationData allocation = ReleaseAllocation(buffer_start);
    CHECK(FreePages(allocation.allocation_base, allocation.allocation_length));
    return true;
  }
  return false;
}

Handle<JSArrayBuffer> SetupArrayBuffer(Isolate* isolate, void* backing_store,
                                       size_t size, bool is_external,
                                       SharedFlag shared) {
  Handle<JSArrayBuffer> buffer =
      isolate->factory()->NewJSArrayBuffer(shared, TENURED);
  DCHECK_GE(kMaxInt, size);
  if (shared == SharedFlag::kShared) DCHECK(FLAG_experimental_wasm_threads);
  constexpr bool is_wasm_memory = true;
  JSArrayBuffer::Setup(buffer, isolate, is_external, backing_store,
                       static_cast<int>(size), shared, is_wasm_memory);
  buffer->set_is_neuterable(false);
  buffer->set_is_growable(true);
  return buffer;
}

MaybeHandle<JSArrayBuffer> NewArrayBuffer(Isolate* isolate, size_t size,
                                          bool require_guard_regions,
                                          SharedFlag shared) {
  // Check against kMaxInt, since the byte length is stored as int in the
  // JSArrayBuffer. Note that wasm_max_mem_pages can be raised from the command
  // line, and we don't want to fail a CHECK then.
  if (size > FLAG_wasm_max_mem_pages * kWasmPageSize || size > kMaxInt) {
    // TODO(titzer): lift restriction on maximum memory allocated here.
    return {};
  }

  WasmMemoryTracker* memory_tracker = isolate->wasm_engine()->memory_tracker();

  // Set by TryAllocateBackingStore or GetEmptyBackingStore
  void* allocation_base = nullptr;
  size_t allocation_length = 0;

  void* memory =
      (size == 0)
          ? memory_tracker->GetEmptyBackingStore(
                &allocation_base, &allocation_length, isolate->heap())
          : TryAllocateBackingStore(memory_tracker, isolate->heap(), size,
                                    require_guard_regions, &allocation_base,
                                    &allocation_length);

  if (size > 0 && memory == nullptr) return {};

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
