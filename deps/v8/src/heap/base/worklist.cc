// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/base/worklist.h"

namespace heap::base::internal {

// static
SegmentBase* SegmentBase::GetSentinelSegmentAddress() {
  static SegmentBase sentinel_segment(0);
  return &sentinel_segment;
}

}  // namespace heap::base::internal
