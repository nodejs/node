// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IA32_CONSTANTS_IA32_H_
#define V8_IA32_CONSTANTS_IA32_H_

#include "src/globals.h"

namespace v8 {
namespace internal {
// Actual value of root register is offset from the root array's start
// to take advantage of negative displacement values.
// For x86, this value is provided for uniformity with other platforms, although
// currently no root register is present.
constexpr int kRootRegisterBias = 0;

// Used temporarily to track clobbering of the root register.
// TODO(v8:6666): Remove this once use the root register.
constexpr size_t kRootRegisterSentinel = 0xcafeca11;

// TODO(sigurds): Change this value once we use relative jumps.
constexpr size_t kMaxPCRelativeCodeRangeInMB = 0;
}  // namespace internal
}  // namespace v8

#endif  // V8_IA32_CONSTANTS_IA32_H_
