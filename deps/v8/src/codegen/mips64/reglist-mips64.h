// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_MIPS64_REGLIST_MIPS64_H_
#define V8_CODEGEN_MIPS64_REGLIST_MIPS64_H_

#include "src/codegen/mips64/constants-mips64.h"
#include "src/codegen/register-arch.h"
#include "src/codegen/reglist-base.h"

namespace v8 {
namespace internal {

using RegList = RegListBase<Register>;
using DoubleRegList = RegListBase<DoubleRegister>;
ASSERT_TRIVIALLY_COPYABLE(RegList);
ASSERT_TRIVIALLY_COPYABLE(DoubleRegList);

const RegList kJSCallerSaved = {v0, v1, a0, a1, a2, a3, a4,
                                a5, a6, a7, t0, t1, t2, t3};

const int kNumJSCallerSaved = 14;

// Callee-saved registers preserved when switching from C to JavaScript.
const RegList kCalleeSaved = {s0,   // s0
                              s1,   // s1
                              s2,   // s2
                              s3,   // s3
                              s4,   // s4
                              s5,   // s5
                              s6,   // s6 (roots in Javascript code)
                              s7,   // s7 (cp in Javascript code)
                              fp};  // fp/s8

const int kNumCalleeSaved = 9;

const DoubleRegList kCalleeSavedFPU = {f20, f22, f24, f26, f28, f30};

const int kNumCalleeSavedFPU = 6;

const DoubleRegList kCallerSavedFPU = {f0,  f2,  f4,  f6,  f8,
                                       f10, f12, f14, f16, f18};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_MIPS64_REGLIST_MIPS64_H_
