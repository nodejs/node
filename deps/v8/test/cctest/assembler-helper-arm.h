// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_ASSEMBLER_HELPER_ARM_H_
#define V8_CCTEST_ASSEMBLER_HELPER_ARM_H_

#include <functional>

#include "src/execution/simulator.h"
#include "src/handles/handles.h"

namespace v8 {
namespace internal {

// TODO(arm): Refine these signatures per test case, they can have arbitrary
// return and argument types and arbitrary number of arguments.
using F_iiiii = void*(int x, int p1, int p2, int p3, int p4);
using F_piiii = void*(void* p0, int p1, int p2, int p3, int p4);
using F_ppiii = void*(void* p0, void* p1, int p2, int p3, int p4);
using F_pppii = void*(void* p0, void* p1, void* p2, int p3, int p4);
using F_ippii = void*(int p0, void* p1, void* p2, int p3, int p4);

Handle<Code> AssembleCodeImpl(Isolate* isolate,
                              std::function<void(MacroAssembler&)> assemble);

template <typename Signature>
GeneratedCode<Signature> AssembleCode(
    Isolate* isolate, std::function<void(MacroAssembler&)> assemble) {
  return GeneratedCode<Signature>::FromCode(
      *AssembleCodeImpl(isolate, assemble));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_ASSEMBLER_HELPER_ARM_H_
