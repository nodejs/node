// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_SEGMENTED_TABLE_INL_H_
#define V8_COMMON_SEGMENTED_TABLE_INL_H_

#include "src/common/segmented-table.h"
// Include the non-inl header before the rest of the headers.

#include "src/base/emulated-virtual-address-subspace.h"
#include "src/common/assert-scope.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

template <typename Entry, size_t size>
typename SegmentedTable<Entry, size>::Segment
SegmentedTable<Entry, size>::Segment::At(uint32_t offset) {
  DCHECK(IsAligned(offset, kSegmentSize));
  uint32_t number = offset / kSegmentSize;
  return Segment(number);
}

template <typename Entry, size_t size>
typename SegmentedTable<Entry, size>::Segment
SegmentedTable<Entry, size>::Segment::Containing(uint32_t entry_index) {
  uint32_t number = entry_index / kEntriesPerSegment;
  return Segment(number);
}

template <typename Entry, size_t size>
Entry& SegmentedTable<Entry, size>::at(uint32_t index) {
  return base_[index];
}

template <typename Entry, size_t size>
const Entry& SegmentedTable<Entry, size>::at(uint32_t index) const {
  return base_[index];
}

template <typename Entry, size_t size>
typename SegmentedTable<Entry, size>::WriteIterator
SegmentedTable<Entry, size>::iter_at(uint32_t index) {
  return WriteIterator(base_, index);
}

template <typename Entry, size_t size>
bool SegmentedTable<Entry, size>::is_initialized() const {
  DCHECK(!base_ || reinterpret_cast<Address>(base_) == vas_->base());
  return vas_ != nullptr;
}

template <typename Entry, size_t size>
Address SegmentedTable<Entry, size>::base() const {
  DCHECK(is_initialized());
  return reinterpret_cast<Address>(base_);
}

template <typename Entry, size_t size>
void SegmentedTable<Entry, size>::Initialize() {
  DCHECK(!is_initialized());
  DCHECK_EQ(vas_, nullptr);

  VirtualAddressSpace* root_space = GetPlatformVirtualAddressSpace();

#ifdef V8_TARGET_ARCH_64_BIT
  static_assert(kUseContiguousMemory);
  DCHECK(IsAligned(kReservationSize, root_space->allocation_granularity()));

  if (root_space->CanAllocateSubspaces()) {
    auto subspace = root_space->AllocateSubspace(VirtualAddressSpace::kNoHint,
                                                 kReservationSize, kAlignment,
                                                 PagePermissions::kReadWrite);
    vas_ = subspace.release();
    if (kUseSegmentPool) {
      segment_pool_grow_mutex_ = new base::Mutex();
    }
  } else {
    // This may be required on old Windows versions that don't support
    // VirtualAlloc2, which is required for subspaces. In that case, just use a
    // fully-backed emulated subspace.
    Address reservation_base = root_space->AllocatePages(
        VirtualAddressSpace::kNoHint, kReservationSize, kAlignment,
        PagePermissions::kNoAccess);
    if (reservation_base) {
      vas_ = new base::EmulatedVirtualAddressSubspace(
          root_space, reservation_base, kReservationSize, kReservationSize);
    }
    // EmulatedVirtualAddressSubspace does not suppert AllocatePagesArray.
    DCHECK(!kUseSegmentPool);
  }
  if (!vas_) {
    V8::FatalProcessOutOfMemory(
        nullptr, "SegmentedTable::InitializeTable (subspace allocation)");
  }
#else  // V8_TARGET_ARCH_64_BIT
  static_assert(!kUseContiguousMemory);
  vas_ = root_space;
#endif

  base_ = reinterpret_cast<Entry*>(vas_->base());

  if constexpr (kUseContiguousMemory && kIsWriteProtected) {
    CHECK(ThreadIsolation::WriteProtectMemory(
        base(), size, PageAllocator::Permission::kNoAccess));
  }
}

template <typename Entry, size_t size>
void SegmentedTable<Entry, size>::TearDown() {
  DCHECK(is_initialized());

  if (segment_pool_grow_mutex_) {
    delete segment_pool_grow_mutex_;
  }

  base_ = nullptr;
#ifdef V8_TARGET_ARCH_64_BIT
  delete vas_;
#endif
  vas_ = nullptr;
}

template <typename Entry, size_t size>
typename SegmentedTable<Entry, size>::FreelistHead
SegmentedTable<Entry, size>::InitializeFreeList(Segment segment,
                                                uint32_t start_offset) {
  DCHECK_LT(start_offset, kEntriesPerSegment);
  uint32_t num_entries = kEntriesPerSegment - start_offset;

  uint32_t first = segment.first_entry() + start_offset;
  uint32_t last = segment.last_entry();
  {
    WriteIterator it = iter_at(first);
    while (it.index() != last) {
      it->MakeFreelistEntry(it.index() + 1);
      ++it;
    }
    it->MakeFreelistEntry(0);
  }

  return FreelistHead(first, num_entries);
}

template <typename Entry, size_t size>
std::optional<typename SegmentedTable<Entry, size>::Segment>
SegmentedTable<Entry, size>::TryGetSegmentFromPool() {
  DCHECK(kUseSegmentPool);
  for (int i = 0; i < static_cast<int>(kSegmentPoolSize); ++i) {
    uint32_t segment = segment_pool_[i].load(std::memory_order_relaxed);
    if (segment != 0) {
      if (segment_pool_[i].compare_exchange_weak(segment, 0,
                                                 std::memory_order_acq_rel)) {
        return Segment::At(segment);
      } else {
        // Retry if CAS failed. This is needed to ensure the pool really is
        // empty when this method returns {}.
        --i;
      }
    }
  }
  return {};
}

template <typename Entry, size_t size>
std::optional<typename SegmentedTable<Entry, size>::Segment>
SegmentedTable<Entry, size>::TryAllocateSegment() {
  if constexpr (!kUseSegmentPool) {
    Address start =
        vas_->AllocatePages(VirtualAddressSpace::kNoHint, kSegmentSize,
                            kSegmentSize, PagePermissions::kReadWrite);
    if (!start) {
      return {};
    }
    uint32_t offset = static_cast<uint32_t>(start - vas_->base());
    return Segment::At(offset);
  }

  if (auto segment = TryGetSegmentFromPool()) {
    return segment;
  }

  base::MutexGuard guard(*segment_pool_grow_mutex_);
  // Try again to get a segment from the pool with the mutex held. Either
  // another thread filled the pool before us and we should get a segment here,
  // or the pool is guaranteed to be empty and we can proceed to fill it.
  if (auto segment = TryGetSegmentFromPool()) {
    return segment;
  }

  return FillSegmentsPool(true);
}

template <typename Entry, size_t size>
std::optional<typename SegmentedTable<Entry, size>::Segment>
SegmentedTable<Entry, size>::FillSegmentsPool(bool return_a_segment) {
  std::optional<Segment> res;
  for (size_t i = 0; i < kSegmentPoolSize; ++i) {
    DCHECK_EQ(segment_pool_[i].load(std::memory_order_acquire), 0);
    Address start =
        vas_->AllocatePages(VirtualAddressSpace::kNoHint, kSegmentSize,
                            kAlignment, PagePermissions::kReadWrite);
    if (!start) continue;
    uint32_t offset = static_cast<uint32_t>(start - vas_->base());
    if (return_a_segment && i == 0) {
      res.emplace(Segment::At(offset));
      continue;
    }
    DCHECK_NE(offset, 0);
    segment_pool_[i].store(offset, std::memory_order_release);
  }
  return res;
}

template <typename Entry, size_t size>
std::pair<typename SegmentedTable<Entry, size>::Segment,
          typename SegmentedTable<Entry, size>::FreelistHead>
SegmentedTable<Entry, size>::AllocateAndInitializeSegment() {
  if (auto res = TryAllocateAndInitializeSegment()) {
    return *res;
  }
  V8::FatalProcessOutOfMemory(nullptr,
                              "SegmentedTable::AllocateAndInitializeSegment");
}

template <typename Entry, size_t size>
std::optional<std::pair<typename SegmentedTable<Entry, size>::Segment,
                        typename SegmentedTable<Entry, size>::FreelistHead>>
SegmentedTable<Entry, size>::TryAllocateAndInitializeSegment() {
  auto segment = TryAllocateSegment();
  if (!segment) return {};
  DCHECK_IMPLIES(!kUseContiguousMemory,
                 (*segment).number() > kNumReadOnlySegments);
  FreelistHead freelist = InitializeFreeList(*segment);
  return {{*segment, freelist}};
}

template <typename Entry, size_t size>
void SegmentedTable<Entry, size>::FreeTableSegment(Segment segment) {
  if (kUseSegmentPool) {
    // Offset 0 is used as a marker for free entries. Thus we cannot store it in
    // the pool.
    if (segment.offset() != 0) {
      base::MutexGuard guard(*segment_pool_grow_mutex_);
      for (size_t i = 0; i < kSegmentPoolSize; ++i) {
        uint32_t cur = segment_pool_[i].load(std::memory_order_relaxed);
        if (cur == 0) {
          if (segment_pool_[i].compare_exchange_weak(
                  cur, segment.offset(), std::memory_order_acq_rel)) {
            return;
          }
        }
      }
    }
  }
  Address segment_start = vas_->base() + segment.offset();
  vas_->FreePages(segment_start, kSegmentSize);
}

template <typename Entry, size_t size>
SegmentedTable<Entry, size>::WriteIterator::WriteIterator(Entry* base,
                                                          uint32_t index)
    : base_(base), index_(index), write_scope_("pointer table write") {}

}  // namespace internal
}  // namespace v8

#endif  // V8_COMMON_SEGMENTED_TABLE_INL_H_
