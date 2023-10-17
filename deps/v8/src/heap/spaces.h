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

  Space(Heap* heap, AllocationSpace id, std::unique_ptr<FreeList> free_list,
        AllocationCounter& allocation_counter)
      : BaseSpace(heap, id),
        free_list_(std::move(free_list)),
        allocation_counter_(allocation_counter) {}

  ~Space() override = default;

  Space(const Space&) = delete;
  Space& operator=(const Space&) = delete;

  virtual void AddAllocationObserver(AllocationObserver* observer);
  virtual void RemoveAllocationObserver(AllocationObserver* observer);
  virtual void PauseAllocationObservers() {}
  virtual void ResumeAllocationObservers() {}

  // Returns size of objects. Can differ from the allocated size
  // (e.g. see OldLargeObjectSpace).
  virtual size_t SizeOfObjects() const { return Size(); }

  // Return the available bytes without growing.
  virtual size_t Available() const = 0;

  virtual int RoundSizeDownToObjectAlignment(int size) const {
    if (id_ == CODE_SPACE) {
      return RoundDown(size, kCodeAlignment);
    } else if (V8_COMPRESS_POINTERS_8GB_BOOL) {
      return RoundDown(size, kObjectAlignment8GbHeap);
    } else {
      return RoundDown(size, kTaggedSize);
    }
  }

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
  AllocationCounter& allocation_counter_;
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
  inline PageRange(Address start, Address limit);

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
  inline ConstPageRange(Address start, Address limit);

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

class LinearAreaOriginalData {
 public:
  Address get_original_top_acquire() const {
    return original_top_.load(std::memory_order_acquire);
  }
  Address get_original_limit_relaxed() const {
    return original_limit_.load(std::memory_order_relaxed);
  }

  void set_original_top_release(Address top) {
    original_top_.store(top, std::memory_order_release);
  }
  void set_original_limit_relaxed(Address limit) {
    original_limit_.store(limit, std::memory_order_relaxed);
  }

  base::SharedMutex* linear_area_lock() { return &linear_area_lock_; }

 private:
  // The top and the limit at the time of setting the linear allocation area.
  // These values can be accessed by background tasks. Protected by
  // pending_allocation_mutex_.
  std::atomic<Address> original_top_ = 0;
  std::atomic<Address> original_limit_ = 0;

  // Protects original_top_ and original_limit_.
  base::SharedMutex linear_area_lock_;
};

class SpaceWithLinearArea : public Space {
 public:
  SpaceWithLinearArea(Heap* heap, AllocationSpace id,
                      std::unique_ptr<FreeList> free_list,
                      AllocationCounter& allocation_counter,
                      LinearAllocationArea& allocation_info,
                      LinearAreaOriginalData& linear_area_original_data)
      : Space(heap, id, std::move(free_list), allocation_counter),
        allocation_info_(allocation_info),
        linear_area_original_data_(linear_area_original_data) {}

  virtual bool SupportsAllocationObserver() const = 0;

  // Returns the allocation pointer in this space.
  Address top() const { return allocation_info_.top(); }
  Address limit() const { return allocation_info_.limit(); }

  // The allocation top address.
  Address* allocation_top_address() const {
    return allocation_info_.top_address();
  }

  // The allocation limit address.
  Address* allocation_limit_address() const {
    return allocation_info_.limit_address();
  }

  // Methods needed for allocation observers.
  V8_EXPORT_PRIVATE void AddAllocationObserver(
      AllocationObserver* observer) override;
  V8_EXPORT_PRIVATE void RemoveAllocationObserver(
      AllocationObserver* observer) override;
  V8_EXPORT_PRIVATE void ResumeAllocationObservers() override;
  V8_EXPORT_PRIVATE void PauseAllocationObservers() override;

  V8_EXPORT_PRIVATE void AdvanceAllocationObservers();
  V8_EXPORT_PRIVATE void InvokeAllocationObservers(Address soon_object,
                                                   size_t size_in_bytes,
                                                   size_t aligned_size_in_bytes,
                                                   size_t allocation_size);

  void MarkLabStartInitialized();
  virtual void FreeLinearAllocationArea() = 0;

  // When allocation observers are active we may use a lower limit to allow the
  // observers to 'interrupt' earlier than the natural limit. Given a linear
  // area bounded by [start, end), this function computes the limit to use to
  // allow proper observation based on existing observers. min_size specifies
  // the minimum size that the limited area should have.
  Address ComputeLimit(Address start, Address end, size_t min_size) const;
  V8_EXPORT_PRIVATE virtual void UpdateInlineAllocationLimit() = 0;

  void PrintAllocationsOrigins() const;

  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult
  AllocateRaw(int size_in_bytes, AllocationAlignment alignment,
              AllocationOrigin origin = AllocationOrigin::kRuntime);

  // Allocate the requested number of bytes in the space if possible, return a
  // failure object if not.
  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult AllocateRawUnaligned(
      int size_in_bytes, AllocationOrigin origin = AllocationOrigin::kRuntime);

  // Allocate the requested number of bytes in the space double aligned if
  // possible, return a failure object if not.
  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult
  AllocateRawAligned(int size_in_bytes, AllocationAlignment alignment,
                     AllocationOrigin origin = AllocationOrigin::kRuntime);

  base::SharedMutex* linear_area_lock() {
    return linear_area_original_data_.linear_area_lock();
  }

  Address original_top_acquire() const {
    return linear_area_original_data_.get_original_top_acquire();
  }
  Address original_limit_relaxed() const {
    return linear_area_original_data_.get_original_limit_relaxed();
  }

  void MoveOriginalTopForward() {
    base::SharedMutexGuard<base::kExclusive> guard(linear_area_lock());
    DCHECK_GE(top(), linear_area_original_data_.get_original_top_acquire());
    DCHECK_LE(top(), linear_area_original_data_.get_original_limit_relaxed());
    linear_area_original_data_.set_original_top_release(top());
  }

 protected:
  V8_EXPORT_PRIVATE void UpdateAllocationOrigins(AllocationOrigin origin);

  // Allocates an object from the linear allocation area. Assumes that the
  // linear allocation area is large enough to fit the object.
  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult
  AllocateFastUnaligned(int size_in_bytes, AllocationOrigin origin);
  // Tries to allocate an aligned object from the linear allocation area.
  // Returns nullptr if the linear allocation area does not fit the object.
  // Otherwise, returns the object pointer and writes the allocation size
  // (object size + alignment filler size) to the size_in_bytes.
  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult
  AllocateFastAligned(int size_in_bytes, int* aligned_size_in_bytes,
                      AllocationAlignment alignment, AllocationOrigin origin);

  // Slow path of allocation function
  V8_WARN_UNUSED_RESULT V8_INLINE AllocationResult
  AllocateRawSlow(int size_in_bytes, AllocationAlignment alignment,
                  AllocationOrigin origin);

  // Sets up a linear allocation area that fits the given number of bytes.
  // Returns false if there is not enough space and the caller has to retry
  // after collecting garbage.
  // Writes to `max_aligned_size` the actual number of bytes used for checking
  // that there is enough space.
  virtual bool EnsureAllocation(int size_in_bytes,
                                AllocationAlignment alignment,
                                AllocationOrigin origin,
                                int* out_max_aligned_size) = 0;

#if DEBUG
  V8_EXPORT_PRIVATE virtual void VerifyTop() const;
#endif  // DEBUG

  LinearAllocationArea& allocation_info_;
  LinearAreaOriginalData& linear_area_original_data_;

  size_t allocations_origins_[static_cast<int>(
      AllocationOrigin::kNumberOfAllocationOrigins)] = {0};
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
