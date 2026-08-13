// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMMON_SEGMENTED_TABLE_INL_H_
#define V8_COMMON_SEGMENTED_TABLE_INL_H_

#include "src/common/segmented-table.h"
// Include the non-inl header before the rest of the headers.

#include "src/base/emulated-virtual-address-subspace.h"
#include "src/common/assert-scope.h"
#include "src/sandbox/testing.h"
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

  std::optional<VirtualAddressSpace::MemoryProtectionKeyId> pkey;
  if (kUseContiguousMemory && kIsWriteProtected && ThreadIsolation::Enabled()) {
#if V8_HAS_PKU_JIT_WRITE_PROTECT
    pkey = ThreadIsolation::pkey();
#else
    UNREACHABLE();
#endif  // V8_HAS_PKU_JIT_WRITE_PROTECT
  }

  if (root_space->CanAllocateSubspaces()) {
    auto subspace = root_space->AllocateSubspace(
        VirtualAddressSpace::kNoHint, kReservationSize, kAlignment,
        PagePermissions::kReadWrite, pkey);
    vas_ = subspace.release();
  } else {
    // This may be required on old Windows versions that don't support
    // VirtualAlloc2, which is required for subspaces. In that case, just
    // use a fully-backed emulated subspace.
    DCHECK(!pkey);
    Address reservation_base = root_space->AllocatePages(
        VirtualAddressSpace::kNoHint, kReservationSize, kAlignment,
        PagePermissions::kNoAccess);
    if (reservation_base) {
      vas_ = new base::EmulatedVirtualAddressSubspace(
          root_space, reservation_base, kReservationSize, kReservationSize);
    }
  }
  if (!vas_) {
    V8::FatalProcessOutOfMemory(
        nullptr, "SegmentedTable::InitializeTable (subspace allocation)");
  }
  vas_->SetName(kPointerTableAddressSpaceName);
#ifdef V8_ENABLE_MEMORY_CORRUPTION_API
  // In-sandbox corruption of handles can lead to out-of-bounds accesses into
  // our tables, but these will stay inside the table's reserved region and hit
  // PROT_NONE mappings, so they are safe. They can be both read- and write
  // accesses though as we also write new pointers into existing table entries.
  // Writing is fine as long as the value is not attacker controlled. It's only
  // when we write attacker-controlled data that we (likely) have a security
  // problem (but then it doesn't matter whether it's out-of-bounds or not).
  SandboxTesting::RegisterSafeMemoryRegion(
      vas_->base(), vas_->size(), SandboxTesting::kReadAndWriteAccessIsSafe);
#endif  // V8_ENABLE_MEMORY_CORRUPTION_API
#else
  static_assert(!kUseContiguousMemory);
  vas_ = root_space;
#endif  // V8_TARGET_ARCH_64_BIT

  base_ = reinterpret_cast<Entry*>(vas_->base());
}

template <typename Entry, size_t size>
void SegmentedTable<Entry, size>::TearDown() {
  DCHECK(is_initialized());

#ifdef V8_ENABLE_MEMORY_CORRUPTION_API
  SandboxTesting::UnregisterSafeMemoryRegion(vas_->base());
#endif

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
SegmentedTable<Entry, size>::TryAllocateSegment() {
  Address start =
      vas_->AllocatePages(VirtualAddressSpace::kNoHint, kSegmentSize,
                          kSegmentSize, PagePermissions::kReadWrite);
  if (!start) {
    return {};
  }
  uint32_t offset = static_cast<uint32_t>(start - vas_->base());
  return Segment::At(offset);
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
