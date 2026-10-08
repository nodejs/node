// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/pending-allocations.h"

#include <limits>

#include "src/execution/isolate.h"
#include "src/heap/concurrent-marking.h"
#include "src/heap/heap-inl.h"

namespace v8::internal {

bool YoungPendingAllocations::ContainsSynchronized(Address addr) const {
  base::MutexGuard guard(&mutex_);
  return Snapshot{top_, limit_, large_object_, 0}.Contains(addr);
}

void YoungPendingAllocations::UpdateLab(Address top, Address limit) {
  base::MutexGuard guard(&mutex_);
  // Avoid version_ field increment when nothing changes. This helps avoid the
  // slow path in concurrent markers but also ABA on 32-bit platforms.
  if (top_ == top && limit_ == limit) return;
  top_ = top;
  limit_ = limit;
  BumpVersion();
}

void YoungPendingAllocations::RemoveLab() {
  UpdateLab(kNullAddress, kNullAddress);
}

void YoungPendingAllocations::UpdateLargeObject(Address addr) {
  base::MutexGuard guard(&mutex_);
  // Avoid version_ field increment when nothing changes. This helps avoid the
  // slow path in concurrent markers but also ABA on 32-bit platforms.
  if (large_object_ == addr) return;
  large_object_ = addr;
  BumpVersion();
}

void YoungPendingAllocations::RemoveLargeObject() {
  UpdateLargeObject(kNullAddress);
}

void YoungPendingAllocations::UpdateSnapshot(Snapshot* snapshot) const {
  base::MutexGuard guard(&mutex_);
  snapshot->top = top_;
  snapshot->limit = limit_;
  snapshot->large_object = large_object_;
  snapshot->version = version_.load(std::memory_order_acquire);
}

void YoungPendingAllocations::ResetVersion() {
  heap_->safepoint()->AssertActive();
  DCHECK_IMPLIES(v8_flags.concurrent_marking,
                 heap_->concurrent_marking()->IsStopped());
  DCHECK_EQ(top_, kNullAddress);
  DCHECK_EQ(limit_, kNullAddress);
  DCHECK_EQ(large_object_, kNullAddress);
  version_.exchange(1, std::memory_order_acq_rel);
}

void YoungPendingAllocations::BumpVersion() {
  // Only concurrent markers use the version mechanism, so the version is only
  // updated during incremental marking.
  if (is_marking()) {
    CHECK_NE(version_.load(std::memory_order_relaxed),
             std::numeric_limits<uintptr_t>::max());
    version_.fetch_add(1, std::memory_order_acq_rel);
  }
}

bool YoungPendingAllocations::is_marking() const {
  return heap_->isolate()->isolate_data()->is_marking();
}

}  // namespace v8::internal
