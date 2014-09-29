// Copyright 2012 the V8 project authors. All rights reserved.
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
#include "src/mips64/macro-assembler-mips64.h"
#include "src/mips64/simulator-mips64.h"

#include "test/cctest/cctest.h"

using namespace v8::internal;


// Define these function prototypes to match JSEntryFunction in execution.cc.
typedef Object* (*F1)(int x, int p1, int p2, int p3, int p4);
typedef Object* (*F2)(int x, int y, int p2, int p3, int p4);
typedef Object* (*F3)(void* p, int p1, int p2, int p3, int p4);


#define __ assm.


TEST(MIPS0) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assm(isolate, NULL, 0);

  // Addition.
  __ addu(v0, a0, a1);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  F2 f = FUNCTION_CAST<F2>(code->entry());
  int64_t res =
      reinterpret_cast<int64_t>(CALL_GENERATED_CODE(f, 0xab0, 0xc, 0, 0, 0));
  ::printf("f() = %ld\n", res);
  CHECK_EQ(0xabcL, res);
}


TEST(MIPS1) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assm(isolate, NULL, 0);
  Label L, C;

  __ mov(a1, a0);
  __ li(v0, 0);
  __ b(&C);
  __ nop();

  __ bind(&L);
  __ addu(v0, v0, a1);
  __ addiu(a1, a1, -1);

  __ bind(&C);
  __ xori(v1, a1, 0);
  __ Branch(&L, ne, v1, Operand((int64_t)0));
  __ nop();

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  F1 f = FUNCTION_CAST<F1>(code->entry());
  int64_t res =
     reinterpret_cast<int64_t>(CALL_GENERATED_CODE(f, 50, 0, 0, 0, 0));
  ::printf("f() = %ld\n", res);
  CHECK_EQ(1275L, res);
}


TEST(MIPS2) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assm(isolate, NULL, 0);

  Label exit, error;

  // ----- Test all instructions.

  // Test lui, ori, and addiu, used in the li pseudo-instruction.
  // This way we can then safely load registers with chosen values.

  __ ori(a4, zero_reg, 0);
  __ lui(a4, 0x1234);
  __ ori(a4, a4, 0);
  __ ori(a4, a4, 0x0f0f);
  __ ori(a4, a4, 0xf0f0);
  __ addiu(a5, a4, 1);
  __ addiu(a6, a5, -0x10);

  // Load values in temporary registers.
  __ li(a4, 0x00000004);
  __ li(a5, 0x00001234);
  __ li(a6, 0x12345678);
  __ li(a7, 0x7fffffff);
  __ li(t0, 0xfffffffc);
  __ li(t1, 0xffffedcc);
  __ li(t2, 0xedcba988);
  __ li(t3, 0x80000000);

  // SPECIAL class.
  __ srl(v0, a6, 8);    // 0x00123456
  __ sll(v0, v0, 11);   // 0x91a2b000
  __ sra(v0, v0, 3);    // 0xf2345600
  __ srav(v0, v0, a4);  // 0xff234560
  __ sllv(v0, v0, a4);  // 0xf2345600
  __ srlv(v0, v0, a4);  // 0x0f234560
  __ Branch(&error, ne, v0, Operand(0x0f234560));
  __ nop();

  __ addu(v0, a4, a5);  // 0x00001238
  __ subu(v0, v0, a4);  // 0x00001234
  __ Branch(&error, ne, v0, Operand(0x00001234));
  __ nop();
  __ addu(v1, a7, a4);  // 32bit addu result is sign-extended into 64bit reg.
  __ Branch(&error, ne, v1, Operand(0xffffffff80000003));
  __ nop();
  __ subu(v1, t3, a4);  // 0x7ffffffc
  __ Branch(&error, ne, v1, Operand(0x7ffffffc));
  __ nop();

  __ and_(v0, a5, a6);  // 0x0000000000001230
  __ or_(v0, v0, a5);   // 0x0000000000001234
  __ xor_(v0, v0, a6);  // 0x000000001234444c
  __ nor(v0, v0, a6);   // 0xffffffffedcba987
  __ Branch(&error, ne, v0, Operand(0xffffffffedcba983));
  __ nop();

  // Shift both 32bit number to left, to preserve meaning of next comparison.
  __ dsll32(a7, a7, 0);
  __ dsll32(t3, t3, 0);

  __ slt(v0, t3, a7);
  __ Branch(&error, ne, v0, Operand(0x1));
  __ nop();
  __ sltu(v0, t3, a7);
  __ Branch(&error, ne, v0, Operand(zero_reg));
  __ nop();

  // Restore original values in registers.
  __ dsrl32(a7, a7, 0);
  __ dsrl32(t3, t3, 0);
  // End of SPECIAL class.

  __ addiu(v0, zero_reg, 0x7421);  // 0x00007421
  __ addiu(v0, v0, -0x1);          // 0x00007420
  __ addiu(v0, v0, -0x20);         // 0x00007400
  __ Branch(&error, ne, v0, Operand(0x00007400));
  __ nop();
  __ addiu(v1, a7, 0x1);  // 0x80000000 - result is sign-extended.
  __ Branch(&error, ne, v1, Operand(0xffffffff80000000));
  __ nop();

  __ slti(v0, a5, 0x00002000);  // 0x1
  __ slti(v0, v0, 0xffff8000);  // 0x0
  __ Branch(&error, ne, v0, Operand(zero_reg));
  __ nop();
  __ sltiu(v0, a5, 0x00002000);  // 0x1
  __ sltiu(v0, v0, 0x00008000);  // 0x1
  __ Branch(&error, ne, v0, Operand(0x1));
  __ nop();

  __ andi(v0, a5, 0xf0f0);  // 0x00001030
  __ ori(v0, v0, 0x8a00);   // 0x00009a30
  __ xori(v0, v0, 0x83cc);  // 0x000019fc
  __ Branch(&error, ne, v0, Operand(0x000019fc));
  __ nop();
  __ lui(v1, 0x8123);  // Result is sign-extended into 64bit register.
  __ Branch(&error, ne, v1, Operand(0xffffffff81230000));
  __ nop();

  // Bit twiddling instructions & conditional moves.
  // Uses a4-t3 as set above.
  __ Clz(v0, a4);       // 29
  __ Clz(v1, a5);       // 19
  __ addu(v0, v0, v1);  // 48
  __ Clz(v1, a6);       // 3
  __ addu(v0, v0, v1);  // 51
  __ Clz(v1, t3);       // 0
  __ addu(v0, v0, v1);  // 51
  __ Branch(&error, ne, v0, Operand(51));
  __ Movn(a0, a7, a4);  // Move a0<-a7 (a4 is NOT 0).
  __ Ins(a0, a5, 12, 8);  // 0x7ff34fff
  __ Branch(&error, ne, a0, Operand(0x7ff34fff));
  __ Movz(a0, t2, t3);    // a0 not updated (t3 is NOT 0).
  __ Ext(a1, a0, 8, 12);  // 0x34f
  __ Branch(&error, ne, a1, Operand(0x34f));
  __ Movz(a0, t2, v1);    // a0<-t2, v0 is 0, from 8 instr back.
  __ Branch(&error, ne, a0, Operand(t2));

  // Everything was correctly executed. Load the expected result.
  __ li(v0, 0x31415926);
  __ b(&exit);
  __ nop();

  __ bind(&error);
  // Got an error. Return a wrong result.
  __ li(v0, 666);

  __ bind(&exit);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  F2 f = FUNCTION_CAST<F2>(code->entry());
  int64_t res =
      reinterpret_cast<int64_t>(CALL_GENERATED_CODE(f, 0xab0, 0xc, 0, 0, 0));
  ::printf("f() = %ld\n", res);

  CHECK_EQ(0x31415926L, res);
}


TEST(MIPS3) {
  // Test floating point instructions.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  typedef struct {
    double a;
    double b;
    double c;
    double d;
    double e;
    double f;
    double g;
    double h;
    double i;
  } T;
  T t;

  // Create a function that accepts &t, and loads, manipulates, and stores
  // the doubles t.a ... t.f.
  MacroAssembler assm(isolate, NULL, 0);
  Label L, C;

  __ ldc1(f4, MemOperand(a0, OFFSET_OF(T, a)) );
  __ ldc1(f6, MemOperand(a0, OFFSET_OF(T, b)) );
  __ add_d(f8, f4, f6);
  __ sdc1(f8, MemOperand(a0, OFFSET_OF(T, c)) );  // c = a + b.

  __ mov_d(f10, f8);  // c
  __ neg_d(f12, f6);  // -b
  __ sub_d(f10, f10, f12);
  __ sdc1(f10, MemOperand(a0, OFFSET_OF(T, d)) );  // d = c - (-b).

  __ sdc1(f4, MemOperand(a0, OFFSET_OF(T, b)) );   // b = a.

  __ li(a4, 120);
  __ mtc1(a4, f14);
  __ cvt_d_w(f14, f14);   // f14 = 120.0.
  __ mul_d(f10, f10, f14);
  __ sdc1(f10, MemOperand(a0, OFFSET_OF(T, e)) );  // e = d * 120 = 1.8066e16.

  __ div_d(f12, f10, f4);
  __ sdc1(f12, MemOperand(a0, OFFSET_OF(T, f)) );  // f = e / a = 120.44.

  __ sqrt_d(f14, f12);
  __ sdc1(f14, MemOperand(a0, OFFSET_OF(T, g)) );
  // g = sqrt(f) = 10.97451593465515908537

  if (kArchVariant == kMips64r2) {
    __ ldc1(f4, MemOperand(a0, OFFSET_OF(T, h)) );
    __ ldc1(f6, MemOperand(a0, OFFSET_OF(T, i)) );
    __ madd_d(f14, f6, f4, f6);
    __ sdc1(f14, MemOperand(a0, OFFSET_OF(T, h)) );
  }

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  F3 f = FUNCTION_CAST<F3>(code->entry());
  t.a = 1.5e14;
  t.b = 2.75e11;
  t.c = 0.0;
  t.d = 0.0;
  t.e = 0.0;
  t.f = 0.0;
  t.h = 1.5;
  t.i = 2.75;
  Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
  USE(dummy);
  CHECK_EQ(1.5e14, t.a);
  CHECK_EQ(1.5e14, t.b);
  CHECK_EQ(1.50275e14, t.c);
  CHECK_EQ(1.50550e14, t.d);
  CHECK_EQ(1.8066e16, t.e);
  CHECK_EQ(120.44, t.f);
  CHECK_EQ(10.97451593465515908537, t.g);
  if (kArchVariant == kMips64r2) {
    CHECK_EQ(6.875, t.h);
  }
}


TEST(MIPS4) {
  // Test moves between floating point and integer registers.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  typedef struct {
    double a;
    double b;
    double c;
  } T;
  T t;

  Assembler assm(isolate, NULL, 0);
  Label L, C;

  __ ldc1(f4, MemOperand(a0, OFFSET_OF(T, a)) );
  __ ldc1(f5, MemOperand(a0, OFFSET_OF(T, b)) );

  // Swap f4 and f5, by using 3 integer registers, a4-a6,
  // both two 32-bit chunks, and one 64-bit chunk.
  // mXhc1 is mips32/64-r2 only, not r1,
  // but we will not support r1 in practice.
  __ mfc1(a4, f4);
  __ mfhc1(a5, f4);
  __ dmfc1(a6, f5);

  __ mtc1(a4, f5);
  __ mthc1(a5, f5);
  __ dmtc1(a6, f4);

  // Store the swapped f4 and f5 back to memory.
  __ sdc1(f4, MemOperand(a0, OFFSET_OF(T, a)) );
  __ sdc1(f5, MemOperand(a0, OFFSET_OF(T, c)) );

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  F3 f = FUNCTION_CAST<F3>(code->entry());
  t.a = 1.5e22;
  t.b = 2.75e11;
  t.c = 17.17;
  Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
  USE(dummy);

  CHECK_EQ(2.75e11, t.a);
  CHECK_EQ(2.75e11, t.b);
  CHECK_EQ(1.5e22, t.c);
}


TEST(MIPS5) {
  // Test conversions between doubles and integers.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  typedef struct {
    double a;
    double b;
    int i;
    int j;
  } T;
  T t;

  Assembler assm(isolate, NULL, 0);
  Label L, C;

  // Load all structure elements to registers.
  __ ldc1(f4, MemOperand(a0, OFFSET_OF(T, a)) );
  __ ldc1(f6, MemOperand(a0, OFFSET_OF(T, b)) );
  __ lw(a4, MemOperand(a0, OFFSET_OF(T, i)) );
  __ lw(a5, MemOperand(a0, OFFSET_OF(T, j)) );

  // Convert double in f4 to int in element i.
  __ cvt_w_d(f8, f4);
  __ mfc1(a6, f8);
  __ sw(a6, MemOperand(a0, OFFSET_OF(T, i)) );

  // Convert double in f6 to int in element j.
  __ cvt_w_d(f10, f6);
  __ mfc1(a7, f10);
  __ sw(a7, MemOperand(a0, OFFSET_OF(T, j)) );

  // Convert int in original i (a4) to double in a.
  __ mtc1(a4, f12);
  __ cvt_d_w(f0, f12);
  __ sdc1(f0, MemOperand(a0, OFFSET_OF(T, a)) );

  // Convert int in original j (a5) to double in b.
  __ mtc1(a5, f14);
  __ cvt_d_w(f2, f14);
  __ sdc1(f2, MemOperand(a0, OFFSET_OF(T, b)) );

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  F3 f = FUNCTION_CAST<F3>(code->entry());
  t.a = 1.5e4;
  t.b = 2.75e8;
  t.i = 12345678;
  t.j = -100000;
  Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
  USE(dummy);

  CHECK_EQ(12345678.0, t.a);
  CHECK_EQ(-100000.0, t.b);
  CHECK_EQ(15000, t.i);
  CHECK_EQ(275000000, t.j);
}


TEST(MIPS6) {
  // Test simple memory loads and stores.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  typedef struct {
    uint32_t ui;
    int32_t si;
    int32_t r1;
    int32_t r2;
    int32_t r3;
    int32_t r4;
    int32_t r5;
    int32_t r6;
  } T;
  T t;

  Assembler assm(isolate, NULL, 0);
  Label L, C;

  // Basic word load/store.
  __ lw(a4, MemOperand(a0, OFFSET_OF(T, ui)) );
  __ sw(a4, MemOperand(a0, OFFSET_OF(T, r1)) );

  // lh with positive data.
  __ lh(a5, MemOperand(a0, OFFSET_OF(T, ui)) );
  __ sw(a5, MemOperand(a0, OFFSET_OF(T, r2)) );

  // lh with negative data.
  __ lh(a6, MemOperand(a0, OFFSET_OF(T, si)) );
  __ sw(a6, MemOperand(a0, OFFSET_OF(T, r3)) );

  // lhu with negative data.
  __ lhu(a7, MemOperand(a0, OFFSET_OF(T, si)) );
  __ sw(a7, MemOperand(a0, OFFSET_OF(T, r4)) );

  // lb with negative data.
  __ lb(t0, MemOperand(a0, OFFSET_OF(T, si)) );
  __ sw(t0, MemOperand(a0, OFFSET_OF(T, r5)) );

  // sh writes only 1/2 of word.
  __ lui(t1, 0x3333);
  __ ori(t1, t1, 0x3333);
  __ sw(t1, MemOperand(a0, OFFSET_OF(T, r6)) );
  __ lhu(t1, MemOperand(a0, OFFSET_OF(T, si)) );
  __ sh(t1, MemOperand(a0, OFFSET_OF(T, r6)) );

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  F3 f = FUNCTION_CAST<F3>(code->entry());
  t.ui = 0x11223344;
  t.si = 0x99aabbcc;
  Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
  USE(dummy);

  CHECK_EQ(0x11223344, t.r1);
  CHECK_EQ(0x3344, t.r2);
  CHECK_EQ(0xffffbbcc, t.r3);
  CHECK_EQ(0x0000bbcc, t.r4);
  CHECK_EQ(0xffffffcc, t.r5);
  CHECK_EQ(0x3333bbcc, t.r6);
}


TEST(MIPS7) {
  // Test floating point compare and branch instructions.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  typedef struct {
    double a;
    double b;
    double c;
    double d;
    double e;
    double f;
    int32_t result;
  } T;
  T t;

  // Create a function that accepts &t, and loads, manipulates, and stores
  // the doubles t.a ... t.f.
  MacroAssembler assm(isolate, NULL, 0);
  Label neither_is_nan, less_than, outa_here;

  __ ldc1(f4, MemOperand(a0, OFFSET_OF(T, a)) );
  __ ldc1(f6, MemOperand(a0, OFFSET_OF(T, b)) );
  if (kArchVariant != kMips64r6) {
    __ c(UN, D, f4, f6);
    __ bc1f(&neither_is_nan);
  } else {
    __ cmp(UN, L, f2, f4, f6);
    __ bc1eqz(&neither_is_nan, f2);
  }
  __ nop();
  __ sw(zero_reg, MemOperand(a0, OFFSET_OF(T, result)) );
  __ Branch(&outa_here);

  __ bind(&neither_is_nan);

  if (kArchVariant == kMips64r6) {
    __ cmp(OLT, L, f2, f6, f4);
    __ bc1nez(&less_than, f2);
  } else {
    __ c(OLT, D, f6, f4, 2);
    __ bc1t(&less_than, 2);
  }

  __ nop();
  __ sw(zero_reg, MemOperand(a0, OFFSET_OF(T, result)) );
  __ Branch(&outa_here);

  __ bind(&less_than);
  __ Addu(a4, zero_reg, Operand(1));
  __ sw(a4, MemOperand(a0, OFFSET_OF(T, result)) );  // Set true.


  // This test-case should have additional tests.

  __ bind(&outa_here);

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  F3 f = FUNCTION_CAST<F3>(code->entry());
  t.a = 1.5e14;
  t.b = 2.75e11;
  t.c = 2.0;
  t.d = -4.0;
  t.e = 0.0;
  t.f = 0.0;
  t.result = 0;
  Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
  USE(dummy);
  CHECK_EQ(1.5e14, t.a);
  CHECK_EQ(2.75e11, t.b);
  CHECK_EQ(1, t.result);
}


TEST(MIPS8) {
  // Test ROTR and ROTRV instructions.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  typedef struct {
    int32_t input;
    int32_t result_rotr_4;
    int32_t result_rotr_8;
    int32_t result_rotr_12;
    int32_t result_rotr_16;
    int32_t result_rotr_20;
    int32_t result_rotr_24;
    int32_t result_rotr_28;
    int32_t result_rotrv_4;
    int32_t result_rotrv_8;
    int32_t result_rotrv_12;
    int32_t result_rotrv_16;
    int32_t result_rotrv_20;
    int32_t result_rotrv_24;
    int32_t result_rotrv_28;
  } T;
  T t;

  MacroAssembler assm(isolate, NULL, 0);

  // Basic word load.
  __ lw(a4, MemOperand(a0, OFFSET_OF(T, input)) );

  // ROTR instruction (called through the Ror macro).
  __ Ror(a5, a4, 0x0004);
  __ Ror(a6, a4, 0x0008);
  __ Ror(a7, a4, 0x000c);
  __ Ror(t0, a4, 0x0010);
  __ Ror(t1, a4, 0x0014);
  __ Ror(t2, a4, 0x0018);
  __ Ror(t3, a4, 0x001c);

  // Basic word store.
  __ sw(a5, MemOperand(a0, OFFSET_OF(T, result_rotr_4)) );
  __ sw(a6, MemOperand(a0, OFFSET_OF(T, result_rotr_8)) );
  __ sw(a7, MemOperand(a0, OFFSET_OF(T, result_rotr_12)) );
  __ sw(t0, MemOperand(a0, OFFSET_OF(T, result_rotr_16)) );
  __ sw(t1, MemOperand(a0, OFFSET_OF(T, result_rotr_20)) );
  __ sw(t2, MemOperand(a0, OFFSET_OF(T, result_rotr_24)) );
  __ sw(t3, MemOperand(a0, OFFSET_OF(T, result_rotr_28)) );

  // ROTRV instruction (called through the Ror macro).
  __ li(t3, 0x0004);
  __ Ror(a5, a4, t3);
  __ li(t3, 0x0008);
  __ Ror(a6, a4, t3);
  __ li(t3, 0x000C);
  __ Ror(a7, a4, t3);
  __ li(t3, 0x0010);
  __ Ror(t0, a4, t3);
  __ li(t3, 0x0014);
  __ Ror(t1, a4, t3);
  __ li(t3, 0x0018);
  __ Ror(t2, a4, t3);
  __ li(t3, 0x001C);
  __ Ror(t3, a4, t3);

  // Basic word store.
  __ sw(a5, MemOperand(a0, OFFSET_OF(T, result_rotrv_4)) );
  __ sw(a6, MemOperand(a0, OFFSET_OF(T, result_rotrv_8)) );
  __ sw(a7, MemOperand(a0, OFFSET_OF(T, result_rotrv_12)) );
  __ sw(t0, MemOperand(a0, OFFSET_OF(T, result_rotrv_16)) );
  __ sw(t1, MemOperand(a0, OFFSET_OF(T, result_rotrv_20)) );
  __ sw(t2, MemOperand(a0, OFFSET_OF(T, result_rotrv_24)) );
  __ sw(t3, MemOperand(a0, OFFSET_OF(T, result_rotrv_28)) );

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  F3 f = FUNCTION_CAST<F3>(code->entry());
  t.input = 0x12345678;
  Object* dummy = CALL_GENERATED_CODE(f, &t, 0x0, 0, 0, 0);
  USE(dummy);
  CHECK_EQ(0x81234567, t.result_rotr_4);
  CHECK_EQ(0x78123456, t.result_rotr_8);
  CHECK_EQ(0x67812345, t.result_rotr_12);
  CHECK_EQ(0x56781234, t.result_rotr_16);
  CHECK_EQ(0x45678123, t.result_rotr_20);
  CHECK_EQ(0x34567812, t.result_rotr_24);
  CHECK_EQ(0x23456781, t.result_rotr_28);

  CHECK_EQ(0x81234567, t.result_rotrv_4);
  CHECK_EQ(0x78123456, t.result_rotrv_8);
  CHECK_EQ(0x67812345, t.result_rotrv_12);
  CHECK_EQ(0x56781234, t.result_rotrv_16);
  CHECK_EQ(0x45678123, t.result_rotrv_20);
  CHECK_EQ(0x34567812, t.result_rotrv_24);
  CHECK_EQ(0x23456781, t.result_rotrv_28);
}


TEST(MIPS9) {
  // Test BRANCH improvements.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assm(isolate, NULL, 0);
  Label exit, exit2, exit3;

  __ Branch(&exit, ge, a0, Operand(zero_reg));
  __ Branch(&exit2, ge, a0, Operand(0x00001FFF));
  __ Branch(&exit3, ge, a0, Operand(0x0001FFFF));

  __ bind(&exit);
  __ bind(&exit2);
  __ bind(&exit3);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
}


TEST(MIPS10) {
  // Test conversions between doubles and long integers.
  // Test hos the long ints map to FP regs pairs.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  typedef struct {
    double a;
    double a_converted;
    double b;
    int32_t dbl_mant;
    int32_t dbl_exp;
    int32_t long_hi;
    int32_t long_lo;
    int64_t long_as_int64;
    int32_t b_long_hi;
    int32_t b_long_lo;
    int64_t b_long_as_int64;
  } T;
  T t;

  Assembler assm(isolate, NULL, 0);
  Label L, C;

  if (kArchVariant == kMips64r2) {
    // Rewritten for FR=1 FPU mode:
    //  -  32 FP regs of 64-bits each, no odd/even pairs.
    //  -  Note that cvt_l_d/cvt_d_l ARE legal in FR=1 mode.
    // Load all structure elements to registers.
    __ ldc1(f0, MemOperand(a0, OFFSET_OF(T, a)));

    // Save the raw bits of the double.
    __ mfc1(a4, f0);
    __ mfhc1(a5, f0);
    __ sw(a4, MemOperand(a0, OFFSET_OF(T, dbl_mant)));
    __ sw(a5, MemOperand(a0, OFFSET_OF(T, dbl_exp)));

    // Convert double in f0 to long, save hi/lo parts.
    __ cvt_l_d(f0, f0);
    __ mfc1(a4, f0);  // f0 LS 32 bits of long.
    __ mfhc1(a5, f0);  // f0 MS 32 bits of long.
    __ sw(a4, MemOperand(a0, OFFSET_OF(T, long_lo)));
    __ sw(a5, MemOperand(a0, OFFSET_OF(T, long_hi)));

    // Combine the high/low ints, convert back to double.
    __ dsll32(a6, a5, 0);  // Move a5 to high bits of a6.
    __ or_(a6, a6, a4);
    __ dmtc1(a6, f1);
    __ cvt_d_l(f1, f1);
    __ sdc1(f1, MemOperand(a0, OFFSET_OF(T, a_converted)));


    // Convert the b long integers to double b.
    __ lw(a4, MemOperand(a0, OFFSET_OF(T, b_long_lo)));
    __ lw(a5, MemOperand(a0, OFFSET_OF(T, b_long_hi)));
    __ mtc1(a4, f8);  // f8 LS 32-bits.
    __ mthc1(a5, f8);  // f8 MS 32-bits.
    __ cvt_d_l(f10, f8);
    __ sdc1(f10, MemOperand(a0, OFFSET_OF(T, b)));

    // Convert double b back to long-int.
    __ ldc1(f31, MemOperand(a0, OFFSET_OF(T, b)));
    __ cvt_l_d(f31, f31);
    __ dmfc1(a7, f31);
    __ sd(a7, MemOperand(a0, OFFSET_OF(T, b_long_as_int64)));


    __ jr(ra);
    __ nop();

    CodeDesc desc;
    assm.GetCode(&desc);
    Handle<Code> code = isolate->factory()->NewCode(
        desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
    F3 f = FUNCTION_CAST<F3>(code->entry());
    t.a = 2.147483647e9;       // 0x7fffffff -> 0x41DFFFFFFFC00000 as double.
    t.b_long_hi = 0x000000ff;  // 0xFF00FF00FF -> 0x426FE01FE01FE000 as double.
    t.b_long_lo = 0x00ff00ff;
    Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
    USE(dummy);

    CHECK_EQ(0x41DFFFFF, t.dbl_exp);
    CHECK_EQ(0xFFC00000, t.dbl_mant);
    CHECK_EQ(0, t.long_hi);
    CHECK_EQ(0x7fffffff, t.long_lo);
    CHECK_EQ(2.147483647e9, t.a_converted);

    // 0xFF00FF00FF -> 1.095233372415e12.
    CHECK_EQ(1.095233372415e12, t.b);
    CHECK_EQ(0xFF00FF00FF, t.b_long_as_int64);
  }
}


TEST(MIPS11) {
  // Do not run test on MIPS64r6, as these instructions are removed.
  if (kArchVariant != kMips64r6) {
    // Test LWL, LWR, SWL and SWR instructions.
    CcTest::InitializeVM();
    Isolate* isolate = CcTest::i_isolate();
    HandleScope scope(isolate);

    typedef struct {
      int32_t reg_init;
      int32_t mem_init;
      int32_t lwl_0;
      int32_t lwl_1;
      int32_t lwl_2;
      int32_t lwl_3;
      int32_t lwr_0;
      int32_t lwr_1;
      int32_t lwr_2;
      int32_t lwr_3;
      int32_t swl_0;
      int32_t swl_1;
      int32_t swl_2;
      int32_t swl_3;
      int32_t swr_0;
      int32_t swr_1;
      int32_t swr_2;
      int32_t swr_3;
    } T;
    T t;

    Assembler assm(isolate, NULL, 0);

    // Test all combinations of LWL and vAddr.
    __ lw(a4, MemOperand(a0, OFFSET_OF(T, reg_init)) );
    __ lwl(a4, MemOperand(a0, OFFSET_OF(T, mem_init)) );
    __ sw(a4, MemOperand(a0, OFFSET_OF(T, lwl_0)) );

    __ lw(a5, MemOperand(a0, OFFSET_OF(T, reg_init)) );
    __ lwl(a5, MemOperand(a0, OFFSET_OF(T, mem_init) + 1) );
    __ sw(a5, MemOperand(a0, OFFSET_OF(T, lwl_1)) );

    __ lw(a6, MemOperand(a0, OFFSET_OF(T, reg_init)) );
    __ lwl(a6, MemOperand(a0, OFFSET_OF(T, mem_init) + 2) );
    __ sw(a6, MemOperand(a0, OFFSET_OF(T, lwl_2)) );

    __ lw(a7, MemOperand(a0, OFFSET_OF(T, reg_init)) );
    __ lwl(a7, MemOperand(a0, OFFSET_OF(T, mem_init) + 3) );
    __ sw(a7, MemOperand(a0, OFFSET_OF(T, lwl_3)) );

    // Test all combinations of LWR and vAddr.
    __ lw(a4, MemOperand(a0, OFFSET_OF(T, reg_init)) );
    __ lwr(a4, MemOperand(a0, OFFSET_OF(T, mem_init)) );
    __ sw(a4, MemOperand(a0, OFFSET_OF(T, lwr_0)) );

    __ lw(a5, MemOperand(a0, OFFSET_OF(T, reg_init)) );
    __ lwr(a5, MemOperand(a0, OFFSET_OF(T, mem_init) + 1) );
    __ sw(a5, MemOperand(a0, OFFSET_OF(T, lwr_1)) );

    __ lw(a6, MemOperand(a0, OFFSET_OF(T, reg_init)) );
    __ lwr(a6, MemOperand(a0, OFFSET_OF(T, mem_init) + 2) );
    __ sw(a6, MemOperand(a0, OFFSET_OF(T, lwr_2)) );

    __ lw(a7, MemOperand(a0, OFFSET_OF(T, reg_init)) );
    __ lwr(a7, MemOperand(a0, OFFSET_OF(T, mem_init) + 3) );
    __ sw(a7, MemOperand(a0, OFFSET_OF(T, lwr_3)) );

    // Test all combinations of SWL and vAddr.
    __ lw(a4, MemOperand(a0, OFFSET_OF(T, mem_init)) );
    __ sw(a4, MemOperand(a0, OFFSET_OF(T, swl_0)) );
    __ lw(a4, MemOperand(a0, OFFSET_OF(T, reg_init)) );
    __ swl(a4, MemOperand(a0, OFFSET_OF(T, swl_0)) );

    __ lw(a5, MemOperand(a0, OFFSET_OF(T, mem_init)) );
    __ sw(a5, MemOperand(a0, OFFSET_OF(T, swl_1)) );
    __ lw(a5, MemOperand(a0, OFFSET_OF(T, reg_init)) );
    __ swl(a5, MemOperand(a0, OFFSET_OF(T, swl_1) + 1) );

    __ lw(a6, MemOperand(a0, OFFSET_OF(T, mem_init)) );
    __ sw(a6, MemOperand(a0, OFFSET_OF(T, swl_2)) );
    __ lw(a6, MemOperand(a0, OFFSET_OF(T, reg_init)) );
    __ swl(a6, MemOperand(a0, OFFSET_OF(T, swl_2) + 2) );

    __ lw(a7, MemOperand(a0, OFFSET_OF(T, mem_init)) );
    __ sw(a7, MemOperand(a0, OFFSET_OF(T, swl_3)) );
    __ lw(a7, MemOperand(a0, OFFSET_OF(T, reg_init)) );
    __ swl(a7, MemOperand(a0, OFFSET_OF(T, swl_3) + 3) );

    // Test all combinations of SWR and vAddr.
    __ lw(a4, MemOperand(a0, OFFSET_OF(T, mem_init)) );
    __ sw(a4, MemOperand(a0, OFFSET_OF(T, swr_0)) );
    __ lw(a4, MemOperand(a0, OFFSET_OF(T, reg_init)) );
    __ swr(a4, MemOperand(a0, OFFSET_OF(T, swr_0)) );

    __ lw(a5, MemOperand(a0, OFFSET_OF(T, mem_init)) );
    __ sw(a5, MemOperand(a0, OFFSET_OF(T, swr_1)) );
    __ lw(a5, MemOperand(a0, OFFSET_OF(T, reg_init)) );
    __ swr(a5, MemOperand(a0, OFFSET_OF(T, swr_1) + 1) );

    __ lw(a6, MemOperand(a0, OFFSET_OF(T, mem_init)) );
    __ sw(a6, MemOperand(a0, OFFSET_OF(T, swr_2)) );
    __ lw(a6, MemOperand(a0, OFFSET_OF(T, reg_init)) );
    __ swr(a6, MemOperand(a0, OFFSET_OF(T, swr_2) + 2) );

    __ lw(a7, MemOperand(a0, OFFSET_OF(T, mem_init)) );
    __ sw(a7, MemOperand(a0, OFFSET_OF(T, swr_3)) );
    __ lw(a7, MemOperand(a0, OFFSET_OF(T, reg_init)) );
    __ swr(a7, MemOperand(a0, OFFSET_OF(T, swr_3) + 3) );

    __ jr(ra);
    __ nop();

    CodeDesc desc;
    assm.GetCode(&desc);
    Handle<Code> code = isolate->factory()->NewCode(
        desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
    F3 f = FUNCTION_CAST<F3>(code->entry());
    t.reg_init = 0xaabbccdd;
    t.mem_init = 0x11223344;

    Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
    USE(dummy);

    CHECK_EQ(0x44bbccdd, t.lwl_0);
    CHECK_EQ(0x3344ccdd, t.lwl_1);
    CHECK_EQ(0x223344dd, t.lwl_2);
    CHECK_EQ(0x11223344, t.lwl_3);

    CHECK_EQ(0x11223344, t.lwr_0);
    CHECK_EQ(0xaa112233, t.lwr_1);
    CHECK_EQ(0xaabb1122, t.lwr_2);
    CHECK_EQ(0xaabbcc11, t.lwr_3);

    CHECK_EQ(0x112233aa, t.swl_0);
    CHECK_EQ(0x1122aabb, t.swl_1);
    CHECK_EQ(0x11aabbcc, t.swl_2);
    CHECK_EQ(0xaabbccdd, t.swl_3);

    CHECK_EQ(0xaabbccdd, t.swr_0);
    CHECK_EQ(0xbbccdd44, t.swr_1);
    CHECK_EQ(0xccdd3344, t.swr_2);
    CHECK_EQ(0xdd223344, t.swr_3);
  }
}


TEST(MIPS12) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  typedef struct {
      int32_t  x;
      int32_t  y;
      int32_t  y1;
      int32_t  y2;
      int32_t  y3;
      int32_t  y4;
  } T;
  T t;

  MacroAssembler assm(isolate, NULL, 0);

  __ mov(t2, fp);  // Save frame pointer.
  __ mov(fp, a0);  // Access struct T by fp.
  __ lw(a4, MemOperand(a0, OFFSET_OF(T, y)) );
  __ lw(a7, MemOperand(a0, OFFSET_OF(T, y4)) );

  __ addu(a5, a4, a7);
  __ subu(t0, a4, a7);
  __ nop();
  __ push(a4);  // These instructions disappear after opt.
  __ Pop();
  __ addu(a4, a4, a4);
  __ nop();
  __ Pop();     // These instructions disappear after opt.
  __ push(a7);
  __ nop();
  __ push(a7);  // These instructions disappear after opt.
  __ pop(a7);
  __ nop();
  __ push(a7);
  __ pop(t0);
  __ nop();
  __ sw(a4, MemOperand(fp, OFFSET_OF(T, y)) );
  __ lw(a4, MemOperand(fp, OFFSET_OF(T, y)) );
  __ nop();
  __ sw(a4, MemOperand(fp, OFFSET_OF(T, y)) );
  __ lw(a5, MemOperand(fp, OFFSET_OF(T, y)) );
  __ nop();
  __ push(a5);
  __ lw(a5, MemOperand(fp, OFFSET_OF(T, y)) );
  __ pop(a5);
  __ nop();
  __ push(a5);
  __ lw(a6, MemOperand(fp, OFFSET_OF(T, y)) );
  __ pop(a5);
  __ nop();
  __ push(a5);
  __ lw(a6, MemOperand(fp, OFFSET_OF(T, y)) );
  __ pop(a6);
  __ nop();
  __ push(a6);
  __ lw(a6, MemOperand(fp, OFFSET_OF(T, y)) );
  __ pop(a5);
  __ nop();
  __ push(a5);
  __ lw(a6, MemOperand(fp, OFFSET_OF(T, y)) );
  __ pop(a7);
  __ nop();

  __ mov(fp, t2);
  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  F3 f = FUNCTION_CAST<F3>(code->entry());
  t.x = 1;
  t.y = 2;
  t.y1 = 3;
  t.y2 = 4;
  t.y3 = 0XBABA;
  t.y4 = 0xDEDA;

  Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
  USE(dummy);

  CHECK_EQ(3, t.y1);
}


TEST(MIPS13) {
  // Test Cvt_d_uw and Trunc_uw_d macros.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  typedef struct {
    double cvt_big_out;
    double cvt_small_out;
    uint32_t trunc_big_out;
    uint32_t trunc_small_out;
    uint32_t cvt_big_in;
    uint32_t cvt_small_in;
  } T;
  T t;

  MacroAssembler assm(isolate, NULL, 0);

  __ sw(a4, MemOperand(a0, OFFSET_OF(T, cvt_small_in)));
  __ Cvt_d_uw(f10, a4, f22);
  __ sdc1(f10, MemOperand(a0, OFFSET_OF(T, cvt_small_out)));

  __ Trunc_uw_d(f10, f10, f22);
  __ swc1(f10, MemOperand(a0, OFFSET_OF(T, trunc_small_out)));

  __ sw(a4, MemOperand(a0, OFFSET_OF(T, cvt_big_in)));
  __ Cvt_d_uw(f8, a4, f22);
  __ sdc1(f8, MemOperand(a0, OFFSET_OF(T, cvt_big_out)));

  __ Trunc_uw_d(f8, f8, f22);
  __ swc1(f8, MemOperand(a0, OFFSET_OF(T, trunc_big_out)));

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  F3 f = FUNCTION_CAST<F3>(code->entry());

  t.cvt_big_in = 0xFFFFFFFF;
  t.cvt_small_in  = 333;

  Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
  USE(dummy);

  CHECK_EQ(t.cvt_big_out, static_cast<double>(t.cvt_big_in));
  CHECK_EQ(t.cvt_small_out, static_cast<double>(t.cvt_small_in));

  CHECK_EQ(static_cast<int>(t.trunc_big_out), static_cast<int>(t.cvt_big_in));
  CHECK_EQ(static_cast<int>(t.trunc_small_out),
           static_cast<int>(t.cvt_small_in));
}


TEST(MIPS14) {
  // Test round, floor, ceil, trunc, cvt.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

#define ROUND_STRUCT_ELEMENT(x) \
  int32_t x##_up_out; \
  int32_t x##_down_out; \
  int32_t neg_##x##_up_out; \
  int32_t neg_##x##_down_out; \
  uint32_t x##_err1_out; \
  uint32_t x##_err2_out; \
  uint32_t x##_err3_out; \
  uint32_t x##_err4_out; \
  int32_t x##_invalid_result;

  typedef struct {
    double round_up_in;
    double round_down_in;
    double neg_round_up_in;
    double neg_round_down_in;
    double err1_in;
    double err2_in;
    double err3_in;
    double err4_in;

    ROUND_STRUCT_ELEMENT(round)
    ROUND_STRUCT_ELEMENT(floor)
    ROUND_STRUCT_ELEMENT(ceil)
    ROUND_STRUCT_ELEMENT(trunc)
    ROUND_STRUCT_ELEMENT(cvt)
  } T;
  T t;

#undef ROUND_STRUCT_ELEMENT

  MacroAssembler assm(isolate, NULL, 0);

  // Save FCSR.
  __ cfc1(a1, FCSR);
  // Disable FPU exceptions.
  __ ctc1(zero_reg, FCSR);
#define RUN_ROUND_TEST(x) \
  __ ldc1(f0, MemOperand(a0, OFFSET_OF(T, round_up_in))); \
  __ x##_w_d(f0, f0); \
  __ swc1(f0, MemOperand(a0, OFFSET_OF(T, x##_up_out))); \
  \
  __ ldc1(f0, MemOperand(a0, OFFSET_OF(T, round_down_in))); \
  __ x##_w_d(f0, f0); \
  __ swc1(f0, MemOperand(a0, OFFSET_OF(T, x##_down_out))); \
  \
  __ ldc1(f0, MemOperand(a0, OFFSET_OF(T, neg_round_up_in))); \
  __ x##_w_d(f0, f0); \
  __ swc1(f0, MemOperand(a0, OFFSET_OF(T, neg_##x##_up_out))); \
  \
  __ ldc1(f0, MemOperand(a0, OFFSET_OF(T, neg_round_down_in))); \
  __ x##_w_d(f0, f0); \
  __ swc1(f0, MemOperand(a0, OFFSET_OF(T, neg_##x##_down_out))); \
  \
  __ ldc1(f0, MemOperand(a0, OFFSET_OF(T, err1_in))); \
  __ ctc1(zero_reg, FCSR); \
  __ x##_w_d(f0, f0); \
  __ cfc1(a2, FCSR); \
  __ sw(a2, MemOperand(a0, OFFSET_OF(T, x##_err1_out))); \
  \
  __ ldc1(f0, MemOperand(a0, OFFSET_OF(T, err2_in))); \
  __ ctc1(zero_reg, FCSR); \
  __ x##_w_d(f0, f0); \
  __ cfc1(a2, FCSR); \
  __ sw(a2, MemOperand(a0, OFFSET_OF(T, x##_err2_out))); \
  \
  __ ldc1(f0, MemOperand(a0, OFFSET_OF(T, err3_in))); \
  __ ctc1(zero_reg, FCSR); \
  __ x##_w_d(f0, f0); \
  __ cfc1(a2, FCSR); \
  __ sw(a2, MemOperand(a0, OFFSET_OF(T, x##_err3_out))); \
  \
  __ ldc1(f0, MemOperand(a0, OFFSET_OF(T, err4_in))); \
  __ ctc1(zero_reg, FCSR); \
  __ x##_w_d(f0, f0); \
  __ cfc1(a2, FCSR); \
  __ sw(a2, MemOperand(a0, OFFSET_OF(T, x##_err4_out))); \
  __ swc1(f0, MemOperand(a0, OFFSET_OF(T, x##_invalid_result)));

  RUN_ROUND_TEST(round)
  RUN_ROUND_TEST(floor)
  RUN_ROUND_TEST(ceil)
  RUN_ROUND_TEST(trunc)
  RUN_ROUND_TEST(cvt)

  // Restore FCSR.
  __ ctc1(a1, FCSR);

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  F3 f = FUNCTION_CAST<F3>(code->entry());

  t.round_up_in = 123.51;
  t.round_down_in = 123.49;
  t.neg_round_up_in = -123.5;
  t.neg_round_down_in = -123.49;
  t.err1_in = 123.51;
  t.err2_in = 1;
  t.err3_in = static_cast<double>(1) + 0xFFFFFFFF;
  t.err4_in = NAN;

  Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
  USE(dummy);

#define GET_FPU_ERR(x) (static_cast<int>(x & kFCSRFlagMask))
#define CHECK_ROUND_RESULT(type) \
  CHECK(GET_FPU_ERR(t.type##_err1_out) & kFCSRInexactFlagMask); \
  CHECK_EQ(0, GET_FPU_ERR(t.type##_err2_out)); \
  CHECK(GET_FPU_ERR(t.type##_err3_out) & kFCSRInvalidOpFlagMask); \
  CHECK(GET_FPU_ERR(t.type##_err4_out) & kFCSRInvalidOpFlagMask); \
  CHECK_EQ(static_cast<int32_t>(kFPUInvalidResult), t.type##_invalid_result);

  CHECK_ROUND_RESULT(round);
  CHECK_ROUND_RESULT(floor);
  CHECK_ROUND_RESULT(ceil);
  CHECK_ROUND_RESULT(cvt);
}


TEST(MIPS15) {
  // Test chaining of label usages within instructions (issue 1644).
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  Assembler assm(isolate, NULL, 0);

  Label target;
  __ beq(v0, v1, &target);
  __ nop();
  __ bne(v0, v1, &target);
  __ nop();
  __ bind(&target);
  __ nop();
}


// ----- mips64 tests -----------------------------------------------

TEST(MIPS16) {
  // Test 64-bit memory loads and stores.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  typedef struct {
    int64_t r1;
    int64_t r2;
    int64_t r3;
    int64_t r4;
    int64_t r5;
    int64_t r6;
    uint32_t ui;
    int32_t si;
  } T;
  T t;

  Assembler assm(isolate, NULL, 0);
  Label L, C;

  // Basic 32-bit word load/store, with un-signed data.
  __ lw(a4, MemOperand(a0, OFFSET_OF(T, ui)) );
  __ sw(a4, MemOperand(a0, OFFSET_OF(T, r1)) );

  // Check that the data got zero-extended into 64-bit a4.
  __ sd(a4, MemOperand(a0, OFFSET_OF(T, r2)) );

  // Basic 32-bit word load/store, with SIGNED data.
  __ lw(a5, MemOperand(a0, OFFSET_OF(T, si)) );
  __ sw(a5, MemOperand(a0, OFFSET_OF(T, r3)) );

  // Check that the data got sign-extended into 64-bit a4.
  __ sd(a5, MemOperand(a0, OFFSET_OF(T, r4)) );

  // 32-bit UNSIGNED word load/store, with SIGNED data.
  __ lwu(a6, MemOperand(a0, OFFSET_OF(T, si)) );
  __ sw(a6, MemOperand(a0, OFFSET_OF(T, r5)) );

  // Check that the data got zero-extended into 64-bit a4.
  __ sd(a6, MemOperand(a0, OFFSET_OF(T, r6)) );

  // lh with positive data.
  __ lh(a5, MemOperand(a0, OFFSET_OF(T, ui)) );
  __ sw(a5, MemOperand(a0, OFFSET_OF(T, r2)) );

  // lh with negative data.
  __ lh(a6, MemOperand(a0, OFFSET_OF(T, si)) );
  __ sw(a6, MemOperand(a0, OFFSET_OF(T, r3)) );

  // lhu with negative data.
  __ lhu(a7, MemOperand(a0, OFFSET_OF(T, si)) );
  __ sw(a7, MemOperand(a0, OFFSET_OF(T, r4)) );

  // lb with negative data.
  __ lb(t0, MemOperand(a0, OFFSET_OF(T, si)) );
  __ sw(t0, MemOperand(a0, OFFSET_OF(T, r5)) );

  // // sh writes only 1/2 of word.
  __ lui(t1, 0x3333);
  __ ori(t1, t1, 0x3333);
  __ sw(t1, MemOperand(a0, OFFSET_OF(T, r6)) );
  __ lhu(t1, MemOperand(a0, OFFSET_OF(T, si)) );
  __ sh(t1, MemOperand(a0, OFFSET_OF(T, r6)) );

  __ jr(ra);
  __ nop();

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
  F3 f = FUNCTION_CAST<F3>(code->entry());
  t.ui = 0x44332211;
  t.si = 0x99aabbcc;
  t.r1 = 0x1111111111111111;
  t.r2 = 0x2222222222222222;
  t.r3 = 0x3333333333333333;
  t.r4 = 0x4444444444444444;
  t.r5 = 0x5555555555555555;
  t.r6 = 0x6666666666666666;
  Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
  USE(dummy);

  // Unsigned data, 32 & 64.
  CHECK_EQ(0x1111111144332211L, t.r1);
  CHECK_EQ(0x0000000000002211L, t.r2);

  // Signed data, 32 & 64.
  CHECK_EQ(0x33333333ffffbbccL, t.r3);
  CHECK_EQ(0xffffffff0000bbccL, t.r4);

  // Signed data, 32 & 64.
  CHECK_EQ(0x55555555ffffffccL, t.r5);
  CHECK_EQ(0x000000003333bbccL, t.r6);
}

#undef __
