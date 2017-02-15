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

#include <iostream>  // NOLINT(readability/streams)

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/arm/simulator-arm.h"
#include "src/base/utils/random-number-generator.h"
#include "src/disassembler.h"
#include "src/factory.h"
#include "src/macro-assembler.h"
#include "src/ostreams.h"

using namespace v8::base;
using namespace v8::internal;


// Define these function prototypes to match JSEntryFunction in execution.cc.
typedef Object* (*F1)(int x, int p1, int p2, int p3, int p4);
typedef Object* (*F2)(int x, int y, int p2, int p3, int p4);
typedef Object* (*F3)(void* p0, int p1, int p2, int p3, int p4);
typedef Object* (*F4)(void* p0, void* p1, int p2, int p3, int p4);
typedef Object* (*F5)(uint32_t p0, void* p1, void* p2, int p3, int p4);

#define __ assm.

TEST(0) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);

  __ add(r0, r0, Operand(r1));
  __ mov(pc, Operand(lr));

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  F2 f = FUNCTION_CAST<F2>(code->entry());
  int res =
      reinterpret_cast<int>(CALL_GENERATED_CODE(isolate, f, 3, 4, 0, 0, 0));
  ::printf("f() = %d\n", res);
  CHECK_EQ(7, res);
}


TEST(1) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);
  Label L, C;

  __ mov(r1, Operand(r0));
  __ mov(r0, Operand::Zero());
  __ b(&C);

  __ bind(&L);
  __ add(r0, r0, Operand(r1));
  __ sub(r1, r1, Operand(1));

  __ bind(&C);
  __ teq(r1, Operand::Zero());
  __ b(ne, &L);
  __ mov(pc, Operand(lr));

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  F1 f = FUNCTION_CAST<F1>(code->entry());
  int res =
      reinterpret_cast<int>(CALL_GENERATED_CODE(isolate, f, 100, 0, 0, 0, 0));
  ::printf("f() = %d\n", res);
  CHECK_EQ(5050, res);
}


TEST(2) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);
  Label L, C;

  __ mov(r1, Operand(r0));
  __ mov(r0, Operand(1));
  __ b(&C);

  __ bind(&L);
  __ mul(r0, r1, r0);
  __ sub(r1, r1, Operand(1));

  __ bind(&C);
  __ teq(r1, Operand::Zero());
  __ b(ne, &L);
  __ mov(pc, Operand(lr));

  // some relocated stuff here, not executed
  __ RecordComment("dead code, just testing relocations");
  __ mov(r0, Operand(isolate->factory()->true_value()));
  __ RecordComment("dead code, just testing immediate operands");
  __ mov(r0, Operand(-1));
  __ mov(r0, Operand(0xFF000000));
  __ mov(r0, Operand(0xF0F0F0F0));
  __ mov(r0, Operand(0xFFF0FFFF));

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  F1 f = FUNCTION_CAST<F1>(code->entry());
  int res =
      reinterpret_cast<int>(CALL_GENERATED_CODE(isolate, f, 10, 0, 0, 0, 0));
  ::printf("f() = %d\n", res);
  CHECK_EQ(3628800, res);
}


TEST(3) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  typedef struct {
    int i;
    char c;
    int16_t s;
  } T;
  T t;

  Assembler assm(isolate, NULL, 0);
  Label L, C;

  __ mov(ip, Operand(sp));
  __ stm(db_w, sp, r4.bit() | fp.bit() | lr.bit());
  __ sub(fp, ip, Operand(4));
  __ mov(r4, Operand(r0));
  __ ldr(r0, MemOperand(r4, offsetof(T, i)));
  __ mov(r2, Operand(r0, ASR, 1));
  __ str(r2, MemOperand(r4, offsetof(T, i)));
  __ ldrsb(r2, MemOperand(r4, offsetof(T, c)));
  __ add(r0, r2, Operand(r0));
  __ mov(r2, Operand(r2, LSL, 2));
  __ strb(r2, MemOperand(r4, offsetof(T, c)));
  __ ldrsh(r2, MemOperand(r4, offsetof(T, s)));
  __ add(r0, r2, Operand(r0));
  __ mov(r2, Operand(r2, ASR, 3));
  __ strh(r2, MemOperand(r4, offsetof(T, s)));
  __ ldm(ia_w, sp, r4.bit() | fp.bit() | pc.bit());

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  F3 f = FUNCTION_CAST<F3>(code->entry());
  t.i = 100000;
  t.c = 10;
  t.s = 1000;
  int res =
      reinterpret_cast<int>(CALL_GENERATED_CODE(isolate, f, &t, 0, 0, 0, 0));
  ::printf("f() = %d\n", res);
  CHECK_EQ(101010, res);
  CHECK_EQ(100000/2, t.i);
  CHECK_EQ(10*4, t.c);
  CHECK_EQ(1000/8, t.s);
}


TEST(4) {
  // Test the VFP floating point instructions.
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
    int i;
    double j;
    double m;
    double n;
    float o;
    float p;
    float x;
    float y;
  } T;
  T t;

  // Create a function that accepts &t, and loads, manipulates, and stores
  // the doubles and floats.
  Assembler assm(isolate, NULL, 0);
  Label L, C;

  if (CpuFeatures::IsSupported(VFPv3)) {
    CpuFeatureScope scope(&assm, VFPv3);

    __ mov(ip, Operand(sp));
    __ stm(db_w, sp, r4.bit() | fp.bit() | lr.bit());
    __ sub(fp, ip, Operand(4));

    __ mov(r4, Operand(r0));
    __ vldr(d6, r4, offsetof(T, a));
    __ vldr(d7, r4, offsetof(T, b));
    __ vadd(d5, d6, d7);
    __ vstr(d5, r4, offsetof(T, c));

    __ vmla(d5, d6, d7);
    __ vmls(d5, d5, d6);

    __ vmov(r2, r3, d5);
    __ vmov(d4, r2, r3);
    __ vstr(d4, r4, offsetof(T, b));

    // Load t.x and t.y, switch values, and store back to the struct.
    __ vldr(s0, r4, offsetof(T, x));
    __ vldr(s1, r4, offsetof(T, y));
    __ vmov(s2, s0);
    __ vmov(s0, s1);
    __ vmov(s1, s2);
    __ vstr(s0, r4, offsetof(T, x));
    __ vstr(s1, r4, offsetof(T, y));

    // Move a literal into a register that can be encoded in the instruction.
    __ vmov(d4, 1.0);
    __ vstr(d4, r4, offsetof(T, e));

    // Move a literal into a register that requires 64 bits to encode.
    // 0x3ff0000010000000 = 1.000000059604644775390625
    __ vmov(d4, 1.000000059604644775390625);
    __ vstr(d4, r4, offsetof(T, d));

    // Convert from floating point to integer.
    __ vmov(d4, 2.0);
    __ vcvt_s32_f64(s1, d4);
    __ vstr(s1, r4, offsetof(T, i));

    // Convert from integer to floating point.
    __ mov(lr, Operand(42));
    __ vmov(s1, lr);
    __ vcvt_f64_s32(d4, s1);
    __ vstr(d4, r4, offsetof(T, f));

    // Convert from fixed point to floating point.
    __ mov(lr, Operand(2468));
    __ vmov(s8, lr);
    __ vcvt_f64_s32(d4, 2);
    __ vstr(d4, r4, offsetof(T, j));

    // Test vabs.
    __ vldr(d1, r4, offsetof(T, g));
    __ vabs(d0, d1);
    __ vstr(d0, r4, offsetof(T, g));
    __ vldr(d2, r4, offsetof(T, h));
    __ vabs(d0, d2);
    __ vstr(d0, r4, offsetof(T, h));

    // Test vneg.
    __ vldr(d1, r4, offsetof(T, m));
    __ vneg(d0, d1);
    __ vstr(d0, r4, offsetof(T, m));
    __ vldr(d1, r4, offsetof(T, n));
    __ vneg(d0, d1);
    __ vstr(d0, r4, offsetof(T, n));

    // Test vmov for single-precision immediates.
    __ vmov(s0, 0.25f);
    __ vstr(s0, r4, offsetof(T, o));
    __ vmov(s0, -16.0f);
    __ vstr(s0, r4, offsetof(T, p));

    __ ldm(ia_w, sp, r4.bit() | fp.bit() | pc.bit());

    CodeDesc desc;
    assm.GetCode(&desc);
    Handle<Code> code = isolate->factory()->NewCode(
        desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
    OFStream os(stdout);
    code->Print(os);
#endif
    F3 f = FUNCTION_CAST<F3>(code->entry());
    t.a = 1.5;
    t.b = 2.75;
    t.c = 17.17;
    t.d = 0.0;
    t.e = 0.0;
    t.f = 0.0;
    t.g = -2718.2818;
    t.h = 31415926.5;
    t.i = 0;
    t.j = 0;
    t.m = -2718.2818;
    t.n = 123.456;
    t.x = 4.5;
    t.y = 9.0;
    Object* dummy = CALL_GENERATED_CODE(isolate, f, &t, 0, 0, 0, 0);
    USE(dummy);
    CHECK_EQ(-16.0f, t.p);
    CHECK_EQ(0.25f, t.o);
    CHECK_EQ(-123.456, t.n);
    CHECK_EQ(2718.2818, t.m);
    CHECK_EQ(2, t.i);
    CHECK_EQ(2718.2818, t.g);
    CHECK_EQ(31415926.5, t.h);
    CHECK_EQ(617.0, t.j);
    CHECK_EQ(42.0, t.f);
    CHECK_EQ(1.0, t.e);
    CHECK_EQ(1.000000059604644775390625, t.d);
    CHECK_EQ(4.25, t.c);
    CHECK_EQ(-4.1875, t.b);
    CHECK_EQ(1.5, t.a);
    CHECK_EQ(4.5f, t.y);
    CHECK_EQ(9.0f, t.x);
  }
}


TEST(5) {
  // Test the ARMv7 bitfield instructions.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);

  if (CpuFeatures::IsSupported(ARMv7)) {
    CpuFeatureScope scope(&assm, ARMv7);
    // On entry, r0 = 0xAAAAAAAA = 0b10..10101010.
    __ ubfx(r0, r0, 1, 12);  // 0b00..010101010101 = 0x555
    __ sbfx(r0, r0, 0, 5);   // 0b11..111111110101 = -11
    __ bfc(r0, 1, 3);        // 0b11..111111110001 = -15
    __ mov(r1, Operand(7));
    __ bfi(r0, r1, 3, 3);    // 0b11..111111111001 = -7
    __ mov(pc, Operand(lr));

    CodeDesc desc;
    assm.GetCode(&desc);
    Handle<Code> code = isolate->factory()->NewCode(
        desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
    OFStream os(stdout);
    code->Print(os);
#endif
    F1 f = FUNCTION_CAST<F1>(code->entry());
    int res = reinterpret_cast<int>(
        CALL_GENERATED_CODE(isolate, f, 0xAAAAAAAA, 0, 0, 0, 0));
    ::printf("f() = %d\n", res);
    CHECK_EQ(-7, res);
  }
}


TEST(6) {
  // Test saturating instructions.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);

  __ usat(r1, 8, Operand(r0));           // Sat 0xFFFF to 0-255 = 0xFF.
  __ usat(r2, 12, Operand(r0, ASR, 9));  // Sat (0xFFFF>>9) to 0-4095 = 0x7F.
  __ usat(r3, 1, Operand(r0, LSL, 16));  // Sat (0xFFFF<<16) to 0-1 = 0x0.
  __ add(r0, r1, Operand(r2));
  __ add(r0, r0, Operand(r3));
  __ mov(pc, Operand(lr));

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  F1 f = FUNCTION_CAST<F1>(code->entry());
  int res = reinterpret_cast<int>(
      CALL_GENERATED_CODE(isolate, f, 0xFFFF, 0, 0, 0, 0));
  ::printf("f() = %d\n", res);
  CHECK_EQ(382, res);
}


enum VCVTTypes {
  s32_f64,
  u32_f64
};

static void TestRoundingMode(VCVTTypes types,
                             VFPRoundingMode mode,
                             double value,
                             int expected,
                             bool expected_exception = false) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);

  Label wrong_exception;

  __ vmrs(r1);
  // Set custom FPSCR.
  __ bic(r2, r1, Operand(kVFPRoundingModeMask | kVFPExceptionMask));
  __ orr(r2, r2, Operand(mode));
  __ vmsr(r2);

  // Load value, convert, and move back result to r0 if everything went well.
  __ vmov(d1, value);
  switch (types) {
    case s32_f64:
      __ vcvt_s32_f64(s0, d1, kFPSCRRounding);
      break;

    case u32_f64:
      __ vcvt_u32_f64(s0, d1, kFPSCRRounding);
      break;

    default:
      UNREACHABLE();
      break;
  }
  // Check for vfp exceptions
  __ vmrs(r2);
  __ tst(r2, Operand(kVFPExceptionMask));
  // Check that we behaved as expected.
  __ b(&wrong_exception, expected_exception ? eq : ne);
  // There was no exception. Retrieve the result and return.
  __ vmov(r0, s0);
  __ mov(pc, Operand(lr));

  // The exception behaviour is not what we expected.
  // Load a special value and return.
  __ bind(&wrong_exception);
  __ mov(r0, Operand(11223344));
  __ mov(pc, Operand(lr));

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  F1 f = FUNCTION_CAST<F1>(code->entry());
  int res =
      reinterpret_cast<int>(CALL_GENERATED_CODE(isolate, f, 0, 0, 0, 0, 0));
  ::printf("res = %d\n", res);
  CHECK_EQ(expected, res);
}


TEST(7) {
  CcTest::InitializeVM();
  // Test vfp rounding modes.

  // s32_f64 (double to integer).

  TestRoundingMode(s32_f64, RN,  0, 0);
  TestRoundingMode(s32_f64, RN,  0.5, 0);
  TestRoundingMode(s32_f64, RN, -0.5, 0);
  TestRoundingMode(s32_f64, RN,  1.5, 2);
  TestRoundingMode(s32_f64, RN, -1.5, -2);
  TestRoundingMode(s32_f64, RN,  123.7, 124);
  TestRoundingMode(s32_f64, RN, -123.7, -124);
  TestRoundingMode(s32_f64, RN,  123456.2,  123456);
  TestRoundingMode(s32_f64, RN, -123456.2, -123456);
  TestRoundingMode(s32_f64, RN, static_cast<double>(kMaxInt), kMaxInt);
  TestRoundingMode(s32_f64, RN, (kMaxInt + 0.49), kMaxInt);
  TestRoundingMode(s32_f64, RN, (kMaxInt + 1.0), kMaxInt, true);
  TestRoundingMode(s32_f64, RN, (kMaxInt + 0.5), kMaxInt, true);
  TestRoundingMode(s32_f64, RN, static_cast<double>(kMinInt), kMinInt);
  TestRoundingMode(s32_f64, RN, (kMinInt - 0.5), kMinInt);
  TestRoundingMode(s32_f64, RN, (kMinInt - 1.0), kMinInt, true);
  TestRoundingMode(s32_f64, RN, (kMinInt - 0.51), kMinInt, true);

  TestRoundingMode(s32_f64, RM,  0, 0);
  TestRoundingMode(s32_f64, RM,  0.5, 0);
  TestRoundingMode(s32_f64, RM, -0.5, -1);
  TestRoundingMode(s32_f64, RM,  123.7, 123);
  TestRoundingMode(s32_f64, RM, -123.7, -124);
  TestRoundingMode(s32_f64, RM,  123456.2,  123456);
  TestRoundingMode(s32_f64, RM, -123456.2, -123457);
  TestRoundingMode(s32_f64, RM, static_cast<double>(kMaxInt), kMaxInt);
  TestRoundingMode(s32_f64, RM, (kMaxInt + 0.5), kMaxInt);
  TestRoundingMode(s32_f64, RM, (kMaxInt + 1.0), kMaxInt, true);
  TestRoundingMode(s32_f64, RM, static_cast<double>(kMinInt), kMinInt);
  TestRoundingMode(s32_f64, RM, (kMinInt - 0.5), kMinInt, true);
  TestRoundingMode(s32_f64, RM, (kMinInt + 0.5), kMinInt);

  TestRoundingMode(s32_f64, RZ,  0, 0);
  TestRoundingMode(s32_f64, RZ,  0.5, 0);
  TestRoundingMode(s32_f64, RZ, -0.5, 0);
  TestRoundingMode(s32_f64, RZ,  123.7,  123);
  TestRoundingMode(s32_f64, RZ, -123.7, -123);
  TestRoundingMode(s32_f64, RZ,  123456.2,  123456);
  TestRoundingMode(s32_f64, RZ, -123456.2, -123456);
  TestRoundingMode(s32_f64, RZ, static_cast<double>(kMaxInt), kMaxInt);
  TestRoundingMode(s32_f64, RZ, (kMaxInt + 0.5), kMaxInt);
  TestRoundingMode(s32_f64, RZ, (kMaxInt + 1.0), kMaxInt, true);
  TestRoundingMode(s32_f64, RZ, static_cast<double>(kMinInt), kMinInt);
  TestRoundingMode(s32_f64, RZ, (kMinInt - 0.5), kMinInt);
  TestRoundingMode(s32_f64, RZ, (kMinInt - 1.0), kMinInt, true);


  // u32_f64 (double to integer).

  // Negative values.
  TestRoundingMode(u32_f64, RN, -0.5, 0);
  TestRoundingMode(u32_f64, RN, -123456.7, 0, true);
  TestRoundingMode(u32_f64, RN, static_cast<double>(kMinInt), 0, true);
  TestRoundingMode(u32_f64, RN, kMinInt - 1.0, 0, true);

  TestRoundingMode(u32_f64, RM, -0.5, 0, true);
  TestRoundingMode(u32_f64, RM, -123456.7, 0, true);
  TestRoundingMode(u32_f64, RM, static_cast<double>(kMinInt), 0, true);
  TestRoundingMode(u32_f64, RM, kMinInt - 1.0, 0, true);

  TestRoundingMode(u32_f64, RZ, -0.5, 0);
  TestRoundingMode(u32_f64, RZ, -123456.7, 0, true);
  TestRoundingMode(u32_f64, RZ, static_cast<double>(kMinInt), 0, true);
  TestRoundingMode(u32_f64, RZ, kMinInt - 1.0, 0, true);

  // Positive values.
  // kMaxInt is the maximum *signed* integer: 0x7fffffff.
  static const uint32_t kMaxUInt = 0xffffffffu;
  TestRoundingMode(u32_f64, RZ,  0, 0);
  TestRoundingMode(u32_f64, RZ,  0.5, 0);
  TestRoundingMode(u32_f64, RZ,  123.7,  123);
  TestRoundingMode(u32_f64, RZ,  123456.2,  123456);
  TestRoundingMode(u32_f64, RZ, static_cast<double>(kMaxInt), kMaxInt);
  TestRoundingMode(u32_f64, RZ, (kMaxInt + 0.5), kMaxInt);
  TestRoundingMode(u32_f64, RZ, (kMaxInt + 1.0),
                                static_cast<uint32_t>(kMaxInt) + 1);
  TestRoundingMode(u32_f64, RZ, (kMaxUInt + 0.5), kMaxUInt);
  TestRoundingMode(u32_f64, RZ, (kMaxUInt + 1.0), kMaxUInt, true);

  TestRoundingMode(u32_f64, RM,  0, 0);
  TestRoundingMode(u32_f64, RM,  0.5, 0);
  TestRoundingMode(u32_f64, RM,  123.7, 123);
  TestRoundingMode(u32_f64, RM,  123456.2,  123456);
  TestRoundingMode(u32_f64, RM, static_cast<double>(kMaxInt), kMaxInt);
  TestRoundingMode(u32_f64, RM, (kMaxInt + 0.5), kMaxInt);
  TestRoundingMode(u32_f64, RM, (kMaxInt + 1.0),
                                static_cast<uint32_t>(kMaxInt) + 1);
  TestRoundingMode(u32_f64, RM, (kMaxUInt + 0.5), kMaxUInt);
  TestRoundingMode(u32_f64, RM, (kMaxUInt + 1.0), kMaxUInt, true);

  TestRoundingMode(u32_f64, RN,  0, 0);
  TestRoundingMode(u32_f64, RN,  0.5, 0);
  TestRoundingMode(u32_f64, RN,  1.5, 2);
  TestRoundingMode(u32_f64, RN,  123.7, 124);
  TestRoundingMode(u32_f64, RN,  123456.2,  123456);
  TestRoundingMode(u32_f64, RN, static_cast<double>(kMaxInt), kMaxInt);
  TestRoundingMode(u32_f64, RN, (kMaxInt + 0.49), kMaxInt);
  TestRoundingMode(u32_f64, RN, (kMaxInt + 0.5),
                                static_cast<uint32_t>(kMaxInt) + 1);
  TestRoundingMode(u32_f64, RN, (kMaxUInt + 0.49), kMaxUInt);
  TestRoundingMode(u32_f64, RN, (kMaxUInt + 0.5), kMaxUInt, true);
  TestRoundingMode(u32_f64, RN, (kMaxUInt + 1.0), kMaxUInt, true);
}


TEST(8) {
  // Test VFP multi load/store with ia_w.
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
  } D;
  D d;

  typedef struct {
    float a;
    float b;
    float c;
    float d;
    float e;
    float f;
    float g;
    float h;
  } F;
  F f;

  // Create a function that uses vldm/vstm to move some double and
  // single precision values around in memory.
  Assembler assm(isolate, NULL, 0);

  __ mov(ip, Operand(sp));
  __ stm(db_w, sp, r4.bit() | fp.bit() | lr.bit());
  __ sub(fp, ip, Operand(4));

  __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(D, a))));
  __ vldm(ia_w, r4, d0, d3);
  __ vldm(ia_w, r4, d4, d7);

  __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(D, a))));
  __ vstm(ia_w, r4, d6, d7);
  __ vstm(ia_w, r4, d0, d5);

  __ add(r4, r1, Operand(static_cast<int32_t>(offsetof(F, a))));
  __ vldm(ia_w, r4, s0, s3);
  __ vldm(ia_w, r4, s4, s7);

  __ add(r4, r1, Operand(static_cast<int32_t>(offsetof(F, a))));
  __ vstm(ia_w, r4, s6, s7);
  __ vstm(ia_w, r4, s0, s5);

  __ ldm(ia_w, sp, r4.bit() | fp.bit() | pc.bit());

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  F4 fn = FUNCTION_CAST<F4>(code->entry());
  d.a = 1.1;
  d.b = 2.2;
  d.c = 3.3;
  d.d = 4.4;
  d.e = 5.5;
  d.f = 6.6;
  d.g = 7.7;
  d.h = 8.8;

  f.a = 1.0;
  f.b = 2.0;
  f.c = 3.0;
  f.d = 4.0;
  f.e = 5.0;
  f.f = 6.0;
  f.g = 7.0;
  f.h = 8.0;

  Object* dummy = CALL_GENERATED_CODE(isolate, fn, &d, &f, 0, 0, 0);
  USE(dummy);

  CHECK_EQ(7.7, d.a);
  CHECK_EQ(8.8, d.b);
  CHECK_EQ(1.1, d.c);
  CHECK_EQ(2.2, d.d);
  CHECK_EQ(3.3, d.e);
  CHECK_EQ(4.4, d.f);
  CHECK_EQ(5.5, d.g);
  CHECK_EQ(6.6, d.h);

  CHECK_EQ(7.0f, f.a);
  CHECK_EQ(8.0f, f.b);
  CHECK_EQ(1.0f, f.c);
  CHECK_EQ(2.0f, f.d);
  CHECK_EQ(3.0f, f.e);
  CHECK_EQ(4.0f, f.f);
  CHECK_EQ(5.0f, f.g);
  CHECK_EQ(6.0f, f.h);
}


TEST(9) {
  // Test VFP multi load/store with ia.
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
  } D;
  D d;

  typedef struct {
    float a;
    float b;
    float c;
    float d;
    float e;
    float f;
    float g;
    float h;
  } F;
  F f;

  // Create a function that uses vldm/vstm to move some double and
  // single precision values around in memory.
  Assembler assm(isolate, NULL, 0);

  __ mov(ip, Operand(sp));
  __ stm(db_w, sp, r4.bit() | fp.bit() | lr.bit());
  __ sub(fp, ip, Operand(4));

  __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(D, a))));
  __ vldm(ia, r4, d0, d3);
  __ add(r4, r4, Operand(4 * 8));
  __ vldm(ia, r4, d4, d7);

  __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(D, a))));
  __ vstm(ia, r4, d6, d7);
  __ add(r4, r4, Operand(2 * 8));
  __ vstm(ia, r4, d0, d5);

  __ add(r4, r1, Operand(static_cast<int32_t>(offsetof(F, a))));
  __ vldm(ia, r4, s0, s3);
  __ add(r4, r4, Operand(4 * 4));
  __ vldm(ia, r4, s4, s7);

  __ add(r4, r1, Operand(static_cast<int32_t>(offsetof(F, a))));
  __ vstm(ia, r4, s6, s7);
  __ add(r4, r4, Operand(2 * 4));
  __ vstm(ia, r4, s0, s5);

  __ ldm(ia_w, sp, r4.bit() | fp.bit() | pc.bit());

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  F4 fn = FUNCTION_CAST<F4>(code->entry());
  d.a = 1.1;
  d.b = 2.2;
  d.c = 3.3;
  d.d = 4.4;
  d.e = 5.5;
  d.f = 6.6;
  d.g = 7.7;
  d.h = 8.8;

  f.a = 1.0;
  f.b = 2.0;
  f.c = 3.0;
  f.d = 4.0;
  f.e = 5.0;
  f.f = 6.0;
  f.g = 7.0;
  f.h = 8.0;

  Object* dummy = CALL_GENERATED_CODE(isolate, fn, &d, &f, 0, 0, 0);
  USE(dummy);

  CHECK_EQ(7.7, d.a);
  CHECK_EQ(8.8, d.b);
  CHECK_EQ(1.1, d.c);
  CHECK_EQ(2.2, d.d);
  CHECK_EQ(3.3, d.e);
  CHECK_EQ(4.4, d.f);
  CHECK_EQ(5.5, d.g);
  CHECK_EQ(6.6, d.h);

  CHECK_EQ(7.0f, f.a);
  CHECK_EQ(8.0f, f.b);
  CHECK_EQ(1.0f, f.c);
  CHECK_EQ(2.0f, f.d);
  CHECK_EQ(3.0f, f.e);
  CHECK_EQ(4.0f, f.f);
  CHECK_EQ(5.0f, f.g);
  CHECK_EQ(6.0f, f.h);
}


TEST(10) {
  // Test VFP multi load/store with db_w.
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
  } D;
  D d;

  typedef struct {
    float a;
    float b;
    float c;
    float d;
    float e;
    float f;
    float g;
    float h;
  } F;
  F f;

  // Create a function that uses vldm/vstm to move some double and
  // single precision values around in memory.
  Assembler assm(isolate, NULL, 0);

  __ mov(ip, Operand(sp));
  __ stm(db_w, sp, r4.bit() | fp.bit() | lr.bit());
  __ sub(fp, ip, Operand(4));

  __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(D, h)) + 8));
  __ vldm(db_w, r4, d4, d7);
  __ vldm(db_w, r4, d0, d3);

  __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(D, h)) + 8));
  __ vstm(db_w, r4, d0, d5);
  __ vstm(db_w, r4, d6, d7);

  __ add(r4, r1, Operand(static_cast<int32_t>(offsetof(F, h)) + 4));
  __ vldm(db_w, r4, s4, s7);
  __ vldm(db_w, r4, s0, s3);

  __ add(r4, r1, Operand(static_cast<int32_t>(offsetof(F, h)) + 4));
  __ vstm(db_w, r4, s0, s5);
  __ vstm(db_w, r4, s6, s7);

  __ ldm(ia_w, sp, r4.bit() | fp.bit() | pc.bit());

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  F4 fn = FUNCTION_CAST<F4>(code->entry());
  d.a = 1.1;
  d.b = 2.2;
  d.c = 3.3;
  d.d = 4.4;
  d.e = 5.5;
  d.f = 6.6;
  d.g = 7.7;
  d.h = 8.8;

  f.a = 1.0;
  f.b = 2.0;
  f.c = 3.0;
  f.d = 4.0;
  f.e = 5.0;
  f.f = 6.0;
  f.g = 7.0;
  f.h = 8.0;

  Object* dummy = CALL_GENERATED_CODE(isolate, fn, &d, &f, 0, 0, 0);
  USE(dummy);

  CHECK_EQ(7.7, d.a);
  CHECK_EQ(8.8, d.b);
  CHECK_EQ(1.1, d.c);
  CHECK_EQ(2.2, d.d);
  CHECK_EQ(3.3, d.e);
  CHECK_EQ(4.4, d.f);
  CHECK_EQ(5.5, d.g);
  CHECK_EQ(6.6, d.h);

  CHECK_EQ(7.0f, f.a);
  CHECK_EQ(8.0f, f.b);
  CHECK_EQ(1.0f, f.c);
  CHECK_EQ(2.0f, f.d);
  CHECK_EQ(3.0f, f.e);
  CHECK_EQ(4.0f, f.f);
  CHECK_EQ(5.0f, f.g);
  CHECK_EQ(6.0f, f.h);
}


TEST(11) {
  // Test instructions using the carry flag.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  typedef struct {
    int32_t a;
    int32_t b;
    int32_t c;
    int32_t d;
  } I;
  I i;

  i.a = 0xabcd0001;
  i.b = 0xabcd0000;

  Assembler assm(isolate, NULL, 0);

  // Test HeapObject untagging.
  __ ldr(r1, MemOperand(r0, offsetof(I, a)));
  __ mov(r1, Operand(r1, ASR, 1), SetCC);
  __ adc(r1, r1, Operand(r1), LeaveCC, cs);
  __ str(r1, MemOperand(r0, offsetof(I, a)));

  __ ldr(r2, MemOperand(r0, offsetof(I, b)));
  __ mov(r2, Operand(r2, ASR, 1), SetCC);
  __ adc(r2, r2, Operand(r2), LeaveCC, cs);
  __ str(r2, MemOperand(r0, offsetof(I, b)));

  // Test corner cases.
  __ mov(r1, Operand(0xffffffff));
  __ mov(r2, Operand::Zero());
  __ mov(r3, Operand(r1, ASR, 1), SetCC);  // Set the carry.
  __ adc(r3, r1, Operand(r2));
  __ str(r3, MemOperand(r0, offsetof(I, c)));

  __ mov(r1, Operand(0xffffffff));
  __ mov(r2, Operand::Zero());
  __ mov(r3, Operand(r2, ASR, 1), SetCC);  // Unset the carry.
  __ adc(r3, r1, Operand(r2));
  __ str(r3, MemOperand(r0, offsetof(I, d)));

  __ mov(pc, Operand(lr));

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  F3 f = FUNCTION_CAST<F3>(code->entry());
  Object* dummy = CALL_GENERATED_CODE(isolate, f, &i, 0, 0, 0, 0);
  USE(dummy);

  CHECK_EQ(static_cast<int32_t>(0xabcd0001), i.a);
  CHECK_EQ(static_cast<int32_t>(0xabcd0000) >> 1, i.b);
  CHECK_EQ(0x00000000, i.c);
  CHECK_EQ(static_cast<int32_t>(0xffffffff), i.d);
}


TEST(12) {
  // Test chaining of label usages within instructions (issue 1644).
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);
  Label target;
  __ b(eq, &target);
  __ b(ne, &target);
  __ bind(&target);
  __ nop();
}


TEST(13) {
  // Test VFP instructions using registers d16-d31.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  if (!CpuFeatures::IsSupported(VFP32DREGS)) {
    return;
  }

  typedef struct {
    double a;
    double b;
    double c;
    double x;
    double y;
    double z;
    double i;
    double j;
    double k;
    uint32_t low;
    uint32_t high;
  } T;
  T t;

  // Create a function that accepts &t, and loads, manipulates, and stores
  // the doubles and floats.
  Assembler assm(isolate, NULL, 0);
  Label L, C;

  if (CpuFeatures::IsSupported(VFPv3)) {
    CpuFeatureScope scope(&assm, VFPv3);

    __ stm(db_w, sp, r4.bit() | lr.bit());

    // Load a, b, c into d16, d17, d18.
    __ mov(r4, Operand(r0));
    __ vldr(d16, r4, offsetof(T, a));
    __ vldr(d17, r4, offsetof(T, b));
    __ vldr(d18, r4, offsetof(T, c));

    __ vneg(d25, d16);
    __ vadd(d25, d25, d17);
    __ vsub(d25, d25, d18);
    __ vmul(d25, d25, d25);
    __ vdiv(d25, d25, d18);

    __ vmov(d16, d25);
    __ vsqrt(d17, d25);
    __ vneg(d17, d17);
    __ vabs(d17, d17);
    __ vmla(d18, d16, d17);

    // Store d16, d17, d18 into a, b, c.
    __ mov(r4, Operand(r0));
    __ vstr(d16, r4, offsetof(T, a));
    __ vstr(d17, r4, offsetof(T, b));
    __ vstr(d18, r4, offsetof(T, c));

    // Load x, y, z into d29-d31.
    __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(T, x))));
    __ vldm(ia_w, r4, d29, d31);

    // Swap d29 and d30 via r registers.
    __ vmov(r1, r2, d29);
    __ vmov(d29, d30);
    __ vmov(d30, r1, r2);

    // Convert to and from integer.
    __ vcvt_s32_f64(s1, d31);
    __ vcvt_f64_u32(d31, s1);

    // Store d29-d31 into x, y, z.
    __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(T, x))));
    __ vstm(ia_w, r4, d29, d31);

    // Move constants into d20, d21, d22 and store into i, j, k.
    __ vmov(d20, 14.7610017472335499);
    __ vmov(d21, 16.0);
    __ mov(r1, Operand(372106121));
    __ mov(r2, Operand(1079146608));
    __ vmov(d22, VmovIndexLo, r1);
    __ vmov(d22, VmovIndexHi, r2);
    __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(T, i))));
    __ vstm(ia_w, r4, d20, d22);
    // Move d22 into low and high.
    __ vmov(r4, VmovIndexLo, d22);
    __ str(r4, MemOperand(r0, offsetof(T, low)));
    __ vmov(r4, VmovIndexHi, d22);
    __ str(r4, MemOperand(r0, offsetof(T, high)));

    __ ldm(ia_w, sp, r4.bit() | pc.bit());

    CodeDesc desc;
    assm.GetCode(&desc);
    Handle<Code> code = isolate->factory()->NewCode(
        desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
    OFStream os(stdout);
    code->Print(os);
#endif
    F3 f = FUNCTION_CAST<F3>(code->entry());
    t.a = 1.5;
    t.b = 2.75;
    t.c = 17.17;
    t.x = 1.5;
    t.y = 2.75;
    t.z = 17.17;
    Object* dummy = CALL_GENERATED_CODE(isolate, f, &t, 0, 0, 0, 0);
    USE(dummy);
    CHECK_EQ(14.7610017472335499, t.a);
    CHECK_EQ(3.84200491244266251, t.b);
    CHECK_EQ(73.8818412254460241, t.c);
    CHECK_EQ(2.75, t.x);
    CHECK_EQ(1.5, t.y);
    CHECK_EQ(17.0, t.z);
    CHECK_EQ(14.7610017472335499, t.i);
    CHECK_EQ(16.0, t.j);
    CHECK_EQ(73.8818412254460241, t.k);
    CHECK_EQ(372106121u, t.low);
    CHECK_EQ(1079146608u, t.high);
  }
}


TEST(14) {
  // Test the VFP Canonicalized Nan mode.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  typedef struct {
    double left;
    double right;
    double add_result;
    double sub_result;
    double mul_result;
    double div_result;
  } T;
  T t;

  // Create a function that makes the four basic operations.
  Assembler assm(isolate, NULL, 0);

  // Ensure FPSCR state (as JSEntryStub does).
  Label fpscr_done;
  __ vmrs(r1);
  __ tst(r1, Operand(kVFPDefaultNaNModeControlBit));
  __ b(ne, &fpscr_done);
  __ orr(r1, r1, Operand(kVFPDefaultNaNModeControlBit));
  __ vmsr(r1);
  __ bind(&fpscr_done);

  __ vldr(d0, r0, offsetof(T, left));
  __ vldr(d1, r0, offsetof(T, right));
  __ vadd(d2, d0, d1);
  __ vstr(d2, r0, offsetof(T, add_result));
  __ vsub(d2, d0, d1);
  __ vstr(d2, r0, offsetof(T, sub_result));
  __ vmul(d2, d0, d1);
  __ vstr(d2, r0, offsetof(T, mul_result));
  __ vdiv(d2, d0, d1);
  __ vstr(d2, r0, offsetof(T, div_result));

  __ mov(pc, Operand(lr));

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  F3 f = FUNCTION_CAST<F3>(code->entry());
  t.left = bit_cast<double>(kHoleNanInt64);
  t.right = 1;
  t.add_result = 0;
  t.sub_result = 0;
  t.mul_result = 0;
  t.div_result = 0;
  Object* dummy = CALL_GENERATED_CODE(isolate, f, &t, 0, 0, 0, 0);
  USE(dummy);
  const uint32_t kArmNanUpper32 = 0x7ff80000;
  const uint32_t kArmNanLower32 = 0x00000000;
#ifdef DEBUG
  const uint64_t kArmNanInt64 =
      (static_cast<uint64_t>(kArmNanUpper32) << 32) | kArmNanLower32;
  CHECK(kArmNanInt64 != kHoleNanInt64);
#endif
  // With VFP2 the sign of the canonicalized Nan is undefined. So
  // we remove the sign bit for the upper tests.
  CHECK_EQ(kArmNanUpper32,
           (bit_cast<int64_t>(t.add_result) >> 32) & 0x7fffffff);
  CHECK_EQ(kArmNanLower32, bit_cast<int64_t>(t.add_result) & 0xffffffffu);
  CHECK_EQ(kArmNanUpper32,
           (bit_cast<int64_t>(t.sub_result) >> 32) & 0x7fffffff);
  CHECK_EQ(kArmNanLower32, bit_cast<int64_t>(t.sub_result) & 0xffffffffu);
  CHECK_EQ(kArmNanUpper32,
           (bit_cast<int64_t>(t.mul_result) >> 32) & 0x7fffffff);
  CHECK_EQ(kArmNanLower32, bit_cast<int64_t>(t.mul_result) & 0xffffffffu);
  CHECK_EQ(kArmNanUpper32,
           (bit_cast<int64_t>(t.div_result) >> 32) & 0x7fffffff);
  CHECK_EQ(kArmNanLower32, bit_cast<int64_t>(t.div_result) & 0xffffffffu);
}


TEST(15) {
  // Test the Neon instructions.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  typedef struct {
    uint32_t src0;
    uint32_t src1;
    uint32_t src2;
    uint32_t src3;
    uint32_t src4;
    uint32_t src5;
    uint32_t src6;
    uint32_t src7;
    uint32_t dst0;
    uint32_t dst1;
    uint32_t dst2;
    uint32_t dst3;
    uint32_t dst4;
    uint32_t dst5;
    uint32_t dst6;
    uint32_t dst7;
    uint32_t srcA0;
    uint32_t srcA1;
    uint32_t dstA0;
    uint32_t dstA1;
    uint32_t dstA2;
    uint32_t dstA3;
    uint32_t dstA4;
    uint32_t dstA5;
    uint32_t dstA6;
    uint32_t dstA7;
  } T;
  T t;

  // Create a function that accepts &t, and loads, manipulates, and stores
  // the doubles and floats.
  Assembler assm(isolate, NULL, 0);


  if (CpuFeatures::IsSupported(NEON)) {
    CpuFeatureScope scope(&assm, NEON);

    __ stm(db_w, sp, r4.bit() | lr.bit());
    // Move 32 bytes with neon.
    __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(T, src0))));
    __ vld1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(r4));
    __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(T, dst0))));
    __ vst1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(r4));

    // Expand 8 bytes into 8 words(16 bits).
    __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(T, srcA0))));
    __ vld1(Neon8, NeonListOperand(d0), NeonMemOperand(r4));
    __ vmovl(NeonU8, q0, d0);
    __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(T, dstA0))));
    __ vst1(Neon8, NeonListOperand(d0, 2), NeonMemOperand(r4));

    // The same expansion, but with different source and destination registers.
    __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(T, srcA0))));
    __ vld1(Neon8, NeonListOperand(d1), NeonMemOperand(r4));
    __ vmovl(NeonU8, q1, d1);
    __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(T, dstA4))));
    __ vst1(Neon8, NeonListOperand(d2, 2), NeonMemOperand(r4));

    __ ldm(ia_w, sp, r4.bit() | pc.bit());

    CodeDesc desc;
    assm.GetCode(&desc);
    Handle<Code> code = isolate->factory()->NewCode(
        desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
    OFStream os(stdout);
    code->Print(os);
#endif
    F3 f = FUNCTION_CAST<F3>(code->entry());
    t.src0 = 0x01020304;
    t.src1 = 0x11121314;
    t.src2 = 0x21222324;
    t.src3 = 0x31323334;
    t.src4 = 0x41424344;
    t.src5 = 0x51525354;
    t.src6 = 0x61626364;
    t.src7 = 0x71727374;
    t.dst0 = 0;
    t.dst1 = 0;
    t.dst2 = 0;
    t.dst3 = 0;
    t.dst4 = 0;
    t.dst5 = 0;
    t.dst6 = 0;
    t.dst7 = 0;
    t.srcA0 = 0x41424344;
    t.srcA1 = 0x81828384;
    t.dstA0 = 0;
    t.dstA1 = 0;
    t.dstA2 = 0;
    t.dstA3 = 0;
    t.dstA4 = 0;
    t.dstA5 = 0;
    t.dstA6 = 0;
    t.dstA7 = 0;
    Object* dummy = CALL_GENERATED_CODE(isolate, f, &t, 0, 0, 0, 0);
    USE(dummy);
    CHECK_EQ(0x01020304u, t.dst0);
    CHECK_EQ(0x11121314u, t.dst1);
    CHECK_EQ(0x21222324u, t.dst2);
    CHECK_EQ(0x31323334u, t.dst3);
    CHECK_EQ(0x41424344u, t.dst4);
    CHECK_EQ(0x51525354u, t.dst5);
    CHECK_EQ(0x61626364u, t.dst6);
    CHECK_EQ(0x71727374u, t.dst7);
    CHECK_EQ(0x00430044u, t.dstA0);
    CHECK_EQ(0x00410042u, t.dstA1);
    CHECK_EQ(0x00830084u, t.dstA2);
    CHECK_EQ(0x00810082u, t.dstA3);
    CHECK_EQ(0x00430044u, t.dstA4);
    CHECK_EQ(0x00410042u, t.dstA5);
    CHECK_EQ(0x00830084u, t.dstA6);
    CHECK_EQ(0x00810082u, t.dstA7);
  }
}


TEST(16) {
  // Test the pkh, uxtb, uxtab and uxtb16 instructions.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  typedef struct {
    uint32_t src0;
    uint32_t src1;
    uint32_t src2;
    uint32_t dst0;
    uint32_t dst1;
    uint32_t dst2;
    uint32_t dst3;
    uint32_t dst4;
  } T;
  T t;

  // Create a function that accepts &t, and loads, manipulates, and stores
  // the doubles and floats.
  Assembler assm(isolate, NULL, 0);

  __ stm(db_w, sp, r4.bit() | lr.bit());

  __ mov(r4, Operand(r0));
  __ ldr(r0, MemOperand(r4, offsetof(T, src0)));
  __ ldr(r1, MemOperand(r4, offsetof(T, src1)));

  __ pkhbt(r2, r0, Operand(r1, LSL, 8));
  __ str(r2, MemOperand(r4, offsetof(T, dst0)));

  __ pkhtb(r2, r0, Operand(r1, ASR, 8));
  __ str(r2, MemOperand(r4, offsetof(T, dst1)));

  __ uxtb16(r2, r0, 8);
  __ str(r2, MemOperand(r4, offsetof(T, dst2)));

  __ uxtb(r2, r0, 8);
  __ str(r2, MemOperand(r4, offsetof(T, dst3)));

  __ ldr(r0, MemOperand(r4, offsetof(T, src2)));
  __ uxtab(r2, r0, r1, 8);
  __ str(r2, MemOperand(r4, offsetof(T, dst4)));

  __ ldm(ia_w, sp, r4.bit() | pc.bit());

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  F3 f = FUNCTION_CAST<F3>(code->entry());
  t.src0 = 0x01020304;
  t.src1 = 0x11121314;
  t.src2 = 0x11121300;
  t.dst0 = 0;
  t.dst1 = 0;
  t.dst2 = 0;
  t.dst3 = 0;
  t.dst4 = 0;
  Object* dummy = CALL_GENERATED_CODE(isolate, f, &t, 0, 0, 0, 0);
  USE(dummy);
  CHECK_EQ(0x12130304u, t.dst0);
  CHECK_EQ(0x01021213u, t.dst1);
  CHECK_EQ(0x00010003u, t.dst2);
  CHECK_EQ(0x00000003u, t.dst3);
  CHECK_EQ(0x11121313u, t.dst4);
}


TEST(17) {
  // Test generating labels at high addresses.
  // Should not assert.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  // Generate a code segment that will be longer than 2^24 bytes.
  Assembler assm(isolate, NULL, 0);
  for (size_t i = 0; i < 1 << 23 ; ++i) {  // 2^23
    __ nop();
  }

  Label target;
  __ b(eq, &target);
  __ bind(&target);
  __ nop();
}


#define TEST_SDIV(expected_, dividend_, divisor_)          \
  t.dividend = dividend_;                                  \
  t.divisor = divisor_;                                    \
  t.result = 0;                                            \
  dummy = CALL_GENERATED_CODE(isolate, f, &t, 0, 0, 0, 0); \
  CHECK_EQ(expected_, t.result);


TEST(sdiv) {
  // Test the sdiv.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  Assembler assm(isolate, NULL, 0);

  struct T {
    int32_t dividend;
    int32_t divisor;
    int32_t result;
  } t;

  if (CpuFeatures::IsSupported(SUDIV)) {
    CpuFeatureScope scope(&assm, SUDIV);

    __ mov(r3, Operand(r0));

    __ ldr(r0, MemOperand(r3, offsetof(T, dividend)));
    __ ldr(r1, MemOperand(r3, offsetof(T, divisor)));

    __ sdiv(r2, r0, r1);
    __ str(r2, MemOperand(r3, offsetof(T, result)));

  __ bx(lr);

    CodeDesc desc;
    assm.GetCode(&desc);
    Handle<Code> code = isolate->factory()->NewCode(
        desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
    OFStream os(stdout);
    code->Print(os);
#endif
    F3 f = FUNCTION_CAST<F3>(code->entry());
    Object* dummy;
    TEST_SDIV(0, kMinInt, 0);
    TEST_SDIV(0, 1024, 0);
    TEST_SDIV(1073741824, kMinInt, -2);
    TEST_SDIV(kMinInt, kMinInt, -1);
    TEST_SDIV(5, 10, 2);
    TEST_SDIV(3, 10, 3);
    TEST_SDIV(-5, 10, -2);
    TEST_SDIV(-3, 10, -3);
    TEST_SDIV(-5, -10, 2);
    TEST_SDIV(-3, -10, 3);
    TEST_SDIV(5, -10, -2);
    TEST_SDIV(3, -10, -3);
    USE(dummy);
  }
}


#undef TEST_SDIV


#define TEST_UDIV(expected_, dividend_, divisor_)          \
  t.dividend = dividend_;                                  \
  t.divisor = divisor_;                                    \
  t.result = 0;                                            \
  dummy = CALL_GENERATED_CODE(isolate, f, &t, 0, 0, 0, 0); \
  CHECK_EQ(expected_, t.result);


TEST(udiv) {
  // Test the udiv.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  Assembler assm(isolate, NULL, 0);

  struct T {
    uint32_t dividend;
    uint32_t divisor;
    uint32_t result;
  } t;

  if (CpuFeatures::IsSupported(SUDIV)) {
    CpuFeatureScope scope(&assm, SUDIV);

    __ mov(r3, Operand(r0));

    __ ldr(r0, MemOperand(r3, offsetof(T, dividend)));
    __ ldr(r1, MemOperand(r3, offsetof(T, divisor)));

    __ sdiv(r2, r0, r1);
    __ str(r2, MemOperand(r3, offsetof(T, result)));

    __ bx(lr);

    CodeDesc desc;
    assm.GetCode(&desc);
    Handle<Code> code = isolate->factory()->NewCode(
        desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
    OFStream os(stdout);
    code->Print(os);
#endif
    F3 f = FUNCTION_CAST<F3>(code->entry());
    Object* dummy;
    TEST_UDIV(0u, 0, 0);
    TEST_UDIV(0u, 1024, 0);
    TEST_UDIV(5u, 10, 2);
    TEST_UDIV(3u, 10, 3);
    USE(dummy);
  }
}


#undef TEST_UDIV


TEST(smmla) {
  CcTest::InitializeVM();
  Isolate* const isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  RandomNumberGenerator* const rng = isolate->random_number_generator();
  Assembler assm(isolate, nullptr, 0);
  __ smmla(r1, r1, r2, r3);
  __ str(r1, MemOperand(r0));
  __ bx(lr);
  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef OBJECT_PRINT
  code->Print(std::cout);
#endif
  F3 f = FUNCTION_CAST<F3>(code->entry());
  for (size_t i = 0; i < 128; ++i) {
    int32_t r, x = rng->NextInt(), y = rng->NextInt(), z = rng->NextInt();
    Object* dummy = CALL_GENERATED_CODE(isolate, f, &r, x, y, z, 0);
    CHECK_EQ(bits::SignedMulHighAndAdd32(x, y, z), r);
    USE(dummy);
  }
}


TEST(smmul) {
  CcTest::InitializeVM();
  Isolate* const isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  RandomNumberGenerator* const rng = isolate->random_number_generator();
  Assembler assm(isolate, nullptr, 0);
  __ smmul(r1, r1, r2);
  __ str(r1, MemOperand(r0));
  __ bx(lr);
  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef OBJECT_PRINT
  code->Print(std::cout);
#endif
  F3 f = FUNCTION_CAST<F3>(code->entry());
  for (size_t i = 0; i < 128; ++i) {
    int32_t r, x = rng->NextInt(), y = rng->NextInt();
    Object* dummy = CALL_GENERATED_CODE(isolate, f, &r, x, y, 0, 0);
    CHECK_EQ(bits::SignedMulHigh32(x, y), r);
    USE(dummy);
  }
}


TEST(sxtb) {
  CcTest::InitializeVM();
  Isolate* const isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  RandomNumberGenerator* const rng = isolate->random_number_generator();
  Assembler assm(isolate, nullptr, 0);
  __ sxtb(r1, r1);
  __ str(r1, MemOperand(r0));
  __ bx(lr);
  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef OBJECT_PRINT
  code->Print(std::cout);
#endif
  F3 f = FUNCTION_CAST<F3>(code->entry());
  for (size_t i = 0; i < 128; ++i) {
    int32_t r, x = rng->NextInt();
    Object* dummy = CALL_GENERATED_CODE(isolate, f, &r, x, 0, 0, 0);
    CHECK_EQ(static_cast<int32_t>(static_cast<int8_t>(x)), r);
    USE(dummy);
  }
}


TEST(sxtab) {
  CcTest::InitializeVM();
  Isolate* const isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  RandomNumberGenerator* const rng = isolate->random_number_generator();
  Assembler assm(isolate, nullptr, 0);
  __ sxtab(r1, r2, r1);
  __ str(r1, MemOperand(r0));
  __ bx(lr);
  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef OBJECT_PRINT
  code->Print(std::cout);
#endif
  F3 f = FUNCTION_CAST<F3>(code->entry());
  for (size_t i = 0; i < 128; ++i) {
    int32_t r, x = rng->NextInt(), y = rng->NextInt();
    Object* dummy = CALL_GENERATED_CODE(isolate, f, &r, x, y, 0, 0);
    CHECK_EQ(static_cast<int32_t>(static_cast<int8_t>(x)) + y, r);
    USE(dummy);
  }
}


TEST(sxth) {
  CcTest::InitializeVM();
  Isolate* const isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  RandomNumberGenerator* const rng = isolate->random_number_generator();
  Assembler assm(isolate, nullptr, 0);
  __ sxth(r1, r1);
  __ str(r1, MemOperand(r0));
  __ bx(lr);
  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef OBJECT_PRINT
  code->Print(std::cout);
#endif
  F3 f = FUNCTION_CAST<F3>(code->entry());
  for (size_t i = 0; i < 128; ++i) {
    int32_t r, x = rng->NextInt();
    Object* dummy = CALL_GENERATED_CODE(isolate, f, &r, x, 0, 0, 0);
    CHECK_EQ(static_cast<int32_t>(static_cast<int16_t>(x)), r);
    USE(dummy);
  }
}


TEST(sxtah) {
  CcTest::InitializeVM();
  Isolate* const isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  RandomNumberGenerator* const rng = isolate->random_number_generator();
  Assembler assm(isolate, nullptr, 0);
  __ sxtah(r1, r2, r1);
  __ str(r1, MemOperand(r0));
  __ bx(lr);
  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef OBJECT_PRINT
  code->Print(std::cout);
#endif
  F3 f = FUNCTION_CAST<F3>(code->entry());
  for (size_t i = 0; i < 128; ++i) {
    int32_t r, x = rng->NextInt(), y = rng->NextInt();
    Object* dummy = CALL_GENERATED_CODE(isolate, f, &r, x, y, 0, 0);
    CHECK_EQ(static_cast<int32_t>(static_cast<int16_t>(x)) + y, r);
    USE(dummy);
  }
}


TEST(uxtb) {
  CcTest::InitializeVM();
  Isolate* const isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  RandomNumberGenerator* const rng = isolate->random_number_generator();
  Assembler assm(isolate, nullptr, 0);
  __ uxtb(r1, r1);
  __ str(r1, MemOperand(r0));
  __ bx(lr);
  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef OBJECT_PRINT
  code->Print(std::cout);
#endif
  F3 f = FUNCTION_CAST<F3>(code->entry());
  for (size_t i = 0; i < 128; ++i) {
    int32_t r, x = rng->NextInt();
    Object* dummy = CALL_GENERATED_CODE(isolate, f, &r, x, 0, 0, 0);
    CHECK_EQ(static_cast<int32_t>(static_cast<uint8_t>(x)), r);
    USE(dummy);
  }
}


TEST(uxtab) {
  CcTest::InitializeVM();
  Isolate* const isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  RandomNumberGenerator* const rng = isolate->random_number_generator();
  Assembler assm(isolate, nullptr, 0);
  __ uxtab(r1, r2, r1);
  __ str(r1, MemOperand(r0));
  __ bx(lr);
  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef OBJECT_PRINT
  code->Print(std::cout);
#endif
  F3 f = FUNCTION_CAST<F3>(code->entry());
  for (size_t i = 0; i < 128; ++i) {
    int32_t r, x = rng->NextInt(), y = rng->NextInt();
    Object* dummy = CALL_GENERATED_CODE(isolate, f, &r, x, y, 0, 0);
    CHECK_EQ(static_cast<int32_t>(static_cast<uint8_t>(x)) + y, r);
    USE(dummy);
  }
}


TEST(uxth) {
  CcTest::InitializeVM();
  Isolate* const isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  RandomNumberGenerator* const rng = isolate->random_number_generator();
  Assembler assm(isolate, nullptr, 0);
  __ uxth(r1, r1);
  __ str(r1, MemOperand(r0));
  __ bx(lr);
  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef OBJECT_PRINT
  code->Print(std::cout);
#endif
  F3 f = FUNCTION_CAST<F3>(code->entry());
  for (size_t i = 0; i < 128; ++i) {
    int32_t r, x = rng->NextInt();
    Object* dummy = CALL_GENERATED_CODE(isolate, f, &r, x, 0, 0, 0);
    CHECK_EQ(static_cast<int32_t>(static_cast<uint16_t>(x)), r);
    USE(dummy);
  }
}


TEST(uxtah) {
  CcTest::InitializeVM();
  Isolate* const isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  RandomNumberGenerator* const rng = isolate->random_number_generator();
  Assembler assm(isolate, nullptr, 0);
  __ uxtah(r1, r2, r1);
  __ str(r1, MemOperand(r0));
  __ bx(lr);
  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef OBJECT_PRINT
  code->Print(std::cout);
#endif
  F3 f = FUNCTION_CAST<F3>(code->entry());
  for (size_t i = 0; i < 128; ++i) {
    int32_t r, x = rng->NextInt(), y = rng->NextInt();
    Object* dummy = CALL_GENERATED_CODE(isolate, f, &r, x, y, 0, 0);
    CHECK_EQ(static_cast<int32_t>(static_cast<uint16_t>(x)) + y, r);
    USE(dummy);
  }
}


#define TEST_RBIT(expected_, input_)                       \
  t.input = input_;                                        \
  t.result = 0;                                            \
  dummy = CALL_GENERATED_CODE(isolate, f, &t, 0, 0, 0, 0); \
  CHECK_EQ(expected_, t.result);


TEST(rbit) {
  CcTest::InitializeVM();
  Isolate* const isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  Assembler assm(isolate, nullptr, 0);

  if (CpuFeatures::IsSupported(ARMv7)) {
    CpuFeatureScope scope(&assm, ARMv7);

    typedef struct {
      uint32_t input;
      uint32_t result;
    } T;
    T t;

    __ ldr(r1, MemOperand(r0, offsetof(T, input)));
    __ rbit(r1, r1);
    __ str(r1, MemOperand(r0, offsetof(T, result)));
    __ bx(lr);

    CodeDesc desc;
    assm.GetCode(&desc);
    Handle<Code> code = isolate->factory()->NewCode(
        desc, Code::ComputeFlags(Code::STUB), Handle<Code>());

#ifdef OBJECT_PRINT
    code->Print(std::cout);
#endif

    F3 f = FUNCTION_CAST<F3>(code->entry());
    Object* dummy = NULL;
    TEST_RBIT(0xffffffff, 0xffffffff);
    TEST_RBIT(0x00000000, 0x00000000);
    TEST_RBIT(0xffff0000, 0x0000ffff);
    TEST_RBIT(0xff00ff00, 0x00ff00ff);
    TEST_RBIT(0xf0f0f0f0, 0x0f0f0f0f);
    TEST_RBIT(0x1e6a2c48, 0x12345678);
    USE(dummy);
  }
}


TEST(code_relative_offset) {
  // Test extracting the offset of a label from the beginning of the code
  // in a register.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  // Initialize a code object that will contain the code.
  Handle<Object> code_object(isolate->heap()->undefined_value(), isolate);

  Assembler assm(isolate, NULL, 0);

  Label start, target_away, target_faraway;

  __ stm(db_w, sp, r4.bit() | r5.bit() | lr.bit());

  // r3 is used as the address zero, the test will crash when we load it.
  __ mov(r3, Operand::Zero());

  // r5 will be a pointer to the start of the code.
  __ mov(r5, Operand(code_object));
  __ mov_label_offset(r4, &start);

  __ mov_label_offset(r1, &target_faraway);
  __ str(r1, MemOperand(sp, kPointerSize, NegPreIndex));

  __ mov_label_offset(r1, &target_away);

  // Jump straight to 'target_away' the first time and use the relative
  // position the second time. This covers the case when extracting the
  // position of a label which is linked.
  __ mov(r2, Operand::Zero());
  __ bind(&start);
  __ cmp(r2, Operand::Zero());
  __ b(eq, &target_away);
  __ add(pc, r5, r1);
  // Emit invalid instructions to push the label between 2^8 and 2^16
  // instructions away. The test will crash if they are reached.
  for (int i = 0; i < (1 << 10); i++) {
    __ ldr(r3, MemOperand(r3));
  }
  __ bind(&target_away);
  // This will be hit twice: r0 = r0 + 5 + 5.
  __ add(r0, r0, Operand(5));

  __ ldr(r1, MemOperand(sp, kPointerSize, PostIndex), ne);
  __ add(pc, r5, r4, LeaveCC, ne);

  __ mov(r2, Operand(1));
  __ b(&start);
  // Emit invalid instructions to push the label between 2^16 and 2^24
  // instructions away. The test will crash if they are reached.
  for (int i = 0; i < (1 << 21); i++) {
    __ ldr(r3, MemOperand(r3));
  }
  __ bind(&target_faraway);
  // r0 = r0 + 5 + 5 + 11
  __ add(r0, r0, Operand(11));

  __ ldm(ia_w, sp, r4.bit() | r5.bit() | pc.bit());

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), code_object);
  F1 f = FUNCTION_CAST<F1>(code->entry());
  int res =
      reinterpret_cast<int>(CALL_GENERATED_CODE(isolate, f, 21, 0, 0, 0, 0));
  ::printf("f() = %d\n", res);
  CHECK_EQ(42, res);
}

TEST(msr_mrs) {
  // Test msr and mrs.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);

  // Create a helper function:
  //  void TestMsrMrs(uint32_t nzcv,
  //                  uint32_t * result_conditionals,
  //                  uint32_t * result_mrs);
  __ msr(CPSR_f, Operand(r0));

  // Test that the condition flags have taken effect.
  __ mov(r3, Operand(0));
  __ orr(r3, r3, Operand(1 << 31), LeaveCC, mi);  // N
  __ orr(r3, r3, Operand(1 << 30), LeaveCC, eq);  // Z
  __ orr(r3, r3, Operand(1 << 29), LeaveCC, cs);  // C
  __ orr(r3, r3, Operand(1 << 28), LeaveCC, vs);  // V
  __ str(r3, MemOperand(r1));

  // Also check mrs, ignoring everything other than the flags.
  __ mrs(r3, CPSR);
  __ and_(r3, r3, Operand(kSpecialCondition));
  __ str(r3, MemOperand(r2));

  __ bx(lr);

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  F5 f = FUNCTION_CAST<F5>(code->entry());
  Object* dummy = nullptr;
  USE(dummy);

#define CHECK_MSR_MRS(n, z, c, v)                                       \
  do {                                                                  \
    uint32_t nzcv = (n << 31) | (z << 30) | (c << 29) | (v << 28);      \
    uint32_t result_conditionals = -1;                                  \
    uint32_t result_mrs = -1;                                           \
    dummy = CALL_GENERATED_CODE(isolate, f, nzcv, &result_conditionals, \
                                &result_mrs, 0, 0);                     \
    CHECK_EQ(nzcv, result_conditionals);                                \
    CHECK_EQ(nzcv, result_mrs);                                         \
  } while (0);

  //            N  Z  C  V
  CHECK_MSR_MRS(0, 0, 0, 0);
  CHECK_MSR_MRS(0, 0, 0, 1);
  CHECK_MSR_MRS(0, 0, 1, 0);
  CHECK_MSR_MRS(0, 0, 1, 1);
  CHECK_MSR_MRS(0, 1, 0, 0);
  CHECK_MSR_MRS(0, 1, 0, 1);
  CHECK_MSR_MRS(0, 1, 1, 0);
  CHECK_MSR_MRS(0, 1, 1, 1);
  CHECK_MSR_MRS(1, 0, 0, 0);
  CHECK_MSR_MRS(1, 0, 0, 1);
  CHECK_MSR_MRS(1, 0, 1, 0);
  CHECK_MSR_MRS(1, 0, 1, 1);
  CHECK_MSR_MRS(1, 1, 0, 0);
  CHECK_MSR_MRS(1, 1, 0, 1);
  CHECK_MSR_MRS(1, 1, 1, 0);
  CHECK_MSR_MRS(1, 1, 1, 1);

#undef CHECK_MSR_MRS
}

TEST(ARMv8_float32_vrintX) {
  // Test the vrintX floating point instructions.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  typedef struct {
    float input;
    float ar;
    float nr;
    float mr;
    float pr;
    float zr;
  } T;
  T t;

  // Create a function that accepts &t, and loads, manipulates, and stores
  // the floats.
  Assembler assm(isolate, NULL, 0);
  Label L, C;


  if (CpuFeatures::IsSupported(ARMv8)) {
    CpuFeatureScope scope(&assm, ARMv8);

    __ mov(ip, Operand(sp));
    __ stm(db_w, sp, r4.bit() | fp.bit() | lr.bit());

    __ mov(r4, Operand(r0));

    // Test vrinta
    __ vldr(s6, r4, offsetof(T, input));
    __ vrinta(s5, s6);
    __ vstr(s5, r4, offsetof(T, ar));

    // Test vrintn
    __ vldr(s6, r4, offsetof(T, input));
    __ vrintn(s5, s6);
    __ vstr(s5, r4, offsetof(T, nr));

    // Test vrintp
    __ vldr(s6, r4, offsetof(T, input));
    __ vrintp(s5, s6);
    __ vstr(s5, r4, offsetof(T, pr));

    // Test vrintm
    __ vldr(s6, r4, offsetof(T, input));
    __ vrintm(s5, s6);
    __ vstr(s5, r4, offsetof(T, mr));

    // Test vrintz
    __ vldr(s6, r4, offsetof(T, input));
    __ vrintz(s5, s6);
    __ vstr(s5, r4, offsetof(T, zr));

    __ ldm(ia_w, sp, r4.bit() | fp.bit() | pc.bit());

    CodeDesc desc;
    assm.GetCode(&desc);
    Handle<Code> code = isolate->factory()->NewCode(
        desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
    OFStream os(stdout);
    code->Print(os);
#endif
    F3 f = FUNCTION_CAST<F3>(code->entry());

    Object* dummy = nullptr;
    USE(dummy);

#define CHECK_VRINT(input_val, ares, nres, mres, pres, zres) \
  t.input = input_val;                                       \
  dummy = CALL_GENERATED_CODE(isolate, f, &t, 0, 0, 0, 0);   \
  CHECK_EQ(ares, t.ar);                                      \
  CHECK_EQ(nres, t.nr);                                      \
  CHECK_EQ(mres, t.mr);                                      \
  CHECK_EQ(pres, t.pr);                                      \
  CHECK_EQ(zres, t.zr);

    CHECK_VRINT(-0.5, -1.0, -0.0, -1.0, -0.0, -0.0)
    CHECK_VRINT(-0.6, -1.0, -1.0, -1.0, -0.0, -0.0)
    CHECK_VRINT(-1.1, -1.0, -1.0, -2.0, -1.0, -1.0)
    CHECK_VRINT(0.5, 1.0, 0.0, 0.0, 1.0, 0.0)
    CHECK_VRINT(0.6, 1.0, 1.0, 0.0, 1.0, 0.0)
    CHECK_VRINT(1.1, 1.0, 1.0, 1.0, 2.0, 1.0)
    float inf = std::numeric_limits<float>::infinity();
    CHECK_VRINT(inf, inf, inf, inf, inf, inf)
    CHECK_VRINT(-inf, -inf, -inf, -inf, -inf, -inf)
    CHECK_VRINT(-0.0, -0.0, -0.0, -0.0, -0.0, -0.0)

    // Check NaN propagation.
    float nan = std::numeric_limits<float>::quiet_NaN();
    t.input = nan;
    dummy = CALL_GENERATED_CODE(isolate, f, &t, 0, 0, 0, 0);
    CHECK_EQ(bit_cast<int32_t>(nan), bit_cast<int32_t>(t.ar));
    CHECK_EQ(bit_cast<int32_t>(nan), bit_cast<int32_t>(t.nr));
    CHECK_EQ(bit_cast<int32_t>(nan), bit_cast<int32_t>(t.mr));
    CHECK_EQ(bit_cast<int32_t>(nan), bit_cast<int32_t>(t.pr));
    CHECK_EQ(bit_cast<int32_t>(nan), bit_cast<int32_t>(t.zr));

#undef CHECK_VRINT
  }
}


TEST(ARMv8_vrintX) {
  // Test the vrintX floating point instructions.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  typedef struct {
    double input;
    double ar;
    double nr;
    double mr;
    double pr;
    double zr;
  } T;
  T t;

  // Create a function that accepts &t, and loads, manipulates, and stores
  // the doubles and floats.
  Assembler assm(isolate, NULL, 0);
  Label L, C;


  if (CpuFeatures::IsSupported(ARMv8)) {
    CpuFeatureScope scope(&assm, ARMv8);

    __ mov(ip, Operand(sp));
    __ stm(db_w, sp, r4.bit() | fp.bit() | lr.bit());

    __ mov(r4, Operand(r0));

    // Test vrinta
    __ vldr(d6, r4, offsetof(T, input));
    __ vrinta(d5, d6);
    __ vstr(d5, r4, offsetof(T, ar));

    // Test vrintn
    __ vldr(d6, r4, offsetof(T, input));
    __ vrintn(d5, d6);
    __ vstr(d5, r4, offsetof(T, nr));

    // Test vrintp
    __ vldr(d6, r4, offsetof(T, input));
    __ vrintp(d5, d6);
    __ vstr(d5, r4, offsetof(T, pr));

    // Test vrintm
    __ vldr(d6, r4, offsetof(T, input));
    __ vrintm(d5, d6);
    __ vstr(d5, r4, offsetof(T, mr));

    // Test vrintz
    __ vldr(d6, r4, offsetof(T, input));
    __ vrintz(d5, d6);
    __ vstr(d5, r4, offsetof(T, zr));

    __ ldm(ia_w, sp, r4.bit() | fp.bit() | pc.bit());

    CodeDesc desc;
    assm.GetCode(&desc);
    Handle<Code> code = isolate->factory()->NewCode(
        desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
    OFStream os(stdout);
    code->Print(os);
#endif
    F3 f = FUNCTION_CAST<F3>(code->entry());

    Object* dummy = nullptr;
    USE(dummy);

#define CHECK_VRINT(input_val, ares, nres, mres, pres, zres) \
  t.input = input_val;                                       \
  dummy = CALL_GENERATED_CODE(isolate, f, &t, 0, 0, 0, 0);   \
  CHECK_EQ(ares, t.ar);                                      \
  CHECK_EQ(nres, t.nr);                                      \
  CHECK_EQ(mres, t.mr);                                      \
  CHECK_EQ(pres, t.pr);                                      \
  CHECK_EQ(zres, t.zr);

    CHECK_VRINT(-0.5, -1.0, -0.0, -1.0, -0.0, -0.0)
    CHECK_VRINT(-0.6, -1.0, -1.0, -1.0, -0.0, -0.0)
    CHECK_VRINT(-1.1, -1.0, -1.0, -2.0, -1.0, -1.0)
    CHECK_VRINT(0.5, 1.0, 0.0, 0.0, 1.0, 0.0)
    CHECK_VRINT(0.6, 1.0, 1.0, 0.0, 1.0, 0.0)
    CHECK_VRINT(1.1, 1.0, 1.0, 1.0, 2.0, 1.0)
    double inf = std::numeric_limits<double>::infinity();
    CHECK_VRINT(inf, inf, inf, inf, inf, inf)
    CHECK_VRINT(-inf, -inf, -inf, -inf, -inf, -inf)
    CHECK_VRINT(-0.0, -0.0, -0.0, -0.0, -0.0, -0.0)

    // Check NaN propagation.
    double nan = std::numeric_limits<double>::quiet_NaN();
    t.input = nan;
    dummy = CALL_GENERATED_CODE(isolate, f, &t, 0, 0, 0, 0);
    CHECK_EQ(bit_cast<int64_t>(nan), bit_cast<int64_t>(t.ar));
    CHECK_EQ(bit_cast<int64_t>(nan), bit_cast<int64_t>(t.nr));
    CHECK_EQ(bit_cast<int64_t>(nan), bit_cast<int64_t>(t.mr));
    CHECK_EQ(bit_cast<int64_t>(nan), bit_cast<int64_t>(t.pr));
    CHECK_EQ(bit_cast<int64_t>(nan), bit_cast<int64_t>(t.zr));

#undef CHECK_VRINT
  }
}

TEST(ARMv8_vsel) {
  // Test the vsel floating point instructions.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);

  // Used to indicate whether a condition passed or failed.
  static constexpr float kResultPass = 1.0f;
  static constexpr float kResultFail = -kResultPass;

  struct ResultsF32 {
    float vseleq_;
    float vselge_;
    float vselgt_;
    float vselvs_;

    // The following conditions aren't architecturally supported, but the
    // assembler implements them by swapping the inputs.
    float vselne_;
    float vsellt_;
    float vselle_;
    float vselvc_;
  };

  struct ResultsF64 {
    double vseleq_;
    double vselge_;
    double vselgt_;
    double vselvs_;

    // The following conditions aren't architecturally supported, but the
    // assembler implements them by swapping the inputs.
    double vselne_;
    double vsellt_;
    double vselle_;
    double vselvc_;
  };

  if (CpuFeatures::IsSupported(ARMv8)) {
    CpuFeatureScope scope(&assm, ARMv8);

    // Create a helper function:
    //  void TestVsel(uint32_t nzcv,
    //                ResultsF32* results_f32,
    //                ResultsF64* results_f64);
    __ msr(CPSR_f, Operand(r0));

    __ vmov(s1, kResultPass);
    __ vmov(s2, kResultFail);

    __ vsel(eq, s0, s1, s2);
    __ vstr(s0, r1, offsetof(ResultsF32, vseleq_));
    __ vsel(ge, s0, s1, s2);
    __ vstr(s0, r1, offsetof(ResultsF32, vselge_));
    __ vsel(gt, s0, s1, s2);
    __ vstr(s0, r1, offsetof(ResultsF32, vselgt_));
    __ vsel(vs, s0, s1, s2);
    __ vstr(s0, r1, offsetof(ResultsF32, vselvs_));

    __ vsel(ne, s0, s1, s2);
    __ vstr(s0, r1, offsetof(ResultsF32, vselne_));
    __ vsel(lt, s0, s1, s2);
    __ vstr(s0, r1, offsetof(ResultsF32, vsellt_));
    __ vsel(le, s0, s1, s2);
    __ vstr(s0, r1, offsetof(ResultsF32, vselle_));
    __ vsel(vc, s0, s1, s2);
    __ vstr(s0, r1, offsetof(ResultsF32, vselvc_));

    __ vmov(d1, kResultPass);
    __ vmov(d2, kResultFail);

    __ vsel(eq, d0, d1, d2);
    __ vstr(d0, r2, offsetof(ResultsF64, vseleq_));
    __ vsel(ge, d0, d1, d2);
    __ vstr(d0, r2, offsetof(ResultsF64, vselge_));
    __ vsel(gt, d0, d1, d2);
    __ vstr(d0, r2, offsetof(ResultsF64, vselgt_));
    __ vsel(vs, d0, d1, d2);
    __ vstr(d0, r2, offsetof(ResultsF64, vselvs_));

    __ vsel(ne, d0, d1, d2);
    __ vstr(d0, r2, offsetof(ResultsF64, vselne_));
    __ vsel(lt, d0, d1, d2);
    __ vstr(d0, r2, offsetof(ResultsF64, vsellt_));
    __ vsel(le, d0, d1, d2);
    __ vstr(d0, r2, offsetof(ResultsF64, vselle_));
    __ vsel(vc, d0, d1, d2);
    __ vstr(d0, r2, offsetof(ResultsF64, vselvc_));

    __ bx(lr);

    CodeDesc desc;
    assm.GetCode(&desc);
    Handle<Code> code = isolate->factory()->NewCode(
        desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
    OFStream os(stdout);
    code->Print(os);
#endif
    F5 f = FUNCTION_CAST<F5>(code->entry());
    Object* dummy = nullptr;
    USE(dummy);

    STATIC_ASSERT(kResultPass == -kResultFail);
#define CHECK_VSEL(n, z, c, v, vseleq, vselge, vselgt, vselvs)                \
  do {                                                                        \
    ResultsF32 results_f32;                                                   \
    ResultsF64 results_f64;                                                   \
    uint32_t nzcv = (n << 31) | (z << 30) | (c << 29) | (v << 28);            \
    dummy = CALL_GENERATED_CODE(isolate, f, nzcv, &results_f32, &results_f64, \
                                0, 0);                                        \
    CHECK_EQ(vseleq, results_f32.vseleq_);                                    \
    CHECK_EQ(vselge, results_f32.vselge_);                                    \
    CHECK_EQ(vselgt, results_f32.vselgt_);                                    \
    CHECK_EQ(vselvs, results_f32.vselvs_);                                    \
    CHECK_EQ(-vseleq, results_f32.vselne_);                                   \
    CHECK_EQ(-vselge, results_f32.vsellt_);                                   \
    CHECK_EQ(-vselgt, results_f32.vselle_);                                   \
    CHECK_EQ(-vselvs, results_f32.vselvc_);                                   \
    CHECK_EQ(vseleq, results_f64.vseleq_);                                    \
    CHECK_EQ(vselge, results_f64.vselge_);                                    \
    CHECK_EQ(vselgt, results_f64.vselgt_);                                    \
    CHECK_EQ(vselvs, results_f64.vselvs_);                                    \
    CHECK_EQ(-vseleq, results_f64.vselne_);                                   \
    CHECK_EQ(-vselge, results_f64.vsellt_);                                   \
    CHECK_EQ(-vselgt, results_f64.vselle_);                                   \
    CHECK_EQ(-vselvs, results_f64.vselvc_);                                   \
  } while (0);

    //         N  Z  C  V  vseleq       vselge       vselgt       vselvs
    CHECK_VSEL(0, 0, 0, 0, kResultFail, kResultPass, kResultPass, kResultFail);
    CHECK_VSEL(0, 0, 0, 1, kResultFail, kResultFail, kResultFail, kResultPass);
    CHECK_VSEL(0, 0, 1, 0, kResultFail, kResultPass, kResultPass, kResultFail);
    CHECK_VSEL(0, 0, 1, 1, kResultFail, kResultFail, kResultFail, kResultPass);
    CHECK_VSEL(0, 1, 0, 0, kResultPass, kResultPass, kResultFail, kResultFail);
    CHECK_VSEL(0, 1, 0, 1, kResultPass, kResultFail, kResultFail, kResultPass);
    CHECK_VSEL(0, 1, 1, 0, kResultPass, kResultPass, kResultFail, kResultFail);
    CHECK_VSEL(0, 1, 1, 1, kResultPass, kResultFail, kResultFail, kResultPass);
    CHECK_VSEL(1, 0, 0, 0, kResultFail, kResultFail, kResultFail, kResultFail);
    CHECK_VSEL(1, 0, 0, 1, kResultFail, kResultPass, kResultPass, kResultPass);
    CHECK_VSEL(1, 0, 1, 0, kResultFail, kResultFail, kResultFail, kResultFail);
    CHECK_VSEL(1, 0, 1, 1, kResultFail, kResultPass, kResultPass, kResultPass);
    CHECK_VSEL(1, 1, 0, 0, kResultPass, kResultFail, kResultFail, kResultFail);
    CHECK_VSEL(1, 1, 0, 1, kResultPass, kResultPass, kResultFail, kResultPass);
    CHECK_VSEL(1, 1, 1, 0, kResultPass, kResultFail, kResultFail, kResultFail);
    CHECK_VSEL(1, 1, 1, 1, kResultPass, kResultPass, kResultFail, kResultPass);

#undef CHECK_VSEL
  }
}

TEST(ARMv8_vminmax_f64) {
  // Test the vminnm and vmaxnm floating point instructions.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);

  struct Inputs {
    double left_;
    double right_;
  };

  struct Results {
    double vminnm_;
    double vmaxnm_;
  };

  if (CpuFeatures::IsSupported(ARMv8)) {
    CpuFeatureScope scope(&assm, ARMv8);

    // Create a helper function:
    //  void TestVminmax(const Inputs* inputs,
    //                   Results* results);
    __ vldr(d1, r0, offsetof(Inputs, left_));
    __ vldr(d2, r0, offsetof(Inputs, right_));

    __ vminnm(d0, d1, d2);
    __ vstr(d0, r1, offsetof(Results, vminnm_));
    __ vmaxnm(d0, d1, d2);
    __ vstr(d0, r1, offsetof(Results, vmaxnm_));

    __ bx(lr);

    CodeDesc desc;
    assm.GetCode(&desc);
    Handle<Code> code = isolate->factory()->NewCode(
        desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
    OFStream os(stdout);
    code->Print(os);
#endif
    F4 f = FUNCTION_CAST<F4>(code->entry());
    Object* dummy = nullptr;
    USE(dummy);

#define CHECK_VMINMAX(left, right, vminnm, vmaxnm)                             \
  do {                                                                         \
    Inputs inputs = {left, right};                                             \
    Results results;                                                           \
    dummy = CALL_GENERATED_CODE(isolate, f, &inputs, &results, 0, 0, 0);       \
    /* Use a bit_cast to correctly identify -0.0 and NaNs. */                  \
    CHECK_EQ(bit_cast<uint64_t>(vminnm), bit_cast<uint64_t>(results.vminnm_)); \
    CHECK_EQ(bit_cast<uint64_t>(vmaxnm), bit_cast<uint64_t>(results.vmaxnm_)); \
  } while (0);

    double nan_a = bit_cast<double>(UINT64_C(0x7ff8000000000001));
    double nan_b = bit_cast<double>(UINT64_C(0x7ff8000000000002));

    CHECK_VMINMAX(1.0, -1.0, -1.0, 1.0);
    CHECK_VMINMAX(-1.0, 1.0, -1.0, 1.0);
    CHECK_VMINMAX(0.0, -1.0, -1.0, 0.0);
    CHECK_VMINMAX(-1.0, 0.0, -1.0, 0.0);
    CHECK_VMINMAX(-0.0, -1.0, -1.0, -0.0);
    CHECK_VMINMAX(-1.0, -0.0, -1.0, -0.0);
    CHECK_VMINMAX(0.0, 1.0, 0.0, 1.0);
    CHECK_VMINMAX(1.0, 0.0, 0.0, 1.0);

    CHECK_VMINMAX(0.0, 0.0, 0.0, 0.0);
    CHECK_VMINMAX(-0.0, -0.0, -0.0, -0.0);
    CHECK_VMINMAX(-0.0, 0.0, -0.0, 0.0);
    CHECK_VMINMAX(0.0, -0.0, -0.0, 0.0);

    CHECK_VMINMAX(0.0, nan_a, 0.0, 0.0);
    CHECK_VMINMAX(nan_a, 0.0, 0.0, 0.0);
    CHECK_VMINMAX(nan_a, nan_b, nan_a, nan_a);
    CHECK_VMINMAX(nan_b, nan_a, nan_b, nan_b);

#undef CHECK_VMINMAX
  }
}

TEST(ARMv8_vminmax_f32) {
  // Test the vminnm and vmaxnm floating point instructions.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);

  struct Inputs {
    float left_;
    float right_;
  };

  struct Results {
    float vminnm_;
    float vmaxnm_;
  };

  if (CpuFeatures::IsSupported(ARMv8)) {
    CpuFeatureScope scope(&assm, ARMv8);

    // Create a helper function:
    //  void TestVminmax(const Inputs* inputs,
    //                   Results* results);
    __ vldr(s1, r0, offsetof(Inputs, left_));
    __ vldr(s2, r0, offsetof(Inputs, right_));

    __ vminnm(s0, s1, s2);
    __ vstr(s0, r1, offsetof(Results, vminnm_));
    __ vmaxnm(s0, s1, s2);
    __ vstr(s0, r1, offsetof(Results, vmaxnm_));

    __ bx(lr);

    CodeDesc desc;
    assm.GetCode(&desc);
    Handle<Code> code = isolate->factory()->NewCode(
        desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
    OFStream os(stdout);
    code->Print(os);
#endif
    F4 f = FUNCTION_CAST<F4>(code->entry());
    Object* dummy = nullptr;
    USE(dummy);

#define CHECK_VMINMAX(left, right, vminnm, vmaxnm)                             \
  do {                                                                         \
    Inputs inputs = {left, right};                                             \
    Results results;                                                           \
    dummy = CALL_GENERATED_CODE(isolate, f, &inputs, &results, 0, 0, 0);       \
    /* Use a bit_cast to correctly identify -0.0 and NaNs. */                  \
    CHECK_EQ(bit_cast<uint32_t>(vminnm), bit_cast<uint32_t>(results.vminnm_)); \
    CHECK_EQ(bit_cast<uint32_t>(vmaxnm), bit_cast<uint32_t>(results.vmaxnm_)); \
  } while (0);

    float nan_a = bit_cast<float>(UINT32_C(0x7fc00001));
    float nan_b = bit_cast<float>(UINT32_C(0x7fc00002));

    CHECK_VMINMAX(1.0f, -1.0f, -1.0f, 1.0f);
    CHECK_VMINMAX(-1.0f, 1.0f, -1.0f, 1.0f);
    CHECK_VMINMAX(0.0f, -1.0f, -1.0f, 0.0f);
    CHECK_VMINMAX(-1.0f, 0.0f, -1.0f, 0.0f);
    CHECK_VMINMAX(-0.0f, -1.0f, -1.0f, -0.0f);
    CHECK_VMINMAX(-1.0f, -0.0f, -1.0f, -0.0f);
    CHECK_VMINMAX(0.0f, 1.0f, 0.0f, 1.0f);
    CHECK_VMINMAX(1.0f, 0.0f, 0.0f, 1.0f);

    CHECK_VMINMAX(0.0f, 0.0f, 0.0f, 0.0f);
    CHECK_VMINMAX(-0.0f, -0.0f, -0.0f, -0.0f);
    CHECK_VMINMAX(-0.0f, 0.0f, -0.0f, 0.0f);
    CHECK_VMINMAX(0.0f, -0.0f, -0.0f, 0.0f);

    CHECK_VMINMAX(0.0f, nan_a, 0.0f, 0.0f);
    CHECK_VMINMAX(nan_a, 0.0f, 0.0f, 0.0f);
    CHECK_VMINMAX(nan_a, nan_b, nan_a, nan_a);
    CHECK_VMINMAX(nan_b, nan_a, nan_b, nan_b);

#undef CHECK_VMINMAX
  }
}

template <typename T, typename Inputs, typename Results>
static F4 GenerateMacroFloatMinMax(MacroAssembler& assm) {
  T a = T::from_code(0);  // d0/s0
  T b = T::from_code(1);  // d1/s1
  T c = T::from_code(2);  // d2/s2

  // Create a helper function:
  //  void TestFloatMinMax(const Inputs* inputs,
  //                       Results* results);
  Label ool_min_abc, ool_min_aab, ool_min_aba;
  Label ool_max_abc, ool_max_aab, ool_max_aba;

  Label done_min_abc, done_min_aab, done_min_aba;
  Label done_max_abc, done_max_aab, done_max_aba;

  // a = min(b, c);
  __ vldr(b, r0, offsetof(Inputs, left_));
  __ vldr(c, r0, offsetof(Inputs, right_));
  __ FloatMin(a, b, c, &ool_min_abc);
  __ bind(&done_min_abc);
  __ vstr(a, r1, offsetof(Results, min_abc_));

  // a = min(a, b);
  __ vldr(a, r0, offsetof(Inputs, left_));
  __ vldr(b, r0, offsetof(Inputs, right_));
  __ FloatMin(a, a, b, &ool_min_aab);
  __ bind(&done_min_aab);
  __ vstr(a, r1, offsetof(Results, min_aab_));

  // a = min(b, a);
  __ vldr(b, r0, offsetof(Inputs, left_));
  __ vldr(a, r0, offsetof(Inputs, right_));
  __ FloatMin(a, b, a, &ool_min_aba);
  __ bind(&done_min_aba);
  __ vstr(a, r1, offsetof(Results, min_aba_));

  // a = max(b, c);
  __ vldr(b, r0, offsetof(Inputs, left_));
  __ vldr(c, r0, offsetof(Inputs, right_));
  __ FloatMax(a, b, c, &ool_max_abc);
  __ bind(&done_max_abc);
  __ vstr(a, r1, offsetof(Results, max_abc_));

  // a = max(a, b);
  __ vldr(a, r0, offsetof(Inputs, left_));
  __ vldr(b, r0, offsetof(Inputs, right_));
  __ FloatMax(a, a, b, &ool_max_aab);
  __ bind(&done_max_aab);
  __ vstr(a, r1, offsetof(Results, max_aab_));

  // a = max(b, a);
  __ vldr(b, r0, offsetof(Inputs, left_));
  __ vldr(a, r0, offsetof(Inputs, right_));
  __ FloatMax(a, b, a, &ool_max_aba);
  __ bind(&done_max_aba);
  __ vstr(a, r1, offsetof(Results, max_aba_));

  __ bx(lr);

  // Generate out-of-line cases.
  __ bind(&ool_min_abc);
  __ FloatMinOutOfLine(a, b, c);
  __ b(&done_min_abc);

  __ bind(&ool_min_aab);
  __ FloatMinOutOfLine(a, a, b);
  __ b(&done_min_aab);

  __ bind(&ool_min_aba);
  __ FloatMinOutOfLine(a, b, a);
  __ b(&done_min_aba);

  __ bind(&ool_max_abc);
  __ FloatMaxOutOfLine(a, b, c);
  __ b(&done_max_abc);

  __ bind(&ool_max_aab);
  __ FloatMaxOutOfLine(a, a, b);
  __ b(&done_max_aab);

  __ bind(&ool_max_aba);
  __ FloatMaxOutOfLine(a, b, a);
  __ b(&done_max_aba);

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = assm.isolate()->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  return FUNCTION_CAST<F4>(code->entry());
}

TEST(macro_float_minmax_f64) {
  // Test the FloatMin and FloatMax macros.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assm(isolate, NULL, 0, CodeObjectRequired::kYes);

  struct Inputs {
    double left_;
    double right_;
  };

  struct Results {
    // Check all register aliasing possibilities in order to exercise all
    // code-paths in the macro assembler.
    double min_abc_;
    double min_aab_;
    double min_aba_;
    double max_abc_;
    double max_aab_;
    double max_aba_;
  };

  F4 f = GenerateMacroFloatMinMax<DwVfpRegister, Inputs, Results>(assm);

  Object* dummy = nullptr;
  USE(dummy);

#define CHECK_MINMAX(left, right, min, max)                                  \
  do {                                                                       \
    Inputs inputs = {left, right};                                           \
    Results results;                                                         \
    dummy = CALL_GENERATED_CODE(isolate, f, &inputs, &results, 0, 0, 0);     \
    /* Use a bit_cast to correctly identify -0.0 and NaNs. */                \
    CHECK_EQ(bit_cast<uint64_t>(min), bit_cast<uint64_t>(results.min_abc_)); \
    CHECK_EQ(bit_cast<uint64_t>(min), bit_cast<uint64_t>(results.min_aab_)); \
    CHECK_EQ(bit_cast<uint64_t>(min), bit_cast<uint64_t>(results.min_aba_)); \
    CHECK_EQ(bit_cast<uint64_t>(max), bit_cast<uint64_t>(results.max_abc_)); \
    CHECK_EQ(bit_cast<uint64_t>(max), bit_cast<uint64_t>(results.max_aab_)); \
    CHECK_EQ(bit_cast<uint64_t>(max), bit_cast<uint64_t>(results.max_aba_)); \
  } while (0)

  double nan_a = bit_cast<double>(UINT64_C(0x7ff8000000000001));
  double nan_b = bit_cast<double>(UINT64_C(0x7ff8000000000002));

  CHECK_MINMAX(1.0, -1.0, -1.0, 1.0);
  CHECK_MINMAX(-1.0, 1.0, -1.0, 1.0);
  CHECK_MINMAX(0.0, -1.0, -1.0, 0.0);
  CHECK_MINMAX(-1.0, 0.0, -1.0, 0.0);
  CHECK_MINMAX(-0.0, -1.0, -1.0, -0.0);
  CHECK_MINMAX(-1.0, -0.0, -1.0, -0.0);
  CHECK_MINMAX(0.0, 1.0, 0.0, 1.0);
  CHECK_MINMAX(1.0, 0.0, 0.0, 1.0);

  CHECK_MINMAX(0.0, 0.0, 0.0, 0.0);
  CHECK_MINMAX(-0.0, -0.0, -0.0, -0.0);
  CHECK_MINMAX(-0.0, 0.0, -0.0, 0.0);
  CHECK_MINMAX(0.0, -0.0, -0.0, 0.0);

  CHECK_MINMAX(0.0, nan_a, nan_a, nan_a);
  CHECK_MINMAX(nan_a, 0.0, nan_a, nan_a);
  CHECK_MINMAX(nan_a, nan_b, nan_a, nan_a);
  CHECK_MINMAX(nan_b, nan_a, nan_b, nan_b);

#undef CHECK_MINMAX
}

TEST(macro_float_minmax_f32) {
  // Test the FloatMin and FloatMax macros.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assm(isolate, NULL, 0, CodeObjectRequired::kYes);

  struct Inputs {
    float left_;
    float right_;
  };

  struct Results {
    // Check all register aliasing possibilities in order to exercise all
    // code-paths in the macro assembler.
    float min_abc_;
    float min_aab_;
    float min_aba_;
    float max_abc_;
    float max_aab_;
    float max_aba_;
  };

  F4 f = GenerateMacroFloatMinMax<SwVfpRegister, Inputs, Results>(assm);
  Object* dummy = nullptr;
  USE(dummy);

#define CHECK_MINMAX(left, right, min, max)                                  \
  do {                                                                       \
    Inputs inputs = {left, right};                                           \
    Results results;                                                         \
    dummy = CALL_GENERATED_CODE(isolate, f, &inputs, &results, 0, 0, 0);     \
    /* Use a bit_cast to correctly identify -0.0 and NaNs. */                \
    CHECK_EQ(bit_cast<uint32_t>(min), bit_cast<uint32_t>(results.min_abc_)); \
    CHECK_EQ(bit_cast<uint32_t>(min), bit_cast<uint32_t>(results.min_aab_)); \
    CHECK_EQ(bit_cast<uint32_t>(min), bit_cast<uint32_t>(results.min_aba_)); \
    CHECK_EQ(bit_cast<uint32_t>(max), bit_cast<uint32_t>(results.max_abc_)); \
    CHECK_EQ(bit_cast<uint32_t>(max), bit_cast<uint32_t>(results.max_aab_)); \
    CHECK_EQ(bit_cast<uint32_t>(max), bit_cast<uint32_t>(results.max_aba_)); \
  } while (0)

  float nan_a = bit_cast<float>(UINT32_C(0x7fc00001));
  float nan_b = bit_cast<float>(UINT32_C(0x7fc00002));

  CHECK_MINMAX(1.0f, -1.0f, -1.0f, 1.0f);
  CHECK_MINMAX(-1.0f, 1.0f, -1.0f, 1.0f);
  CHECK_MINMAX(0.0f, -1.0f, -1.0f, 0.0f);
  CHECK_MINMAX(-1.0f, 0.0f, -1.0f, 0.0f);
  CHECK_MINMAX(-0.0f, -1.0f, -1.0f, -0.0f);
  CHECK_MINMAX(-1.0f, -0.0f, -1.0f, -0.0f);
  CHECK_MINMAX(0.0f, 1.0f, 0.0f, 1.0f);
  CHECK_MINMAX(1.0f, 0.0f, 0.0f, 1.0f);

  CHECK_MINMAX(0.0f, 0.0f, 0.0f, 0.0f);
  CHECK_MINMAX(-0.0f, -0.0f, -0.0f, -0.0f);
  CHECK_MINMAX(-0.0f, 0.0f, -0.0f, 0.0f);
  CHECK_MINMAX(0.0f, -0.0f, -0.0f, 0.0f);

  CHECK_MINMAX(0.0f, nan_a, nan_a, nan_a);
  CHECK_MINMAX(nan_a, 0.0f, nan_a, nan_a);
  CHECK_MINMAX(nan_a, nan_b, nan_a, nan_a);
  CHECK_MINMAX(nan_b, nan_a, nan_b, nan_b);

#undef CHECK_MINMAX
}

TEST(unaligned_loads) {
  // All supported ARM targets allow unaligned accesses.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  typedef struct {
    uint32_t ldrh;
    uint32_t ldrsh;
    uint32_t ldr;
  } T;
  T t;

  Assembler assm(isolate, NULL, 0);
  __ ldrh(ip, MemOperand(r1, r2));
  __ str(ip, MemOperand(r0, offsetof(T, ldrh)));
  __ ldrsh(ip, MemOperand(r1, r2));
  __ str(ip, MemOperand(r0, offsetof(T, ldrsh)));
  __ ldr(ip, MemOperand(r1, r2));
  __ str(ip, MemOperand(r0, offsetof(T, ldr)));
  __ bx(lr);

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  F4 f = FUNCTION_CAST<F4>(code->entry());

  Object* dummy = nullptr;
  USE(dummy);

#ifndef V8_TARGET_LITTLE_ENDIAN
#error This test assumes a little-endian layout.
#endif
  uint64_t data = UINT64_C(0x84838281807f7e7d);
  dummy = CALL_GENERATED_CODE(isolate, f, &t, &data, 0, 0, 0);
  CHECK_EQ(0x00007e7d, t.ldrh);
  CHECK_EQ(0x00007e7d, t.ldrsh);
  CHECK_EQ(0x807f7e7d, t.ldr);
  dummy = CALL_GENERATED_CODE(isolate, f, &t, &data, 1, 0, 0);
  CHECK_EQ(0x00007f7e, t.ldrh);
  CHECK_EQ(0x00007f7e, t.ldrsh);
  CHECK_EQ(0x81807f7e, t.ldr);
  dummy = CALL_GENERATED_CODE(isolate, f, &t, &data, 2, 0, 0);
  CHECK_EQ(0x0000807f, t.ldrh);
  CHECK_EQ(0xffff807f, t.ldrsh);
  CHECK_EQ(0x8281807f, t.ldr);
  dummy = CALL_GENERATED_CODE(isolate, f, &t, &data, 3, 0, 0);
  CHECK_EQ(0x00008180, t.ldrh);
  CHECK_EQ(0xffff8180, t.ldrsh);
  CHECK_EQ(0x83828180, t.ldr);
}

TEST(unaligned_stores) {
  // All supported ARM targets allow unaligned accesses.
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);
  __ strh(r3, MemOperand(r0, r2));
  __ str(r3, MemOperand(r1, r2));
  __ bx(lr);

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  F4 f = FUNCTION_CAST<F4>(code->entry());

  Object* dummy = nullptr;
  USE(dummy);

#ifndef V8_TARGET_LITTLE_ENDIAN
#error This test assumes a little-endian layout.
#endif
  {
    uint64_t strh = 0;
    uint64_t str = 0;
    dummy = CALL_GENERATED_CODE(isolate, f, &strh, &str, 0, 0xfedcba98, 0);
    CHECK_EQ(UINT64_C(0x000000000000ba98), strh);
    CHECK_EQ(UINT64_C(0x00000000fedcba98), str);
  }
  {
    uint64_t strh = 0;
    uint64_t str = 0;
    dummy = CALL_GENERATED_CODE(isolate, f, &strh, &str, 1, 0xfedcba98, 0);
    CHECK_EQ(UINT64_C(0x0000000000ba9800), strh);
    CHECK_EQ(UINT64_C(0x000000fedcba9800), str);
  }
  {
    uint64_t strh = 0;
    uint64_t str = 0;
    dummy = CALL_GENERATED_CODE(isolate, f, &strh, &str, 2, 0xfedcba98, 0);
    CHECK_EQ(UINT64_C(0x00000000ba980000), strh);
    CHECK_EQ(UINT64_C(0x0000fedcba980000), str);
  }
  {
    uint64_t strh = 0;
    uint64_t str = 0;
    dummy = CALL_GENERATED_CODE(isolate, f, &strh, &str, 3, 0xfedcba98, 0);
    CHECK_EQ(UINT64_C(0x000000ba98000000), strh);
    CHECK_EQ(UINT64_C(0x00fedcba98000000), str);
  }
}

TEST(vswp) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  Assembler assm(isolate, NULL, 0);

  typedef struct {
    double result0;
    double result1;
    double result2;
    double result3;
  } T;
  T t;

  __ vmov(d0, 1.0);
  __ vmov(d1, -1.0);
  __ vswp(d0, d1);
  __ vstr(d0, r0, offsetof(T, result0));
  __ vstr(d1, r0, offsetof(T, result1));

  if (CpuFeatures::IsSupported(VFP32DREGS)) {
    __ vmov(d30, 1.0);
    __ vmov(d31, -1.0);
    __ vswp(d30, d31);
    __ vstr(d30, r0, offsetof(T, result2));
    __ vstr(d31, r0, offsetof(T, result3));
  }

  __ bx(lr);

  CodeDesc desc;
  assm.GetCode(&desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::ComputeFlags(Code::STUB), Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  F3 f = FUNCTION_CAST<F3>(code->entry());
  Object* dummy = CALL_GENERATED_CODE(isolate, f, &t, 0, 0, 0, 0);
  USE(dummy);
  CHECK_EQ(-1.0, t.result0);
  CHECK_EQ(1.0, t.result1);
  if (CpuFeatures::IsSupported(VFP32DREGS)) {
    CHECK_EQ(-1.0, t.result2);
    CHECK_EQ(1.0, t.result3);
  }
}

TEST(regress4292_b) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);
  Label end;
  __ mov(r0, Operand(isolate->factory()->infinity_value()));
  for (int i = 0; i < 1020; ++i) {
    __ b(hi, &end);
  }
  __ bind(&end);
}


TEST(regress4292_bl) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);
  Label end;
  __ mov(r0, Operand(isolate->factory()->infinity_value()));
  for (int i = 0; i < 1020; ++i) {
    __ bl(hi, &end);
  }
  __ bind(&end);
}


TEST(regress4292_blx) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);
  Label end;
  __ mov(r0, Operand(isolate->factory()->infinity_value()));
  for (int i = 0; i < 1020; ++i) {
    __ blx(&end);
  }
  __ bind(&end);
}


TEST(regress4292_CheckConstPool) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  Assembler assm(isolate, NULL, 0);
  __ mov(r0, Operand(isolate->factory()->infinity_value()));
  __ BlockConstPoolFor(1019);
  for (int i = 0; i < 1019; ++i) __ nop();
  __ vldr(d0, MemOperand(r0, 0));
}

#undef __
