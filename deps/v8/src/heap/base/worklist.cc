// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/base/worklist.h"

namespace heap::base {

// static
bool WorklistBase::predictable_order_ = false;

// static
void WorklistBase::EnforcePredictableOrder() { predictable_order_ = true; }

namespace internal {

// static
SegmentBase* SegmentBase::GetSentinelSegmentAddress() {
  static SegmentBase sentinel_segment(0);
  return &sentinel_segment;
}

}  // namespace internal
}  // namespace heap::base
