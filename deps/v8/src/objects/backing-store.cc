// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/backing-store.h"
#include "src/execution/isolate.h"
#include "src/handles/global-handles.h"
#include "src/logging/counters.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-objects-inl.h"

#define TRACE_BS(...)                                  \
  do {                                                 \
    if (FLAG_trace_backing_store) PrintF(__VA_ARGS__); \
  } while (false)

namespace v8 {
namespace internal {

namespace {
#if V8_TARGET_ARCH_64_BIT
constexpr bool kUseGuardRegions = true;
#else
constexpr bool kUseGuardRegions = false;
#endif

#if V8_TARGET_ARCH_MIPS64
// MIPS64 has a user space of 2^40 bytes on most processors,
// address space limits needs to be smaller.
constexpr size_t kAddressSpaceLimit = 0x8000000000L;  // 512 GiB
#elif V8_TARGET_ARCH_64_BIT
constexpr size_t kAddressSpaceLimit = 0x10100000000L;  // 1 TiB + 4 GiB
#else
constexpr size_t kAddressSpaceLimit = 0xC0000000;  // 3 GiB
#endif

constexpr uint64_t kOneGiB = 1024 * 1024 * 1024;
constexpr uint64_t kNegativeGuardSize = 2 * kOneGiB;
constexpr uint64_t kFullGuardSize = 10 * kOneGiB;

std::atomic<uint64_t> reserved_address_space_{0};

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

base::AddressRegion GetGuardedRegion(void* buffer_start, size_t byte_length) {
  // Guard regions always look like this:
  // |xxx(2GiB)xxx|.......(4GiB)..xxxxx|xxxxxx(4GiB)xxxxxx|
  //              ^ buffer_start
  //                              ^ byte_length
  // ^ negative guard region           ^ positive guard region

  Address start = reinterpret_cast<Address>(buffer_start);
  DCHECK_EQ(8, sizeof(size_t));  // only use on 64-bit
  DCHECK_EQ(0, start % AllocatePageSize());
  return base::AddressRegion(start - (2 * kOneGiB),
                             static_cast<size_t>(kFullGuardSize));
}

void RecordStatus(Isolate* isolate, AllocationStatus status) {
  isolate->counters()->wasm_memory_allocation_result()->AddSample(
      static_cast<int>(status));
}

inline void DebugCheckZero(void* start, size_t byte_length) {
#if DEBUG
  // Double check memory is zero-initialized.
  const byte* bytes = reinterpret_cast<const byte*>(start);
  for (size_t i = 0; i < byte_length; i++) {
    DCHECK_EQ(0, bytes[i]);
  }
#endif
}
}  // namespace

bool BackingStore::ReserveAddressSpace(uint64_t num_bytes) {
  uint64_t reservation_limit = kAddressSpaceLimit;
  while (true) {
    uint64_t old_count = reserved_address_space_.load();
    if (old_count > reservation_limit) return false;
    if (reservation_limit - old_count < num_bytes) return false;
    if (reserved_address_space_.compare_exchange_weak(old_count,
                                                      old_count + num_bytes)) {
      return true;
    }
  }
}

void BackingStore::ReleaseReservation(uint64_t num_bytes) {
  uint64_t old_reserved = reserved_address_space_.fetch_sub(num_bytes);
  USE(old_reserved);
  DCHECK_LE(num_bytes, old_reserved);
}

// The backing store for a Wasm shared memory remembers all the isolates
// with which it has been shared.
struct SharedWasmMemoryData {
  std::vector<Isolate*> isolates_;
};

void BackingStore::Clear() {
  buffer_start_ = nullptr;
  byte_length_ = 0;
  has_guard_regions_ = false;
  if (holds_shared_ptr_to_allocator_) {
    type_specific_data_.v8_api_array_buffer_allocator_shared
        .std::shared_ptr<v8::ArrayBuffer::Allocator>::~shared_ptr();
    holds_shared_ptr_to_allocator_ = false;
  }
  type_specific_data_.v8_api_array_buffer_allocator = nullptr;
}

BackingStore::~BackingStore() {
  GlobalBackingStoreRegistry::Unregister(this);

  if (buffer_start_ == nullptr) return;  // nothing to deallocate

  if (is_wasm_memory_) {
    DCHECK(free_on_destruct_);
    DCHECK(!custom_deleter_);
    TRACE_BS("BSw:free  bs=%p mem=%p (length=%zu, capacity=%zu)\n", this,
             buffer_start_, byte_length(), byte_capacity_);
    if (is_shared_) {
      // Deallocate the list of attached memory objects.
      SharedWasmMemoryData* shared_data = get_shared_wasm_memory_data();
      delete shared_data;
      type_specific_data_.shared_wasm_memory_data = nullptr;
    }

    // Wasm memories are always allocated through the page allocator.
    auto region =
        has_guard_regions_
            ? GetGuardedRegion(buffer_start_, byte_length_)
            : base::AddressRegion(reinterpret_cast<Address>(buffer_start_),
                                  byte_capacity_);
    bool pages_were_freed =
        region.size() == 0 /* no need to free any pages */ ||
        FreePages(GetPlatformPageAllocator(),
                  reinterpret_cast<void*>(region.begin()), region.size());
    CHECK(pages_were_freed);
    BackingStore::ReleaseReservation(has_guard_regions_ ? kFullGuardSize
                                                        : byte_capacity_);
    Clear();
    return;
  }
  if (custom_deleter_) {
    DCHECK(free_on_destruct_);
    TRACE_BS("BS:custome deleter bs=%p mem=%p (length=%zu, capacity=%zu)\n",
             this, buffer_start_, byte_length(), byte_capacity_);
    type_specific_data_.deleter.callback(buffer_start_, byte_length_,
                                         type_specific_data_.deleter.data);
    Clear();
    return;
  }
  if (free_on_destruct_) {
    // JSArrayBuffer backing store. Deallocate through the embedder's allocator.
    auto allocator = get_v8_api_array_buffer_allocator();
    TRACE_BS("BS:free   bs=%p mem=%p (length=%zu, capacity=%zu)\n", this,
             buffer_start_, byte_length(), byte_capacity_);
    allocator->Free(buffer_start_, byte_length_);
  }
  Clear();
}

// Allocate a backing store using the array buffer allocator from the embedder.
std::unique_ptr<BackingStore> BackingStore::Allocate(
    Isolate* isolate, size_t byte_length, SharedFlag shared,
    InitializedFlag initialized) {
  void* buffer_start = nullptr;
  auto allocator = isolate->array_buffer_allocator();
  CHECK_NOT_NULL(allocator);
  if (byte_length != 0) {
    auto counters = isolate->counters();
    int mb_length = static_cast<int>(byte_length / MB);
    if (mb_length > 0) {
      counters->array_buffer_big_allocations()->AddSample(mb_length);
    }
    if (shared == SharedFlag::kShared) {
      counters->shared_array_allocations()->AddSample(mb_length);
    }
    auto allocate_buffer = [allocator, initialized](size_t byte_length) {
      if (initialized == InitializedFlag::kUninitialized) {
        return allocator->AllocateUninitialized(byte_length);
      }
      void* buffer_start = allocator->Allocate(byte_length);
      if (buffer_start) {
        // TODO(wasm): node does not implement the zero-initialization API.
        // Reenable this debug check when node does implement it properly.
        constexpr bool
            kDebugCheckZeroDisabledDueToNodeNotImplementingZeroInitAPI = true;
        if ((!(kDebugCheckZeroDisabledDueToNodeNotImplementingZeroInitAPI)) &&
            !FLAG_mock_arraybuffer_allocator) {
          DebugCheckZero(buffer_start, byte_length);
        }
      }
      return buffer_start;
    };

    buffer_start = isolate->heap()->AllocateExternalBackingStore(
        allocate_buffer, byte_length);

    if (buffer_start == nullptr) {
      // Allocation failed.
      counters->array_buffer_new_size_failures()->AddSample(mb_length);
      return {};
    }
  }

  auto result = new BackingStore(buffer_start,  // start
                                 byte_length,   // length
                                 byte_length,   // capacity
                                 shared,        // shared
                                 false,         // is_wasm_memory
                                 true,          // free_on_destruct
                                 false,         // has_guard_regions
                                 false);        // custom_deleter

  TRACE_BS("BS:alloc  bs=%p mem=%p (length=%zu)\n", result,
           result->buffer_start(), byte_length);
  result->SetAllocatorFromIsolate(isolate);
  return std::unique_ptr<BackingStore>(result);
}

void BackingStore::SetAllocatorFromIsolate(Isolate* isolate) {
  if (auto allocator_shared = isolate->array_buffer_allocator_shared()) {
    holds_shared_ptr_to_allocator_ = true;
    new (&type_specific_data_.v8_api_array_buffer_allocator_shared)
        std::shared_ptr<v8::ArrayBuffer::Allocator>(
            std::move(allocator_shared));
  } else {
    type_specific_data_.v8_api_array_buffer_allocator =
        isolate->array_buffer_allocator();
  }
}

// Allocate a backing store for a Wasm memory. Always use the page allocator
// and add guard regions.
std::unique_ptr<BackingStore> BackingStore::TryAllocateWasmMemory(
    Isolate* isolate, size_t initial_pages, size_t maximum_pages,
    SharedFlag shared) {
  // Cannot reserve 0 pages on some OSes.
  if (maximum_pages == 0) maximum_pages = 1;

  TRACE_BS("BSw:try   %zu pages, %zu max\n", initial_pages, maximum_pages);

  bool guards = kUseGuardRegions;

  // For accounting purposes, whether a GC was necessary.
  bool did_retry = false;

  // A helper to try running a function up to 3 times, executing a GC
  // if the first and second attempts failed.
  auto gc_retry = [&](const std::function<bool()>& fn) {
    for (int i = 0; i < 3; i++) {
      if (fn()) return true;
      // Collect garbage and retry.
      did_retry = true;
      // TODO(wasm): try Heap::EagerlyFreeExternalMemory() first?
      isolate->heap()->MemoryPressureNotification(
          MemoryPressureLevel::kCritical, true);
    }
    return false;
  };

  // Compute size of reserved memory.

  size_t engine_max_pages = wasm::max_mem_pages();
  size_t byte_capacity =
      std::min(engine_max_pages, maximum_pages) * wasm::kWasmPageSize;
  size_t reservation_size =
      guards ? static_cast<size_t>(kFullGuardSize) : byte_capacity;

  //--------------------------------------------------------------------------
  // 1. Enforce maximum address space reservation per engine.
  //--------------------------------------------------------------------------
  auto reserve_memory_space = [&] {
    return BackingStore::ReserveAddressSpace(reservation_size);
  };

  if (!gc_retry(reserve_memory_space)) {
    // Crash on out-of-memory if the correctness fuzzer is running.
    if (FLAG_correctness_fuzzer_suppressions) {
      FATAL("could not allocate wasm memory backing store");
    }
    RecordStatus(isolate, AllocationStatus::kAddressSpaceLimitReachedFailure);
    TRACE_BS("BSw:try   failed to reserve address space\n");
    return {};
  }

  //--------------------------------------------------------------------------
  // 2. Allocate pages (inaccessible by default).
  //--------------------------------------------------------------------------
  void* allocation_base = nullptr;
  auto allocate_pages = [&] {
    allocation_base =
        AllocatePages(GetPlatformPageAllocator(), nullptr, reservation_size,
                      wasm::kWasmPageSize, PageAllocator::kNoAccess);
    return allocation_base != nullptr;
  };
  if (!gc_retry(allocate_pages)) {
    // Page allocator could not reserve enough pages.
    BackingStore::ReleaseReservation(reservation_size);
    RecordStatus(isolate, AllocationStatus::kOtherFailure);
    TRACE_BS("BSw:try   failed to allocate pages\n");
    return {};
  }

  // Get a pointer to the start of the buffer, skipping negative guard region
  // if necessary.
  byte* buffer_start = reinterpret_cast<byte*>(allocation_base) +
                       (guards ? kNegativeGuardSize : 0);

  //--------------------------------------------------------------------------
  // 3. Commit the initial pages (allow read/write).
  //--------------------------------------------------------------------------
  size_t byte_length = initial_pages * wasm::kWasmPageSize;
  auto commit_memory = [&] {
    return byte_length == 0 ||
           SetPermissions(GetPlatformPageAllocator(), buffer_start, byte_length,
                          PageAllocator::kReadWrite);
  };
  if (!gc_retry(commit_memory)) {
    // SetPermissions put us over the process memory limit.
    V8::FatalProcessOutOfMemory(nullptr, "BackingStore::AllocateWasmMemory()");
    TRACE_BS("BSw:try   failed to set permissions\n");
  }

  DebugCheckZero(buffer_start, byte_length);  // touch the bytes.

  RecordStatus(isolate, did_retry ? AllocationStatus::kSuccessAfterRetry
                                  : AllocationStatus::kSuccess);

  auto result = new BackingStore(buffer_start,   // start
                                 byte_length,    // length
                                 byte_capacity,  // capacity
                                 shared,         // shared
                                 true,           // is_wasm_memory
                                 true,           // free_on_destruct
                                 guards,         // has_guard_regions
                                 false);         // custom_deleter

  TRACE_BS("BSw:alloc bs=%p mem=%p (length=%zu, capacity=%zu)\n", result,
           result->buffer_start(), byte_length, byte_capacity);

  // Shared Wasm memories need an anchor for the memory object list.
  if (shared == SharedFlag::kShared) {
    result->type_specific_data_.shared_wasm_memory_data =
        new SharedWasmMemoryData();
  }

  return std::unique_ptr<BackingStore>(result);
}

// Allocate a backing store for a Wasm memory. Always use the page allocator
// and add guard regions.
std::unique_ptr<BackingStore> BackingStore::AllocateWasmMemory(
    Isolate* isolate, size_t initial_pages, size_t maximum_pages,
    SharedFlag shared) {
  // Wasm pages must be a multiple of the allocation page size.
  DCHECK_EQ(0, wasm::kWasmPageSize % AllocatePageSize());

  // Enforce engine limitation on the maximum number of pages.
  if (initial_pages > wasm::max_mem_pages()) return nullptr;

  auto backing_store =
      TryAllocateWasmMemory(isolate, initial_pages, maximum_pages, shared);
  if (!backing_store && maximum_pages > initial_pages) {
    // If reserving {maximum_pages} failed, try with maximum = initial.
    backing_store =
        TryAllocateWasmMemory(isolate, initial_pages, initial_pages, shared);
  }
  return backing_store;
}

std::unique_ptr<BackingStore> BackingStore::CopyWasmMemory(Isolate* isolate,
                                                           size_t new_pages) {
  DCHECK_GE(new_pages * wasm::kWasmPageSize, byte_length_);
  // Note that we could allocate uninitialized to save initialization cost here,
  // but since Wasm memories are allocated by the page allocator, the zeroing
  // cost is already built-in.
  // TODO(titzer): should we use a suitable maximum here?
  auto new_backing_store = BackingStore::AllocateWasmMemory(
      isolate, new_pages, new_pages,
      is_shared() ? SharedFlag::kShared : SharedFlag::kNotShared);

  if (!new_backing_store ||
      new_backing_store->has_guard_regions() != has_guard_regions_) {
    return {};
  }

  if (byte_length_ > 0) {
    memcpy(new_backing_store->buffer_start(), buffer_start_, byte_length_);
  }

  return new_backing_store;
}

// Try to grow the size of a wasm memory in place, without realloc + copy.
bool BackingStore::GrowWasmMemoryInPlace(Isolate* isolate, size_t delta_pages,
                                         size_t max_pages) {
  DCHECK(is_wasm_memory_);
  max_pages = std::min(max_pages, byte_capacity_ / wasm::kWasmPageSize);

  if (delta_pages == 0) return true;          // degenerate grow.
  if (delta_pages > max_pages) return false;  // would never work.

  // Do a compare-exchange loop, because we also need to adjust page
  // permissions. Note that multiple racing grows both try to set page
  // permissions for the entire range (to be RW), so the operating system
  // should deal with that raciness. We know we succeeded when we can
  // compare/swap the old length with the new length.
  size_t old_length = 0;
  size_t new_length = 0;
  while (true) {
    old_length = byte_length_.load(std::memory_order_acquire);
    size_t current_pages = old_length / wasm::kWasmPageSize;

    // Check if we have exceed the supplied maximum.
    if (current_pages > (max_pages - delta_pages)) return false;

    new_length = (current_pages + delta_pages) * wasm::kWasmPageSize;

    // Try to adjust the permissions on the memory.
    if (!i::SetPermissions(GetPlatformPageAllocator(), buffer_start_,
                           new_length, PageAllocator::kReadWrite)) {
      return false;
    }
    if (byte_length_.compare_exchange_weak(old_length, new_length,
                                           std::memory_order_acq_rel)) {
      // Successfully updated both the length and permissions.
      break;
    }
  }

  if (!is_shared_) {
    // Only do per-isolate accounting for non-shared backing stores.
    reinterpret_cast<v8::Isolate*>(isolate)
        ->AdjustAmountOfExternalAllocatedMemory(new_length - old_length);
  }
  return true;
}

void BackingStore::AttachSharedWasmMemoryObject(
    Isolate* isolate, Handle<WasmMemoryObject> memory_object) {
  DCHECK(is_wasm_memory_);
  DCHECK(is_shared_);
  // We need to take the global registry lock for this operation.
  GlobalBackingStoreRegistry::AddSharedWasmMemoryObject(isolate, this,
                                                        memory_object);
}

void BackingStore::BroadcastSharedWasmMemoryGrow(
    Isolate* isolate, std::shared_ptr<BackingStore> backing_store,
    size_t new_pages) {
  GlobalBackingStoreRegistry::BroadcastSharedWasmMemoryGrow(
      isolate, backing_store, new_pages);
}

void BackingStore::RemoveSharedWasmMemoryObjects(Isolate* isolate) {
  GlobalBackingStoreRegistry::Purge(isolate);
}

void BackingStore::UpdateSharedWasmMemoryObjects(Isolate* isolate) {
  GlobalBackingStoreRegistry::UpdateSharedWasmMemoryObjects(isolate);
}

std::unique_ptr<BackingStore> BackingStore::WrapAllocation(
    Isolate* isolate, void* allocation_base, size_t allocation_length,
    SharedFlag shared, bool free_on_destruct) {
  auto result = new BackingStore(allocation_base,    // start
                                 allocation_length,  // length
                                 allocation_length,  // capacity
                                 shared,             // shared
                                 false,              // is_wasm_memory
                                 free_on_destruct,   // free_on_destruct
                                 false,              // has_guard_regions
                                 false);             // custom_deleter
  result->SetAllocatorFromIsolate(isolate);
  TRACE_BS("BS:wrap   bs=%p mem=%p (length=%zu)\n", result,
           result->buffer_start(), result->byte_length());
  return std::unique_ptr<BackingStore>(result);
}

std::unique_ptr<BackingStore> BackingStore::WrapAllocation(
    void* allocation_base, size_t allocation_length,
    v8::BackingStoreDeleterCallback deleter, void* deleter_data,
    SharedFlag shared) {
  auto result = new BackingStore(allocation_base,    // start
                                 allocation_length,  // length
                                 allocation_length,  // capacity
                                 shared,             // shared
                                 false,              // is_wasm_memory
                                 true,               // free_on_destruct
                                 false,              // has_guard_regions
                                 true);              // custom_deleter
  result->type_specific_data_.deleter = {deleter, deleter_data};
  TRACE_BS("BS:wrap   bs=%p mem=%p (length=%zu)\n", result,
           result->buffer_start(), result->byte_length());
  return std::unique_ptr<BackingStore>(result);
}

std::unique_ptr<BackingStore> BackingStore::EmptyBackingStore(
    SharedFlag shared) {
  auto result = new BackingStore(nullptr,  // start
                                 0,        // length
                                 0,        // capacity
                                 shared,   // shared
                                 false,    // is_wasm_memory
                                 false,    // free_on_destruct
                                 false,    // has_guard_regions
                                 false);   // custom_deleter

  return std::unique_ptr<BackingStore>(result);
}

v8::ArrayBuffer::Allocator* BackingStore::get_v8_api_array_buffer_allocator() {
  CHECK(!is_wasm_memory_);
  auto array_buffer_allocator =
      holds_shared_ptr_to_allocator_
          ? type_specific_data_.v8_api_array_buffer_allocator_shared.get()
          : type_specific_data_.v8_api_array_buffer_allocator;
  CHECK_NOT_NULL(array_buffer_allocator);
  return array_buffer_allocator;
}

SharedWasmMemoryData* BackingStore::get_shared_wasm_memory_data() {
  CHECK(is_wasm_memory_ && is_shared_);
  auto shared_wasm_memory_data = type_specific_data_.shared_wasm_memory_data;
  CHECK(shared_wasm_memory_data);
  return shared_wasm_memory_data;
}

namespace {
// Implementation details of GlobalBackingStoreRegistry.
struct GlobalBackingStoreRegistryImpl {
  GlobalBackingStoreRegistryImpl() {}
  base::Mutex mutex_;
  std::unordered_map<const void*, std::weak_ptr<BackingStore>> map_;
};
base::LazyInstance<GlobalBackingStoreRegistryImpl>::type global_registry_impl_ =
    LAZY_INSTANCE_INITIALIZER;
inline GlobalBackingStoreRegistryImpl* impl() {
  return global_registry_impl_.Pointer();
}
}  // namespace

void GlobalBackingStoreRegistry::Register(
    std::shared_ptr<BackingStore> backing_store) {
  if (!backing_store || !backing_store->buffer_start()) return;

  if (!backing_store->free_on_destruct()) {
    // If the backing store buffer is managed by the embedder,
    // then we don't have to guarantee that there is single unique
    // BackingStore per buffer_start() because the destructor of
    // of the BackingStore will be a no-op in that case.

    // All WASM memory has to be registered.
    CHECK(!backing_store->is_wasm_memory());
    return;
  }

  base::MutexGuard scope_lock(&impl()->mutex_);
  if (backing_store->globally_registered_) return;
  TRACE_BS("BS:reg    bs=%p mem=%p (length=%zu, capacity=%zu)\n",
           backing_store.get(), backing_store->buffer_start(),
           backing_store->byte_length(), backing_store->byte_capacity());
  std::weak_ptr<BackingStore> weak = backing_store;
  auto result = impl()->map_.insert({backing_store->buffer_start(), weak});
  CHECK(result.second);
  backing_store->globally_registered_ = true;
}

void GlobalBackingStoreRegistry::Unregister(BackingStore* backing_store) {
  if (!backing_store->globally_registered_) return;

  DCHECK_NOT_NULL(backing_store->buffer_start());

  base::MutexGuard scope_lock(&impl()->mutex_);
  const auto& result = impl()->map_.find(backing_store->buffer_start());
  if (result != impl()->map_.end()) {
    DCHECK(!result->second.lock());
    impl()->map_.erase(result);
  }
  backing_store->globally_registered_ = false;
}

std::shared_ptr<BackingStore> GlobalBackingStoreRegistry::Lookup(
    void* buffer_start, size_t length) {
  base::MutexGuard scope_lock(&impl()->mutex_);
  TRACE_BS("BS:lookup   mem=%p (%zu bytes)\n", buffer_start, length);
  const auto& result = impl()->map_.find(buffer_start);
  if (result == impl()->map_.end()) {
    return std::shared_ptr<BackingStore>();
  }
  auto backing_store = result->second.lock();
  CHECK_EQ(buffer_start, backing_store->buffer_start());
  if (backing_store->is_wasm_memory()) {
    // Grow calls to shared WebAssembly threads can be triggered from different
    // workers, length equality cannot be guaranteed here.
    CHECK_LE(length, backing_store->byte_length());
  } else {
    CHECK_EQ(length, backing_store->byte_length());
  }
  return backing_store;
}

void GlobalBackingStoreRegistry::Purge(Isolate* isolate) {
  // We need to keep a reference to all backing stores that are inspected
  // in the purging loop below. Otherwise, we might get a deadlock
  // if the temporary backing store reference created in the loop is
  // the last reference. In that case the destructor of the backing store
  // may try to take the &impl()->mutex_ in order to unregister itself.
  std::vector<std::shared_ptr<BackingStore>> prevent_destruction_under_lock;
  base::MutexGuard scope_lock(&impl()->mutex_);
  // Purge all entries in the map that refer to the given isolate.
  for (auto& entry : impl()->map_) {
    auto backing_store = entry.second.lock();
    prevent_destruction_under_lock.emplace_back(backing_store);
    if (!backing_store) continue;  // skip entries where weak ptr is null
    if (!backing_store->is_wasm_memory()) continue;  // skip non-wasm memory
    if (!backing_store->is_shared()) continue;       // skip non-shared memory
    SharedWasmMemoryData* shared_data =
        backing_store->get_shared_wasm_memory_data();
    // Remove this isolate from the isolates list.
    auto& isolates = shared_data->isolates_;
    for (size_t i = 0; i < isolates.size(); i++) {
      if (isolates[i] == isolate) isolates[i] = nullptr;
    }
  }
}

void GlobalBackingStoreRegistry::AddSharedWasmMemoryObject(
    Isolate* isolate, BackingStore* backing_store,
    Handle<WasmMemoryObject> memory_object) {
  // Add to the weak array list of shared memory objects in the isolate.
  isolate->AddSharedWasmMemory(memory_object);

  // Add the isolate to the list of isolates sharing this backing store.
  base::MutexGuard scope_lock(&impl()->mutex_);
  SharedWasmMemoryData* shared_data =
      backing_store->get_shared_wasm_memory_data();
  auto& isolates = shared_data->isolates_;
  int free_entry = -1;
  for (size_t i = 0; i < isolates.size(); i++) {
    if (isolates[i] == isolate) return;
    if (isolates[i] == nullptr) free_entry = static_cast<int>(i);
  }
  if (free_entry >= 0)
    isolates[free_entry] = isolate;
  else
    isolates.push_back(isolate);
}

void GlobalBackingStoreRegistry::BroadcastSharedWasmMemoryGrow(
    Isolate* isolate, std::shared_ptr<BackingStore> backing_store,
    size_t new_pages) {
  {
    // The global lock protects the list of isolates per backing store.
    base::MutexGuard scope_lock(&impl()->mutex_);
    SharedWasmMemoryData* shared_data =
        backing_store->get_shared_wasm_memory_data();
    for (Isolate* other : shared_data->isolates_) {
      if (other && other != isolate) {
        other->stack_guard()->RequestGrowSharedMemory();
      }
    }
  }
  // Update memory objects in this isolate.
  UpdateSharedWasmMemoryObjects(isolate);
}

void GlobalBackingStoreRegistry::UpdateSharedWasmMemoryObjects(
    Isolate* isolate) {
  HandleScope scope(isolate);
  Handle<WeakArrayList> shared_wasm_memories =
      isolate->factory()->shared_wasm_memories();

  for (int i = 0; i < shared_wasm_memories->length(); i++) {
    HeapObject obj;
    if (!shared_wasm_memories->Get(i).GetHeapObject(&obj)) continue;

    Handle<WasmMemoryObject> memory_object(WasmMemoryObject::cast(obj),
                                           isolate);
    Handle<JSArrayBuffer> old_buffer(memory_object->array_buffer(), isolate);
    std::shared_ptr<BackingStore> backing_store = old_buffer->GetBackingStore();

    if (old_buffer->byte_length() != backing_store->byte_length()) {
      Handle<JSArrayBuffer> new_buffer =
          isolate->factory()->NewJSSharedArrayBuffer(std::move(backing_store));
      memory_object->update_instances(isolate, new_buffer);
    }
  }
}

}  // namespace internal
}  // namespace v8

#undef TRACE_BS
