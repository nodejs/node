// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_INCREMENTAL_MARKING_INL_H_
#define V8_HEAP_INCREMENTAL_MARKING_INL_H_

#include "src/execution/isolate.h"
#include "src/heap/heap-inl.h"
#include "src/heap/incremental-marking.h"

namespace v8 {
namespace internal {

void IncrementalMarking::TransferColor(HeapObject from, HeapObject to) {
  if (atomic_marking_state()->IsBlack(to)) {
    DCHECK(black_allocation());
    return;
  }

  DCHECK(atomic_marking_state()->IsWhite(to));
  if (atomic_marking_state()->IsGrey(from)) {
    bool success = atomic_marking_state()->WhiteToGrey(to);
    DCHECK(success);
    USE(success);
  } else if (atomic_marking_state()->IsBlack(from)) {
    bool success = atomic_marking_state()->WhiteToBlack(to);
    DCHECK(success);
    USE(success);
  }
}

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_INCREMENTAL_MARKING_INL_H_
