// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/macro-assembler.h"
#include "src/codegen/s390/assembler-s390-inl.h"
#include "src/execution/simulator.h"
#include "test/common/assembler-tester.h"
#include "test/common/flag-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace internal {

#define __ masm.

// Test the s390 assembler by compiling some simple functions into
// a buffer and executing them.  These tests do not initialize the
// V8 library, create a context, or use any V8 objects.

class MacroAssemblerTest : public TestWithIsolate {};

const char* ErrorMessage(const char* msg) {
  // We only print the abort reason if debug code is enabled. Otherwise we
  // just stop execution. Return an empty string in that case.
  return v8_flags.debug_code ? msg : "";
}

TEST_F(MacroAssemblerTest, TestHardAbort) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);

  __ Abort(AbortReason::kNoReason);

  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  // We need an isolate here to execute in the simulator.
  auto f = GeneratedCode<void>::FromBuffer(isolate(), buffer->start());

  ASSERT_DEATH_IF_SUPPORTED(
      { f.Call(); }, v8_flags.debug_code ? "abort: no reason" : "");
}

TEST_F(MacroAssemblerTest, TestCheck) {
  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);

  // Fail if the first parameter is 17.
  __ lgfi(r3, Operand(17));
  __ CmpS64(r2, r3);  // 1st parameter is in {r2}.
  __ Check(Condition::ne, AbortReason::kNoReason);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  // We need an isolate here to execute in the simulator.
  auto f = GeneratedCode<void, int>::FromBuffer(isolate(), buffer->start());

  f.Call(0);
  f.Call(18);
  ASSERT_DEATH_IF_SUPPORTED(
      { f.Call(17); }, v8_flags.debug_code ? "abort: no reason" : "");
}

#ifdef V8_ENABLE_DEBUG_CODE
TEST_F(MacroAssemblerTest, TestAssertZeroExtended) {
  FLAG_SCOPE(debug_code);
  FLAG_SCOPE(slow_debug_code);

  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler masm(isolate(), AssemblerOptions{}, CodeObjectRequired{false},
                      buffer->CreateView());
  __ set_root_array_available(false);
  __ set_abort_hard(true);

  __ AssertZeroExtended(r2);  // 1st parameter is in {r2}.
  __ Ret();

  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  buffer->MakeExecutable();
  // We need an isolate here to execute in the simulator.
  auto f =
      GeneratedCode<void, uint64_t>::FromBuffer(isolate(), buffer->start());

  f.Call(0);
  f.Call(42);
  f.Call(0xFFFFFFFFULL);
  ASSERT_DEATH_IF_SUPPORTED(
      { f.Call(0x100000000ULL); },
      ErrorMessage("abort: 32 bit value in register is not zero-extended"));
  ASSERT_DEATH_IF_SUPPORTED(
      { f.Call(0xFFFFFFFFFFFFFFFFULL); },
      ErrorMessage("abort: 32 bit value in register is not zero-extended"));
  ASSERT_DEATH_IF_SUPPORTED(
      { f.Call(0x8000000000000000ULL); },
      ErrorMessage("abort: 32 bit value in register is not zero-extended"));
}
#endif  // V8_ENABLE_DEBUG_CODE

#undef __

}  // namespace internal
}  // namespace v8
