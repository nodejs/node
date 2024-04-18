// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_BASIC_MEMORY_CHUNK_H_
#define V8_HEAP_BASIC_MEMORY_CHUNK_H_

#include <type_traits>
#include <unordered_map>

#include "src/base/atomic-utils.h"
#include "src/base/flags.h"
#include "src/base/functional.h"
#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/heap/marking.h"
#include "src/heap/memory-chunk-header.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/objects/heap-object.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

class BaseSpace;

class BasicMemoryChunk : public MemoryChunkHeader {
 public:
  // Only works if the pointer is in the first kPageSize of the MemoryChunk.
  static BasicMemoryChunk* FromAddress(Address a) {
    DCHECK(!V8_ENABLE_THIRD_PARTY_HEAP_BOOL);
    return reinterpret_cast<BasicMemoryChunk*>(BaseAddress(a));
  }

  // Only works if the object is in the first kPageSize of the MemoryChunk.
  static BasicMemoryChunk* FromHeapObject(Tagged<HeapObject> o) {
    DCHECK(!V8_ENABLE_THIRD_PARTY_HEAP_BOOL);
    return reinterpret_cast<BasicMemoryChunk*>(BaseAddress(o.ptr()));
  }

  // Only works if the object is in the first kPageSize of the MemoryChunk.
  static BasicMemoryChunk* FromHeapObject(const HeapObjectLayout* o) {
    DCHECK(!V8_ENABLE_THIRD_PARTY_HEAP_BOOL);
    return reinterpret_cast<BasicMemoryChunk*>(
        BaseAddress(reinterpret_cast<Address>(o)));
  }

  static inline void UpdateHighWaterMark(Address mark) {
    if (mark == kNullAddress) return;
    // Need to subtract one from the mark because when a chunk is full the
    // top points to the next address after the chunk, which effectively belongs
    // to another chunk. See the comment to Page::FromAllocationAreaAddress.
    BasicMemoryChunk* chunk = BasicMemoryChunk::FromAddress(mark - 1);
    intptr_t new_mark = static_cast<intptr_t>(mark - chunk->address());
    intptr_t old_mark = chunk->high_water_mark_.load(std::memory_order_relaxed);
    while ((new_mark > old_mark) &&
           !chunk->high_water_mark_.compare_exchange_weak(
               old_mark, new_mark, std::memory_order_acq_rel)) {
    }
  }

  BasicMemoryChunk(Heap* heap, BaseSpace* space, size_t chunk_size,
                   Address area_start, Address area_end,
                   VirtualMemory reservation);

  Address address() const { return reinterpret_cast<Address>(this); }

  // Returns the offset of a given address to this page.
  inline size_t Offset(Address a) const {
    return static_cast<size_t>(a - address());
  }

  size_t size() const { return size_; }
  void set_size(size_t size) { size_ = size; }

  Address area_start() const { return area_start_; }

  Address area_end() const { return area_end_; }
  void set_area_end(Address area_end) { area_end_ = area_end; }

  size_t area_size() const {
    return static_cast<size_t>(area_end() - area_start());
  }

  Heap* heap() const {
    DCHECK_NOT_NULL(heap_);
    return heap_;
  }

  // Gets the chunk's owner or null if the space has been detached.
  BaseSpace* owner() const { return owner_; }
  void set_owner(BaseSpace* space) { owner_ = space; }

  bool InOldSpace() const;
  V8_EXPORT_PRIVATE bool InLargeObjectSpace() const;

  bool IsWritable() const {
    // If this is a read-only space chunk but heap_ is non-null, it has not yet
    // been sealed and can be written to.
    return !InReadOnlySpace() || heap_ != nullptr;
  }

  bool Contains(Address addr) const {
    return addr >= area_start() && addr < area_end();
  }

  // Checks whether |addr| can be a limit of addresses in this page. It's a
  // limit if it's in the page, or if it's just after the last byte of the page.
  bool ContainsLimit(Address addr) const {
    return addr >= area_start() && addr <= area_end();
  }

  size_t wasted_memory() const { return wasted_memory_; }
  void add_wasted_memory(size_t waste) { wasted_memory_ += waste; }
  size_t allocated_bytes() const { return allocated_bytes_; }

  Address HighWaterMark() const { return address() + high_water_mark_; }

  VirtualMemory* reserved_memory() { return &reservation_; }

  void ResetAllocationStatistics() {
    allocated_bytes_ = area_size();
    wasted_memory_ = 0;
  }

  void IncreaseAllocatedBytes(size_t bytes) {
    DCHECK_LE(bytes, area_size());
    allocated_bytes_ += bytes;
  }

  void DecreaseAllocatedBytes(size_t bytes) {
    DCHECK_LE(bytes, area_size());
    DCHECK_GE(allocated_bytes(), bytes);
    allocated_bytes_ -= bytes;
  }

#ifdef THREAD_SANITIZER
  // Perform a dummy acquire load to tell TSAN that there is no data race in
  // mark-bit initialization. See MemoryChunk::Initialize for the corresponding
  // release store.
  void SynchronizedHeapLoad() const;
#endif

 protected:
  // Overall size of the chunk, including the header and guards.
  size_t size_;

  Heap* heap_;

  // Start and end of allocatable memory on this chunk.
  Address area_start_;
  Address area_end_;

  // Byte allocated on the page, which includes all objects on the page and the
  // linear allocation area.
  size_t allocated_bytes_;
  // Freed memory that was not added to the free list.
  size_t wasted_memory_ = 0;

  // Assuming the initial allocation on a page is sequential, count highest
  // number of bytes ever allocated on the page.
  std::atomic<intptr_t> high_water_mark_;

  // The space owning this memory chunk.
  std::atomic<BaseSpace*> owner_;

  // If the chunk needs to remember its memory reservation, it is stored here.
  VirtualMemory reservation_;

  friend class BasicMemoryChunkValidator;
  friend class ConcurrentMarkingState;
  friend class MarkingState;
  friend class AtomicMarkingState;
  friend class NonAtomicMarkingState;
  friend class MemoryAllocator;
  friend class PagedSpace;
};

DEFINE_OPERATORS_FOR_FLAGS(BasicMemoryChunk::MainThreadFlags)

}  // namespace internal

namespace base {

// Define special hash function for chunk pointers, to be used with std data
// structures, e.g.
// std::unordered_set<BasicMemoryChunk*, base::hash<BasicMemoryChunk*>
// This hash function discards the trailing zero bits (chunk alignment).
// Notice that, when pointer compression is enabled, it also discards the
// cage base.
template <>
struct hash<const i::BasicMemoryChunk*> {
  V8_INLINE size_t operator()(const i::BasicMemoryChunk* chunk) const {
    return static_cast<v8::internal::Tagged_t>(
               reinterpret_cast<uintptr_t>(chunk)) >>
           kPageSizeBits;
  }
};

template <>
struct hash<i::BasicMemoryChunk*> {
  V8_INLINE size_t operator()(i::BasicMemoryChunk* chunk) const {
    return hash<const i::BasicMemoryChunk*>()(chunk);
  }
};

}  // namespace base
}  // namespace v8

#endif  // V8_HEAP_BASIC_MEMORY_CHUNK_H_
