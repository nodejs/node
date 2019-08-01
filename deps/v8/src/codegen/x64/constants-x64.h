// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_X64_CONSTANTS_X64_H_
#define V8_CODEGEN_X64_CONSTANTS_X64_H_

#include "src/common/globals.h"

namespace v8 {
namespace internal {
// Actual value of root register is offset from the root array's start
// to take advantage of negative displacement values.
// TODO(sigurds): Choose best value.
constexpr int kRootRegisterBias = 128;

constexpr size_t kMaxPCRelativeCodeRangeInMB = 2048;
}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_X64_CONSTANTS_X64_H_
