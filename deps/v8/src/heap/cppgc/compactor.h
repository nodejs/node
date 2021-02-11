// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_COMPACTOR_H_
#define V8_HEAP_CPPGC_COMPACTOR_H_

#include "src/heap/cppgc/compaction-worklists.h"
#include "src/heap/cppgc/garbage-collector.h"
#include "src/heap/cppgc/raw-heap.h"

namespace cppgc {
namespace internal {

class V8_EXPORT_PRIVATE Compactor final {
  using CompactableSpaceHandling =
      Sweeper::SweepingConfig::CompactableSpaceHandling;

 public:
  explicit Compactor(RawHeap&);
  ~Compactor() { DCHECK(!is_enabled_); }

  void InitializeIfShouldCompact(GarbageCollector::Config::MarkingType,
                                 GarbageCollector::Config::StackState);
  // Returns true is compaction was cancelled.
  bool CancelIfShouldNotCompact(GarbageCollector::Config::MarkingType,
                                GarbageCollector::Config::StackState);
  CompactableSpaceHandling CompactSpacesIfEnabled();

  CompactionWorklists* compaction_worklists() {
    return compaction_worklists_.get();
  }

  void EnableForNextGCForTesting() { enable_for_next_gc_for_testing_ = true; }

  bool IsEnabledForTesting() const { return is_enabled_; }

 private:
  bool ShouldCompact(GarbageCollector::Config::MarkingType,
                     GarbageCollector::Config::StackState);

  RawHeap& heap_;
  // Compactor does not own the compactable spaces. The heap owns all spaces.
  std::vector<NormalPageSpace*> compactable_spaces_;

  std::unique_ptr<CompactionWorklists> compaction_worklists_;

  bool is_enabled_ = false;

  bool enable_for_next_gc_for_testing_ = false;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_COMPACTOR_H_
