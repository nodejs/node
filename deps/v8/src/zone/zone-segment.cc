// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/zone/zone-segment.h"

namespace v8 {
namespace internal {

void Segment::ZapContents() {
#ifdef DEBUG
  memset(start(), kZapDeadByte, capacity());
#endif
}

void Segment::ZapHeader() {
#ifdef DEBUG
  memset(this, kZapDeadByte, sizeof(Segment));
#endif
}

}  // namespace internal
}  // namespace v8
