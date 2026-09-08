// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/segmented-table.h"

#include "src/base/emulated-virtual-address-subspace.h"
#include "src/utils/allocation.h"

#ifdef V8_ENABLE_MEMORY_CORRUPTION_API
#include "src/sandbox/testing.h"
#endif  // V8_ENABLE_MEMORY_CORRUPTION_API

namespace v8::internal {

void SegmentedTableBase::Initialize(size_t reservation_size,
                                    size_t read_only_reservation_size,
                                    bool wants_write_protection) {
  CHECK_NULL(vas_);
  CHECK_NULL(base_);

  VirtualAddressSpace* root_space = GetPlatformVirtualAddressSpace();

#ifdef V8_TARGET_ARCH_64_BIT
  static_assert(kUseContiguousMemory);
  CHECK(IsAligned(reservation_size, root_space->allocation_granularity()));
  CHECK(IsAligned(read_only_reservation_size,
                  root_space->allocation_granularity()));

  std::optional<VirtualAddressSpace::MemoryProtectionKeyId> pkey;
  if (kUseContiguousMemory && wants_write_protection &&
      ThreadIsolation::Enabled()) {
#if V8_HAS_PKU_JIT_WRITE_PROTECT
    pkey = ThreadIsolation::pkey();
#else
    UNREACHABLE();
#endif  // V8_HAS_PKU_JIT_WRITE_PROTECT
  }

  if (root_space->CanAllocateSubspaces()) [[likely]] {
    auto subspace = root_space->AllocateSubspace(
        VirtualAddressSpace::kNoHint, reservation_size, kAlignment,
        PagePermissions::kReadWrite, pkey);
    vas_ = subspace.release();
  } else {
    // This may be required on old Windows versions that don't support
    // VirtualAlloc2, which is required for subspaces. In that case, just
    // use a fully-backed emulated subspace.
    CHECK(!pkey);
    Address reservation_base = root_space->AllocatePages(
        VirtualAddressSpace::kNoHint, reservation_size, kAlignment,
        PagePermissions::kNoAccess);
    if (reservation_base) {
      vas_ = new base::EmulatedVirtualAddressSubspace(
          root_space, reservation_base, reservation_size, reservation_size);
    }
  }
  if (!vas_) [[unlikely]] {
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
  base_ = reinterpret_cast<void*>(vas_->base());

  if constexpr (kUseContiguousMemory) {
    if (read_only_reservation_size) {
      read_only_reservation_size_ = read_only_reservation_size;
      CHECK_LE(read_only_reservation_size, reservation_size);
      CHECK_EQ(0, read_only_reservation_size % kSegmentSize);

      const Address first_segment =
          vas_->AllocatePages(this->vas_->base(), read_only_reservation_size,
                              kAlignment, PagePermissions::kRead);
      if (first_segment != vas_->base()) [[unlikely]] {
        V8::FatalProcessOutOfMemory(
            nullptr,
            "SegmentedTableBase::Initialize (r/o segments allocation)");
      }
      CHECK_EQ(first_segment - vas_->base(), kInternalReadOnlySegmentsOffset);
    }
  }
}

void SegmentedTableBase::TearDown() {
  CHECK(is_initialized());

#ifdef V8_ENABLE_MEMORY_CORRUPTION_API
  SandboxTesting::UnregisterSafeMemoryRegion(vas_->base());
#endif

  if (read_only_reservation_size_) {
    // Deallocate all read only segments.
    vas_->FreePages(vas_->base(), read_only_reservation_size_);
  }

  base_ = nullptr;
#ifdef V8_TARGET_ARCH_64_BIT
  delete vas_;
#endif
  vas_ = nullptr;
}

void SegmentedTableBase::FreeTableSegment(SegmentBase segment) {
  const Address segment_start = vas_->base() + segment.offset();
  vas_->FreePages(segment_start, kSegmentSize);
}

std::optional<SegmentedTableBase::SegmentBase>
SegmentedTableBase::TryAllocateSegment() {
  const Address start =
      vas_->AllocatePages(VirtualAddressSpace::kNoHint, kSegmentSize,
                          kSegmentSize, PagePermissions::kReadWrite);
  if (!start) [[unlikely]] {
    return {};
  }
  const uint32_t offset = static_cast<uint32_t>(start - vas_->base());
  auto segment = SegmentBase::At(offset);
  DCHECK_IMPLIES(
      kUseContiguousMemory && read_only_reservation_size_,
      segment.number() >= read_only_reservation_size_ / kSegmentSize);
  return segment;
}

SegmentedTableBase::SegmentBase SegmentedTableBase::AllocateReadOnlySegment() {
  // If this check fails during snapshot generation increase the read-only
  // reservation part of the table.
  CHECK_LT(read_only_segments_used_ * kSegmentSize,
           read_only_reservation_size_);
  return SegmentBase::At(kInternalReadOnlySegmentsOffset +
                         kSegmentSize * read_only_segments_used_++);
}

uint32_t SegmentedTableBase::ResetReadOnlySegments() {
  return std::exchange(read_only_segments_used_, 0);
}

void SegmentedTableBase::UnsealReadOnlySegments() {
  CHECK(is_initialized());
  if (read_only_reservation_size_ == 0) return;
  const bool success = this->vas_->SetPagePermissions(
      this->vas_->base(), read_only_reservation_size_,
      PagePermissions::kReadWrite);
  CHECK(success);
}

void SegmentedTableBase::SealReadOnlySegments() {
  CHECK(is_initialized());
  if (read_only_reservation_size_ == 0) return;
  const bool success = this->vas_->SetPagePermissions(
      this->vas_->base(), read_only_reservation_size_, PagePermissions::kRead);
  CHECK(success);
}

}  // namespace v8::internal
