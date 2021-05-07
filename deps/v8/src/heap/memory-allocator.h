// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_MEMORY_ALLOCATOR_H_
#define V8_HEAP_MEMORY_ALLOCATOR_H_

#include <atomic>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "include/v8-platform.h"
#include "src/base/bounded-page-allocator.h"
#include "src/base/export-template.h"
#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/base/platform/semaphore.h"
#include "src/heap/heap.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/spaces.h"
#include "src/tasks/cancelable-task.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

class Heap;
class Isolate;
class ReadOnlyPage;

// The process-wide singleton that keeps track of code range regions with the
// intention to reuse free code range regions as a workaround for CFG memory
// leaks (see crbug.com/870054).
class CodeRangeAddressHint {
 public:
  // Returns the most recently freed code range start address for the given
  // size. If there is no such entry, then a random address is returned.
  V8_EXPORT_PRIVATE Address GetAddressHint(size_t code_range_size);

  V8_EXPORT_PRIVATE void NotifyFreedCodeRange(Address code_range_start,
                                              size_t code_range_size);

 private:
  base::Mutex mutex_;
  // A map from code range size to an array of recently freed code range
  // addresses. There should be O(1) different code range sizes.
  // The length of each array is limited by the peak number of code ranges,
  // which should be also O(1).
  std::unordered_map<size_t, std::vector<Address>> recently_freed_;
};

// ----------------------------------------------------------------------------
// A space acquires chunks of memory from the operating system. The memory
// allocator allocates and deallocates pages for the paged heap spaces and large
// pages for large object space.
class MemoryAllocator {
 public:
  // Unmapper takes care of concurrently unmapping and uncommitting memory
  // chunks.
  class Unmapper {
   public:
    class UnmapFreeMemoryJob;

    Unmapper(Heap* heap, MemoryAllocator* allocator)
        : heap_(heap), allocator_(allocator) {
      chunks_[kRegular].reserve(kReservedQueueingSlots);
      chunks_[kPooled].reserve(kReservedQueueingSlots);
    }

    void AddMemoryChunkSafe(MemoryChunk* chunk) {
      if (!chunk->IsLargePage() && chunk->executable() != EXECUTABLE) {
        AddMemoryChunkSafe<kRegular>(chunk);
      } else {
        AddMemoryChunkSafe<kNonRegular>(chunk);
      }
    }

    MemoryChunk* TryGetPooledMemoryChunkSafe() {
      // Procedure:
      // (1) Try to get a chunk that was declared as pooled and already has
      // been uncommitted.
      // (2) Try to steal any memory chunk of kPageSize that would've been
      // unmapped.
      MemoryChunk* chunk = GetMemoryChunkSafe<kPooled>();
      if (chunk == nullptr) {
        chunk = GetMemoryChunkSafe<kRegular>();
        if (chunk != nullptr) {
          // For stolen chunks we need to manually free any allocated memory.
          chunk->ReleaseAllAllocatedMemory();
        }
      }
      return chunk;
    }

    V8_EXPORT_PRIVATE void FreeQueuedChunks();
    void CancelAndWaitForPendingTasks();
    void PrepareForGC();
    V8_EXPORT_PRIVATE void EnsureUnmappingCompleted();
    V8_EXPORT_PRIVATE void TearDown();
    size_t NumberOfCommittedChunks();
    V8_EXPORT_PRIVATE int NumberOfChunks();
    size_t CommittedBufferedMemory();

   private:
    static const int kReservedQueueingSlots = 64;
    static const int kMaxUnmapperTasks = 4;

    enum ChunkQueueType {
      kRegular,     // Pages of kPageSize that do not live in a CodeRange and
                    // can thus be used for stealing.
      kNonRegular,  // Large chunks and executable chunks.
      kPooled,      // Pooled chunks, already uncommited and ready for reuse.
      kNumberOfChunkQueues,
    };

    enum class FreeMode {
      kUncommitPooled,
      kReleasePooled,
    };

    template <ChunkQueueType type>
    void AddMemoryChunkSafe(MemoryChunk* chunk) {
      base::MutexGuard guard(&mutex_);
      chunks_[type].push_back(chunk);
    }

    template <ChunkQueueType type>
    MemoryChunk* GetMemoryChunkSafe() {
      base::MutexGuard guard(&mutex_);
      if (chunks_[type].empty()) return nullptr;
      MemoryChunk* chunk = chunks_[type].back();
      chunks_[type].pop_back();
      return chunk;
    }

    bool MakeRoomForNewTasks();

    template <FreeMode mode>
    void PerformFreeMemoryOnQueuedChunks(JobDelegate* delegate = nullptr);

    void PerformFreeMemoryOnQueuedNonRegularChunks(
        JobDelegate* delegate = nullptr);

    Heap* const heap_;
    MemoryAllocator* const allocator_;
    base::Mutex mutex_;
    std::vector<MemoryChunk*> chunks_[kNumberOfChunkQueues];
    std::unique_ptr<v8::JobHandle> job_handle_;

    friend class MemoryAllocator;
  };

  enum AllocationMode {
    kRegular,
    kPooled,
  };

  enum FreeMode {
    kFull,
    kAlreadyPooled,
    kPreFreeAndQueue,
    kPooledAndQueue,
  };

  V8_EXPORT_PRIVATE static intptr_t GetCommitPageSize();

  // Computes the memory area of discardable memory within a given memory area
  // [addr, addr+size) and returns the result as base::AddressRegion. If the
  // memory is not discardable base::AddressRegion is an empty region.
  V8_EXPORT_PRIVATE static base::AddressRegion ComputeDiscardMemoryArea(
      Address addr, size_t size);

  V8_EXPORT_PRIVATE MemoryAllocator(Isolate* isolate, size_t max_capacity,
                                    size_t code_range_size);

  V8_EXPORT_PRIVATE void TearDown();

  // Allocates a Page from the allocator. AllocationMode is used to indicate
  // whether pooled allocation, which only works for MemoryChunk::kPageSize,
  // should be tried first.
  template <MemoryAllocator::AllocationMode alloc_mode = kRegular,
            typename SpaceType>
  EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
  Page* AllocatePage(size_t size, SpaceType* owner, Executability executable);

  LargePage* AllocateLargePage(size_t size, LargeObjectSpace* owner,
                               Executability executable);

  ReadOnlyPage* AllocateReadOnlyPage(size_t size, ReadOnlySpace* owner);

  std::unique_ptr<::v8::PageAllocator::SharedMemoryMapping> RemapSharedPage(
      ::v8::PageAllocator::SharedMemory* shared_memory, Address new_address);

  template <MemoryAllocator::FreeMode mode = kFull>
  EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
  void Free(MemoryChunk* chunk);
  void FreeReadOnlyPage(ReadOnlyPage* chunk);

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
  // been allocated by this MemoryAllocator.
  V8_INLINE bool IsOutsideAllocatedSpace(Address address) const {
    return address < lowest_ever_allocated_ ||
           address >= highest_ever_allocated_;
  }

  // Returns a BasicMemoryChunk in which the memory region from commit_area_size
  // to reserve_area_size of the chunk area is reserved but not committed, it
  // could be committed later by calling MemoryChunk::CommitArea.
  V8_EXPORT_PRIVATE BasicMemoryChunk* AllocateBasicChunk(
      size_t reserve_area_size, size_t commit_area_size,
      Executability executable, BaseSpace* space);

  // Returns a MemoryChunk in which the memory region from commit_area_size to
  // reserve_area_size of the chunk area is reserved but not committed, it
  // could be committed later by calling MemoryChunk::CommitArea.
  V8_EXPORT_PRIVATE MemoryChunk* AllocateChunk(size_t reserve_area_size,
                                               size_t commit_area_size,
                                               Executability executable,
                                               BaseSpace* space);

  Address AllocateAlignedMemory(size_t reserve_size, size_t commit_size,
                                size_t alignment, Executability executable,
                                void* hint, VirtualMemory* controller);

  void FreeMemory(v8::PageAllocator* page_allocator, Address addr, size_t size);

  // Partially release |bytes_to_free| bytes starting at |start_free|. Note that
  // internally memory is freed from |start_free| to the end of the reservation.
  // Additional memory beyond the page is not accounted though, so
  // |bytes_to_free| is computed by the caller.
  void PartialFreeMemory(BasicMemoryChunk* chunk, Address start_free,
                         size_t bytes_to_free, Address new_area_end);

  // Checks if an allocated MemoryChunk was intended to be used for executable
  // memory.
  bool IsMemoryChunkExecutable(MemoryChunk* chunk) {
    return executable_memory_.find(chunk) != executable_memory_.end();
  }

  // Commit memory region owned by given reservation object.  Returns true if
  // it succeeded and false otherwise.
  bool CommitMemory(VirtualMemory* reservation);

  // Uncommit memory region owned by given reservation object. Returns true if
  // it succeeded and false otherwise.
  bool UncommitMemory(VirtualMemory* reservation);

  // Zaps a contiguous block of memory [start..(start+size)[ with
  // a given zap value.
  void ZapBlock(Address start, size_t size, uintptr_t zap_value);

  V8_WARN_UNUSED_RESULT bool CommitExecutableMemory(VirtualMemory* vm,
                                                    Address start,
                                                    size_t commit_size,
                                                    size_t reserved_size);

  // Page allocator instance for allocating non-executable pages.
  // Guaranteed to be a valid pointer.
  v8::PageAllocator* data_page_allocator() { return data_page_allocator_; }

  // Page allocator instance for allocating executable pages.
  // Guaranteed to be a valid pointer.
  v8::PageAllocator* code_page_allocator() { return code_page_allocator_; }

  // Returns page allocator suitable for allocating pages with requested
  // executability.
  v8::PageAllocator* page_allocator(Executability executable) {
    return executable == EXECUTABLE ? code_page_allocator_
                                    : data_page_allocator_;
  }

  // A region of memory that may contain executable code including reserved
  // OS page with read-write access in the beginning.
  const base::AddressRegion& code_range() const {
    // |code_range_| >= |optional RW pages| + |code_page_allocator_instance_|
    DCHECK_IMPLIES(!code_range_.is_empty(), code_page_allocator_instance_);
    DCHECK_IMPLIES(!code_range_.is_empty(),
                   code_range_.contains(code_page_allocator_instance_->begin(),
                                        code_page_allocator_instance_->size()));
    return code_range_;
  }

  Unmapper* unmapper() { return &unmapper_; }

  // Performs all necessary bookkeeping to free the memory, but does not free
  // it.
  void UnregisterMemory(MemoryChunk* chunk);
  void UnregisterMemory(BasicMemoryChunk* chunk,
                        Executability executable = NOT_EXECUTABLE);
  void UnregisterSharedMemory(BasicMemoryChunk* chunk);

  void RegisterReadOnlyMemory(ReadOnlyPage* page);

 private:
  void InitializeCodePageAllocator(v8::PageAllocator* page_allocator,
                                   size_t requested);

  // PreFreeMemory logically frees the object, i.e., it unregisters the
  // memory, logs a delete event and adds the chunk to remembered unmapped
  // pages.
  void PreFreeMemory(MemoryChunk* chunk);

  // PerformFreeMemory can be called concurrently when PreFree was executed
  // before.
  void PerformFreeMemory(MemoryChunk* chunk);

  // See AllocatePage for public interface. Note that currently we only
  // support pools for NOT_EXECUTABLE pages of size MemoryChunk::kPageSize.
  template <typename SpaceType>
  MemoryChunk* AllocatePagePooled(SpaceType* owner);

  // Initializes pages in a chunk. Returns the first page address.
  // This function and GetChunkId() are provided for the mark-compact
  // collector to rebuild page headers in the from space, which is
  // used as a marking stack and its page headers are destroyed.
  Page* InitializePagesInChunk(int chunk_id, int pages_in_chunk,
                               PagedSpace* owner);

  void UpdateAllocatedSpaceLimits(Address low, Address high) {
    // The use of atomic primitives does not guarantee correctness (wrt.
    // desired semantics) by default. The loop here ensures that we update the
    // values only if they did not change in between.
    Address ptr = lowest_ever_allocated_.load(std::memory_order_relaxed);
    while ((low < ptr) && !lowest_ever_allocated_.compare_exchange_weak(
                              ptr, low, std::memory_order_acq_rel)) {
    }
    ptr = highest_ever_allocated_.load(std::memory_order_relaxed);
    while ((high > ptr) && !highest_ever_allocated_.compare_exchange_weak(
                               ptr, high, std::memory_order_acq_rel)) {
    }
  }

  void RegisterExecutableMemoryChunk(MemoryChunk* chunk) {
    base::MutexGuard guard(&executable_memory_mutex_);
    DCHECK(chunk->IsFlagSet(MemoryChunk::IS_EXECUTABLE));
    DCHECK_EQ(executable_memory_.find(chunk), executable_memory_.end());
    executable_memory_.insert(chunk);
  }

  void UnregisterExecutableMemoryChunk(MemoryChunk* chunk) {
    base::MutexGuard guard(&executable_memory_mutex_);
    DCHECK_NE(executable_memory_.find(chunk), executable_memory_.end());
    executable_memory_.erase(chunk);
    chunk->heap()->UnregisterUnprotectedMemoryChunk(chunk);
  }

  Isolate* isolate_;

  // This object controls virtual space reserved for code on the V8 heap. This
  // is only valid for 64-bit architectures where kRequiresCodeRange.
  VirtualMemory code_reservation_;

  // Page allocator used for allocating data pages. Depending on the
  // configuration it may be a page allocator instance provided by
  // v8::Platform or a BoundedPageAllocator (when pointer compression is
  // enabled).
  v8::PageAllocator* data_page_allocator_;

  // Page allocator used for allocating code pages. Depending on the
  // configuration it may be a page allocator instance provided by
  // v8::Platform or a BoundedPageAllocator (when pointer compression is
  // enabled or on those 64-bit architectures where pc-relative 32-bit
  // displacement can be used for call and jump instructions).
  v8::PageAllocator* code_page_allocator_;

  // A part of the |code_reservation_| that may contain executable code
  // including reserved page with read-write access in the beginning.
  // See details below.
  base::AddressRegion code_range_;

  // This unique pointer owns the instance of bounded code allocator
  // that controls executable pages allocation. It does not control the
  // optionally existing page in the beginning of the |code_range_|.
  // So, summarizing all above, the following conditions hold:
  // 1) |code_reservation_| >= |code_range_|
  // 2) |code_range_| >= |optional RW pages| +
  // |code_page_allocator_instance_|. 3) |code_reservation_| is
  // AllocatePageSize()-aligned 4) |code_page_allocator_instance_| is
  // MemoryChunk::kAlignment-aligned 5) |code_range_| is
  // CommitPageSize()-aligned
  std::unique_ptr<base::BoundedPageAllocator> code_page_allocator_instance_;

  // Maximum space size in bytes.
  size_t capacity_;

  // Allocated space size in bytes.
  std::atomic<size_t> size_;
  // Allocated executable space size in bytes.
  std::atomic<size_t> size_executable_;

  // We keep the lowest and highest addresses allocated as a quick way
  // of determining that pointers are outside the heap. The estimate is
  // conservative, i.e. not all addresses in 'allocated' space are allocated
  // to our heap. The range is [lowest, highest[, inclusive on the low end
  // and exclusive on the high end.
  std::atomic<Address> lowest_ever_allocated_;
  std::atomic<Address> highest_ever_allocated_;

  VirtualMemory last_chunk_;
  Unmapper unmapper_;

  // Data structure to remember allocated executable memory chunks.
  std::unordered_set<MemoryChunk*> executable_memory_;
  base::Mutex executable_memory_mutex_;

  friend class heap::TestCodePageAllocatorScope;
  friend class heap::TestMemoryAllocatorScope;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MemoryAllocator);
};

extern template EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
    Page* MemoryAllocator::AllocatePage<MemoryAllocator::kRegular, PagedSpace>(
        size_t size, PagedSpace* owner, Executability executable);
extern template EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
    Page* MemoryAllocator::AllocatePage<MemoryAllocator::kRegular, SemiSpace>(
        size_t size, SemiSpace* owner, Executability executable);
extern template EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
    Page* MemoryAllocator::AllocatePage<MemoryAllocator::kPooled, SemiSpace>(
        size_t size, SemiSpace* owner, Executability executable);

extern template EXPORT_TEMPLATE_DECLARE(
    V8_EXPORT_PRIVATE) void MemoryAllocator::
    Free<MemoryAllocator::kFull>(MemoryChunk* chunk);
extern template EXPORT_TEMPLATE_DECLARE(
    V8_EXPORT_PRIVATE) void MemoryAllocator::
    Free<MemoryAllocator::kAlreadyPooled>(MemoryChunk* chunk);
extern template EXPORT_TEMPLATE_DECLARE(
    V8_EXPORT_PRIVATE) void MemoryAllocator::
    Free<MemoryAllocator::kPreFreeAndQueue>(MemoryChunk* chunk);
extern template EXPORT_TEMPLATE_DECLARE(
    V8_EXPORT_PRIVATE) void MemoryAllocator::
    Free<MemoryAllocator::kPooledAndQueue>(MemoryChunk* chunk);

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_MEMORY_ALLOCATOR_H_
