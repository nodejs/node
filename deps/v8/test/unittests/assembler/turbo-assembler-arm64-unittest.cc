// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arm64/macro-assembler-arm64-inl.h"
#include "src/macro-assembler.h"
#include "src/simulator.h"
#include "test/common/assembler-tester.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest-support.h"

namespace v8 {
namespace internal {

#define __ tasm.

// If we are running on android and the output is not redirected (i.e. ends up
// in the android log) then we cannot find the error message in the output. This
// macro just returns the empty string in that case.
#if defined(ANDROID) && !defined(V8_ANDROID_LOG_STDOUT)
#define ERROR_MESSAGE(msg) ""
#else
#define ERROR_MESSAGE(msg) msg
#endif

// Test the x64 assembler by compiling some simple functions into
// a buffer and executing them.  These tests do not initialize the
// V8 library, create a context, or use any V8 objects.

class TurboAssemblerTest : public TestWithIsolate {};

TEST_F(TurboAssemblerTest, TestHardAbort) {
  auto buffer = AllocateAssemblerBuffer();
  TurboAssembler tasm(nullptr, AssemblerOptions{}, CodeObjectRequired::kNo,
                      buffer->CreateView());
  __ set_abort_hard(true);

  __ Abort(AbortReason::kNoReason);

  CodeDesc desc;
  tasm.GetCode(nullptr, &desc);
  buffer->MakeExecutable();
  // We need an isolate here to execute in the simulator.
  auto f = GeneratedCode<void>::FromBuffer(isolate(), buffer->start());

  ASSERT_DEATH_IF_SUPPORTED({ f.Call(); }, ERROR_MESSAGE("abort: no reason"));
}

TEST_F(TurboAssemblerTest, TestCheck) {
  auto buffer = AllocateAssemblerBuffer();
  TurboAssembler tasm(nullptr, AssemblerOptions{}, CodeObjectRequired::kNo,
                      buffer->CreateView());
  __ set_abort_hard(true);

  // Fail if the first parameter is 17.
  __ Mov(w1, Immediate(17));
  __ Cmp(w0, w1);  // 1st parameter is in {w0}.
  __ Check(Condition::ne, AbortReason::kNoReason);
  __ Ret();

  CodeDesc desc;
  tasm.GetCode(nullptr, &desc);
  buffer->MakeExecutable();
  // We need an isolate here to execute in the simulator.
  auto f = GeneratedCode<void, int>::FromBuffer(isolate(), buffer->start());

  f.Call(0);
  f.Call(18);
  ASSERT_DEATH_IF_SUPPORTED({ f.Call(17); }, ERROR_MESSAGE("abort: no reason"));
}

#undef __
#undef ERROR_MESSAGE

}  // namespace internal
}  // namespace v8
