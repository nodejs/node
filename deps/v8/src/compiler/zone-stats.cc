// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/zone-stats.h"

namespace v8 {
namespace internal {
namespace compiler {

ZoneStats::StatsScope::StatsScope(ZoneStats* zone_stats)
    : zone_stats_(zone_stats),
      total_allocated_bytes_at_start_(zone_stats->GetTotalAllocatedBytes()),
      max_allocated_bytes_(0) {
  zone_stats_->stats_.push_back(this);
  for (Zone* zone : zone_stats_->zones_) {
    size_t size = static_cast<size_t>(zone->allocation_size());
    std::pair<InitialValues::iterator, bool> res =
        initial_values_.insert(std::make_pair(zone, size));
    USE(res);
    DCHECK(res.second);
  }
}

ZoneStats::StatsScope::~StatsScope() {
  DCHECK_EQ(zone_stats_->stats_.back(), this);
  zone_stats_->stats_.pop_back();
}

size_t ZoneStats::StatsScope::GetMaxAllocatedBytes() {
  return std::max(max_allocated_bytes_, GetCurrentAllocatedBytes());
}

size_t ZoneStats::StatsScope::GetCurrentAllocatedBytes() {
  size_t total = 0;
  for (Zone* zone : zone_stats_->zones_) {
    total += static_cast<size_t>(zone->allocation_size());
    // Adjust for initial values.
    InitialValues::iterator it = initial_values_.find(zone);
    if (it != initial_values_.end()) {
      total -= it->second;
    }
  }
  return total;
}

size_t ZoneStats::StatsScope::GetTotalAllocatedBytes() {
  return zone_stats_->GetTotalAllocatedBytes() -
         total_allocated_bytes_at_start_;
}

void ZoneStats::StatsScope::ZoneReturned(Zone* zone) {
  size_t current_total = GetCurrentAllocatedBytes();
  // Update max.
  max_allocated_bytes_ = std::max(max_allocated_bytes_, current_total);
  // Drop zone from initial value map.
  InitialValues::iterator it = initial_values_.find(zone);
  if (it != initial_values_.end()) {
    initial_values_.erase(it);
  }
}

ZoneStats::ZoneStats(AccountingAllocator* allocator)
    : max_allocated_bytes_(0), total_deleted_bytes_(0), allocator_(allocator) {}

ZoneStats::~ZoneStats() {
  DCHECK(zones_.empty());
  DCHECK(stats_.empty());
}

size_t ZoneStats::GetMaxAllocatedBytes() const {
  return std::max(max_allocated_bytes_, GetCurrentAllocatedBytes());
}

size_t ZoneStats::GetCurrentAllocatedBytes() const {
  size_t total = 0;
  for (Zone* zone : zones_) {
    total += static_cast<size_t>(zone->allocation_size());
  }
  return total;
}

size_t ZoneStats::GetTotalAllocatedBytes() const {
  return total_deleted_bytes_ + GetCurrentAllocatedBytes();
}

Zone* ZoneStats::NewEmptyZone(const char* zone_name) {
  Zone* zone = new Zone(allocator_, zone_name);
  zones_.push_back(zone);
  return zone;
}

void ZoneStats::ReturnZone(Zone* zone) {
  size_t current_total = GetCurrentAllocatedBytes();
  // Update max.
  max_allocated_bytes_ = std::max(max_allocated_bytes_, current_total);
  // Update stats.
  for (StatsScope* stat_scope : stats_) {
    stat_scope->ZoneReturned(zone);
  }
  // Remove from used.
  Zones::iterator it = std::find(zones_.begin(), zones_.end(), zone);
  DCHECK(it != zones_.end());
  zones_.erase(it);
  total_deleted_bytes_ += static_cast<size_t>(zone->allocation_size());
  delete zone;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
