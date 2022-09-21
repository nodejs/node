// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc-js/unified-heap-marking-state.h"

#include "src/base/logging.h"
#include "src/heap/mark-compact.h"

namespace v8 {
namespace internal {

UnifiedHeapMarkingState::UnifiedHeapMarkingState(
    Heap* heap, MarkingWorklists::Local* local_marking_worklist)
    : heap_(heap),
      marking_state_(heap_ ? heap_->mark_compact_collector()->marking_state()
                           : nullptr),
      local_marking_worklist_(local_marking_worklist),
      track_retaining_path_(v8_flags.track_retaining_path) {
  DCHECK_IMPLIES(v8_flags.track_retaining_path,
                 !v8_flags.concurrent_marking && !v8_flags.parallel_marking);
  DCHECK_IMPLIES(heap_, marking_state_);
}

void UnifiedHeapMarkingState::Update(
    MarkingWorklists::Local* local_marking_worklist) {
  local_marking_worklist_ = local_marking_worklist;
  DCHECK_NOT_NULL(heap_);
}

}  // namespace internal
}  // namespace v8
