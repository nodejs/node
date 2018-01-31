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
#include "src/objects-inl.h"
#include "src/register-configuration.h"
#include "test/cctest/cctest.h"
#include "test/cctest/test-code-stubs.h"

namespace v8 {
namespace internal {
namespace test_code_stubs_x64 {

#define __ masm.

ConvertDToIFunc MakeConvertDToIFuncTrampoline(Isolate* isolate,
                                              Register destination_reg) {
  HandleScope handles(isolate);

  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  MacroAssembler masm(isolate, buffer, static_cast<int>(allocated),
                      v8::internal::CodeObjectRequired::kYes);

  DoubleToIStub stub(isolate, destination_reg);
  byte* start = stub.GetCode()->instruction_start();

  __ pushq(rbx);
  __ pushq(rcx);
  __ pushq(rdx);
  __ pushq(rsi);
  __ pushq(rdi);

  const RegisterConfiguration* config = RegisterConfiguration::Default();

  // Save registers make sure they don't get clobbered.
  int reg_num = 0;
  for (; reg_num < config->num_allocatable_general_registers(); ++reg_num) {
    Register reg =
        Register::from_code(config->GetAllocatableGeneralCode(reg_num));
    if (reg != rsp && reg != rbp && reg != destination_reg) {
      __ pushq(reg);
    }
  }

  // Put the double argument into the designated double argument slot.
  __ subq(rsp, Immediate(kDoubleSize));
  __ Movsd(MemOperand(rsp, 0), xmm0);

  // Call through to the actual stub
  __ Call(start, RelocInfo::EXTERNAL_REFERENCE);

  __ addq(rsp, Immediate(kDoubleSize));

  // Make sure no registers have been unexpectedly clobbered
  for (--reg_num; reg_num >= 0; --reg_num) {
    Register reg =
        Register::from_code(config->GetAllocatableGeneralCode(reg_num));
    if (reg != rsp && reg != rbp && reg != destination_reg) {
      __ cmpq(reg, MemOperand(rsp, 0));
      __ Assert(equal, kRegisterWasClobbered);
      __ addq(rsp, Immediate(kPointerSize));
    }
  }

  __ movq(rax, destination_reg);

  __ popq(rdi);
  __ popq(rsi);
  __ popq(rdx);
  __ popq(rcx);
  __ popq(rbx);

  __ ret(0);

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  return reinterpret_cast<ConvertDToIFunc>(
      reinterpret_cast<intptr_t>(buffer));
}

#undef __


static Isolate* GetIsolateFrom(LocalContext* context) {
  return reinterpret_cast<Isolate*>((*context)->GetIsolate());
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

  Register dest_registers[] = {rax, rbx, rcx, rdx, rsi, rdi, r8, r9};

  for (size_t d = 0; d < sizeof(dest_registers) / sizeof(Register); d++) {
    RunAllTruncationTests(
        MakeConvertDToIFuncTrampoline(isolate, dest_registers[d]));
  }
}

}  // namespace test_code_stubs_x64
}  // namespace internal
}  // namespace v8
