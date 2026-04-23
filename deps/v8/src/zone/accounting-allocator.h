// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ZONE_ACCOUNTING_ALLOCATOR_H_
#define V8_ZONE_ACCOUNTING_ALLOCATOR_H_

#include <atomic>
#include <unordered_set>

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/logging/tracing-flags.h"

namespace v8 {

namespace base {
class BoundedPageAllocator;
}  // namespace base

namespace internal {

class Isolate;
class Segment;
class VirtualMemory;
class Zone;

class V8_EXPORT_PRIVATE AccountingAllocator {
 public:
  // TODO(409953791): Consider making this constructor private. We have several
  // places where we create an ad-hoc `AccountingAllocator` only for creating a
  // single, temporary Zone where we should instead use some already available
  // allocator. Increasing the API friction might help reducing the number of
  // allocator instances.
  AccountingAllocator();
  explicit AccountingAllocator(Isolate* isolate);
  AccountingAllocator(const AccountingAllocator&) = delete;
  AccountingAllocator& operator=(const AccountingAllocator&) = delete;
  virtual ~AccountingAllocator();

  // Allocates a new segment. Returns nullptr on failed allocation.
  Segment* AllocateSegment(size_t bytes);

  // Return unneeded segments to either insert them into the pool or release
  // them if the pool is already full or memory pressure is high.
  void ReturnSegment(Segment* memory);

  size_t GetCurrentMemoryUsage() const {
    return current_memory_usage_.load(std::memory_order_relaxed);
  }

  size_t GetMaxMemoryUsage() const {
    return max_memory_usage_.load(std::memory_order_relaxed);
  }

  void TraceZoneCreation(const Zone* zone) {
    if (V8_LIKELY(!TracingFlags::is_zone_stats_enabled())) return;
    TraceZoneCreationImpl(zone);
  }

  void TraceZoneDestruction(const Zone* zone) {
    if (V8_LIKELY(!TracingFlags::is_zone_stats_enabled())) return;
    TraceZoneDestructionImpl(zone);
  }

  void TraceAllocateSegment(Segment* segment) {
    if (V8_LIKELY(!TracingFlags::is_zone_stats_enabled())) return;
    TraceAllocateSegmentImpl(segment);
  }

 protected:
  virtual void TraceZoneCreationImpl(const Zone* zone) {}
  virtual void TraceZoneDestructionImpl(const Zone* zone) {}
  virtual void TraceAllocateSegmentImpl(Segment* segment) {}

  Isolate* isolate() const { return isolate_; }

 private:
  Isolate* const isolate_ = nullptr;
  std::atomic<size_t> current_memory_usage_{0};
  std::atomic<size_t> max_memory_usage_{0};
};

// TODO(479122452): Consider merging this into `AccountingAllocator`,
// or at least use `TracingAccountingAllocator` in more places instead.
// As of 2026-01, we are often creating Zones from `AccountingAllocator`s,
// which then don't appear in the tracing output of `--trace-zone-stats`.
class TracingAccountingAllocator : public AccountingAllocator {
 public:
  explicit TracingAccountingAllocator(Isolate* isolate)
      : AccountingAllocator(isolate) {}
  ~TracingAccountingAllocator() = default;

 protected:
  void TraceZoneCreationImpl(const Zone* zone) override;
  void TraceZoneDestructionImpl(const Zone* zone) override;
  void TraceAllocateSegmentImpl(Segment* segment) override;

 private:
  void UpdateMemoryTrafficAndReportMemoryUsage(size_t memory_traffic_delta);
  std::string Dump(bool dump_details);

  std::atomic<size_t> nesting_depth_{0};

  base::Mutex mutex_;
  std::unordered_set<const Zone*> active_zones_;
#ifdef V8_ENABLE_PRECISE_ZONE_STATS
  TypeStats type_stats_;
#endif
  // This value is increased on both allocations and deallocations.
  size_t memory_traffic_since_last_report_ = 0;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ZONE_ACCOUNTING_ALLOCATOR_H_
