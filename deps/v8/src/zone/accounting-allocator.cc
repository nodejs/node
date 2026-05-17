// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/zone/accounting-allocator.h"

#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/heap/memory-pool.h"
#include "src/tracing/tracing-category-observer.h"
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
    // Only consult the pool if the size is exactly the zone page size. Larger
    // sizes may be required and just bypass the pool.
    if (bytes == kMinZonePageSize) {
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

  static base::AllocationResult<void*> AllocateSegmentWithRetry(
      Isolate* isolate, size_t bytes) {
    static constexpr size_t kAllocationTries = 2u;
    base::AllocationResult<void*> result = {nullptr, 0u};
    for (size_t i = 0; i < kAllocationTries; ++i) {
      result = AllocateSegment(isolate, bytes);
      if (V8_LIKELY(result.ptr != nullptr)) break;
      OnCriticalMemoryPressure();
    }
    return result;
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
#if defined(V8_HOST_ARCH_64_BIT)
  static constexpr size_t kMinZonePageSize = 512 * KB;
#else
  // For 32-bit platforms, use smaller reservation size to avoid virtual memory
  // exhaustion in scenarios when many zones get created, e.g. when compiling a
  // large number of Wasm modules.
  static constexpr size_t kMinZonePageSize = 256 * KB;
#endif
  static constexpr size_t kMinZonePageAlignment = 16 * KB;
};

}  // namespace

AccountingAllocator::AccountingAllocator() : AccountingAllocator(nullptr) {}

AccountingAllocator::AccountingAllocator(Isolate* isolate)
    : isolate_(isolate) {}

AccountingAllocator::~AccountingAllocator() = default;

Segment* AccountingAllocator::AllocateSegment(size_t requested_bytes) {
  base::AllocationResult<void*> memory;
  const bool use_managed_memory_for_isolate =
      (isolate_ != nullptr ||
       v8_flags.managed_zone_memory_for_isolate_independent_memory);
  if (v8_flags.managed_zone_memory && use_managed_memory_for_isolate) {
    memory = ManagedZones::AllocateSegmentWithRetry(isolate_, requested_bytes);
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
  const bool use_managed_memory_for_isolate =
      (isolate_ != nullptr ||
       v8_flags.managed_zone_memory_for_isolate_independent_memory);
  if (v8_flags.managed_zone_memory && use_managed_memory_for_isolate) {
    ManagedZones::ReturnSegment(isolate_, segment, segment_size);
  } else {
    free(segment);
  }
}

void TracingAccountingAllocator::TraceZoneCreationImpl(const Zone* zone) {
  base::MutexGuard lock(&mutex_);
  active_zones_.insert(zone);
  nesting_depth_++;
}

void TracingAccountingAllocator::TraceZoneDestructionImpl(const Zone* zone) {
  base::MutexGuard lock(&mutex_);
#ifdef V8_ENABLE_PRECISE_ZONE_STATS
  if (v8_flags.trace_zone_type_stats) {
    type_stats_.MergeWith(zone->type_stats());
  }
#endif
  UpdateMemoryTrafficAndReportMemoryUsage(zone->segment_bytes_allocated());
  active_zones_.erase(zone);
  nesting_depth_--;

#ifdef V8_ENABLE_PRECISE_ZONE_STATS
  if (v8_flags.trace_zone_type_stats && active_zones_.empty()) {
    type_stats_.Dump();
  }
#endif
}

void TracingAccountingAllocator::TraceAllocateSegmentImpl(
    v8::internal::Segment* segment) {
  base::MutexGuard lock(&mutex_);
  UpdateMemoryTrafficAndReportMemoryUsage(segment->total_size());
}

void TracingAccountingAllocator::UpdateMemoryTrafficAndReportMemoryUsage(
    size_t memory_traffic_delta) {
  if (!v8_flags.trace_zone_stats &&
      !(TracingFlags::zone_stats.load(std::memory_order_relaxed) &
        v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING)) {
    // Don't print anything if the zone tracing was enabled only because of
    // v8_flags.trace_zone_type_stats.
    return;
  }

  memory_traffic_since_last_report_ += memory_traffic_delta;
  if (memory_traffic_since_last_report_ < v8_flags.zone_stats_tolerance) return;
  memory_traffic_since_last_report_ = 0;

  std::string trace_str = Dump(true);
  if (v8_flags.trace_zone_stats) {
    PrintF(
        "{"
        "\"type\": \"v8-zone-trace\", "
        "\"stats\": %s"
        "}\n",
        trace_str.c_str());
  }
  if (V8_UNLIKELY(TracingFlags::zone_stats.load(std::memory_order_relaxed) &
                  v8::tracing::TracingCategoryObserver::ENABLED_BY_TRACING)) {
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("v8.zone_stats"),
                         "V8.Zone_Stats", TRACE_EVENT_SCOPE_THREAD, "stats",
                         TRACE_STR_COPY(trace_str.c_str()));
  }
}

std::string TracingAccountingAllocator::Dump(bool dump_details) {
  std::ostringstream out;

  // Note: Neither isolate nor zones are locked, so be careful with accesses
  // as the allocator is potentially used on a concurrent thread.
  if (isolate()) {
    double time = isolate()->time_millis_since_init();
    out << "{" << "\"isolate\": \"" << reinterpret_cast<void*>(isolate())
        << "\", " << "\"time\": " << time << ", ";
  } else {
    out << "{" << "\"isolate\": null, ";
  }
  size_t total_segment_bytes_allocated = 0;
  size_t total_zone_allocation_size = 0;
  size_t total_zone_freed_size = 0;

  if (dump_details) {
    // Print detailed zone stats if memory usage changes direction.
    out << "\"zones\": [";
    bool first = true;
    for (const Zone* zone : active_zones_) {
      size_t zone_segment_bytes_allocated = zone->segment_bytes_allocated();
      size_t zone_allocation_size = zone->allocation_size_for_tracing();
      size_t freed_size = zone->freed_size_for_tracing();
      if (first) {
        first = false;
      } else {
        out << ", ";
      }
      out << "{" << "\"name\": \"" << zone->name() << "\", "
          << "\"allocated\": " << zone_segment_bytes_allocated << ", "
          << "\"used\": " << zone_allocation_size << ", "
          << "\"freed\": " << freed_size << "}";
      total_segment_bytes_allocated += zone_segment_bytes_allocated;
      total_zone_allocation_size += zone_allocation_size;
      total_zone_freed_size += freed_size;
    }
    out << "], ";
  } else {
    // Just calculate total allocated/used memory values.
    for (const Zone* zone : active_zones_) {
      total_segment_bytes_allocated += zone->segment_bytes_allocated();
      total_zone_allocation_size += zone->allocation_size_for_tracing();
      total_zone_freed_size += zone->freed_size_for_tracing();
    }
  }
  out << "\"allocated\": " << total_segment_bytes_allocated << ", "
      << "\"used\": " << total_zone_allocation_size << ", "
      << "\"freed\": " << total_zone_freed_size << "}";

  return std::move(out).str();
}

}  // namespace internal
}  // namespace v8
