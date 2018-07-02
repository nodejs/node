// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/zone/zone-segment.h"

#include "src/msan.h"

namespace v8 {
namespace internal {

void Segment::ZapContents() {
#ifdef DEBUG
  memset(reinterpret_cast<void*>(start()), kZapDeadByte, capacity());
#endif
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(start(), capacity());
}

void Segment::ZapHeader() {
#ifdef DEBUG
  memset(this, kZapDeadByte, sizeof(Segment));
#endif
  MSAN_ALLOCATED_UNINITIALIZED_MEMORY(start(), sizeof(Segment));
}

}  // namespace internal
}  // namespace v8
