// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/marking-state.h"

#include <unordered_set>

#include "src/heap/cppgc/heap-base.h"
#include "src/heap/cppgc/stats-collector.h"

namespace cppgc {
namespace internal {

void MutatorMarkingState::FlushNotFullyConstructedObjects() {
  std::unordered_set<HeapObjectHeader*> objects =
      not_fully_constructed_worklist_.Extract<AccessMode::kAtomic>();
  for (HeapObjectHeader* object : objects) {
    if (MarkNoPush(*object))
      previously_not_fully_constructed_worklist_.Push(object);
  }
}

void MutatorMarkingState::FlushDiscoveredEphemeronPairs() {
  StatsCollector::EnabledScope stats_scope(
      heap_.stats_collector(), StatsCollector::kMarkFlushEphemerons);
  discovered_ephemeron_pairs_worklist_.Publish();
  if (!discovered_ephemeron_pairs_worklist_.IsGlobalEmpty()) {
    ephemeron_pairs_for_processing_worklist_.Merge(
        discovered_ephemeron_pairs_worklist_);
  }
}

}  // namespace internal
}  // namespace cppgc
