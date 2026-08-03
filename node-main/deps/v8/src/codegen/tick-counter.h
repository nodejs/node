// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_TICK_COUNTER_H_
#define V8_CODEGEN_TICK_COUNTER_H_

#include <cstddef>

#include "src/base/macros.h"
#include "src/heap/local-heap.h"

namespace v8 {
namespace internal {

class LocalHeap;

// This method generates a tick. Also makes the current thread to enter a
// safepoint iff it was required to do so. The tick is used as a deterministic
// correlate of time to detect performance or divergence bugs in Turbofan.
// TickAndMaybeEnterSafepoint() should be called frequently thoughout the
// compilation.
class TickCounter {
 public:
  void TickAndMaybeEnterSafepoint() {
    ++ticks_;
    // Magical number to detect performance bugs or compiler divergence.
    // Selected as being roughly 10x of what's needed frequently.
    constexpr size_t kMaxTicks = 100000000;
    USE(kMaxTicks);
    DCHECK_LT(ticks_, kMaxTicks);

    if (local_heap_) local_heap_->Safepoint();
  }
  void AttachLocalHeap(LocalHeap* local_heap);
  void DetachLocalHeap();
  size_t CurrentTicks() const { return ticks_; }

 private:
  size_t ticks_ = 0;
  LocalHeap* local_heap_ = nullptr;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_TICK_COUNTER_H_
