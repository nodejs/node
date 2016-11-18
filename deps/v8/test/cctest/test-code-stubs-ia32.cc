// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
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

#include <limits>

#include "src/v8.h"

#include "src/base/platform/platform.h"
#include "src/code-stubs.h"
#include "src/factory.h"
#include "src/macro-assembler.h"
#include "test/cctest/cctest.h"
#include "test/cctest/test-code-stubs.h"

using namespace v8::internal;

#define __ assm.

ConvertDToIFunc MakeConvertDToIFuncTrampoline(Isolate* isolate,
                                              Register source_reg,
                                              Register destination_reg) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(v8::base::OS::Allocate(
      Assembler::kMinimalBufferSize, &actual_size, true));
  CHECK(buffer);
  HandleScope handles(isolate);
  MacroAssembler assm(isolate, buffer, static_cast<int>(actual_size),
                      v8::internal::CodeObjectRequired::kYes);
  int offset =
    source_reg.is(esp) ? 0 : (HeapNumber::kValueOffset - kSmiTagSize);
  DoubleToIStub stub(isolate, source_reg, destination_reg, offset, true);
  byte* start = stub.GetCode()->instruction_start();

  __ push(ebx);
  __ push(ecx);
  __ push(edx);
  __ push(esi);
  __ push(edi);

  if (!source_reg.is(esp)) {
    __ lea(source_reg, MemOperand(esp, 6 * kPointerSize - offset));
  }

  int param_offset = 7 * kPointerSize;
  // Save registers make sure they don't get clobbered.
  int reg_num = 0;
  for (; reg_num < Register::kNumRegisters; ++reg_num) {
    if (RegisterConfiguration::Crankshaft()->IsAllocatableGeneralCode(
            reg_num)) {
      Register reg = Register::from_code(reg_num);
      if (!reg.is(esp) && !reg.is(ebp) && !reg.is(destination_reg)) {
        __ push(reg);
        param_offset += kPointerSize;
      }
    }
  }

  // Re-push the double argument
  __ push(MemOperand(esp, param_offset));
  __ push(MemOperand(esp, param_offset));

  // Call through to the actual stub
  __ call(start, RelocInfo::EXTERNAL_REFERENCE);

  __ add(esp, Immediate(kDoubleSize));

  // Make sure no registers have been unexpectedly clobbered
  for (--reg_num; reg_num >= 0; --reg_num) {
    if (RegisterConfiguration::Crankshaft()->IsAllocatableGeneralCode(
            reg_num)) {
      Register reg = Register::from_code(reg_num);
      if (!reg.is(esp) && !reg.is(ebp) && !reg.is(destination_reg)) {
        __ cmp(reg, MemOperand(esp, 0));
        __ Assert(equal, kRegisterWasClobbered);
        __ add(esp, Immediate(kPointerSize));
      }
    }
  }

  __ mov(eax, destination_reg);

  __ pop(edi);
  __ pop(esi);
  __ pop(edx);
  __ pop(ecx);
  __ pop(ebx);

  __ ret(kDoubleSize);

  CodeDesc desc;
  assm.GetCode(&desc);
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

  Register source_registers[] = {esp, eax, ebx, ecx, edx, edi, esi};
  Register dest_registers[] = {eax, ebx, ecx, edx, edi, esi};

  for (size_t s = 0; s < sizeof(source_registers) / sizeof(Register); s++) {
    for (size_t d = 0; d < sizeof(dest_registers) / sizeof(Register); d++) {
      RunAllTruncationTests(
          MakeConvertDToIFuncTrampoline(isolate,
                                        source_registers[s],
                                        dest_registers[d]));
    }
  }
}
