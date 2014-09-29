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

#include "src/base/platform/platform.h"
#include "src/factory.h"
#include "src/macro-assembler.h"
#include "src/serialize.h"

using namespace v8::internal;

#if __GNUC__
#define STDCALL  __attribute__((stdcall))
#else
#define STDCALL  __stdcall
#endif

typedef int STDCALL F0Type();
typedef F0Type* F0;

#define __ masm->


TEST(LoadAndStoreWithRepresentation) {
  v8::internal::V8::Initialize(NULL);

  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(v8::base::OS::Allocate(
      Assembler::kMinimalBufferSize, &actual_size, true));
  CHECK(buffer);
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);
  MacroAssembler assembler(isolate, buffer, static_cast<int>(actual_size));
  MacroAssembler* masm = &assembler;  // Create a pointer for the __ macro.
  __ push(ebx);
  __ push(edx);
  __ sub(esp, Immediate(1 * kPointerSize));
  Label exit;

  // Test 1.
  __ mov(eax, Immediate(1));  // Test number.
  __ mov(Operand(esp, 0 * kPointerSize), Immediate(0));
  __ mov(ebx, Immediate(-1));
  __ Store(ebx, Operand(esp, 0 * kPointerSize), Representation::UInteger8());
  __ mov(ebx, Operand(esp, 0 * kPointerSize));
  __ mov(edx, Immediate(255));
  __ cmp(ebx, edx);
  __ j(not_equal, &exit);
  __ Load(ebx, Operand(esp, 0 * kPointerSize), Representation::UInteger8());
  __ cmp(ebx, edx);
  __ j(not_equal, &exit);


  // Test 2.
  __ mov(eax, Immediate(2));  // Test number.
  __ mov(Operand(esp, 0 * kPointerSize), Immediate(0));
  __ mov(ebx, Immediate(-1));
  __ Store(ebx, Operand(esp, 0 * kPointerSize), Representation::Integer8());
  __ mov(ebx, Operand(esp, 0 * kPointerSize));
  __ mov(edx, Immediate(255));
  __ cmp(ebx, edx);
  __ j(not_equal, &exit);
  __ Load(ebx, Operand(esp, 0 * kPointerSize), Representation::Integer8());
  __ mov(edx, Immediate(-1));
  __ cmp(ebx, edx);
  __ j(not_equal, &exit);

  // Test 3.
  __ mov(eax, Immediate(3));  // Test number.
  __ mov(Operand(esp, 0 * kPointerSize), Immediate(0));
  __ mov(ebx, Immediate(-1));
  __ Store(ebx, Operand(esp, 0 * kPointerSize), Representation::Integer16());
  __ mov(ebx, Operand(esp, 0 * kPointerSize));
  __ mov(edx, Immediate(65535));
  __ cmp(ebx, edx);
  __ j(not_equal, &exit);
  __ Load(edx, Operand(esp, 0 * kPointerSize), Representation::Integer16());
  __ mov(ebx, Immediate(-1));
  __ cmp(ebx, edx);
  __ j(not_equal, &exit);

  // Test 4.
  __ mov(eax, Immediate(4));  // Test number.
  __ mov(Operand(esp, 0 * kPointerSize), Immediate(0));
  __ mov(ebx, Immediate(-1));
  __ Store(ebx, Operand(esp, 0 * kPointerSize), Representation::UInteger16());
  __ mov(ebx, Operand(esp, 0 * kPointerSize));
  __ mov(edx, Immediate(65535));
  __ cmp(ebx, edx);
  __ j(not_equal, &exit);
  __ Load(edx, Operand(esp, 0 * kPointerSize), Representation::UInteger16());
  __ cmp(ebx, edx);
  __ j(not_equal, &exit);

  // Test 5.
  __ mov(eax, Immediate(5));  // Test XMM move immediate.
  __ Move(xmm0, 0.0);
  __ Move(xmm1, 0.0);
  __ ucomisd(xmm0, xmm1);
  __ j(not_equal, &exit);
  __ Move(xmm2, 991.01);
  __ ucomisd(xmm0, xmm2);
  __ j(equal, &exit);
  __ Move(xmm0, 991.01);
  __ ucomisd(xmm0, xmm2);
  __ j(not_equal, &exit);

  // Test 6.
  __ mov(eax, Immediate(6));
  __ Move(edx, Immediate(0));  // Test Move()
  __ cmp(edx, Immediate(0));
  __ j(not_equal, &exit);
  __ Move(ecx, Immediate(-1));
  __ cmp(ecx, Immediate(-1));
  __ j(not_equal, &exit);
  __ Move(ebx, Immediate(0x77));
  __ cmp(ebx, Immediate(0x77));
  __ j(not_equal, &exit);

  __ xor_(eax, eax);  // Success.
  __ bind(&exit);
  __ add(esp, Immediate(1 * kPointerSize));
  __ pop(edx);
  __ pop(ebx);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}

#undef __
