// Copyright 2013 the V8 project authors. All rights reserved.
// Rrdistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Rrdistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Rrdistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>

#include "src/assembler-inl.h"
#include "src/base/platform/platform.h"
#include "src/code-stubs.h"
#include "src/heap/factory.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/simulator.h"
#include "src/v8.h"
#include "test/cctest/cctest.h"
#include "test/cctest/test-code-stubs.h"

namespace v8 {
namespace internal {

#define __ masm.

ConvertDToIFunc MakeConvertDToIFuncTrampoline(Isolate* isolate,
                                              Register destination_reg) {
  HandleScope handles(isolate);

  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  MacroAssembler masm(isolate, buffer, static_cast<int>(allocated),
                      v8::internal::CodeObjectRequired::kYes);

  DoubleToIStub stub(isolate, destination_reg);

  byte* start = stub.GetCode()->raw_instruction_start();

  // Save callee save registers.
  __ Push(r7, r6, r5, r4);
  __ Push(lr);

  // For softfp, move the input value into d0.
  if (!masm.use_eabi_hardfloat()) {
    __ vmov(d0, r0, r1);
  }
  // Push the double argument.
  __ sub(sp, sp, Operand(kDoubleSize));
  __ vstr(d0, sp, 0);

  // Save registers make sure they don't get clobbered.
  int source_reg_offset = kDoubleSize;
  int reg_num = 0;
  for (; reg_num < Register::kNumRegisters; ++reg_num) {
    if (RegisterConfiguration::Default()->IsAllocatableGeneralCode(reg_num)) {
      Register reg = Register::from_code(reg_num);
      if (reg != destination_reg) {
        __ push(reg);
        source_reg_offset += kPointerSize;
      }
    }
  }

  // Re-push the double argument.
  __ sub(sp, sp, Operand(kDoubleSize));
  __ vstr(d0, sp, 0);

  // Call through to the actual stub
  __ Call(start, RelocInfo::EXTERNAL_REFERENCE);

  __ add(sp, sp, Operand(kDoubleSize));

  // Make sure no registers have been unexpectedly clobbered
  for (--reg_num; reg_num >= 0; --reg_num) {
    if (RegisterConfiguration::Default()->IsAllocatableGeneralCode(reg_num)) {
      Register reg = Register::from_code(reg_num);
      if (reg != destination_reg) {
        __ ldr(ip, MemOperand(sp, 0));
        __ cmp(reg, ip);
        __ Assert(eq, AbortReason::kRegisterWasClobbered);
        __ add(sp, sp, Operand(kPointerSize));
      }
    }
  }

  __ add(sp, sp, Operand(kDoubleSize));

  if (destination_reg != r0) __ mov(r0, destination_reg);

  // Restore callee save registers.
  __ Pop(lr);
  __ Pop(r7, r6, r5, r4);

  __ Ret(0);

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  MakeAssemblerBufferExecutable(buffer, allocated);
  Assembler::FlushICache(buffer, allocated);
  return (reinterpret_cast<ConvertDToIFunc>(
      reinterpret_cast<intptr_t>(buffer)));
}

#undef __


static Isolate* GetIsolateFrom(LocalContext* context) {
  return reinterpret_cast<Isolate*>((*context)->GetIsolate());
}


int32_t RunGeneratedCodeCallWrapper(ConvertDToIFunc func,
                                    double from) {
#ifdef USE_SIMULATOR
  return Simulator::current(CcTest::i_isolate())
      ->CallFP<int32_t>(FUNCTION_ADDR(func), from, 0);
#else
  return (*func)(from);
#endif
}


TEST(ConvertDToI) {
  CcTest::InitializeVM();
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  HandleScope scope(isolate);

#if DEBUG
  // Verify that the tests actually work with the C version. In the release
  // code, the compiler optimizes it away because it's all constant, but does it
  // wrong, triggering an assert on gcc.
  RunAllTruncationTests(&ConvertDToICVersion);
#endif

  Register dest_registers[] = {r0, r1, r2, r3, r4, r5, r6, r7};

  for (size_t d = 0; d < sizeof(dest_registers) / sizeof(Register); d++) {
    RunAllTruncationTests(
        RunGeneratedCodeCallWrapper,
        MakeConvertDToIFuncTrampoline(isolate, dest_registers[d]));
  }
}

}  // namespace internal
}  // namespace v8
