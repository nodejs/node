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

#include "src/v8.h"

#include "src/base/platform/platform.h"
#include "src/code-stubs.h"
#include "src/factory.h"
#include "src/macro-assembler.h"
#include "src/mips64/constants-mips64.h"
#include "src/register-configuration.h"
#include "src/simulator.h"
#include "test/cctest/cctest.h"
#include "test/cctest/test-code-stubs.h"

using namespace v8::internal;

#define __ masm.

ConvertDToIFunc MakeConvertDToIFuncTrampoline(Isolate* isolate,
                                              Register source_reg,
                                              Register destination_reg,
                                              bool inline_fastpath) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(v8::base::OS::Allocate(
      Assembler::kMinimalBufferSize, &actual_size, true));
  CHECK(buffer);
  HandleScope handles(isolate);
  MacroAssembler masm(isolate, buffer, static_cast<int>(actual_size),
                      v8::internal::CodeObjectRequired::kYes);
  DoubleToIStub stub(isolate, source_reg, destination_reg, 0, true,
                     inline_fastpath);

  byte* start = stub.GetCode()->instruction_start();
  Label done;

  // Save callee save registers.
  __ MultiPush(kCalleeSaved | ra.bit());

  // Save callee-saved FPU registers.
  __ MultiPushFPU(kCalleeSavedFPU);
  // Set up the reserved register for 0.0.
  __ Move(kDoubleRegZero, 0.0);

  // For softfp, move the input value into f12.
  if (IsMipsSoftFloatABI) {
    __ Move(f12, a0, a1);
  }
  // Push the double argument.
  __ Dsubu(sp, sp, Operand(kDoubleSize));
  __ sdc1(f12, MemOperand(sp));
  __ Move(source_reg, sp);

  // Save registers make sure they don't get clobbered.
  int source_reg_offset = kDoubleSize;
  int reg_num = 2;
  const RegisterConfiguration* config = RegisterConfiguration::Crankshaft();
  for (; reg_num < config->num_allocatable_general_registers(); ++reg_num) {
    Register reg = Register::from_code(reg_num);
    if (!reg.is(destination_reg)) {
      __ push(reg);
      source_reg_offset += kPointerSize;
    }
  }

  // Re-push the double argument.
  __ Dsubu(sp, sp, Operand(kDoubleSize));
  __ sdc1(f12, MemOperand(sp));

  // Call through to the actual stub
  if (inline_fastpath) {
    __ ldc1(f12, MemOperand(source_reg));
    __ TryInlineTruncateDoubleToI(destination_reg, f12, &done);
    if (destination_reg.is(source_reg) && !source_reg.is(sp)) {
      // Restore clobbered source_reg.
      __ Daddu(source_reg, sp, Operand(source_reg_offset));
    }
  }
  __ Call(start, RelocInfo::EXTERNAL_REFERENCE);
  __ bind(&done);

  __ Daddu(sp, sp, Operand(kDoubleSize));

  // Make sure no registers have been unexpectedly clobbered
  for (--reg_num; reg_num >= 2; --reg_num) {
    Register reg = Register::from_code(reg_num);
    if (!reg.is(destination_reg)) {
      __ ld(at, MemOperand(sp, 0));
      __ Assert(eq, kRegisterWasClobbered, reg, Operand(at));
      __ Daddu(sp, sp, Operand(kPointerSize));
    }
  }

  __ Daddu(sp, sp, Operand(kDoubleSize));

  __ Move(v0, destination_reg);
  Label ok;
  __ Branch(&ok, eq, v0, Operand(zero_reg));
  __ bind(&ok);

  // Restore callee-saved FPU registers.
  __ MultiPopFPU(kCalleeSavedFPU);

  // Restore callee save registers.
  __ MultiPop(kCalleeSaved | ra.bit());

  Label ok1;
  __ Branch(&ok1, eq, v0, Operand(zero_reg));
  __ bind(&ok1);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(&desc);
  Assembler::FlushICache(isolate, buffer, actual_size);
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
  Simulator::current(CcTest::i_isolate())
      ->CallFP(FUNCTION_ADDR(func), from, 0.);
  return static_cast<int32_t>(
      Simulator::current(CcTest::i_isolate())->get_register(v0.code()));
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

  Register source_registers[] = {
      sp, v0, v1, a0, a1, a2, a3, a4, a5, a6, a7, t0, t1};
  Register dest_registers[] = {
      v0, v1, a0, a1, a2, a3, a4, a5, a6, a7, t0, t1};

  for (size_t s = 0; s < sizeof(source_registers) / sizeof(Register); s++) {
    for (size_t d = 0; d < sizeof(dest_registers) / sizeof(Register); d++) {
      RunAllTruncationTests(
          RunGeneratedCodeCallWrapper,
          MakeConvertDToIFuncTrampoline(isolate,
                                        source_registers[s],
                                        dest_registers[d],
                                        false));
      RunAllTruncationTests(
          RunGeneratedCodeCallWrapper,
          MakeConvertDToIFuncTrampoline(isolate,
                                        source_registers[s],
                                        dest_registers[d],
                                        true));
    }
  }
}
