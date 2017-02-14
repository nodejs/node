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

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/macro-assembler.h"

#include "src/arm/macro-assembler-arm.h"
#include "src/arm/simulator-arm.h"


using namespace v8::internal;

typedef void* (*F)(int x, int y, int p2, int p3, int p4);

#define __ masm->

typedef int (*F5)(void*, void*, void*, void*, void*);


TEST(LoadAndStoreWithRepresentation) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(v8::base::OS::Allocate(
      Assembler::kMinimalBufferSize, &actual_size, true));
  CHECK(buffer);
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);
  MacroAssembler assembler(isolate, buffer, static_cast<int>(actual_size),
                           v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assembler;  // Create a pointer for the __ macro.
  __ sub(sp, sp, Operand(1 * kPointerSize));
  Label exit;

  // Test 1.
  __ mov(r0, Operand(1));  // Test number.
  __ mov(r1, Operand(0));
  __ str(r1, MemOperand(sp, 0 * kPointerSize));
  __ mov(r2, Operand(-1));
  __ Store(r2, MemOperand(sp, 0 * kPointerSize), Representation::UInteger8());
  __ ldr(r3, MemOperand(sp, 0 * kPointerSize));
  __ mov(r2, Operand(255));
  __ cmp(r3, r2);
  __ b(ne, &exit);
  __ mov(r2, Operand(255));
  __ Load(r3, MemOperand(sp, 0 * kPointerSize), Representation::UInteger8());
  __ cmp(r3, r2);
  __ b(ne, &exit);

  // Test 2.
  __ mov(r0, Operand(2));  // Test number.
  __ mov(r1, Operand(0));
  __ str(r1, MemOperand(sp, 0 * kPointerSize));
  __ mov(r2, Operand(-1));
  __ Store(r2, MemOperand(sp, 0 * kPointerSize), Representation::Integer8());
  __ ldr(r3, MemOperand(sp, 0 * kPointerSize));
  __ mov(r2, Operand(255));
  __ cmp(r3, r2);
  __ b(ne, &exit);
  __ mov(r2, Operand(-1));
  __ Load(r3, MemOperand(sp, 0 * kPointerSize), Representation::Integer8());
  __ cmp(r3, r2);
  __ b(ne, &exit);

  // Test 3.
  __ mov(r0, Operand(3));  // Test number.
  __ mov(r1, Operand(0));
  __ str(r1, MemOperand(sp, 0 * kPointerSize));
  __ mov(r2, Operand(-1));
  __ Store(r2, MemOperand(sp, 0 * kPointerSize), Representation::UInteger16());
  __ ldr(r3, MemOperand(sp, 0 * kPointerSize));
  __ mov(r2, Operand(65535));
  __ cmp(r3, r2);
  __ b(ne, &exit);
  __ mov(r2, Operand(65535));
  __ Load(r3, MemOperand(sp, 0 * kPointerSize), Representation::UInteger16());
  __ cmp(r3, r2);
  __ b(ne, &exit);

  // Test 4.
  __ mov(r0, Operand(4));  // Test number.
  __ mov(r1, Operand(0));
  __ str(r1, MemOperand(sp, 0 * kPointerSize));
  __ mov(r2, Operand(-1));
  __ Store(r2, MemOperand(sp, 0 * kPointerSize), Representation::Integer16());
  __ ldr(r3, MemOperand(sp, 0 * kPointerSize));
  __ mov(r2, Operand(65535));
  __ cmp(r3, r2);
  __ b(ne, &exit);
  __ mov(r2, Operand(-1));
  __ Load(r3, MemOperand(sp, 0 * kPointerSize), Representation::Integer16());
  __ cmp(r3, r2);
  __ b(ne, &exit);

  __ mov(r0, Operand(0));  // Success.
  __ bind(&exit);
  __ add(sp, sp, Operand(1 * kPointerSize));
  __ bx(lr);

  CodeDesc desc;
  masm->GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());

  // Call the function from C++.
  F5 f = FUNCTION_CAST<F5>(code->entry());
  CHECK(!CALL_GENERATED_CODE(isolate, f, 0, 0, 0, 0, 0));
}

#undef __
