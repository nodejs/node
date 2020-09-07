// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_OBJECT_ALLOCATOR_H_
#define V8_HEAP_CPPGC_OBJECT_ALLOCATOR_H_

#include "include/cppgc/allocation.h"
#include "include/cppgc/internal/gc-info.h"
#include "include/cppgc/macros.h"
#include "src/heap/cppgc/heap-space.h"
#include "src/heap/cppgc/raw-heap.h"

namespace cppgc {

class V8_EXPORT AllocationHandle {
 private:
  AllocationHandle() = default;
  friend class internal::ObjectAllocator;
};

namespace internal {

class StatsCollector;
class PageBackend;

class V8_EXPORT_PRIVATE ObjectAllocator final : public cppgc::AllocationHandle {
 public:
  // NoAllocationScope is used in debug mode to catch unwanted allocations. E.g.
  // allocations during GC.
  class V8_EXPORT_PRIVATE NoAllocationScope final {
    CPPGC_STACK_ALLOCATED();

   public:
    explicit NoAllocationScope(ObjectAllocator&);
    ~NoAllocationScope();

    NoAllocationScope(const NoAllocationScope&) = delete;
    NoAllocationScope& operator=(const NoAllocationScope&) = delete;

   private:
    ObjectAllocator& allocator_;
  };

  ObjectAllocator(RawHeap* heap, PageBackend* page_backend,
                  StatsCollector* stats_collector);

  inline void* AllocateObject(size_t size, GCInfoIndex gcinfo);
  inline void* AllocateObject(size_t size, GCInfoIndex gcinfo,
                              CustomSpaceIndex space_index);

  void ResetLinearAllocationBuffers();

 private:
  // Returns the initially tried SpaceType to allocate an object of |size| bytes
  // on. Returns the largest regular object size bucket for large objects.
  inline static RawHeap::RegularSpaceType GetInitialSpaceIndexForSize(
      size_t size);

  bool is_allocation_allowed() const { return no_allocation_scope_ == 0; }

  inline void* AllocateObjectOnSpace(NormalPageSpace* space, size_t size,
                                     GCInfoIndex gcinfo);
  void* OutOfLineAllocate(NormalPageSpace*, size_t, GCInfoIndex);
  void* OutOfLineAllocateImpl(NormalPageSpace*, size_t, GCInfoIndex);
  void* AllocateFromFreeList(NormalPageSpace*, size_t, GCInfoIndex);

  RawHeap* raw_heap_;
  PageBackend* page_backend_;
  StatsCollector* stats_collector_;
  size_t no_allocation_scope_ = 0;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_OBJECT_ALLOCATOR_H_
