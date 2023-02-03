// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can b in the
// LICENSE file.

#ifndef V8_CODEGEN_LOONG64_REGLIST_LOONG64_H_
#define V8_CODEGEN_LOONG64_REGLIST_LOONG64_H_

#include "src/codegen/loong64/constants-loong64.h"
#include "src/codegen/register-arch.h"
#include "src/codegen/reglist-base.h"

namespace v8 {
namespace internal {

using RegList = RegListBase<Register>;
using DoubleRegList = RegListBase<DoubleRegister>;
ASSERT_TRIVIALLY_COPYABLE(RegList);
ASSERT_TRIVIALLY_COPYABLE(DoubleRegList);

const RegList kJSCallerSaved = {a0, a1, a2, a3, a4, a5, a6, a7,
                                t0, t1, t2, t3, t4, t5, t8};

const int kNumJSCallerSaved = 15;

// Callee-saved registers preserved when switching from C to JavaScript.
const RegList kCalleeSaved = {fp,   // fp
                              s0,   // s0
                              s1,   // s1
                              s2,   // s2
                              s3,   // s3
                              s4,   // s4
                              s5,   // s5
                              s6,   // s6 (roots in Javascript code)
                              s7,   // s7 (cp in Javascript code)
                              s8};  // s8

const int kNumCalleeSaved = 10;

const DoubleRegList kCalleeSavedFPU = {f24, f25, f26, f27, f28, f29, f30, f31};

const int kNumCalleeSavedFPU = 8;

const DoubleRegList kCallerSavedFPU = {f0,  f1,  f2,  f3,  f4,  f5,  f6,  f7,
                                       f8,  f9,  f10, f11, f12, f13, f14, f15,
                                       f16, f17, f18, f19, f20, f21, f22, f23};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_LOONG64_REGLIST_LOONG64_H_
