// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/macro-assembler.h"
#include "src/simulator.h"
#include "test/common/assembler-tester.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace internal {

#define __ tasm.

// Test the x64 assembler by compiling some simple functions into
// a buffer and executing them.  These tests do not initialize the
// V8 library, create a context, or use any V8 objects.

TEST(TurboAssemblerTest, TestHardAbort) {
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  TurboAssembler tasm(nullptr, AssemblerOptions{}, buffer,
                      static_cast<int>(allocated), CodeObjectRequired::kNo);
  __ set_abort_hard(true);

  __ Abort(AbortReason::kNoReason);

  CodeDesc desc;
  tasm.GetCode(nullptr, &desc);
  MakeAssemblerBufferExecutable(buffer, allocated);
  auto f = GeneratedCode<void>::FromBuffer(nullptr, buffer);

  ASSERT_DEATH_IF_SUPPORTED({ f.Call(); }, "abort: no reason");
}

TEST(TurboAssemblerTest, TestCheck) {
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  TurboAssembler tasm(nullptr, AssemblerOptions{}, buffer,
                      static_cast<int>(allocated), CodeObjectRequired::kNo);
  __ set_abort_hard(true);

  // Fail if the first parameter is 17.
  __ movl(rax, Immediate(17));
  __ cmpl(rax, arg_reg_1);
  __ Check(Condition::not_equal, AbortReason::kNoReason);
  __ ret(0);

  CodeDesc desc;
  tasm.GetCode(nullptr, &desc);
  MakeAssemblerBufferExecutable(buffer, allocated);
  auto f = GeneratedCode<void, int>::FromBuffer(nullptr, buffer);

  f.Call(0);
  f.Call(18);
  ASSERT_DEATH_IF_SUPPORTED({ f.Call(17); }, "abort: no reason");
}

#undef __

}  // namespace internal
}  // namespace v8
