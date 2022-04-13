// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/tick-counter.h"

#include "src/base/logging.h"
#include "src/base/macros.h"
#include "src/heap/local-heap.h"

namespace v8 {
namespace internal {

void TickCounter::TickAndMaybeEnterSafepoint() {
  ++ticks_;
  // Magical number to detect performance bugs or compiler divergence.
  // Selected as being roughly 10x of what's needed frequently.
  constexpr size_t kMaxTicks = 100000000;
  USE(kMaxTicks);
  DCHECK_LT(ticks_, kMaxTicks);

  if (local_heap_) local_heap_->Safepoint();
}

void TickCounter::AttachLocalHeap(LocalHeap* local_heap) {
  DCHECK_NULL(local_heap_);
  local_heap_ = local_heap;
  DCHECK_NOT_NULL(local_heap_);
}

void TickCounter::DetachLocalHeap() { local_heap_ = nullptr; }

}  // namespace internal
}  // namespace v8
