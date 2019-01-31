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
  auto buffer = AllocateAssemblerBuffer();
  TurboAssembler tasm(nullptr, AssemblerOptions{}, CodeObjectRequired::kNo,
                      buffer->CreateView());
  __ set_abort_hard(true);

  __ Abort(AbortReason::kNoReason);

  CodeDesc desc;
  tasm.GetCode(nullptr, &desc);
  buffer->MakeExecutable();
  auto f = GeneratedCode<void>::FromBuffer(nullptr, buffer->start());

  ASSERT_DEATH_IF_SUPPORTED({ f.Call(); }, "abort: no reason");
}

TEST(TurboAssemblerTest, TestCheck) {
  auto buffer = AllocateAssemblerBuffer();
  TurboAssembler tasm(nullptr, AssemblerOptions{}, CodeObjectRequired::kNo,
                      buffer->CreateView());
  __ set_abort_hard(true);

  // Fail if the first parameter is 17.
  __ mov(eax, 17);
  __ cmp(eax, Operand(esp, 4));  // compare with 1st parameter.
  __ Check(Condition::not_equal, AbortReason::kNoReason);
  __ ret(0);

  CodeDesc desc;
  tasm.GetCode(nullptr, &desc);
  buffer->MakeExecutable();
  auto f = GeneratedCode<void, int>::FromBuffer(nullptr, buffer->start());

  f.Call(0);
  f.Call(18);
  ASSERT_DEATH_IF_SUPPORTED({ f.Call(17); }, "abort: no reason");
}

#undef __

}  // namespace internal
}  // namespace v8
