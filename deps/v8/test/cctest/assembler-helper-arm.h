// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_ASSEMBLER_HELPER_ARM_H_
#define V8_CCTEST_ASSEMBLER_HELPER_ARM_H_

#include <functional>

#include "src/handles.h"
#include "src/simulator.h"

namespace v8 {
namespace internal {

// TODO(arm): Refine these signatures per test case, they can have arbitrary
// return and argument types and arbitrary number of arguments.
using F_iiiii = Object*(int x, int p1, int p2, int p3, int p4);
using F_piiii = Object*(void* p0, int p1, int p2, int p3, int p4);
using F_ppiii = Object*(void* p0, void* p1, int p2, int p3, int p4);
using F_pppii = Object*(void* p0, void* p1, void* p2, int p3, int p4);
using F_ippii = Object*(int p0, void* p1, void* p2, int p3, int p4);

Handle<Code> AssembleCodeImpl(std::function<void(Assembler&)> assemble);

template <typename Signature>
GeneratedCode<Signature> AssembleCode(
    std::function<void(Assembler&)> assemble) {
  return GeneratedCode<Signature>::FromCode(*AssembleCodeImpl(assemble));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_ASSEMBLER_HELPER_ARM_H_
