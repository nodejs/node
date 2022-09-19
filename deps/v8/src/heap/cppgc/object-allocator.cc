// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/object-allocator.h"

#include "include/cppgc/allocation.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/heap/cppgc/free-list.h"
#include "src/heap/cppgc/globals.h"
#include "src/heap/cppgc/heap-object-header.h"
#include "src/heap/cppgc/heap-page.h"
#include "src/heap/cppgc/heap-space.h"
#include "src/heap/cppgc/heap-visitor.h"
#include "src/heap/cppgc/heap.h"
#include "src/heap/cppgc/memory.h"
#include "src/heap/cppgc/object-start-bitmap.h"
#include "src/heap/cppgc/page-memory.h"
#include "src/heap/cppgc/prefinalizer-handler.h"
#include "src/heap/cppgc/stats-collector.h"
#include "src/heap/cppgc/sweeper.h"

namespace cppgc {
namespace internal {

namespace {

void MarkRangeAsYoung(BasePage* page, Address begin, Address end) {
#if defined(CPPGC_YOUNG_GENERATION)
  DCHECK_LT(begin, end);

  static constexpr auto kEntrySize = AgeTable::kCardSizeInBytes;

  const uintptr_t offset_begin = CagedHeap::OffsetFromAddress(begin);
  const uintptr_t offset_end = CagedHeap::OffsetFromAddress(end);

  const uintptr_t young_offset_begin = (begin == page->PayloadStart())
                                           ? RoundDown(offset_begin, kEntrySize)
                                           : RoundUp(offset_begin, kEntrySize);
  const uintptr_t young_offset_end = (end == page->PayloadEnd())
                                         ? RoundUp(offset_end, kEntrySize)
                                         : RoundDown(offset_end, kEntrySize);

  auto& age_table = page->heap().caged_heap().local_data().age_table;
  for (auto offset = young_offset_begin; offset < young_offset_end;
       offset += AgeTable::kCardSizeInBytes) {
    age_table.SetAge(offset, AgeTable::Age::kYoung);
  }

  // Set to kUnknown the first and the last regions of the newly allocated
  // linear buffer.
  if (begin != page->PayloadStart() && !IsAligned(offset_begin, kEntrySize))
    age_table.SetAge(offset_begin, AgeTable::Age::kMixed);
  if (end != page->PayloadEnd() && !IsAligned(offset_end, kEntrySize))
    age_table.SetAge(offset_end, AgeTable::Age::kMixed);
#endif
}

void AddToFreeList(NormalPageSpace& space, Address start, size_t size) {
  // No need for SetMemoryInaccessible() as LAB memory is retrieved as free
  // inaccessible memory.
  space.free_list().Add({start, size});
  // Concurrent marking may be running while the LAB is set up next to a live
  // object sharing the same cell in the bitmap.
  NormalPage::From(BasePage::FromPayload(start))
      ->object_start_bitmap()
      .SetBit<AccessMode::kAtomic>(start);
}

void ReplaceLinearAllocationBuffer(NormalPageSpace& space,
                                   StatsCollector& stats_collector,
                                   Address new_buffer, size_t new_size) {
  auto& lab = space.linear_allocation_buffer();
  if (lab.size()) {
    AddToFreeList(space, lab.start(), lab.size());
    stats_collector.NotifyExplicitFree(lab.size());
  }

  lab.Set(new_buffer, new_size);
  if (new_size) {
    DCHECK_NOT_NULL(new_buffer);
    stats_collector.NotifyAllocation(new_size);
    auto* page = NormalPage::From(BasePage::FromPayload(new_buffer));
    // Concurrent marking may be running while the LAB is set up next to a live
    // object sharing the same cell in the bitmap.
    page->object_start_bitmap().ClearBit<AccessMode::kAtomic>(new_buffer);
    MarkRangeAsYoung(page, new_buffer, new_buffer + new_size);
  }
}

void* AllocateLargeObject(PageBackend& page_backend, LargePageSpace& space,
                          StatsCollector& stats_collector, size_t size,
                          GCInfoIndex gcinfo) {
  LargePage* page = LargePage::Create(page_backend, space, size);
  space.AddPage(page);

  auto* header = new (page->ObjectHeader())
      HeapObjectHeader(HeapObjectHeader::kLargeObjectSizeInHeader, gcinfo);

  stats_collector.NotifyAllocation(size);
  MarkRangeAsYoung(page, page->PayloadStart(), page->PayloadEnd());

  return header->ObjectStart();
}

}  // namespace

constexpr size_t ObjectAllocator::kSmallestSpaceSize;

ObjectAllocator::ObjectAllocator(RawHeap& heap, PageBackend& page_backend,
                                 StatsCollector& stats_collector,
                                 PreFinalizerHandler& prefinalizer_handler)
    : raw_heap_(heap),
      page_backend_(page_backend),
      stats_collector_(stats_collector),
      prefinalizer_handler_(prefinalizer_handler) {}

void* ObjectAllocator::OutOfLineAllocate(NormalPageSpace& space, size_t size,
                                         AlignVal alignment,
                                         GCInfoIndex gcinfo) {
  void* memory = OutOfLineAllocateImpl(space, size, alignment, gcinfo);
  stats_collector_.NotifySafePointForConservativeCollection();
  if (prefinalizer_handler_.IsInvokingPreFinalizers()) {
    // Objects allocated during pre finalizers should be allocated as black
    // since marking is already done. Atomics are not needed because there is
    // no concurrent marking in the background.
    HeapObjectHeader::FromObject(memory).MarkNonAtomic();
    // Resetting the allocation buffer forces all further allocations in pre
    // finalizers to go through this slow path.
    ReplaceLinearAllocationBuffer(space, stats_collector_, nullptr, 0);
    prefinalizer_handler_.NotifyAllocationInPrefinalizer(size);
  }
  return memory;
}

void* ObjectAllocator::OutOfLineAllocateImpl(NormalPageSpace& space,
                                             size_t size, AlignVal alignment,
                                             GCInfoIndex gcinfo) {
  DCHECK_EQ(0, size & kAllocationMask);
  DCHECK_LE(kFreeListEntrySize, size);
  // Out-of-line allocation allows for checking this is all situations.
  CHECK(!in_disallow_gc_scope());

  // If this allocation is big enough, allocate a large object.
  if (size >= kLargeObjectSizeThreshold) {
    auto& large_space = LargePageSpace::From(
        *raw_heap_.Space(RawHeap::RegularSpaceType::kLarge));
    // LargePage has a natural alignment that already satisfies
    // `kMaxSupportedAlignment`.
    return AllocateLargeObject(page_backend_, large_space, stats_collector_,
                               size, gcinfo);
  }

  size_t request_size = size;
  // Adjust size to be able to accommodate alignment.
  const size_t dynamic_alignment = static_cast<size_t>(alignment);
  if (dynamic_alignment != kAllocationGranularity) {
    CHECK_EQ(2 * sizeof(HeapObjectHeader), dynamic_alignment);
    request_size += kAllocationGranularity;
  }

  RefillLinearAllocationBuffer(space, request_size);

  // The allocation must succeed, as we just refilled the LAB.
  void* result = (dynamic_alignment == kAllocationGranularity)
                     ? AllocateObjectOnSpace(space, size, gcinfo)
                     : AllocateObjectOnSpace(space, size, alignment, gcinfo);
  CHECK(result);
  return result;
}

void ObjectAllocator::RefillLinearAllocationBuffer(NormalPageSpace& space,
                                                   size_t size) {
  // Try to allocate from the freelist.
  if (RefillLinearAllocationBufferFromFreeList(space, size)) return;

  // Lazily sweep pages of this heap until we find a freed area for this
  // allocation or we finish sweeping all pages of this heap.
  Sweeper& sweeper = raw_heap_.heap()->sweeper();
  // TODO(chromium:1056170): Investigate whether this should be a loop which
  // would result in more agressive re-use of memory at the expense of
  // potentially larger allocation time.
  if (sweeper.SweepForAllocationIfRunning(&space, size)) {
    // Sweeper found a block of at least `size` bytes. Allocation from the
    // free list may still fail as actual  buckets are not exhaustively
    // searched for a suitable block. Instead, buckets are tested from larger
    // sizes that are guaranteed to fit the block to smaller bucket sizes that
    // may only potentially fit the block. For the bucket that may exactly fit
    // the allocation of `size` bytes (no overallocation), only the first
    // entry is checked.
    if (RefillLinearAllocationBufferFromFreeList(space, size)) return;
  }

  sweeper.FinishIfRunning();
  // TODO(chromium:1056170): Make use of the synchronously freed memory.

  auto* new_page = NormalPage::Create(page_backend_, space);
  space.AddPage(new_page);

  // Set linear allocation buffer to new page.
  ReplaceLinearAllocationBuffer(space, stats_collector_,
                                new_page->PayloadStart(),
                                new_page->PayloadSize());
}

bool ObjectAllocator::RefillLinearAllocationBufferFromFreeList(
    NormalPageSpace& space, size_t size) {
  const FreeList::Block entry = space.free_list().Allocate(size);
  if (!entry.address) return false;

  // Assume discarded memory on that page is now zero.
  auto& page = *NormalPage::From(BasePage::FromPayload(entry.address));
  if (page.discarded_memory()) {
    stats_collector_.DecrementDiscardedMemory(page.discarded_memory());
    page.ResetDiscardedMemory();
  }

  ReplaceLinearAllocationBuffer(
      space, stats_collector_, static_cast<Address>(entry.address), entry.size);
  return true;
}

void ObjectAllocator::ResetLinearAllocationBuffers() {
  class Resetter : public HeapVisitor<Resetter> {
   public:
    explicit Resetter(StatsCollector& stats) : stats_collector_(stats) {}

    bool VisitLargePageSpace(LargePageSpace&) { return true; }

    bool VisitNormalPageSpace(NormalPageSpace& space) {
      ReplaceLinearAllocationBuffer(space, stats_collector_, nullptr, 0);
      return true;
    }

   private:
    StatsCollector& stats_collector_;
  } visitor(stats_collector_);

  visitor.Traverse(raw_heap_);
}

void ObjectAllocator::Terminate() {
  ResetLinearAllocationBuffers();
}

bool ObjectAllocator::in_disallow_gc_scope() const {
  return raw_heap_.heap()->in_disallow_gc_scope();
}

}  // namespace internal
}  // namespace cppgc
