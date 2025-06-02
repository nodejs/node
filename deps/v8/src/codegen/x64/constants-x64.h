// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_X64_CONSTANTS_X64_H_
#define V8_CODEGEN_X64_CONSTANTS_X64_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {

// The actual value of the kRootRegister is offset from the IsolateData's start
// to take advantage of negative displacement values.
// On x64, the smallest operand encoding allows int8 offsets, thus we select the
// bias s.t. the first 32 8-byte slots of IsolateData are can be encoded this
// way.
constexpr int kRootRegisterBias = 128;

// The maximum size of the code range s.t. pc-relative calls are possible
// between all Code objects in the range.
constexpr size_t kMaxPCRelativeCodeRangeInMB = 2048;

// The maximum size of the stack restore after a fast API call that pops the
// stack parameters of the call off the stack.
constexpr size_t kMaxSizeOfMoveAfterFastCall = 8;

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_X64_CONSTANTS_X64_H_
