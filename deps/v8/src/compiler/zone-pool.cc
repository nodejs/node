// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/zone-pool.h"

namespace v8 {
namespace internal {
namespace compiler {

ZonePool::StatsScope::StatsScope(ZonePool* zone_pool)
    : zone_pool_(zone_pool),
      total_allocated_bytes_at_start_(zone_pool->GetTotalAllocatedBytes()),
      max_allocated_bytes_(0) {
  zone_pool_->stats_.push_back(this);
  for (auto zone : zone_pool_->used_) {
    size_t size = static_cast<size_t>(zone->allocation_size());
    std::pair<InitialValues::iterator, bool> res =
        initial_values_.insert(std::make_pair(zone, size));
    USE(res);
    DCHECK(res.second);
  }
}


ZonePool::StatsScope::~StatsScope() {
  DCHECK_EQ(zone_pool_->stats_.back(), this);
  zone_pool_->stats_.pop_back();
}


size_t ZonePool::StatsScope::GetMaxAllocatedBytes() {
  return std::max(max_allocated_bytes_, GetCurrentAllocatedBytes());
}


size_t ZonePool::StatsScope::GetCurrentAllocatedBytes() {
  size_t total = 0;
  for (Zone* zone : zone_pool_->used_) {
    total += static_cast<size_t>(zone->allocation_size());
    // Adjust for initial values.
    InitialValues::iterator it = initial_values_.find(zone);
    if (it != initial_values_.end()) {
      total -= it->second;
    }
  }
  return total;
}


size_t ZonePool::StatsScope::GetTotalAllocatedBytes() {
  return zone_pool_->GetTotalAllocatedBytes() - total_allocated_bytes_at_start_;
}


void ZonePool::StatsScope::ZoneReturned(Zone* zone) {
  size_t current_total = GetCurrentAllocatedBytes();
  // Update max.
  max_allocated_bytes_ = std::max(max_allocated_bytes_, current_total);
  // Drop zone from initial value map.
  InitialValues::iterator it = initial_values_.find(zone);
  if (it != initial_values_.end()) {
    initial_values_.erase(it);
  }
}


ZonePool::ZonePool(Isolate* isolate)
    : isolate_(isolate), max_allocated_bytes_(0), total_deleted_bytes_(0) {}


ZonePool::~ZonePool() {
  DCHECK(used_.empty());
  DCHECK(stats_.empty());
  for (Zone* zone : unused_) {
    delete zone;
  }
}


size_t ZonePool::GetMaxAllocatedBytes() {
  return std::max(max_allocated_bytes_, GetCurrentAllocatedBytes());
}


size_t ZonePool::GetCurrentAllocatedBytes() {
  size_t total = 0;
  for (Zone* zone : used_) {
    total += static_cast<size_t>(zone->allocation_size());
  }
  return total;
}


size_t ZonePool::GetTotalAllocatedBytes() {
  return total_deleted_bytes_ + GetCurrentAllocatedBytes();
}


Zone* ZonePool::NewEmptyZone() {
  Zone* zone;
  // Grab a zone from pool if possible.
  if (!unused_.empty()) {
    zone = unused_.back();
    unused_.pop_back();
  } else {
    zone = new Zone(isolate_);
  }
  used_.push_back(zone);
  DCHECK_EQ(0, zone->allocation_size());
  return zone;
}


void ZonePool::ReturnZone(Zone* zone) {
  size_t current_total = GetCurrentAllocatedBytes();
  // Update max.
  max_allocated_bytes_ = std::max(max_allocated_bytes_, current_total);
  // Update stats.
  for (auto stat_scope : stats_) {
    stat_scope->ZoneReturned(zone);
  }
  // Remove from used.
  Used::iterator it = std::find(used_.begin(), used_.end(), zone);
  DCHECK(it != used_.end());
  used_.erase(it);
  total_deleted_bytes_ += static_cast<size_t>(zone->allocation_size());
  // Delete zone or clear and stash on unused_.
  if (unused_.size() >= kMaxUnusedSize) {
    delete zone;
  } else {
    zone->DeleteAll();
    DCHECK_EQ(0, zone->allocation_size());
    unused_.push_back(zone);
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
