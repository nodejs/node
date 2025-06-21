// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_IA32_REGLIST_IA32_H_
#define V8_CODEGEN_IA32_REGLIST_IA32_H_

#include "src/codegen/register-arch.h"
#include "src/codegen/reglist-base.h"

namespace v8 {
namespace internal {

using RegList = RegListBase<Register>;
using DoubleRegList = RegListBase<DoubleRegister>;
ASSERT_TRIVIALLY_COPYABLE(RegList);
ASSERT_TRIVIALLY_COPYABLE(DoubleRegList);

// Caller-saved registers
constexpr RegList kJSCallerSaved = {
    eax, ecx, edx,
    ebx,   // used as caller-saved register in JavaScript code
    edi};  // callee function

// Caller-saved registers according to the x86 ABI
constexpr RegList kCallerSaved = {eax, ecx, edx};

constexpr int kNumJSCallerSaved = 5;

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_IA32_REGLIST_IA32_H_
