// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_X64_REGLIST_X64_H_
#define V8_CODEGEN_X64_REGLIST_X64_H_

#include "src/base/macros.h"
#include "src/codegen/register-arch.h"
#include "src/codegen/reglist-base.h"

namespace v8 {
namespace internal {

using RegList = RegListBase<Register>;
using DoubleRegList = RegListBase<DoubleRegister>;
ASSERT_TRIVIALLY_COPYABLE(RegList);
ASSERT_TRIVIALLY_COPYABLE(DoubleRegList);

constexpr RegList kJSCallerSaved = {
    rax, rcx, rdx,
    rbx,   // used as a caller-saved register in JavaScript code
    rdi};  // callee function

constexpr RegList kCallerSaved =
#ifdef V8_TARGET_OS_WIN
    {rax, rcx, rdx, r8, r9, r10, r11};
#else
    {rax, rcx, rdx, rdi, rsi, r8, r9, r10, r11};
#endif  // V8_TARGET_OS_WIN

constexpr int kNumJSCallerSaved = 5;

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_X64_REGLIST_X64_H_
