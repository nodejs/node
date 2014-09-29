// Copyright 2011 the V8 project authors. All rights reserved.
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

#include "src/base/platform/platform.h"
#include "src/disassembler.h"
#include "src/factory.h"
#include "src/macro-assembler.h"
#include "src/ostreams.h"
#include "src/serialize.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;


typedef int (*F0)();
typedef int (*F1)(int x);
typedef int (*F2)(int x, int y);


#define __ assm.

TEST(AssemblerIa320) {
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);

  v8::internal::byte buffer[256];
  Assembler assm(isolate, buffer, sizeof buffer);

  __ mov(eax, Operand(esp, 4));
  __ add(eax, Operand(esp, 8));
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
#endif
  F2 f = FUNCTION_CAST<F2>(code->entry());
  int res = f(3, 4);
  ::printf("f() = %d\n", res);
  CHECK_EQ(7, res);
}


TEST(AssemblerIa321) {
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);

  v8::internal::byte buffer[256];
  Assembler assm(isolate, buffer, sizeof buffer);
  Label L, C;

  __ mov(edx, Operand(esp, 4));
  __ xor_(eax, eax);  // clear eax
  __ jmp(&C);

  __ bind(&L);
  __ add(eax, edx);
  __ sub(edx, Immediate(1));

  __ bind(&C);
  __ test(edx, edx);
  __ j(not_zero, &L);
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
#endif
  F1 f = FUNCTION_CAST<F1>(code->entry());
  int res = f(100);
  ::printf("f() = %d\n", res);
  CHECK_EQ(5050, res);
}


TEST(AssemblerIa322) {
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);

  v8::internal::byte buffer[256];
  Assembler assm(isolate, buffer, sizeof buffer);
  Label L, C;

  __ mov(edx, Operand(esp, 4));
  __ mov(eax, 1);
  __ jmp(&C);

  __ bind(&L);
  __ imul(eax, edx);
  __ sub(edx, Immediate(1));

  __ bind(&C);
  __ test(edx, edx);
  __ j(not_zero, &L);
  __ ret(0);

  // some relocated stuff here, not executed
  __ mov(eax, isolate->factory()->true_value());
  __ jmp(NULL, RelocInfo::RUNTIME_ENTRY);

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
#endif
  F1 f = FUNCTION_CAST<F1>(code->entry());
  int res = f(10);
  ::printf("f() = %d\n", res);
  CHECK_EQ(3628800, res);
}


typedef int (*F3)(float x);

typedef int (*F4)(double x);

static int baz = 42;
TEST(AssemblerIa325) {
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);

  v8::internal::byte buffer[256];
  Assembler assm(isolate, buffer, sizeof buffer);

  __ mov(eax, Operand(reinterpret_cast<intptr_t>(&baz), RelocInfo::NONE32));
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  F0 f = FUNCTION_CAST<F0>(code->entry());
  int res = f();
  CHECK_EQ(42, res);
}


typedef int (*F7)(double x, double y);

TEST(AssemblerIa329) {
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[256];
  MacroAssembler assm(isolate, buffer, sizeof buffer);
  enum { kEqual = 0, kGreater = 1, kLess = 2, kNaN = 3, kUndefined = 4 };
  Label equal_l, less_l, greater_l, nan_l;
  __ fld_d(Operand(esp, 3 * kPointerSize));
  __ fld_d(Operand(esp, 1 * kPointerSize));
  __ FCmp();
  __ j(parity_even, &nan_l);
  __ j(equal, &equal_l);
  __ j(below, &less_l);
  __ j(above, &greater_l);

  __ mov(eax, kUndefined);
  __ ret(0);

  __ bind(&equal_l);
  __ mov(eax, kEqual);
  __ ret(0);

  __ bind(&greater_l);
  __ mov(eax, kGreater);
  __ ret(0);

  __ bind(&less_l);
  __ mov(eax, kLess);
  __ ret(0);

  __ bind(&nan_l);
  __ mov(eax, kNaN);
  __ ret(0);


  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
#endif

  F7 f = FUNCTION_CAST<F7>(code->entry());
  CHECK_EQ(kLess, f(1.1, 2.2));
  CHECK_EQ(kEqual, f(2.2, 2.2));
  CHECK_EQ(kGreater, f(3.3, 2.2));
  CHECK_EQ(kNaN, f(v8::base::OS::nan_value(), 1.1));
}


TEST(AssemblerIa3210) {
  // Test chaining of label usages within instructions (issue 1644).
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  Assembler assm(isolate, NULL, 0);

  Label target;
  __ j(equal, &target);
  __ j(not_equal, &target);
  __ bind(&target);
  __ nop();
}


TEST(AssemblerMultiByteNop) {
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[1024];
  Assembler assm(isolate, buffer, sizeof(buffer));
  __ push(ebx);
  __ push(ecx);
  __ push(edx);
  __ push(edi);
  __ push(esi);
  __ mov(eax, 1);
  __ mov(ebx, 2);
  __ mov(ecx, 3);
  __ mov(edx, 4);
  __ mov(edi, 5);
  __ mov(esi, 6);
  for (int i = 0; i < 16; i++) {
    int before = assm.pc_offset();
    __ Nop(i);
    CHECK_EQ(assm.pc_offset() - before, i);
  }

  Label fail;
  __ cmp(eax, 1);
  __ j(not_equal, &fail);
  __ cmp(ebx, 2);
  __ j(not_equal, &fail);
  __ cmp(ecx, 3);
  __ j(not_equal, &fail);
  __ cmp(edx, 4);
  __ j(not_equal, &fail);
  __ cmp(edi, 5);
  __ j(not_equal, &fail);
  __ cmp(esi, 6);
  __ j(not_equal, &fail);
  __ mov(eax, 42);
  __ pop(esi);
  __ pop(edi);
  __ pop(edx);
  __ pop(ecx);
  __ pop(ebx);
  __ ret(0);
  __ bind(&fail);
  __ mov(eax, 13);
  __ pop(esi);
  __ pop(edi);
  __ pop(edx);
  __ pop(ecx);
  __ pop(ebx);
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  CHECK(code->IsCode());

  F0 f = FUNCTION_CAST<F0>(code->entry());
  int res = f();
  CHECK_EQ(42, res);
}


#undef __
