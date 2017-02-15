// Copyright 2014 the V8 project authors. All rights reserved.
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

#include "src/v8.h"

#include "src/disassembler.h"
#include "src/factory.h"
#include "src/macro-assembler.h"
#include "src/s390/assembler-s390-inl.h"
#include "src/s390/simulator-s390.h"
#include "test/cctest/cctest.h"

using namespace v8::internal;

// Define these function prototypes to match JSEntryFunction in execution.cc.
typedef Object* (*F1)(int x, int p1, int p2, int p3, int p4);
typedef Object* (*F2)(int x, int y, int p2, int p3, int p4);
typedef Object* (*F3)(void* p0, int p1, int p2, int p3, int p4);
typedef Object* (*F4)(void* p0, void* p1, int p2, int p3, int p4);

#define __ assm.

// Simple add parameter 1 to parameter 2 and return
TEST(0) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);

  __ lhi(r1, Operand(3));    // test 4-byte instr
  __ llilf(r2, Operand(4));  // test 6-byte instr
  __ lgr(r2, r2);            // test 2-byte opcode
  __ ar(r2, r1);             // test 2-byte instr
  __ b(r14);

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  code->Print();
#endif
  F2 f = FUNCTION_CAST<F2>(code->entry());
  intptr_t res = reinterpret_cast<intptr_t>(
      CALL_GENERATED_CODE(isolate, f, 3, 4, 0, 0, 0));
  ::printf("f() = %" V8PRIxPTR "\n", res);
  CHECK_EQ(7, static_cast<int>(res));
}

// Loop 100 times, adding loop counter to result
TEST(1) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);
  Label L, C;

#if defined(_AIX)
  __ function_descriptor();
#endif

  __ lr(r3, r2);
  __ lhi(r2, Operand(0, kRelocInfo_NONEPTR));
  __ b(&C);

  __ bind(&L);
  __ ar(r2, r3);
  __ ahi(r3, Operand(-1 & 0xFFFF));

  __ bind(&C);
  __ cfi(r3, Operand(0, kRelocInfo_NONEPTR));
  __ bne(&L);
  __ b(r14);

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  code->Print();
#endif
  F1 f = FUNCTION_CAST<F1>(code->entry());
  intptr_t res = reinterpret_cast<intptr_t>(
      CALL_GENERATED_CODE(isolate, f, 100, 0, 0, 0, 0));
  ::printf("f() = %" V8PRIxPTR "\n", res);
  CHECK_EQ(5050, static_cast<int>(res));
}

TEST(2) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  // Create a function that accepts &t, and loads, manipulates, and stores
  // the doubles and floats.
  Assembler assm(CcTest::i_isolate(), NULL, 0);
  Label L, C;

#if defined(_AIX)
  __ function_descriptor();
#endif

  __ lgr(r3, r2);
  __ lhi(r2, Operand(1));
  __ b(&C);

  __ bind(&L);
  __ lr(r5, r2);    // Set up muliplicant in R4:R5
  __ mr_z(r4, r3);  // this is actually R4:R5 = R5 * R2
  __ lr(r2, r5);
  __ ahi(r3, Operand(-1 & 0xFFFF));

  __ bind(&C);
  __ cfi(r3, Operand(0, kRelocInfo_NONEPTR));
  __ bne(&L);
  __ b(r14);

  // some relocated stuff here, not executed
  __ RecordComment("dead code, just testing relocations");
  __ iilf(r0, Operand(isolate->factory()->true_value()));
  __ RecordComment("dead code, just testing immediate operands");
  __ iilf(r0, Operand(-1));
  __ iilf(r0, Operand(0xFF000000));
  __ iilf(r0, Operand(0xF0F0F0F0));
  __ iilf(r0, Operand(0xFFF0FFFF));

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  code->Print();
#endif
  F1 f = FUNCTION_CAST<F1>(code->entry());
  intptr_t res = reinterpret_cast<intptr_t>(
      CALL_GENERATED_CODE(isolate, f, 10, 0, 0, 0, 0));
  ::printf("f() = %" V8PRIxPTR "\n", res);
  CHECK_EQ(3628800, static_cast<int>(res));
}

TEST(3) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);

  __ ar(r14, r13);
  __ sr(r14, r13);
  __ mr_z(r14, r13);
  __ dr(r14, r13);
  __ or_z(r14, r13);
  __ nr(r14, r13);
  __ xr(r14, r13);

  __ agr(r14, r13);
  __ sgr(r14, r13);
  __ ogr(r14, r13);
  __ ngr(r14, r13);
  __ xgr(r14, r13);

  __ ahi(r13, Operand(123));
  __ aghi(r13, Operand(123));
  __ stm(r1, r2, MemOperand(r3, r0, 123));
  __ slag(r1, r2, Operand(123));
  __ lay(r1, MemOperand(r2, r3, -123));
  __ a(r13, MemOperand(r1, r2, 123));
  __ ay(r13, MemOperand(r1, r2, 123));
  __ brc(Condition(14), Operand(123));
  __ brc(Condition(14), Operand(-123));
  __ brcl(Condition(14), Operand(123), false);
  __ brcl(Condition(14), Operand(-123), false);
  __ iilf(r13, Operand(123456789));
  __ iihf(r13, Operand(-123456789));
  __ mvc(MemOperand(r0, 123), MemOperand(r4, 567), 89);
  __ sll(r13, Operand(10));

  v8::internal::byte* bufPos = assm.buffer_pos();
  ::printf("buffer position = %p", static_cast<void*>(bufPos));
  ::fflush(stdout);
  // OS::DebugBreak();

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  code->Print();
#endif
  USE(code);
  ::exit(0);
}

#if 0
TEST(4) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);
  Label L2, L3, L4;

  __ chi(r2, Operand(10));
  __ ble(&L2);
  __ lr(r2, r4);
  __ ar(r2, r3);
  __ b(&L3);

  __ bind(&L2);
  __ chi(r2, Operand(5));
  __ bgt(&L4);

  __ lhi(r2, Operand::Zero());
  __ b(&L3);

  __ bind(&L4);
  __ lr(r2, r3);
  __ sr(r2, r4);

  __ bind(&L3);
  __ lgfr(r2, r3);
  __ b(r14);

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  code->Print();
#endif
  F2 f = FUNCTION_CAST<F2>(code->entry());
  intptr_t res = reinterpret_cast<intptr_t>(
      CALL_GENERATED_CODE(isolate, f, 3, 4, 3, 0, 0));
  ::printf("f() = %" V8PRIdPTR "\n", res);
  CHECK_EQ(4, static_cast<int>(res));
}


// Test ExtractBitRange
TEST(5) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assm(isolate, NULL, 0);

  __ mov(r2, Operand(0x12345678));
  __ ExtractBitRange(r3, r2, 3, 2);
  __ lgfr(r2, r3);
  __ b(r14);

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  code->Print();
#endif
  F2 f = FUNCTION_CAST<F2>(code->entry());
  intptr_t res =
    reinterpret_cast<intptr_t>(CALL_GENERATED_CODE(isolate, f, 3, 4, 3, 0, 0));
  ::printf("f() = %" V8PRIdPTR "\n", res);
  CHECK_EQ(2, static_cast<int>(res));
}


// Test JumpIfSmi
TEST(6) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assm(isolate, NULL, 0);

  Label yes;

  __ mov(r2, Operand(0x12345678));
  __ JumpIfSmi(r2, &yes);
  __ beq(&yes);
  __ Load(r2, Operand::Zero());
  __ b(r14);
  __ bind(&yes);
  __ Load(r2, Operand(1));
  __ b(r14);

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  code->Print();
#endif
  F2 f = FUNCTION_CAST<F2>(code->entry());
  intptr_t res =
    reinterpret_cast<intptr_t>(CALL_GENERATED_CODE(isolate, f, 3, 4, 3, 0, 0));
  ::printf("f() = %" V8PRIdPTR "\n", res);
  CHECK_EQ(1, static_cast<int>(res));
}


// Test fix<->floating point conversion.
TEST(7) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assm(isolate, NULL, 0);

  Label yes;

  __ mov(r3, Operand(0x1234));
  __ cdfbr(d1, r3);
  __ ldr(d2, d1);
  __ adbr(d1, d2);
  __ cfdbr(Condition(0), r2, d1);
  __ b(r14);

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  code->Print();
#endif
  F2 f = FUNCTION_CAST<F2>(code->entry());
  intptr_t res =
    reinterpret_cast<intptr_t>(CALL_GENERATED_CODE(isolate, f, 3, 4, 3, 0, 0));
  ::printf("f() = %" V8PRIdPTR "\n", res);
  CHECK_EQ(0x2468, static_cast<int>(res));
}


// Test DSGR
TEST(8) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assm(isolate, NULL, 0);

  // Zero upper bits of r3/r4
  __ llihf(r3, Operand::Zero());
  __ llihf(r4, Operand::Zero());
  __ mov(r3, Operand(0x0002));
  __ mov(r4, Operand(0x0002));
  __ dsgr(r2, r4);
  __ b(r14);

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  code->Print();
#endif
  F1 f = FUNCTION_CAST<F1>(code->entry());
  intptr_t res =
    reinterpret_cast<intptr_t>(CALL_GENERATED_CODE(isolate, f, 100, 0,
                                                   0, 0, 0));
  ::printf("f() = %" V8PRIdPTR  "\n", res);
  CHECK_EQ(0, static_cast<int>(res));
}


// Test LZDR
TEST(9) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assm(isolate, NULL, 0);

  __ lzdr(d4);
  __ b(r14);

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  code->Print();
#endif
  F1 f = FUNCTION_CAST<F1>(code->entry());
  intptr_t res =
    reinterpret_cast<intptr_t>(CALL_GENERATED_CODE(isolate, f, 0, 0, 0, 0, 0));
  ::printf("f() = %" V8PRIdPTR  "\n", res);
}
#endif

#undef __
