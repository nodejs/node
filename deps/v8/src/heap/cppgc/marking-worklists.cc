
// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/marking-worklists.h"

namespace cppgc {
namespace internal {

constexpr int MarkingWorklists::kMutatorThreadId;

void MarkingWorklists::ClearForTesting() {
  marking_worklist_.Clear();
  not_fully_constructed_worklist_.Clear();
  previously_not_fully_constructed_worklist_.Clear();
  write_barrier_worklist_.Clear();
  weak_callback_worklist_.Clear();
}

void MarkingWorklists::FlushNotFullyConstructedObjects() {
  if (!not_fully_constructed_worklist_.IsLocalViewEmpty(kMutatorThreadId)) {
    not_fully_constructed_worklist_.FlushToGlobal(kMutatorThreadId);
    previously_not_fully_constructed_worklist_.MergeGlobalPool(
        &not_fully_constructed_worklist_);
  }
  DCHECK(not_fully_constructed_worklist_.IsLocalViewEmpty(
      MarkingWorklists::kMutatorThreadId));
}

}  // namespace internal
}  // namespace cppgc
