// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MEMORY_ALLOCATOR_H_
#define V8_HEAP_MEMORY_ALLOCATOR_H_

#include <atomic>
#include <memory>
#include <optional>
#include <set>
#include <unordered_set>

#include "include/v8-platform.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"
#include "src/heap/large-page-metadata.h"
#include "src/heap/memory-chunk-metadata.h"
#include "src/heap/mutable-page-metadata.h"
#include "src/heap/spaces.h"
#include "src/tasks/cancelable-task.h"
#include "src/utils/allocation.h"

namespace v8::internal {

namespace heap {
class TestMemoryAllocatorScope;
}  // namespace heap

class Heap;
class Isolate;
class MemoryPool;
class ReadOnlyPageMetadata;

// `MemoryAllocator` provides infrastructure to allocate and release pages of
// different kinds. It sits between higher level V8 spaces and
// the OS or a memory pool.  It is designed to be Isolate-local which means that
// pages never get reused across Isolates.
class MemoryAllocator final {
 public:
  enum class AllocationMode {
    // Regular allocation path. Does not use pooled or delayed pages.
    kRegular,
    // Tries allocating a page from delayed and pooled pages before allocating a
    // new page.
    kTryDelayedAndPooled,
  };

  enum class FreeMode {
    // Frees page immediately on the main thread.
    kImmediately,
    // Allows the page to be pooled. Pooling may fail for various reasons. E.g.,
    // the page pool may be disabled or some pooling quotas may be reached.
    kPool,
    // Requires that freeing the page is delayed until `ReleaseDelayedPages()`
    // is called. Such pages can be reused for allocation but since they are not
    // shared globally the caller is guaranteed that the pages stays around
    // temporarily. This is necessary for e.g. pointers updating that needs
    // freed pages to still stay around.
    kDelayThenRelease,
    // Same as `kDelayThenRelease` with the difference that pages are pooled
    // instead of released after delaying.
    kDelayThenPool,
  };

  // Initialize page sizes field in V8::Initialize.
  static void InitializeOncePerProcess();

  V8_INLINE static intptr_t GetCommitPageSize() {
    DCHECK_LT(0, commit_page_size_);
    return commit_page_size_;
  }

  V8_INLINE static intptr_t GetCommitPageSizeBits() {
    DCHECK_LT(0, commit_page_size_bits_);
    return commit_page_size_bits_;
  }

  static void DeleteMemoryChunk(MutablePageMetadata* metadata);

  V8_EXPORT_PRIVATE MemoryAllocator(Isolate* isolate,
                                    v8::PageAllocator* code_page_allocator,
                                    v8::PageAllocator* trusted_page_allocator,
                                    MemoryPool* page_pool, size_t max_capacity);

  MemoryAllocator(const MemoryAllocator&) = delete;
  MemoryAllocator& operator=(const MemoryAllocator&) = delete;

  V8_EXPORT_PRIVATE void TearDown();

  // Allocates a Page from the allocator. `AllocationMode` is used to indicate
  // whether pooled allocation, which only works for pages of size
  // `kRegularPageSize`.
  V8_EXPORT_PRIVATE PageMetadata* AllocatePage(
      MemoryAllocator::AllocationMode alloc_mode, Space* space,
      Executability executable);

  V8_EXPORT_PRIVATE LargePageMetadata* AllocateLargePage(
      LargeObjectSpace* space, size_t object_size, Executability executable,
      AllocationHint hint);

  bool ResizeLargePage(LargePageMetadata* page, size_t old_object_size,
                       size_t new_object_size);

  ReadOnlyPageMetadata* AllocateReadOnlyPage(ReadOnlySpace* space,
                                             Address hint = kNullAddress);

  std::unique_ptr<::v8::PageAllocator::SharedMemoryMapping> RemapSharedPage(
      ::v8::PageAllocator::SharedMemory* shared_memory, Address new_address);

  V8_EXPORT_PRIVATE void Free(MemoryAllocator::FreeMode mode,
                              MutablePageMetadata* page_metadata);
  void FreeReadOnlyPage(ReadOnlyPageMetadata* chunk);

  void ReleaseDelayedPages();

  // Returns allocated spaces in bytes.
  size_t Size() const { return size_; }

  // Returns allocated executable spaces in bytes.
  size_t SizeExecutable() const { return size_executable_; }

  // Returns the maximum available bytes of heaps.
  size_t Available() const {
    const size_t size = Size();
    return capacity_ < size ? 0 : capacity_ - size;
  }

  // Returns an indication of whether a pointer is in a space that has
  // been allocated by this MemoryAllocator. It is conservative, allowing
  // false negatives (i.e., if a pointer is outside the allocated space, it may
  // return false) but not false positives (i.e., if a pointer is inside the
  // allocated space, it will definitely return false).
  V8_INLINE bool IsOutsideAllocatedSpace(Address address) const {
    return IsOutsideAllocatedSpace(address, NOT_EXECUTABLE) &&
           IsOutsideAllocatedSpace(address, EXECUTABLE);
  }
  V8_INLINE bool IsOutsideAllocatedSpace(Address address,
                                         Executability executable) const {
    switch (executable) {
      case NOT_EXECUTABLE:
        return address < lowest_not_executable_ever_allocated_ ||
               address >= highest_not_executable_ever_allocated_;
      case EXECUTABLE:
        return address < lowest_executable_ever_allocated_ ||
               address >= highest_executable_ever_allocated_;
    }
  }

  // Partially release |bytes_to_free| bytes starting at |start_free|. Note that
  // internally memory is freed from |start_free| to the end of the reservation.
  // Additional memory beyond the page is not accounted though, so
  // |bytes_to_free| is computed by the caller.
  void PartialFreeMemory(MemoryChunkMetadata* chunk, Address start_free,
                         size_t bytes_to_free, Address new_area_end);

  void UnregisterReadOnlyPage(ReadOnlyPageMetadata* page);

  // Page allocator instance for allocating non-executable pages.
  // Guaranteed to be a valid pointer.
  v8::PageAllocator* data_page_allocator() const {
    return data_page_allocator_;
  }

  // Page allocator instance for allocating executable pages.
  // Guaranteed to be a valid pointer.
  v8::PageAllocator* code_page_allocator() const {
    return code_page_allocator_;
  }

  // Page allocator instance for allocating "trusted" pages. When the sandbox is
  // enabled, these pages are guaranteed to be allocated outside of the sandbox,
  // so their content cannot be corrupted by an attacker.
  // Guaranteed to be a valid pointer.
  v8::PageAllocator* trusted_page_allocator() const {
    return trusted_page_allocator_;
  }

  // Returns page allocator suitable for allocating pages for the given space.
  v8::PageAllocator* page_allocator(AllocationSpace space) const {
    switch (space) {
      case RO_SPACE:
        return read_only_page_allocator_;
      case CODE_SPACE:
      case CODE_LO_SPACE:
        return code_page_allocator_;
      case TRUSTED_SPACE:
      case SHARED_TRUSTED_SPACE:
      case TRUSTED_LO_SPACE:
      case SHARED_TRUSTED_LO_SPACE:
        return trusted_page_allocator_;
      case NEW_SPACE:
      case NEW_LO_SPACE:
      case OLD_SPACE:
      case LO_SPACE:
      case SHARED_SPACE:
      case SHARED_LO_SPACE:
        return data_page_allocator_;
    }
  }

  // Return the normal or large page that contains this address, if it is owned
  // by this heap, otherwise a nullptr.
  V8_EXPORT_PRIVATE const MemoryChunk* LookupChunkContainingAddress(
      Address addr) const;
  // This version can be used when all threads are either parked or in a
  // safepoint. In that case we can skip taking a mutex.
  V8_EXPORT_PRIVATE const MemoryChunk* LookupChunkContainingAddressInSafepoint(
      Address addr) const;

  // Returns the number of cached chunks for this isolate.
  V8_EXPORT_PRIVATE size_t GetPooledChunksCount();

  // Returns the number of shared cached chunks.
  V8_EXPORT_PRIVATE size_t GetSharedPooledChunksCount();

  // Returns the number of total cached chunks (including cached pages of other
  // isolates).
  V8_EXPORT_PRIVATE size_t GetTotalPooledChunksCount();

  // Releases all pooled chunks for this isolate immediately.
  V8_EXPORT_PRIVATE void ReleasePooledChunksImmediately();

#ifdef DEBUG
  // Checks if an allocated MemoryChunk was intended to be used for executable
  // memory.
  bool IsMemoryChunkExecutable(MutablePageMetadata* chunk) const {
    base::MutexGuard guard(&executable_memory_mutex_);
    return executable_memory_.find(chunk) != executable_memory_.end();
  }
#endif  // DEBUG

 private:
  // Used to store all data about MemoryChunk allocation, e.g. in
  // AllocateUninitializedChunk.
  struct MemoryChunkAllocationResult {
    void* chunk;
    // If we reuse a pooled chunk return the metadata allocation here to be
    // reused.
    void* optional_metadata;
    size_t size;
    size_t area_start;
    size_t area_end;
    VirtualMemory reservation;
  };

  // Computes the size of a MemoryChunk from the size of the object_area.
  static size_t ComputeChunkSize(size_t area_size, AllocationSpace space);

  MemoryPool* memory_pool() { return pool_; }

  // Internal allocation method for all pages/memory chunks. Returns data about
  // the uninitialized memory region.
  V8_WARN_UNUSED_RESULT std::optional<MemoryChunkAllocationResult>
  AllocateUninitializedChunk(BaseSpace* space, size_t area_size,
                             Executability executable, PageSize page_size,
                             AllocationHint hint) {
    return AllocateUninitializedChunkAt(space, area_size, executable,
                                        kNullAddress, page_size, hint);
  }
  V8_WARN_UNUSED_RESULT std::optional<MemoryChunkAllocationResult>
  AllocateUninitializedChunkAt(BaseSpace* space, size_t area_size,
                               Executability executable, Address hint,
                               PageSize page_size,
                               AllocationHint allocation_hint);

  // Internal raw allocation method that allocates an aligned MemoryChunk and
  // sets the right memory permissions.
  Address AllocateAlignedMemory(size_t chunk_size, size_t area_size,
                                size_t alignment, AllocationSpace space,
                                Executability executable, void* hint,
                                VirtualMemory* controller, PageSize page_size,
                                AllocationHint allocation_hint);

  // Sets memory permissions on executable memory chunks. This entails page
  // header (RW), guard pages (no access) and the object area (code modification
  // permissions).
  V8_WARN_UNUSED_RESULT bool SetPermissionsOnExecutableMemoryChunk(
      VirtualMemory* vm, Address start, size_t reserved_size);

  // Frees the given memory region.
  void FreeMemoryRegion(v8::PageAllocator* page_allocator, Address addr,
                        size_t size);

  // PreFreeMemory logically frees the object, i.e., it unregisters the
  // memory, logs a delete event and adds the chunk to remembered unmapped
  // pages.
  void PreFreeMemory(MutablePageMetadata* chunk);

  // PerformFreeMemory can be called concurrently when PreFree was executed
  // before.
  void PerformFreeMemory(MutablePageMetadata* chunk);

  // See AllocatePage for public interface. Note that currently we only
  // support pools for NOT_EXECUTABLE pages of size MemoryChunk::kPageSize.
  std::optional<MemoryChunkAllocationResult>
  AllocateUninitializedPageFromDelayedOrPool(Space* space);

  std::optional<MemoryChunkAllocationResult>
  TryAllocateUninitializedLargePageFromPool(Space* space, size_t chunk_size);

  // Initializes pages in a chunk. Returns the first page address.
  // This function and GetChunkId() are provided for the mark-compact
  // collector to rebuild page headers in the from space, which is
  // used as a marking stack and its page headers are destroyed.
  PageMetadata* InitializePagesInChunk(int chunk_id, int pages_in_chunk,
                                       PagedSpace* space);

  void UpdateAllocatedSpaceLimits(Address low, Address high,
                                  Executability executable);

  // Insert and remove normal and large pages that are owned by this allocator.
  void RecordMemoryChunkCreated(const MemoryChunkMetadata* metadata);
  void RecordMemoryChunkDestroyed(const MemoryChunkMetadata* metadata);

  // Performs all necessary bookkeeping to free the memory, but does not free
  // it.
  void UnregisterMutableMemoryChunk(MutablePageMetadata* chunk);
  void UnregisterSharedMemoryChunk(MemoryChunkMetadata* chunk);
  void UnregisterMemoryChunk(MemoryChunkMetadata* chunk);

  void RegisterReadOnlyMemory(ReadOnlyPageMetadata* page);

#ifdef DEBUG
  void RegisterExecutableMemoryChunk(MutablePageMetadata* chunk);
  void UnregisterExecutableMemoryChunk(MutablePageMetadata* chunk);
#endif  // DEBUG

  Isolate* isolate_;

  // Page allocator used for allocating data pages. Depending on the
  // configuration it may be a page allocator instance provided by v8::Platform
  // or a BoundedPageAllocator (when pointer compression is enabled).
  v8::PageAllocator* data_page_allocator_;

  // Allocator for read-only pages.
  v8::PageAllocator* read_only_page_allocator_;

  // Page allocator used for allocating code pages. Depending on the
  // configuration it may be a page allocator instance provided by v8::Platform
  // or a BoundedPageAllocator from Heap::code_range_ (when pointer compression
  // is enabled or on those 64-bit architectures where pc-relative 32-bit
  // displacement can be used for call and jump instructions).
  v8::PageAllocator* code_page_allocator_;

  // Page allocator used for allocating trusted pages. When the sandbox is
  // enabled, trusted pages are allocated outside of the sandbox so that their
  // content cannot be corrupted by an attacker. When the sandbox is disabled,
  // this is the same as data_page_allocator_.
  v8::PageAllocator* trusted_page_allocator_;

  // Maximum space size in bytes.
  size_t capacity_;

  // Allocated space size in bytes.
  std::atomic<size_t> size_ = 0;
  // Allocated executable space size in bytes.
  std::atomic<size_t> size_executable_ = 0;

  // We keep the lowest and highest addresses allocated as a quick way
  // of determining that pointers are outside the heap. The estimate is
  // conservative, i.e. not all addresses in 'allocated' space are allocated
  // to our heap. The range is [lowest, highest[, inclusive on the low end
  // and exclusive on the high end. Addresses are distinguished between
  // executable and not-executable, as they may generally be placed in distinct
  // areas of the heap.
  std::atomic<Address> lowest_not_executable_ever_allocated_{
      static_cast<Address>(-1ll)};
  std::atomic<Address> highest_not_executable_ever_allocated_{kNullAddress};
  std::atomic<Address> lowest_executable_ever_allocated_{
      static_cast<Address>(-1ll)};
  std::atomic<Address> highest_executable_ever_allocated_{kNullAddress};

  std::optional<VirtualMemory> reserved_chunk_at_virtual_memory_limit_;
  MemoryPool* pool_;

#ifdef DEBUG
  // Data structure to remember allocated executable memory chunks.
  // This data structure is used only in DCHECKs.
  std::unordered_set<MutablePageMetadata*, base::hash<MutablePageMetadata*>>
      executable_memory_;
  mutable base::Mutex executable_memory_mutex_;
#endif  // DEBUG

  // Allocated normal and large pages are stored here, to be used during
  // conservative stack scanning.
  std::unordered_set<const MemoryChunk*, base::hash<const MemoryChunk*>>
      normal_pages_;
  std::set<const MemoryChunk*> large_pages_;

  mutable base::Mutex chunks_mutex_;

  // Delayed pages that can be returned with `ReleaseDelayedPages()`. Until
  // returned they are kept locally to this allocator essentially making them
  // private to this Isolate. This is useful for callers that want to eventually
  // free memory but want to keep the memory Isolate-local until it is returned.
  //
  // Set of regular pages that are delayed then pool. Delayed pages can be
  // immediately reused for allocations.
  std::vector<PageMetadata*> delayed_then_pooled_pages_;
  // Set of large delayed then pooled pages. Delayed pages cannot be reused (not
  // implemented).
  std::vector<LargePageMetadata*> delayed_then_pooled_large_pages_;
  // Set of delayed then released pages. No reuse is possible here.
  std::vector<MutablePageMetadata*> delayed_then_released_pages_;

  V8_EXPORT_PRIVATE static size_t commit_page_size_;
  V8_EXPORT_PRIVATE static size_t commit_page_size_bits_;

  friend class heap::TestCodePageAllocatorScope;
  friend class heap::TestMemoryAllocatorScope;
};

}  // namespace v8::internal

#endif  // V8_HEAP_MEMORY_ALLOCATOR_H_
