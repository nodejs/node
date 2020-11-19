// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/cppgc/marking-state.h"

namespace cppgc {
namespace internal {

void MarkingState::FlushNotFullyConstructedObjects() {
  not_fully_constructed_worklist().Publish();
  if (!not_fully_constructed_worklist_.IsGlobalEmpty()) {
    previously_not_fully_constructed_worklist_.Merge(
        &not_fully_constructed_worklist_);
  }
  DCHECK(not_fully_constructed_worklist_.IsGlobalEmpty());
}

}  // namespace internal
}  // namespace cppgc
