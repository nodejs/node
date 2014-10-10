// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/bits.h"
#include "src/base/logging.h"

namespace v8 {
namespace base {
namespace bits {

uint32_t RoundUpToPowerOfTwo32(uint32_t value) {
  DCHECK_LE(value, 0x80000000u);
  value = value - 1;
  value = value | (value >> 1);
  value = value | (value >> 2);
  value = value | (value >> 4);
  value = value | (value >> 8);
  value = value | (value >> 16);
  return value + 1;
}

}  // namespace bits
}  // namespace base
}  // namespace v8
