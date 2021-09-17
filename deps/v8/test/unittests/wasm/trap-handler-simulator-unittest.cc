// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/trap-handler/trap-handler-simulator.h"

#include "include/v8.h"
#include "src/codegen/macro-assembler-inl.h"
#include "src/execution/simulator.h"
#include "src/trap-handler/trap-handler.h"
#include "test/common/assembler-tester.h"
#include "test/unittests/test-utils.h"

#if !V8_HOST_ARCH_X64 || !V8_TARGET_ARCH_ARM64
#error "Only include this file on arm64 simulator builds on x64."
#endif

namespace v8 {
namespace internal {
namespace trap_handler {

constexpr uintptr_t kFakePc = 11;

class SimulatorTrapHandlerTest : public TestWithIsolate {
 public:
  void SetThreadInWasm() {
    EXPECT_EQ(0, *thread_in_wasm);
    *thread_in_wasm = 1;
  }

  void ResetThreadInWasm() {
    EXPECT_EQ(1, *thread_in_wasm);
    *thread_in_wasm = 0;
  }

  int* thread_in_wasm = trap_handler::GetThreadInWasmThreadLocalAddress();
};

TEST_F(SimulatorTrapHandlerTest, ProbeMemorySuccess) {
  int x = 47;
  EXPECT_EQ(0u, ProbeMemory(reinterpret_cast<uintptr_t>(&x), kFakePc));
}

TEST_F(SimulatorTrapHandlerTest, ProbeMemoryFail) {
  constexpr uintptr_t kNullAddress = 0;
  EXPECT_DEATH_IF_SUPPORTED(ProbeMemory(kNullAddress, kFakePc), "");
}

TEST_F(SimulatorTrapHandlerTest, ProbeMemoryFailWhileInWasm) {
  // Test that we still crash if the trap handler is set up and the "thread in
  // wasm" flag is set, but the PC is not registered as a protected instruction.
  constexpr bool kUseDefaultHandler = true;
  CHECK(v8::V8::EnableWebAssemblyTrapHandler(kUseDefaultHandler));

  constexpr uintptr_t kNullAddress = 0;
  SetThreadInWasm();
  EXPECT_DEATH_IF_SUPPORTED(ProbeMemory(kNullAddress, kFakePc), "");
}

TEST_F(SimulatorTrapHandlerTest, ProbeMemoryWithTrapHandled) {
  constexpr uintptr_t kNullAddress = 0;
  constexpr uintptr_t kFakeLandingPad = 19;

  constexpr bool kUseDefaultHandler = true;
  CHECK(v8::V8::EnableWebAssemblyTrapHandler(kUseDefaultHandler));

  ProtectedInstructionData fake_protected_instruction{kFakePc, kFakeLandingPad};
  int handler_data_index =
      RegisterHandlerData(0, 128, 1, &fake_protected_instruction);

  SetThreadInWasm();
  EXPECT_EQ(kFakeLandingPad, ProbeMemory(kNullAddress, kFakePc));

  // Reset everything.
  ResetThreadInWasm();
  ReleaseHandlerData(handler_data_index);
  RemoveTrapHandler();
}

TEST_F(SimulatorTrapHandlerTest, ProbeMemoryWithLandingPad) {
  EXPECT_EQ(0u, GetRecoveredTrapCount());

  // Test that the trap handler can recover a memory access violation in
  // wasm code (we fake the wasm code and the access violation).
  std::unique_ptr<TestingAssemblerBuffer> buffer = AllocateAssemblerBuffer();
  constexpr Register scratch = x0;
  MacroAssembler masm(nullptr, AssemblerOptions{}, CodeObjectRequired::kNo,
                      buffer->CreateView());
  // Generate an illegal memory access.
  masm.Mov(scratch, 0);
  uint32_t crash_offset = masm.pc_offset();
  masm.Str(scratch, MemOperand(scratch, 0));  // nullptr access
  uint32_t recovery_offset = masm.pc_offset();
  // Return.
  masm.Ret();

  CodeDesc desc;
  masm.GetCode(nullptr, &desc);

  constexpr bool kUseDefaultHandler = true;
  CHECK(v8::V8::EnableWebAssemblyTrapHandler(kUseDefaultHandler));

  ProtectedInstructionData protected_instruction{crash_offset, recovery_offset};
  int handler_data_index =
      RegisterHandlerData(reinterpret_cast<Address>(desc.buffer),
                          desc.instr_size, 1, &protected_instruction);

  // Now execute the code.
  buffer->MakeExecutable();
  GeneratedCode<void> code = GeneratedCode<void>::FromAddress(
      i_isolate(), reinterpret_cast<Address>(desc.buffer));

  SetThreadInWasm();
  code.Call();
  ResetThreadInWasm();

  ReleaseHandlerData(handler_data_index);
  RemoveTrapHandler();

  EXPECT_EQ(1u, GetRecoveredTrapCount());
}

}  // namespace trap_handler
}  // namespace internal
}  // namespace v8
