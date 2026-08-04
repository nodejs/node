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

class NormalPageSpace;

class V8_EXPORT_PRIVATE Compactor final {
  using CompactableSpaceHandling = SweepingConfig::CompactableSpaceHandling;

 public:
  explicit Compactor(RawHeap&);
  ~Compactor() { DCHECK(!is_enabled_); }

  Compactor(const Compactor&) = delete;
  Compactor& operator=(const Compactor&) = delete;

  void InitializeIfShouldCompact(GCConfig::MarkingType, StackState);
  void CancelIfShouldNotCompact(GCConfig::MarkingType, StackState);
  // Returns whether spaces need to be processed by the Sweeper after
  // compaction.
  CompactableSpaceHandling CompactSpacesIfEnabled();

  CompactionWorklists* compaction_worklists() {
    return compaction_worklists_.get();
  }

  void EnableForNextGCForTesting();
  bool IsEnabledForTesting() const { return is_enabled_; }

 private:
  bool ShouldCompact(GCConfig::MarkingType, StackState) const;

  RawHeap& heap_;
  // Compactor does not own the compactable spaces. The heap owns all spaces.
  std::vector<NormalPageSpace*> compactable_spaces_;

  std::unique_ptr<CompactionWorklists> compaction_worklists_;

  bool is_enabled_ = false;
  bool is_cancelled_ = false;
  bool enable_for_next_gc_for_testing_ = false;
};

}  // namespace internal
}  // namespace cppgc

#endif  // V8_HEAP_CPPGC_COMPACTOR_H_
