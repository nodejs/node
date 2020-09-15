// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_HEAP_BASE_H_
#define V8_HEAP_CPPGC_HEAP_BASE_H_

#include <memory>
#include <set>

#include "include/cppgc/heap.h"
#include "include/cppgc/internal/persistent-node.h"
#include "include/cppgc/macros.h"
#include "src/base/macros.h"
#include "src/heap/cppgc/marker.h"
#include "src/heap/cppgc/object-allocator.h"
#include "src/heap/cppgc/raw-heap.h"
#include "src/heap/cppgc/sweeper.h"

#if defined(CPPGC_CAGED_HEAP)
#include "src/heap/cppgc/caged-heap.h"
#endif

namespace heap {
namespace base {
class Stack;
}  // namespace base
}  // namespace heap

namespace cppgc {

class Platform;

namespace internal {

namespace testing {
class TestWithHeap;
}  // namespace testing

class PageBackend;
class PreFinalizerHandler;
class StatsCollector;

// Base class for heap implementations.
class V8_EXPORT_PRIVATE HeapBase {
 public:
  // NoGCScope allows going over limits and avoids triggering garbage
  // collection triggered through allocations or even explicitly.
  class V8_EXPORT_PRIVATE NoGCScope final {
    CPPGC_STACK_ALLOCATED();

   public:
    explicit NoGCScope(HeapBase& heap);
    ~NoGCScope();

    NoGCScope(const NoGCScope&) = delete;
    NoGCScope& operator=(const NoGCScope&) = delete;

   private:
    HeapBase& heap_;
  };

  HeapBase(std::shared_ptr<cppgc::Platform> platform, size_t custom_spaces);
  virtual ~HeapBase();

  HeapBase(const HeapBase&) = delete;
  HeapBase& operator=(const HeapBase&) = delete;

  RawHeap& raw_heap() { return raw_heap_; }
  const RawHeap& raw_heap() const { return raw_heap_; }

  cppgc::Platform* platform() { return platform_.get(); }
  const cppgc::Platform* platform() const { return platform_.get(); }

  PageBackend* page_backend() { return page_backend_.get(); }
  const PageBackend* page_backend() const { return page_backend_.get(); }

  StatsCollector* stats_collector() { return stats_collector_.get(); }
  const StatsCollector* stats_collector() const {
    return stats_collector_.get();
  }

#if defined(CPPGC_CAGED_HEAP)
  CagedHeap& caged_heap() { return caged_heap_; }
  const CagedHeap& caged_heap() const { return caged_heap_; }
#endif

  heap::base::Stack* stack() { return stack_.get(); }

  PreFinalizerHandler* prefinalizer_handler() {
    return prefinalizer_handler_.get();
  }

  MarkerBase* marker() const { return marker_.get(); }

  ObjectAllocator& object_allocator() { return object_allocator_; }

  Sweeper& sweeper() { return sweeper_; }

  PersistentRegion& GetStrongPersistentRegion() {
    return strong_persistent_region_;
  }
  const PersistentRegion& GetStrongPersistentRegion() const {
    return strong_persistent_region_;
  }
  PersistentRegion& GetWeakPersistentRegion() {
    return weak_persistent_region_;
  }
  const PersistentRegion& GetWeakPersistentRegion() const {
    return weak_persistent_region_;
  }

#if defined(CPPGC_YOUNG_GENERATION)
  std::set<void*>& remembered_slots() { return remembered_slots_; }
#endif

  size_t ObjectPayloadSize() const;

  void AdvanceIncrementalGarbageCollectionOnAllocationIfNeeded();

 protected:
  void VerifyMarking(cppgc::Heap::StackState);

  virtual void FinalizeIncrementalGarbageCollectionIfNeeded(
      cppgc::Heap::StackState) = 0;

  bool in_no_gc_scope() const { return no_gc_scope_ > 0; }

  RawHeap raw_heap_;
  std::shared_ptr<cppgc::Platform> platform_;
#if defined(CPPGC_CAGED_HEAP)
  CagedHeap caged_heap_;
#endif
  std::unique_ptr<PageBackend> page_backend_;

  std::unique_ptr<StatsCollector> stats_collector_;
  std::unique_ptr<heap::base::Stack> stack_;
  std::unique_ptr<PreFinalizerHandler> prefinalizer_handler_;
  std::unique_ptr<MarkerBase> marker_;

  ObjectAllocator object_allocator_;
  Sweeper sweeper_;

  PersistentRegion strong_persistent_region_;
  PersistentRegion weak_persistent_region_;

#if defined(CPPGC_YOUNG_GENERATION)
  std::set<void*> remembered_slots_;
#endif

  size_t no_gc_scope_ = 0;

  friend class MarkerBase::IncrementalMarkingTask;
  friend class testing::TestWithHeap;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_HEAP_BASE_H_
