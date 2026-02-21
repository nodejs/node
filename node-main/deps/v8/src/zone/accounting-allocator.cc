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
#include "src/heap/memory-pool.h"
#include "src/utils/allocation.h"
#include "src/zone/zone-segment.h"

namespace v8 {
namespace internal {

namespace {

class ManagedZones final {
 public:
  static std::optional<VirtualMemory> GetOrCreateMemoryForSegment(
      Isolate* isolate, size_t bytes) {
    DCHECK_EQ(0, bytes % kMinZonePageSize);
    // Only consult the pool if we have an isolate and the size is exactly the
    // zone page size. Larger sizes may be required and just bypass the pool.
    if (isolate && bytes == kMinZonePageSize) {
      auto maybe_reservation =
          IsolateGroup::current()->memory_pool()->RemoveZoneReservation(
              isolate);
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
      IsolateGroup::current()->memory_pool()->AddZoneReservation(
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

AccountingAllocator::AccountingAllocator(Isolate* isolate)
    : isolate_(isolate) {}

AccountingAllocator::~AccountingAllocator() = default;

Segment* AccountingAllocator::AllocateSegment(size_t requested_bytes) {
  base::AllocationResult<void*> memory;
  if (v8_flags.managed_zone_memory && isolate_) {
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

void AccountingAllocator::ReturnSegment(Segment* segment) {
  segment->ZapContents();
  size_t segment_size = segment->total_size();
  current_memory_usage_.fetch_sub(segment_size, std::memory_order_relaxed);
  segment->ZapHeader();
  if (isolate_ && v8_flags.managed_zone_memory) {
    ManagedZones::ReturnSegment(isolate_, segment, segment_size);
  } else {
    free(segment);
  }
}

}  // namespace internal
}  // namespace v8
