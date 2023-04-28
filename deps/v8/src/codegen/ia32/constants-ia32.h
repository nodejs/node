// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_IA32_CONSTANTS_IA32_H_
#define V8_CODEGEN_IA32_CONSTANTS_IA32_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {

// The actual value of the kRootRegister is offset from the IsolateData's start
// to take advantage of negative displacement values.
constexpr int kRootRegisterBias = 128;

// The maximum size of the code range s.t. pc-relative calls are possible
// between all Code objects in the range.
constexpr size_t kMaxPCRelativeCodeRangeInMB = 0;

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_IA32_CONSTANTS_IA32_H_
