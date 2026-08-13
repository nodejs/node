// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_PENDING_ALLOCATIONS_H_
#define V8_HEAP_PENDING_ALLOCATIONS_H_

#include <atomic>

#include "src/base/platform/mutex.h"
#include "src/common/globals.h"

namespace v8::internal {

// YoungPendingAllocations tracks potentially uninitialized memory regions in
// the young generation. This is the current active LAB and a single large
// object for the main thread.
//
// Concurrent marking threads query this data via versioned
// snapshots to check if a young object's allocation is currently in progress
// without requiring lock acquisition on every access.
//
// Background compiler threads also query this data (via
// Heap::IsPendingAllocation) to check for pending allocations, but query it
// directly (synchronizing via mutex) without using the version mechanism.
class V8_EXPORT_PRIVATE YoungPendingAllocations final {
 public:
  struct Snapshot final {
    Address top = kNullAddress;
    Address limit = kNullAddress;
    Address large_object = kNullAddress;
    uintptr_t version = 0;

    bool Contains(Address addr) const {
      if (top <= addr && addr < limit) return true;
      if (large_object == addr) return true;
      return false;
    }
  };

  explicit YoungPendingAllocations(Heap* heap) : heap_(heap) {}

  bool ContainsSynchronized(Address addr) const;

  void UpdateLab(Address top, Address limit);
  void RemoveLab();
  void UpdateLargeObject(Address addr);
  void RemoveLargeObject();

  void UpdateSnapshot(Snapshot* snapshot) const;
  void UpdateSnapshotIfOutdated(Snapshot* snapshot) const {
    if (version_.load(std::memory_order_acquire) != snapshot->version)
        [[unlikely]] {
      UpdateSnapshot(snapshot);
    }
  }

  void ResetVersion();

 private:
  void BumpVersion();
  bool is_marking() const;

  Heap* const heap_;
  mutable base::Mutex mutex_;
  Address top_ = kNullAddress;
  Address limit_ = kNullAddress;
  Address large_object_ = kNullAddress;
  std::atomic<uintptr_t> version_{1};
};

}  // namespace v8::internal

#endif  // V8_HEAP_PENDING_ALLOCATIONS_H_
