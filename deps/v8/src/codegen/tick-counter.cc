// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/tick-counter.h"

#include "src/base/logging.h"
#include "src/base/macros.h"

namespace v8 {
namespace internal {

void TickCounter::DoTick() {
  ++ticks_;
  // Magical number to detect performance bugs or compiler divergence.
  // Selected as being roughly 10x of what's needed frequently.
  constexpr size_t kMaxTicks = 100000000;
  USE(kMaxTicks);
  DCHECK_LT(ticks_, kMaxTicks);
}

}  // namespace internal
}  // namespace v8
