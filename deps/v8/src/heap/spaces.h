// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_SPACES_H_
#define V8_HEAP_SPACES_H_

#include <atomic>
#include <memory>

#include "src/base/iterator.h"
#include "src/base/macros.h"
#include "src/common/globals.h"
#include "src/heap/allocation-observer.h"
#include "src/heap/base-space.h"
#include "src/heap/base/active-system-pages.h"
#include "src/heap/basic-memory-chunk.h"
#include "src/heap/free-list.h"
#include "src/heap/linear-allocation-area.h"
#include "src/heap/list.h"
#include "src/heap/main-allocator.h"
#include "src/heap/memory-chunk-layout.h"
#include "src/heap/memory-chunk.h"
#include "src/heap/page.h"
#include "src/heap/slot-set.h"
#include "src/objects/objects.h"
#include "src/utils/allocation.h"
#include "src/utils/utils.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck

namespace v8 {
namespace internal {

namespace heap {
class HeapTester;
class TestCodePageAllocatorScope;
}  // namespace heap

class AllocationObserver;
class FreeList;
class Heap;
class Isolate;
class LargeObjectSpace;
class LargePage;
class ObjectIterator;
class PagedSpaceBase;
class SemiSpace;

// Some assertion macros used in the debugging mode.

#define DCHECK_OBJECT_SIZE(size) \
  DCHECK((0 < size) && (size <= kMaxRegularHeapObjectSize))

#define DCHECK_CODEOBJECT_SIZE(size) \
  DCHECK((0 < size) && (size <= MemoryChunkLayout::MaxRegularCodeObjectSize()))

template <typename Enum, typename Callback>
void ForAll(Callback callback) {
  for (int i = 0; i < static_cast<int>(Enum::kNumValues); i++) {
    callback(static_cast<Enum>(i), i);
  }
}

// ----------------------------------------------------------------------------
// Space is the abstract superclass for all allocation spaces that are not
// sealed after startup (i.e. not ReadOnlySpace).
class V8_EXPORT_PRIVATE Space : public BaseSpace {
 public:
  static inline void MoveExternalBackingStoreBytes(
      ExternalBackingStoreType type, Space* from, Space* to, size_t amount);

  Space(Heap* heap, AllocationSpace id, std::unique_ptr<FreeList> free_list)
      : BaseSpace(heap, id), free_list_(std::move(free_list)) {}

  ~Space() override = default;

  Space(const Space&) = delete;
  Space& operator=(const Space&) = delete;

  // Returns size of objects. Can differ from the allocated size
  // (e.g. see OldLargeObjectSpace).
  virtual size_t SizeOfObjects() const { return Size(); }

  // Return the available bytes without growing.
  virtual size_t Available() const = 0;

  virtual std::unique_ptr<ObjectIterator> GetObjectIterator(Heap* heap) = 0;

  inline void IncrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                                 size_t amount);
  inline void DecrementExternalBackingStoreBytes(ExternalBackingStoreType type,
                                                 size_t amount);

  // Returns amount of off-heap memory in-use by objects in this Space.
  virtual size_t ExternalBackingStoreBytes(
      ExternalBackingStoreType type) const {
    return external_backing_store_bytes_[static_cast<int>(type)];
  }

  virtual MemoryChunk* first_page() { return memory_chunk_list_.front(); }
  virtual MemoryChunk* last_page() { return memory_chunk_list_.back(); }

  virtual const MemoryChunk* first_page() const {
    return memory_chunk_list_.front();
  }
  virtual const MemoryChunk* last_page() const {
    return memory_chunk_list_.back();
  }

  virtual heap::List<MemoryChunk>& memory_chunk_list() {
    return memory_chunk_list_;
  }

  virtual Page* InitializePage(MemoryChunk* chunk) { UNREACHABLE(); }

  FreeList* free_list() { return free_list_.get(); }

  Address FirstPageAddress() const {
    DCHECK_NOT_NULL(first_page());
    return first_page()->address();
  }

#ifdef DEBUG
  virtual void Print() = 0;
#endif

 protected:
  // The List manages the pages that belong to the given space.
  heap::List<MemoryChunk> memory_chunk_list_;
  // Tracks off-heap memory used by this space.
  std::atomic<size_t> external_backing_store_bytes_[static_cast<int>(
      ExternalBackingStoreType::kNumValues)] = {0};
  std::unique_ptr<FreeList> free_list_;
};

static_assert(sizeof(std::atomic<intptr_t>) == kSystemPointerSize);

// -----------------------------------------------------------------------------
// Interface for heap object iterator to be implemented by all object space
// object iterators.

class V8_EXPORT_PRIVATE ObjectIterator : public Malloced {
 public:
  // Note: The destructor can not be marked as `= default` as this causes
  // the compiler on C++20 to define it as `constexpr` resulting in the
  // compiler producing warnings about undefined inlines for Next()
  // on classes inheriting from it.
  virtual ~ObjectIterator() {}
  virtual Tagged<HeapObject> Next() = 0;
};

template <class PAGE_TYPE>
class PageIteratorImpl
    : public base::iterator<std::forward_iterator_tag, PAGE_TYPE> {
 public:
  explicit PageIteratorImpl(PAGE_TYPE* p) : p_(p) {}
  PageIteratorImpl(const PageIteratorImpl<PAGE_TYPE>& other) : p_(other.p_) {}
  PAGE_TYPE* operator*() { return p_; }
  bool operator==(const PageIteratorImpl<PAGE_TYPE>& rhs) const {
    return rhs.p_ == p_;
  }
  bool operator!=(const PageIteratorImpl<PAGE_TYPE>& rhs) const {
    return rhs.p_ != p_;
  }
  inline PageIteratorImpl<PAGE_TYPE>& operator++();
  inline PageIteratorImpl<PAGE_TYPE> operator++(int);

 private:
  PAGE_TYPE* p_;
};

using PageIterator = PageIteratorImpl<Page>;
using ConstPageIterator = PageIteratorImpl<const Page>;
using LargePageIterator = PageIteratorImpl<LargePage>;
using ConstLargePageIterator = PageIteratorImpl<const LargePage>;

class PageRange {
 public:
  using iterator = PageIterator;
  PageRange(Page* begin, Page* end) : begin_(begin), end_(end) {}
  inline explicit PageRange(Page* page);

  iterator begin() { return iterator(begin_); }
  iterator end() { return iterator(end_); }

 private:
  Page* begin_;
  Page* end_;
};

class ConstPageRange {
 public:
  using iterator = ConstPageIterator;
  ConstPageRange(const Page* begin, const Page* end)
      : begin_(begin), end_(end) {}
  inline explicit ConstPageRange(const Page* page);

  iterator begin() { return iterator(begin_); }
  iterator end() { return iterator(end_); }

 private:
  const Page* begin_;
  const Page* end_;
};

// -----------------------------------------------------------------------------
// A space has a circular list of pages. The next page can be accessed via
// Page::next_page() call.

// LocalAllocationBuffer represents a linear allocation area that is created
// from a given {AllocationResult} and can be used to allocate memory without
// synchronization.
//
// The buffer is properly closed upon destruction and reassignment.
// Example:
//   {
//     AllocationResult result = ...;
//     LocalAllocationBuffer a(heap, result, size);
//     LocalAllocationBuffer b = a;
//     CHECK(!a.IsValid());
//     CHECK(b.IsValid());
//     // {a} is invalid now and cannot be used for further allocations.
//   }
//   // Since {b} went out of scope, the LAB is closed, resulting in creating a
//   // filler object for the remaining area.
class LocalAllocationBuffer {
 public:
  // Indicates that a buffer cannot be used for allocations anymore. Can result
  // from either reassigning a buffer, or trying to construct it from an
  // invalid {AllocationResult}.
  static LocalAllocationBuffer InvalidBuffer() {
    return LocalAllocationBuffer(
        nullptr, LinearAllocationArea(kNullAddress, kNullAddress));
  }

  // Creates a new LAB from a given {AllocationResult}. Results in
  // InvalidBuffer if the result indicates a retry.
  static inline LocalAllocationBuffer FromResult(Heap* heap,
                                                 AllocationResult result,
                                                 intptr_t size);

  ~LocalAllocationBuffer() { CloseAndMakeIterable(); }

  LocalAllocationBuffer(const LocalAllocationBuffer& other) = delete;
  V8_EXPORT_PRIVATE LocalAllocationBuffer(LocalAllocationBuffer&& other)
      V8_NOEXCEPT;

  LocalAllocationBuffer& operator=(const LocalAllocationBuffer& other) = delete;
  V8_EXPORT_PRIVATE LocalAllocationBuffer& operator=(
      LocalAllocationBuffer&& other) V8_NOEXCEPT;

  V8_WARN_UNUSED_RESULT inline AllocationResult AllocateRawAligned(
      int size_in_bytes, AllocationAlignment alignment);
  V8_WARN_UNUSED_RESULT inline AllocationResult AllocateRawUnaligned(
      int size_in_bytes);

  inline bool IsValid() { return allocation_info_.top() != kNullAddress; }

  // Try to merge LABs, which is only possible when they are adjacent in memory.
  // Returns true if the merge was successful, false otherwise.
  inline bool TryMerge(LocalAllocationBuffer* other);

  inline bool TryFreeLast(Tagged<HeapObject> object, int object_size);

  // Close a LAB, effectively invalidating it. Returns the unused area.
  V8_EXPORT_PRIVATE LinearAllocationArea CloseAndMakeIterable();
  void MakeIterable();

  Address top() const { return allocation_info_.top(); }
  Address limit() const { return allocation_info_.limit(); }

 private:
  V8_EXPORT_PRIVATE LocalAllocationBuffer(
      Heap* heap, LinearAllocationArea allocation_info) V8_NOEXCEPT;

  Heap* heap_;
  LinearAllocationArea allocation_info_;
};

class V8_EXPORT_PRIVATE SpaceWithLinearArea : public Space {
 public:
  // Creates this space with a new MainAllocator instance.
  SpaceWithLinearArea(
      Heap* heap, AllocationSpace id, std::unique_ptr<FreeList> free_list,
      CompactionSpaceKind compaction_space_kind,
      MainAllocator::SupportsExtendingLAB supports_extending_lab);

  // Creates this space with a new MainAllocator instance and passes
  // `allocation_info` to its constructor.
  SpaceWithLinearArea(
      Heap* heap, AllocationSpace id, std::unique_ptr<FreeList> free_list,
      CompactionSpaceKind compaction_space_kind,
      MainAllocator::SupportsExtendingLAB supports_extending_lab,
      LinearAllocationArea& allocation_info);

  // Creates this space and uses the existing `allocator`. It doesn't create a
  // new MainAllocator instance.
  SpaceWithLinearArea(Heap* heap, AllocationSpace id,
                      std::unique_ptr<FreeList> free_list,
                      CompactionSpaceKind compaction_space_kind,
                      MainAllocator* allocator);

  MainAllocator* main_allocator() { return allocator_; }

  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult
  AllocateRaw(int size_in_bytes, AllocationAlignment alignment,
              AllocationOrigin origin = AllocationOrigin::kRuntime);

 protected:
  // Sets up a linear allocation area that fits the given number of bytes.
  // Returns false if there is not enough space and the caller has to retry
  // after collecting garbage.
  // Writes to `max_aligned_size` the actual number of bytes used for checking
  // that there is enough space.
  virtual bool EnsureAllocation(int size_in_bytes,
                                AllocationAlignment alignment,
                                AllocationOrigin origin,
                                int* out_max_aligned_size) = 0;

  virtual void FreeLinearAllocationArea() = 0;

  virtual void UpdateInlineAllocationLimit() = 0;

  // TODO(chromium:1480975): Move the LAB out of the space.
  MainAllocator* allocator_;
  base::Optional<MainAllocator> owned_allocator_;

  friend class MainAllocator;
};

class V8_EXPORT_PRIVATE SpaceIterator : public Malloced {
 public:
  explicit SpaceIterator(Heap* heap);
  virtual ~SpaceIterator();

  bool HasNext();
  Space* Next();

 private:
  Heap* heap_;
  int current_space_;  // from enum AllocationSpace.
};

// Iterates over all memory chunks in the heap (across all spaces).
class MemoryChunkIterator {
 public:
  explicit MemoryChunkIterator(Heap* heap) : space_iterator_(heap) {}

  V8_INLINE bool HasNext();
  V8_INLINE MemoryChunk* Next();

 private:
  SpaceIterator space_iterator_;
  MemoryChunk* current_chunk_ = nullptr;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_SPACES_H_
