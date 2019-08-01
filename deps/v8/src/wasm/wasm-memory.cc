// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "src/heap/heap-inl.h"
#include "src/logging/counters.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/objects-inl.h"
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

bool RunWithGCAndRetry(const std::function<bool()>& fn, Heap* heap,
                       bool* did_retry) {
  // Try up to three times; getting rid of dead JSArrayBuffer allocations might
  // require two GCs because the first GC maybe incremental and may have
  // floating garbage.
  static constexpr int kAllocationRetries = 2;

  for (int trial = 0;; ++trial) {
    if (fn()) return true;
    // {fn} failed. If {kAllocationRetries} is reached, fail.
    *did_retry = true;
    if (trial == kAllocationRetries) return false;
    // Otherwise, collect garbage and retry.
    heap->MemoryPressureNotification(MemoryPressureLevel::kCritical, true);
  }
}

void* TryAllocateBackingStore(WasmMemoryTracker* memory_tracker, Heap* heap,
                              size_t size, size_t max_size,
                              void** allocation_base,
                              size_t* allocation_length) {
  using AllocationStatus = WasmMemoryTracker::AllocationStatus;
#if V8_TARGET_ARCH_64_BIT
  constexpr bool kRequireFullGuardRegions = true;
#else
  constexpr bool kRequireFullGuardRegions = false;
#endif
  // Let the WasmMemoryTracker know we are going to reserve a bunch of
  // address space.
  size_t reservation_size = std::max(max_size, size);
  bool did_retry = false;

  auto reserve_memory_space = [&] {
    // For guard regions, we always allocate the largest possible offset
    // into the heap, so the addressable memory after the guard page can
    // be made inaccessible.
    //
    // To protect against 32-bit integer overflow issues, we also
    // protect the 2GiB before the valid part of the memory buffer.
    *allocation_length =
        kRequireFullGuardRegions
            ? RoundUp(kWasmMaxHeapOffset + kNegativeGuardSize, CommitPageSize())
            : RoundUp(base::bits::RoundUpToPowerOfTwo(reservation_size),
                      kWasmPageSize);
    DCHECK_GE(*allocation_length, size);
    DCHECK_GE(*allocation_length, kWasmPageSize);

    return memory_tracker->ReserveAddressSpace(*allocation_length);
  };
  if (!RunWithGCAndRetry(reserve_memory_space, heap, &did_retry)) {
    // Reset reservation_size to initial size so that at least the initial size
    // can be allocated if maximum size reservation is not possible.
    reservation_size = size;

    // We are over the address space limit. Fail.
    //
    // When running under the correctness fuzzer (i.e.
    // --correctness-fuzzer-suppressions is preset), we crash
    // instead so it is not incorrectly reported as a correctness
    // violation. See https://crbug.com/828293#c4
    if (FLAG_correctness_fuzzer_suppressions) {
      FATAL("could not allocate wasm memory");
    }
    AddAllocationStatusSample(
        heap->isolate(), AllocationStatus::kAddressSpaceLimitReachedFailure);
    return nullptr;
  }

  // The Reserve makes the whole region inaccessible by default.
  DCHECK_NULL(*allocation_base);
  auto allocate_pages = [&] {
    *allocation_base =
        AllocatePages(GetPlatformPageAllocator(), nullptr, *allocation_length,
                      kWasmPageSize, PageAllocator::kNoAccess);
    return *allocation_base != nullptr;
  };
  if (!RunWithGCAndRetry(allocate_pages, heap, &did_retry)) {
    memory_tracker->ReleaseReservation(*allocation_length);
    AddAllocationStatusSample(heap->isolate(), AllocationStatus::kOtherFailure);
    return nullptr;
  }

  byte* memory = reinterpret_cast<byte*>(*allocation_base);
  if (kRequireFullGuardRegions) {
    memory += kNegativeGuardSize;
  }

  // Make the part we care about accessible.
  auto commit_memory = [&] {
    return size == 0 || SetPermissions(GetPlatformPageAllocator(), memory,
                                       RoundUp(size, kWasmPageSize),
                                       PageAllocator::kReadWrite);
  };
  // SetPermissions commits the extra memory, which may put us over the
  // process memory limit. If so, report this as an OOM.
  if (!RunWithGCAndRetry(commit_memory, heap, &did_retry)) {
    V8::FatalProcessOutOfMemory(nullptr, "TryAllocateBackingStore");
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
constexpr size_t kAddressSpaceLimit = 0x8000000000L;  // 512 GiB
#elif V8_TARGET_ARCH_64_BIT
constexpr size_t kAddressSpaceLimit = 0x10100000000L;  // 1 TiB + 4 GiB
#else
constexpr size_t kAddressSpaceLimit = 0xC0000000;  // 3 GiB
#endif

}  // namespace

WasmMemoryTracker::~WasmMemoryTracker() {
  // All reserved address space should be released before the allocation tracker
  // is destroyed.
  DCHECK_EQ(reserved_address_space_, 0u);
  DCHECK_EQ(allocated_address_space_, 0u);
  DCHECK(allocations_.empty());
}

void* WasmMemoryTracker::TryAllocateBackingStoreForTesting(
    Heap* heap, size_t size, void** allocation_base,
    size_t* allocation_length) {
  return TryAllocateBackingStore(this, heap, size, size, allocation_base,
                                 allocation_length);
}

void WasmMemoryTracker::FreeBackingStoreForTesting(base::AddressRegion memory,
                                                   void* buffer_start) {
  base::MutexGuard scope_lock(&mutex_);
  ReleaseAllocation_Locked(nullptr, buffer_start);
  CHECK(FreePages(GetPlatformPageAllocator(),
                  reinterpret_cast<void*>(memory.begin()), memory.size()));
}

bool WasmMemoryTracker::ReserveAddressSpace(size_t num_bytes) {
  size_t reservation_limit = kAddressSpaceLimit;
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
  base::MutexGuard scope_lock(&mutex_);

  allocated_address_space_ += allocation_length;
  // Report address space usage in MiB so the full range fits in an int on all
  // platforms.
  isolate->counters()->wasm_address_space_usage_mb()->AddSample(
      static_cast<int>(allocated_address_space_ / MB));

  allocations_.emplace(buffer_start,
                       AllocationData{allocation_base, allocation_length,
                                      buffer_start, buffer_length});
}

WasmMemoryTracker::AllocationData WasmMemoryTracker::ReleaseAllocation_Locked(
    Isolate* isolate, const void* buffer_start) {
  auto find_result = allocations_.find(buffer_start);
  CHECK_NE(find_result, allocations_.end());

  size_t num_bytes = find_result->second.allocation_length;
  DCHECK_LE(num_bytes, reserved_address_space_);
  DCHECK_LE(num_bytes, allocated_address_space_);
  reserved_address_space_ -= num_bytes;
  allocated_address_space_ -= num_bytes;

  AllocationData allocation_data = find_result->second;
  allocations_.erase(find_result);
  return allocation_data;
}

const WasmMemoryTracker::AllocationData* WasmMemoryTracker::FindAllocationData(
    const void* buffer_start) {
  base::MutexGuard scope_lock(&mutex_);
  const auto& result = allocations_.find(buffer_start);
  if (result != allocations_.end()) {
    return &result->second;
  }
  return nullptr;
}

bool WasmMemoryTracker::IsWasmMemory(const void* buffer_start) {
  base::MutexGuard scope_lock(&mutex_);
  return allocations_.find(buffer_start) != allocations_.end();
}

bool WasmMemoryTracker::IsWasmSharedMemory(const void* buffer_start) {
  base::MutexGuard scope_lock(&mutex_);
  const auto& result = allocations_.find(buffer_start);
  // Should be a wasm allocation, and registered as a shared allocation.
  return (result != allocations_.end() && result->second.is_shared);
}

void WasmMemoryTracker::MarkWasmMemoryNotGrowable(
    Handle<JSArrayBuffer> buffer) {
  base::MutexGuard scope_lock(&mutex_);
  const auto& allocation = allocations_.find(buffer->backing_store());
  if (allocation == allocations_.end()) return;
  allocation->second.is_growable = false;
}

bool WasmMemoryTracker::IsWasmMemoryGrowable(Handle<JSArrayBuffer> buffer) {
  base::MutexGuard scope_lock(&mutex_);
  if (buffer->backing_store() == nullptr) return true;
  const auto& allocation = allocations_.find(buffer->backing_store());
  if (allocation == allocations_.end()) return false;
  return allocation->second.is_growable;
}

bool WasmMemoryTracker::FreeWasmMemory(Isolate* isolate,
                                       const void* buffer_start) {
  base::MutexGuard scope_lock(&mutex_);
  const auto& result = allocations_.find(buffer_start);
  if (result == allocations_.end()) return false;
  if (result->second.is_shared) {
    // This is a shared WebAssembly.Memory allocation
    FreeMemoryIfNotShared_Locked(isolate, buffer_start);
    return true;
  }
  // This is a WebAssembly.Memory allocation
  const AllocationData allocation =
      ReleaseAllocation_Locked(isolate, buffer_start);
  CHECK(FreePages(GetPlatformPageAllocator(), allocation.allocation_base,
                  allocation.allocation_length));
  return true;
}

void WasmMemoryTracker::RegisterWasmMemoryAsShared(
    Handle<WasmMemoryObject> object, Isolate* isolate) {
  // Only register with the tracker if shared grow is enabled.
  if (!FLAG_wasm_grow_shared_memory) return;
  const void* backing_store = object->array_buffer().backing_store();
  // TODO(V8:8810): This should be a DCHECK, currently some tests do not
  // use a full WebAssembly.Memory, and fail on registering so return early.
  if (!IsWasmMemory(backing_store)) return;
  {
    base::MutexGuard scope_lock(&mutex_);
    // Register as shared allocation when it is post messaged. This happens only
    // the first time a buffer is shared over Postmessage, and track all the
    // memory objects that are associated with this backing store.
    RegisterSharedWasmMemory_Locked(object, isolate);
    // Add isolate to backing store mapping.
    isolates_per_buffer_[backing_store].emplace(isolate);
  }
}

void WasmMemoryTracker::SetPendingUpdateOnGrow(Handle<JSArrayBuffer> old_buffer,
                                               size_t new_size) {
  base::MutexGuard scope_lock(&mutex_);
  // Keep track of the new size of the buffer associated with each backing
  // store.
  AddBufferToGrowMap_Locked(old_buffer, new_size);
  // Request interrupt to GROW_SHARED_MEMORY to other isolates
  TriggerSharedGrowInterruptOnAllIsolates_Locked(old_buffer);
}

void WasmMemoryTracker::UpdateSharedMemoryInstances(Isolate* isolate) {
  base::MutexGuard scope_lock(&mutex_);
  // For every buffer in the grow_entry_map_, update the size for all the
  // memory objects associated with this isolate.
  for (auto it = grow_update_map_.begin(); it != grow_update_map_.end();) {
    UpdateSharedMemoryStateOnInterrupt_Locked(isolate, it->first, it->second);
    // If all the isolates that share this buffer have hit a stack check, their
    // memory objects are updated, and this grow entry can be erased.
    if (AreAllIsolatesUpdated_Locked(it->first)) {
      it = grow_update_map_.erase(it);
    } else {
      it++;
    }
  }
}

void WasmMemoryTracker::RegisterSharedWasmMemory_Locked(
    Handle<WasmMemoryObject> object, Isolate* isolate) {
  DCHECK(object->array_buffer().is_shared());

  void* backing_store = object->array_buffer().backing_store();
  // The allocation of a WasmMemoryObject should always be registered with the
  // WasmMemoryTracker.
  const auto& result = allocations_.find(backing_store);
  if (result == allocations_.end()) return;

  // Register the allocation as shared, if not alreadt marked as shared.
  if (!result->second.is_shared) result->second.is_shared = true;

  // Create persistent global handles for the memory objects that are shared
  GlobalHandles* global_handles = isolate->global_handles();
  object = global_handles->Create(*object);

  // Add to memory_object_vector to track memory objects, instance objects
  // that will need to be updated on a Grow call
  result->second.memory_object_vector.push_back(
      SharedMemoryObjectState(object, isolate));
}

void WasmMemoryTracker::AddBufferToGrowMap_Locked(
    Handle<JSArrayBuffer> old_buffer, size_t new_size) {
  void* backing_store = old_buffer->backing_store();
  auto entry = grow_update_map_.find(old_buffer->backing_store());
  if (entry == grow_update_map_.end()) {
    // No pending grow for this backing store, add to map.
    grow_update_map_.emplace(backing_store, new_size);
    return;
  }
  // If grow on the same buffer is requested before the update is complete,
  // the new_size should always be greater or equal to the old_size. Equal
  // in the case that grow(0) is called, but new buffer handles are mandated
  // by the Spec.
  CHECK_LE(entry->second, new_size);
  entry->second = new_size;
  // Flush instances_updated everytime a new grow size needs to be updates
  ClearUpdatedInstancesOnPendingGrow_Locked(backing_store);
}

void WasmMemoryTracker::TriggerSharedGrowInterruptOnAllIsolates_Locked(
    Handle<JSArrayBuffer> old_buffer) {
  // Request a GrowShareMemory interrupt on all the isolates that share
  // the backing store.
  const auto& isolates = isolates_per_buffer_.find(old_buffer->backing_store());
  for (const auto& isolate : isolates->second) {
    isolate->stack_guard()->RequestGrowSharedMemory();
  }
}

void WasmMemoryTracker::UpdateSharedMemoryStateOnInterrupt_Locked(
    Isolate* isolate, void* backing_store, size_t new_size) {
  // Update objects only if there are memory objects that share this backing
  // store, and this isolate is marked as one of the isolates that shares this
  // buffer.
  if (MemoryObjectsNeedUpdate_Locked(isolate, backing_store)) {
    UpdateMemoryObjectsForIsolate_Locked(isolate, backing_store, new_size);
    // As the memory objects are updated, add this isolate to a set of isolates
    // that are updated on grow. This state is maintained to track if all the
    // isolates that share the backing store have hit a StackCheck.
    isolates_updated_on_grow_[backing_store].emplace(isolate);
  }
}

bool WasmMemoryTracker::AreAllIsolatesUpdated_Locked(
    const void* backing_store) {
  const auto& buffer_isolates = isolates_per_buffer_.find(backing_store);
  // No isolates share this buffer.
  if (buffer_isolates == isolates_per_buffer_.end()) return true;
  const auto& updated_isolates = isolates_updated_on_grow_.find(backing_store);
  // Some isolates share the buffer, but no isolates have been updated yet.
  if (updated_isolates == isolates_updated_on_grow_.end()) return false;
  if (buffer_isolates->second == updated_isolates->second) {
    // If all the isolates that share this backing_store have hit a stack check,
    // and the memory objects have been updated, remove the entry from the
    // updatemap, and return true.
    isolates_updated_on_grow_.erase(backing_store);
    return true;
  }
  return false;
}

void WasmMemoryTracker::ClearUpdatedInstancesOnPendingGrow_Locked(
    const void* backing_store) {
  // On multiple grows to the same buffer, the entries for that buffer should be
  // flushed. This is done so that any consecutive grows to the same buffer will
  // update all instances that share this buffer.
  const auto& value = isolates_updated_on_grow_.find(backing_store);
  if (value != isolates_updated_on_grow_.end()) {
    value->second.clear();
  }
}

void WasmMemoryTracker::UpdateMemoryObjectsForIsolate_Locked(
    Isolate* isolate, void* backing_store, size_t new_size) {
  const auto& result = allocations_.find(backing_store);
  if (result == allocations_.end() || !result->second.is_shared) return;
  for (const auto& memory_obj_state : result->second.memory_object_vector) {
    DCHECK_NE(memory_obj_state.isolate, nullptr);
    if (isolate == memory_obj_state.isolate) {
      HandleScope scope(isolate);
      Handle<WasmMemoryObject> memory_object = memory_obj_state.memory_object;
      DCHECK(memory_object->IsWasmMemoryObject());
      DCHECK(memory_object->array_buffer().is_shared());
      // Permissions adjusted, but create a new buffer with new size
      // and old attributes. Buffer has already been allocated,
      // just create a new buffer with same backing store.
      bool is_external = memory_object->array_buffer().is_external();
      Handle<JSArrayBuffer> new_buffer = SetupArrayBuffer(
          isolate, backing_store, new_size, is_external, SharedFlag::kShared);
      memory_obj_state.memory_object->update_instances(isolate, new_buffer);
    }
  }
}

bool WasmMemoryTracker::MemoryObjectsNeedUpdate_Locked(
    Isolate* isolate, const void* backing_store) {
  // Return true if this buffer has memory_objects it needs to update.
  const auto& result = allocations_.find(backing_store);
  if (result == allocations_.end() || !result->second.is_shared) return false;
  // Only update if the buffer has memory objects that need to be updated.
  if (result->second.memory_object_vector.empty()) return false;
  const auto& isolate_entry = isolates_per_buffer_.find(backing_store);
  return (isolate_entry != isolates_per_buffer_.end() &&
          isolate_entry->second.count(isolate) != 0);
}

void WasmMemoryTracker::FreeMemoryIfNotShared_Locked(
    Isolate* isolate, const void* backing_store) {
  RemoveSharedBufferState_Locked(isolate, backing_store);
  if (CanFreeSharedMemory_Locked(backing_store)) {
    const AllocationData allocation =
        ReleaseAllocation_Locked(isolate, backing_store);
    CHECK(FreePages(GetPlatformPageAllocator(), allocation.allocation_base,
                    allocation.allocation_length));
  }
}

bool WasmMemoryTracker::CanFreeSharedMemory_Locked(const void* backing_store) {
  const auto& value = isolates_per_buffer_.find(backing_store);
  // If no isolates share this buffer, backing store can be freed.
  // Erase the buffer entry.
  if (value == isolates_per_buffer_.end() || value->second.empty()) return true;
  return false;
}

void WasmMemoryTracker::RemoveSharedBufferState_Locked(
    Isolate* isolate, const void* backing_store) {
  if (isolate != nullptr) {
    DestroyMemoryObjectsAndRemoveIsolateEntry_Locked(isolate, backing_store);
    RemoveIsolateFromBackingStore_Locked(isolate, backing_store);
  } else {
    // This happens for externalized contents cleanup shared memory state
    // associated with this buffer across isolates.
    DestroyMemoryObjectsAndRemoveIsolateEntry_Locked(backing_store);
  }
}

void WasmMemoryTracker::DestroyMemoryObjectsAndRemoveIsolateEntry_Locked(
    const void* backing_store) {
  const auto& result = allocations_.find(backing_store);
  CHECK(result != allocations_.end() && result->second.is_shared);
  auto& object_vector = result->second.memory_object_vector;
  if (object_vector.empty()) return;
  for (const auto& mem_obj_state : object_vector) {
    GlobalHandles::Destroy(mem_obj_state.memory_object.location());
  }
  object_vector.clear();
  // Remove isolate from backing store map.
  isolates_per_buffer_.erase(backing_store);
}

void WasmMemoryTracker::DestroyMemoryObjectsAndRemoveIsolateEntry_Locked(
    Isolate* isolate, const void* backing_store) {
  // This gets called when an internal handle to the ArrayBuffer should be
  // freed, on heap tear down for that isolate, remove the memory objects
  // that are associated with this buffer and isolate.
  const auto& result = allocations_.find(backing_store);
  CHECK(result != allocations_.end() && result->second.is_shared);
  auto& object_vector = result->second.memory_object_vector;
  if (object_vector.empty()) return;
  for (auto it = object_vector.begin(); it != object_vector.end();) {
    if (isolate == it->isolate) {
      GlobalHandles::Destroy(it->memory_object.location());
      it = object_vector.erase(it);
    } else {
      ++it;
    }
  }
}

void WasmMemoryTracker::RemoveIsolateFromBackingStore_Locked(
    Isolate* isolate, const void* backing_store) {
  const auto& isolates = isolates_per_buffer_.find(backing_store);
  if (isolates == isolates_per_buffer_.end() || isolates->second.empty())
    return;
  isolates->second.erase(isolate);
}

void WasmMemoryTracker::DeleteSharedMemoryObjectsOnIsolate(Isolate* isolate) {
  base::MutexGuard scope_lock(&mutex_);
  // This is possible for buffers that are externalized, and their handles have
  // been freed, the backing store wasn't released because externalized contents
  // were using it.
  if (isolates_per_buffer_.empty()) return;
  for (auto& entry : isolates_per_buffer_) {
    if (entry.second.find(isolate) == entry.second.end()) continue;
    const void* backing_store = entry.first;
    entry.second.erase(isolate);
    DestroyMemoryObjectsAndRemoveIsolateEntry_Locked(isolate, backing_store);
  }
  for (auto& buffer_isolates : isolates_updated_on_grow_) {
    auto& isolates = buffer_isolates.second;
    isolates.erase(isolate);
  }
}

Handle<JSArrayBuffer> SetupArrayBuffer(Isolate* isolate, void* backing_store,
                                       size_t size, bool is_external,
                                       SharedFlag shared) {
  Handle<JSArrayBuffer> buffer =
      isolate->factory()->NewJSArrayBuffer(shared, AllocationType::kOld);
  constexpr bool is_wasm_memory = true;
  JSArrayBuffer::Setup(buffer, isolate, is_external, backing_store, size,
                       shared, is_wasm_memory);
  buffer->set_is_detachable(false);
  return buffer;
}

MaybeHandle<JSArrayBuffer> AllocateAndSetupArrayBuffer(Isolate* isolate,
                                                       size_t size,
                                                       size_t maximum_size,
                                                       SharedFlag shared) {
  // Enforce flag-limited maximum allocation size.
  if (size > max_mem_bytes()) return {};

  WasmMemoryTracker* memory_tracker = isolate->wasm_engine()->memory_tracker();

  // Set by TryAllocateBackingStore or GetEmptyBackingStore
  void* allocation_base = nullptr;
  size_t allocation_length = 0;

  void* memory = TryAllocateBackingStore(memory_tracker, isolate->heap(), size,
                                         maximum_size, &allocation_base,
                                         &allocation_length);
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

MaybeHandle<JSArrayBuffer> NewArrayBuffer(Isolate* isolate, size_t size) {
  return AllocateAndSetupArrayBuffer(isolate, size, size,
                                     SharedFlag::kNotShared);
}

MaybeHandle<JSArrayBuffer> NewSharedArrayBuffer(Isolate* isolate,
                                                size_t initial_size,
                                                size_t max_size) {
  return AllocateAndSetupArrayBuffer(isolate, initial_size, max_size,
                                     SharedFlag::kShared);
}

void DetachMemoryBuffer(Isolate* isolate, Handle<JSArrayBuffer> buffer,
                        bool free_memory) {
  if (buffer->is_shared()) return;  // Detaching shared buffers is impossible.
  DCHECK(!buffer->is_detachable());

  const bool is_external = buffer->is_external();
  DCHECK(!buffer->is_detachable());
  if (!is_external) {
    buffer->set_is_external(true);
    isolate->heap()->UnregisterArrayBuffer(*buffer);
    if (free_memory) {
      // We need to free the memory before detaching the buffer because
      // FreeBackingStore reads buffer->allocation_base(), which is nulled out
      // by Detach. This means there is a dangling pointer until we detach the
      // buffer. Since there is no way for the user to directly call
      // FreeBackingStore, we can ensure this is safe.
      buffer->FreeBackingStoreFromMainThread();
    }
  }

  DCHECK(buffer->is_external());
  buffer->set_is_wasm_memory(false);
  buffer->set_is_detachable(true);
  buffer->Detach();
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
