// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/macro-assembler.h"
#include "src/codegen/ppc/assembler-ppc-inl.h"
#include "src/execution/simulator.h"
#include "test/common/assembler-tester.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace internal {

#define __ tasm.

// Test the ppc assembler by compiling some simple functions into
// a buffer and executing them.  These tests do not initialize the
// V8 library, create a context, or use any V8 objects.

class TurboAssemblerTest : public TestWithIsolate {};

TEST_F(TurboAssemblerTest, TestHardAbort) {
  auto buffer = AllocateAssemblerBuffer();
  TurboAssembler tasm(isolate(), AssemblerOptions{}, CodeObjectRequired::kNo,
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);

  __ Abort(AbortReason::kNoReason);

  CodeDesc desc;
  tasm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  // We need an isolate here to execute in the simulator.
  auto f = GeneratedCode<void>::FromBuffer(isolate(), buffer->start());

  ASSERT_DEATH_IF_SUPPORTED({ f.Call(); }, "abort: no reason");
}

TEST_F(TurboAssemblerTest, TestCheck) {
  auto buffer = AllocateAssemblerBuffer();
  TurboAssembler tasm(isolate(), AssemblerOptions{}, CodeObjectRequired::kNo,
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);

  // Fail if the first parameter is 17.
  __ mov(r4, Operand(17));
  __ cmp(r3, r4);  // 1st parameter is in {r3}.
  __ Check(Condition::ne, AbortReason::kNoReason);
  __ Ret();

  CodeDesc desc;
  tasm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  // We need an isolate here to execute in the simulator.
  auto f = GeneratedCode<void, int>::FromBuffer(isolate(), buffer->start());

  f.Call(0);
  f.Call(18);
  ASSERT_DEATH_IF_SUPPORTED({ f.Call(17); }, "abort: no reason");
}

#undef __

}  // namespace internal
}  // namespace v8
