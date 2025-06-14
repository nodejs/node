// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/zone/accounting-allocator.h"

#include <memory>

#include "src/base/bounded-page-allocator.h"
#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/heap/page-pool.h"
#include "src/utils/allocation.h"
#include "src/zone/zone-compression.h"
#include "src/zone/zone-segment.h"

namespace v8 {
namespace internal {

// These definitions are here in order to please the linker, which in debug mode
// sometimes requires static constants to be defined in .cc files.
const size_t ZoneCompression::kReservationSize;
const size_t ZoneCompression::kReservationAlignment;

namespace {

// TODO(409953791): Deprecated and should be removed.
class CompressedZones final {
 public:
  static VirtualMemory ReserveAddressSpace(
      v8::PageAllocator* platform_allocator) {
    DCHECK(IsAligned(ZoneCompression::kReservationSize,
                     platform_allocator->AllocatePageSize()));

    void* hint = reinterpret_cast<void*>(RoundDown(
        reinterpret_cast<uintptr_t>(platform_allocator->GetRandomMmapAddr()),
        ZoneCompression::kReservationAlignment));

    VirtualMemory memory(platform_allocator, ZoneCompression::kReservationSize,
                         v8::PageAllocator::AllocationHint().WithAddress(hint),
                         ZoneCompression::kReservationAlignment);
    if (memory.IsReserved()) {
      CHECK(
          IsAligned(memory.address(), ZoneCompression::kReservationAlignment));
      return memory;
    }

    base::FatalOOM(base::OOMType::kProcess,
                   "Failed to reserve memory for compressed zones");
    UNREACHABLE();
  }

  static std::unique_ptr<v8::base::BoundedPageAllocator> CreateBoundedAllocator(
      v8::PageAllocator* platform_allocator, Address reservation_start) {
    CHECK(reservation_start);
    CHECK(IsAligned(reservation_start, ZoneCompression::kReservationAlignment));

    auto allocator = std::make_unique<v8::base::BoundedPageAllocator>(
        platform_allocator, reservation_start,
        ZoneCompression::kReservationSize, kZonePageSize,
        base::PageInitializationMode::kAllocatedPagesCanBeUninitialized,
        base::PageFreeingMode::kMakeInaccessible);

    // Exclude first page from allocation to ensure that accesses through
    // decompressed null pointer will seg-fault.
    allocator->AllocatePagesAt(reservation_start, kZonePageSize,
                               v8::PageAllocator::kNoAccess);
    return allocator;
  }

  static std::pair<std::unique_ptr<VirtualMemory>,
                   std::unique_ptr<base::BoundedPageAllocator>>
  Initialize() {
    v8::PageAllocator* platform_page_allocator = GetPlatformPageAllocator();
    VirtualMemory memory =
        CompressedZones::ReserveAddressSpace(platform_page_allocator);

    auto virtual_memory = std::make_unique<VirtualMemory>(std::move(memory));
    auto bounded_page_allocator = CompressedZones::CreateBoundedAllocator(
        platform_page_allocator, virtual_memory->address());
    return std::make_pair(std::move(virtual_memory),
                          std::move(bounded_page_allocator));
  }

  static base::AllocationResult<void*> AllocateSegment(
      v8::base::BoundedPageAllocator* bounded_page_allocator, size_t bytes) {
    bytes = RoundUp(bytes, CompressedZones::kZonePageSize);
    void* memory = AllocatePages(bounded_page_allocator, bytes,
                                 CompressedZones::kZonePageSize,
                                 PageAllocator::kReadWrite);
    return {memory, bytes};
  }

  static void ReturnSegment(
      v8::base::BoundedPageAllocator* bounded_page_allocator, void* memory,
      size_t bytes) {
    FreePages(bounded_page_allocator, memory, bytes);
  }

 private:
  static constexpr size_t kZonePageSize = 256 * KB;
};

class ManagedZones final {
 public:
  static std::optional<VirtualMemory> GetOrCreateMemoryForSegment(
      Isolate* isolate, size_t bytes) {
    DCHECK_EQ(0, bytes % kMinZonePageSize);
    // Only consult the pool if we have an isolate and the size is exactly the
    // zone page size. Larger sizes may be required and just bypass the pool.
    if (isolate && bytes == kMinZonePageSize) {
      auto maybe_reservation =
          IsolateGroup::current()->page_pool()->RemoveZoneReservation(isolate);
      if (maybe_reservation) {
        return maybe_reservation;
      }
    }

    v8::PageAllocator* platform_page_allocator = GetPlatformPageAllocator();
    VirtualMemory memory(platform_page_allocator, bytes,
                         v8::PageAllocator::AllocationHint(),
                         kMinZonePageAlignment, v8::PageAllocator::kReadWrite);
    if (V8_UNLIKELY(!memory.IsReserved())) {
      return std::nullopt;
    }
    CHECK(IsAligned(memory.address(), kMinZonePageAlignment));
    return memory;
  }

  static base::AllocationResult<void*> AllocateSegment(Isolate* isolate,
                                                       size_t bytes) {
    static constexpr size_t kMaxSize = size_t{2} * GB;
    if (bytes >= kMaxSize) {
      return {nullptr, bytes};
    }
    void* memory = nullptr;
    bytes = RoundUp(bytes, ManagedZones::kMinZonePageSize);
    std::optional<VirtualMemory> maybe_reservation =
        ManagedZones::GetOrCreateMemoryForSegment(isolate, bytes);
    if (V8_LIKELY(maybe_reservation)) {
      VirtualMemory reservation = std::move(maybe_reservation.value());
      DCHECK(reservation.IsReserved());
      DCHECK_EQ(reservation.size(), bytes);
      memory = reinterpret_cast<void*>(reservation.address());
      // Don't let the reservation be freed by destructor. From now on the
      // reservation will be managed via a pair {memory, bytes}.
      reservation.Reset();
    }
    return {memory, bytes};
  }

  static void ReturnSegment(Isolate* isolate, void* memory, size_t bytes) {
    VirtualMemory reservation(GetPlatformPageAllocator(),
                              reinterpret_cast<Address>(memory), bytes);
    if (reservation.size() == ManagedZones::kMinZonePageSize) {
      IsolateGroup::current()->page_pool()->AddZoneReservation(
          isolate, std::move(reservation));
    }
    // Reservation will be automatically freed here otherwise.
  }

 private:
  static constexpr size_t kMinZonePageSize = 512 * KB;
  static constexpr size_t kMinZonePageAlignment = 16 * KB;
};

}  // namespace

AccountingAllocator::AccountingAllocator() : AccountingAllocator(nullptr) {}

AccountingAllocator::AccountingAllocator(Isolate* isolate) : isolate_(isolate) {
  CHECK_IMPLIES(COMPRESS_ZONES_BOOL, !v8_flags.managed_zone_memory);
  if (COMPRESS_ZONES_BOOL) {
    std::tie(reserved_area_, bounded_page_allocator_) =
        CompressedZones::Initialize();
  }
}

AccountingAllocator::~AccountingAllocator() = default;

Segment* AccountingAllocator::AllocateSegment(size_t requested_bytes,
                                              bool supports_compression) {
  base::AllocationResult<void*> memory;
  if (COMPRESS_ZONES_BOOL && supports_compression) {
    memory = CompressedZones::AllocateSegment(bounded_page_allocator_.get(),
                                              requested_bytes);

  } else if (v8_flags.managed_zone_memory && isolate_) {
    memory = ManagedZones::AllocateSegment(isolate_, requested_bytes);
  } else {
    memory = AllocAtLeastWithRetry(requested_bytes);
  }
  if (V8_UNLIKELY(memory.ptr == nullptr)) {
    return nullptr;
  }

  size_t current =
      current_memory_usage_.fetch_add(memory.count, std::memory_order_relaxed) +
      memory.count;
  size_t max = max_memory_usage_.load(std::memory_order_relaxed);
  while (current > max && !max_memory_usage_.compare_exchange_weak(
                              max, current, std::memory_order_relaxed)) {
    // {max} was updated by {compare_exchange_weak}; retry.
  }
  DCHECK_LE(sizeof(Segment), memory.count);
  return new (memory.ptr) Segment(memory.count);
}

void AccountingAllocator::ReturnSegment(Segment* segment,
                                        bool supports_compression) {
  segment->ZapContents();
  size_t segment_size = segment->total_size();
  current_memory_usage_.fetch_sub(segment_size, std::memory_order_relaxed);
  segment->ZapHeader();
  if (COMPRESS_ZONES_BOOL && supports_compression) {
    CompressedZones::ReturnSegment(bounded_page_allocator_.get(), segment,
                                   segment_size);
  } else if (isolate_ && v8_flags.managed_zone_memory) {
    ManagedZones::ReturnSegment(isolate_, segment, segment_size);
  } else {
    free(segment);
  }
}

}  // namespace internal
}  // namespace v8
