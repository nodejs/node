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

#include "v8.h"

#include "disassembler.h"
#include "factory.h"
#include "arm/simulator-arm.h"
#include "arm/assembler-arm-inl.h"
#include "cctest.h"

using namespace v8::internal;


// Define these function prototypes to match JSEntryFunction in execution.cc.
typedef Object* (*F1)(int x, int p1, int p2, int p3, int p4);
typedef Object* (*F2)(int x, int y, int p2, int p3, int p4);
typedef Object* (*F3)(void* p0, int p1, int p2, int p3, int p4);
typedef Object* (*F4)(void* p0, void* p1, int p2, int p3, int p4);


static v8::Persistent<v8::Context> env;


static void InitializeVM() {
  if (env.IsEmpty()) {
    env = v8::Context::New();
  }
}


#define __ assm.

TEST(0) {
  InitializeVM();
  v8::HandleScope scope;

  Assembler assm(Isolate::Current(), NULL, 0);

  __ add(r0, r0, Operand(r1));
  __ mov(pc, Operand(lr));

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = HEAP->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      Handle<Object>(HEAP->undefined_value()))->ToObjectChecked();
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F2 f = FUNCTION_CAST<F2>(Code::cast(code)->entry());
  int res = reinterpret_cast<int>(CALL_GENERATED_CODE(f, 3, 4, 0, 0, 0));
  ::printf("f() = %d\n", res);
  CHECK_EQ(7, res);
}


TEST(1) {
  InitializeVM();
  v8::HandleScope scope;

  Assembler assm(Isolate::Current(), NULL, 0);
  Label L, C;

  __ mov(r1, Operand(r0));
  __ mov(r0, Operand(0, RelocInfo::NONE));
  __ b(&C);

  __ bind(&L);
  __ add(r0, r0, Operand(r1));
  __ sub(r1, r1, Operand(1));

  __ bind(&C);
  __ teq(r1, Operand(0, RelocInfo::NONE));
  __ b(ne, &L);
  __ mov(pc, Operand(lr));

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = HEAP->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      Handle<Object>(HEAP->undefined_value()))->ToObjectChecked();
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F1 f = FUNCTION_CAST<F1>(Code::cast(code)->entry());
  int res = reinterpret_cast<int>(CALL_GENERATED_CODE(f, 100, 0, 0, 0, 0));
  ::printf("f() = %d\n", res);
  CHECK_EQ(5050, res);
}


TEST(2) {
  InitializeVM();
  v8::HandleScope scope;

  Assembler assm(Isolate::Current(), NULL, 0);
  Label L, C;

  __ mov(r1, Operand(r0));
  __ mov(r0, Operand(1));
  __ b(&C);

  __ bind(&L);
  __ mul(r0, r1, r0);
  __ sub(r1, r1, Operand(1));

  __ bind(&C);
  __ teq(r1, Operand(0, RelocInfo::NONE));
  __ b(ne, &L);
  __ mov(pc, Operand(lr));

  // some relocated stuff here, not executed
  __ RecordComment("dead code, just testing relocations");
  __ mov(r0, Operand(FACTORY->true_value()));
  __ RecordComment("dead code, just testing immediate operands");
  __ mov(r0, Operand(-1));
  __ mov(r0, Operand(0xFF000000));
  __ mov(r0, Operand(0xF0F0F0F0));
  __ mov(r0, Operand(0xFFF0FFFF));

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = HEAP->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      Handle<Object>(HEAP->undefined_value()))->ToObjectChecked();
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F1 f = FUNCTION_CAST<F1>(Code::cast(code)->entry());
  int res = reinterpret_cast<int>(CALL_GENERATED_CODE(f, 10, 0, 0, 0, 0));
  ::printf("f() = %d\n", res);
  CHECK_EQ(3628800, res);
}


TEST(3) {
  InitializeVM();
  v8::HandleScope scope;

  typedef struct {
    int i;
    char c;
    int16_t s;
  } T;
  T t;

  Assembler assm(Isolate::Current(), NULL, 0);
  Label L, C;

  __ mov(ip, Operand(sp));
  __ stm(db_w, sp, r4.bit() | fp.bit() | lr.bit());
  __ sub(fp, ip, Operand(4));
  __ mov(r4, Operand(r0));
  __ ldr(r0, MemOperand(r4, OFFSET_OF(T, i)));
  __ mov(r2, Operand(r0, ASR, 1));
  __ str(r2, MemOperand(r4, OFFSET_OF(T, i)));
  __ ldrsb(r2, MemOperand(r4, OFFSET_OF(T, c)));
  __ add(r0, r2, Operand(r0));
  __ mov(r2, Operand(r2, LSL, 2));
  __ strb(r2, MemOperand(r4, OFFSET_OF(T, c)));
  __ ldrsh(r2, MemOperand(r4, OFFSET_OF(T, s)));
  __ add(r0, r2, Operand(r0));
  __ mov(r2, Operand(r2, ASR, 3));
  __ strh(r2, MemOperand(r4, OFFSET_OF(T, s)));
  __ ldm(ia_w, sp, r4.bit() | fp.bit() | pc.bit());

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = HEAP->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      Handle<Object>(HEAP->undefined_value()))->ToObjectChecked();
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F3 f = FUNCTION_CAST<F3>(Code::cast(code)->entry());
  t.i = 100000;
  t.c = 10;
  t.s = 1000;
  int res = reinterpret_cast<int>(CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0));
  ::printf("f() = %d\n", res);
  CHECK_EQ(101010, res);
  CHECK_EQ(100000/2, t.i);
  CHECK_EQ(10*4, t.c);
  CHECK_EQ(1000/8, t.s);
}


TEST(4) {
  // Test the VFP floating point instructions.
  InitializeVM();
  v8::HandleScope scope;

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
    double m;
    double n;
    float x;
    float y;
  } T;
  T t;

  // Create a function that accepts &t, and loads, manipulates, and stores
  // the doubles and floats.
  Assembler assm(Isolate::Current(), NULL, 0);
  Label L, C;


  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatures::Scope scope(VFP3);

    __ mov(ip, Operand(sp));
    __ stm(db_w, sp, r4.bit() | fp.bit() | lr.bit());
    __ sub(fp, ip, Operand(4));

    __ mov(r4, Operand(r0));
    __ vldr(d6, r4, OFFSET_OF(T, a));
    __ vldr(d7, r4, OFFSET_OF(T, b));
    __ vadd(d5, d6, d7);
    __ vstr(d5, r4, OFFSET_OF(T, c));

    __ vmov(r2, r3, d5);
    __ vmov(d4, r2, r3);
    __ vstr(d4, r4, OFFSET_OF(T, b));

    // Load t.x and t.y, switch values, and store back to the struct.
    __ vldr(s0, r4, OFFSET_OF(T, x));
    __ vldr(s31, r4, OFFSET_OF(T, y));
    __ vmov(s16, s0);
    __ vmov(s0, s31);
    __ vmov(s31, s16);
    __ vstr(s0, r4, OFFSET_OF(T, x));
    __ vstr(s31, r4, OFFSET_OF(T, y));

    // Move a literal into a register that can be encoded in the instruction.
    __ vmov(d4, 1.0);
    __ vstr(d4, r4, OFFSET_OF(T, e));

    // Move a literal into a register that requires 64 bits to encode.
    // 0x3ff0000010000000 = 1.000000059604644775390625
    __ vmov(d4, 1.000000059604644775390625);
    __ vstr(d4, r4, OFFSET_OF(T, d));

    // Convert from floating point to integer.
    __ vmov(d4, 2.0);
    __ vcvt_s32_f64(s31, d4);
    __ vstr(s31, r4, OFFSET_OF(T, i));

    // Convert from integer to floating point.
    __ mov(lr, Operand(42));
    __ vmov(s31, lr);
    __ vcvt_f64_s32(d4, s31);
    __ vstr(d4, r4, OFFSET_OF(T, f));

    // Test vabs.
    __ vldr(d1, r4, OFFSET_OF(T, g));
    __ vabs(d0, d1);
    __ vstr(d0, r4, OFFSET_OF(T, g));
    __ vldr(d2, r4, OFFSET_OF(T, h));
    __ vabs(d0, d2);
    __ vstr(d0, r4, OFFSET_OF(T, h));

    // Test vneg.
    __ vldr(d1, r4, OFFSET_OF(T, m));
    __ vneg(d0, d1);
    __ vstr(d0, r4, OFFSET_OF(T, m));
    __ vldr(d1, r4, OFFSET_OF(T, n));
    __ vneg(d0, d1);
    __ vstr(d0, r4, OFFSET_OF(T, n));

    __ ldm(ia_w, sp, r4.bit() | fp.bit() | pc.bit());

    CodeDesc desc;
    assm.GetCode(&desc);
    Object* code = HEAP->CreateCode(
        desc,
        Code::ComputeFlags(Code::STUB),
        Handle<Object>(HEAP->undefined_value()))->ToObjectChecked();
    CHECK(code->IsCode());
#ifdef DEBUG
    Code::cast(code)->Print();
#endif
    F3 f = FUNCTION_CAST<F3>(Code::cast(code)->entry());
    t.a = 1.5;
    t.b = 2.75;
    t.c = 17.17;
    t.d = 0.0;
    t.e = 0.0;
    t.f = 0.0;
    t.g = -2718.2818;
    t.h = 31415926.5;
    t.i = 0;
    t.m = -2718.2818;
    t.n = 123.456;
    t.x = 4.5;
    t.y = 9.0;
    Object* dummy = CALL_GENERATED_CODE(f, &t, 0, 0, 0, 0);
    USE(dummy);
    CHECK_EQ(4.5, t.y);
    CHECK_EQ(9.0, t.x);
    CHECK_EQ(-123.456, t.n);
    CHECK_EQ(2718.2818, t.m);
    CHECK_EQ(2, t.i);
    CHECK_EQ(2718.2818, t.g);
    CHECK_EQ(31415926.5, t.h);
    CHECK_EQ(42.0, t.f);
    CHECK_EQ(1.0, t.e);
    CHECK_EQ(1.000000059604644775390625, t.d);
    CHECK_EQ(4.25, t.c);
    CHECK_EQ(4.25, t.b);
    CHECK_EQ(1.5, t.a);
  }
}


TEST(5) {
  // Test the ARMv7 bitfield instructions.
  InitializeVM();
  v8::HandleScope scope;

  Assembler assm(Isolate::Current(), NULL, 0);

  if (CpuFeatures::IsSupported(ARMv7)) {
    CpuFeatures::Scope scope(ARMv7);
    // On entry, r0 = 0xAAAAAAAA = 0b10..10101010.
    __ ubfx(r0, r0, 1, 12);  // 0b00..010101010101 = 0x555
    __ sbfx(r0, r0, 0, 5);   // 0b11..111111110101 = -11
    __ bfc(r0, 1, 3);        // 0b11..111111110001 = -15
    __ mov(r1, Operand(7));
    __ bfi(r0, r1, 3, 3);    // 0b11..111111111001 = -7
    __ mov(pc, Operand(lr));

    CodeDesc desc;
    assm.GetCode(&desc);
    Object* code = HEAP->CreateCode(
        desc,
        Code::ComputeFlags(Code::STUB),
        Handle<Object>(HEAP->undefined_value()))->ToObjectChecked();
    CHECK(code->IsCode());
#ifdef DEBUG
    Code::cast(code)->Print();
#endif
    F1 f = FUNCTION_CAST<F1>(Code::cast(code)->entry());
    int res = reinterpret_cast<int>(
                CALL_GENERATED_CODE(f, 0xAAAAAAAA, 0, 0, 0, 0));
    ::printf("f() = %d\n", res);
    CHECK_EQ(-7, res);
  }
}


TEST(6) {
  // Test saturating instructions.
  InitializeVM();
  v8::HandleScope scope;

  Assembler assm(Isolate::Current(), NULL, 0);

  if (CpuFeatures::IsSupported(ARMv7)) {
    CpuFeatures::Scope scope(ARMv7);
    __ usat(r1, 8, Operand(r0));           // Sat 0xFFFF to 0-255 = 0xFF.
    __ usat(r2, 12, Operand(r0, ASR, 9));  // Sat (0xFFFF>>9) to 0-4095 = 0x7F.
    __ usat(r3, 1, Operand(r0, LSL, 16));  // Sat (0xFFFF<<16) to 0-1 = 0x0.
    __ add(r0, r1, Operand(r2));
    __ add(r0, r0, Operand(r3));
    __ mov(pc, Operand(lr));

    CodeDesc desc;
    assm.GetCode(&desc);
    Object* code = HEAP->CreateCode(
        desc,
        Code::ComputeFlags(Code::STUB),
        Handle<Object>(HEAP->undefined_value()))->ToObjectChecked();
    CHECK(code->IsCode());
#ifdef DEBUG
    Code::cast(code)->Print();
#endif
    F1 f = FUNCTION_CAST<F1>(Code::cast(code)->entry());
    int res = reinterpret_cast<int>(
                CALL_GENERATED_CODE(f, 0xFFFF, 0, 0, 0, 0));
    ::printf("f() = %d\n", res);
    CHECK_EQ(382, res);
  }
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
  InitializeVM();
  v8::HandleScope scope;

  Assembler assm(Isolate::Current(), NULL, 0);

  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatures::Scope scope(VFP3);

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
    __ b(&wrong_exception,
         expected_exception ? eq : ne);
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
    Object* code = HEAP->CreateCode(
        desc,
        Code::ComputeFlags(Code::STUB),
        Handle<Object>(HEAP->undefined_value()))->ToObjectChecked();
    CHECK(code->IsCode());
#ifdef DEBUG
    Code::cast(code)->Print();
#endif
    F1 f = FUNCTION_CAST<F1>(Code::cast(code)->entry());
    int res = reinterpret_cast<int>(
                CALL_GENERATED_CODE(f, 0, 0, 0, 0, 0));
    ::printf("res = %d\n", res);
    CHECK_EQ(expected, res);
  }
}


TEST(7) {
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
  InitializeVM();
  v8::HandleScope scope;

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
  Assembler assm(Isolate::Current(), NULL, 0);

  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatures::Scope scope(VFP3);

    __ mov(ip, Operand(sp));
    __ stm(db_w, sp, r4.bit() | fp.bit() | lr.bit());
    __ sub(fp, ip, Operand(4));

    __ add(r4, r0, Operand(OFFSET_OF(D, a)));
    __ vldm(ia_w, r4, d0, d3);
    __ vldm(ia_w, r4, d4, d7);

    __ add(r4, r0, Operand(OFFSET_OF(D, a)));
    __ vstm(ia_w, r4, d6, d7);
    __ vstm(ia_w, r4, d0, d5);

    __ add(r4, r1, Operand(OFFSET_OF(F, a)));
    __ vldm(ia_w, r4, s0, s3);
    __ vldm(ia_w, r4, s4, s7);

    __ add(r4, r1, Operand(OFFSET_OF(F, a)));
    __ vstm(ia_w, r4, s6, s7);
    __ vstm(ia_w, r4, s0, s5);

    __ ldm(ia_w, sp, r4.bit() | fp.bit() | pc.bit());

    CodeDesc desc;
    assm.GetCode(&desc);
    Object* code = HEAP->CreateCode(
        desc,
        Code::ComputeFlags(Code::STUB),
        Handle<Object>(HEAP->undefined_value()))->ToObjectChecked();
    CHECK(code->IsCode());
#ifdef DEBUG
    Code::cast(code)->Print();
#endif
    F4 fn = FUNCTION_CAST<F4>(Code::cast(code)->entry());
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

    Object* dummy = CALL_GENERATED_CODE(fn, &d, &f, 0, 0, 0);
    USE(dummy);

    CHECK_EQ(7.7, d.a);
    CHECK_EQ(8.8, d.b);
    CHECK_EQ(1.1, d.c);
    CHECK_EQ(2.2, d.d);
    CHECK_EQ(3.3, d.e);
    CHECK_EQ(4.4, d.f);
    CHECK_EQ(5.5, d.g);
    CHECK_EQ(6.6, d.h);

    CHECK_EQ(7.0, f.a);
    CHECK_EQ(8.0, f.b);
    CHECK_EQ(1.0, f.c);
    CHECK_EQ(2.0, f.d);
    CHECK_EQ(3.0, f.e);
    CHECK_EQ(4.0, f.f);
    CHECK_EQ(5.0, f.g);
    CHECK_EQ(6.0, f.h);
  }
}


TEST(9) {
  // Test VFP multi load/store with ia.
  InitializeVM();
  v8::HandleScope scope;

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
  Assembler assm(Isolate::Current(), NULL, 0);

  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatures::Scope scope(VFP3);

    __ mov(ip, Operand(sp));
    __ stm(db_w, sp, r4.bit() | fp.bit() | lr.bit());
    __ sub(fp, ip, Operand(4));

    __ add(r4, r0, Operand(OFFSET_OF(D, a)));
    __ vldm(ia, r4, d0, d3);
    __ add(r4, r4, Operand(4 * 8));
    __ vldm(ia, r4, d4, d7);

    __ add(r4, r0, Operand(OFFSET_OF(D, a)));
    __ vstm(ia, r4, d6, d7);
    __ add(r4, r4, Operand(2 * 8));
    __ vstm(ia, r4, d0, d5);

    __ add(r4, r1, Operand(OFFSET_OF(F, a)));
    __ vldm(ia, r4, s0, s3);
    __ add(r4, r4, Operand(4 * 4));
    __ vldm(ia, r4, s4, s7);

    __ add(r4, r1, Operand(OFFSET_OF(F, a)));
    __ vstm(ia, r4, s6, s7);
    __ add(r4, r4, Operand(2 * 4));
    __ vstm(ia, r4, s0, s5);

    __ ldm(ia_w, sp, r4.bit() | fp.bit() | pc.bit());

    CodeDesc desc;
    assm.GetCode(&desc);
    Object* code = HEAP->CreateCode(
        desc,
        Code::ComputeFlags(Code::STUB),
        Handle<Object>(HEAP->undefined_value()))->ToObjectChecked();
    CHECK(code->IsCode());
#ifdef DEBUG
    Code::cast(code)->Print();
#endif
    F4 fn = FUNCTION_CAST<F4>(Code::cast(code)->entry());
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

    Object* dummy = CALL_GENERATED_CODE(fn, &d, &f, 0, 0, 0);
    USE(dummy);

    CHECK_EQ(7.7, d.a);
    CHECK_EQ(8.8, d.b);
    CHECK_EQ(1.1, d.c);
    CHECK_EQ(2.2, d.d);
    CHECK_EQ(3.3, d.e);
    CHECK_EQ(4.4, d.f);
    CHECK_EQ(5.5, d.g);
    CHECK_EQ(6.6, d.h);

    CHECK_EQ(7.0, f.a);
    CHECK_EQ(8.0, f.b);
    CHECK_EQ(1.0, f.c);
    CHECK_EQ(2.0, f.d);
    CHECK_EQ(3.0, f.e);
    CHECK_EQ(4.0, f.f);
    CHECK_EQ(5.0, f.g);
    CHECK_EQ(6.0, f.h);
  }
}


TEST(10) {
  // Test VFP multi load/store with db_w.
  InitializeVM();
  v8::HandleScope scope;

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
  Assembler assm(Isolate::Current(), NULL, 0);

  if (CpuFeatures::IsSupported(VFP3)) {
    CpuFeatures::Scope scope(VFP3);

    __ mov(ip, Operand(sp));
    __ stm(db_w, sp, r4.bit() | fp.bit() | lr.bit());
    __ sub(fp, ip, Operand(4));

    __ add(r4, r0, Operand(OFFSET_OF(D, h) + 8));
    __ vldm(db_w, r4, d4, d7);
    __ vldm(db_w, r4, d0, d3);

    __ add(r4, r0, Operand(OFFSET_OF(D, h) + 8));
    __ vstm(db_w, r4, d0, d5);
    __ vstm(db_w, r4, d6, d7);

    __ add(r4, r1, Operand(OFFSET_OF(F, h) + 4));
    __ vldm(db_w, r4, s4, s7);
    __ vldm(db_w, r4, s0, s3);

    __ add(r4, r1, Operand(OFFSET_OF(F, h) + 4));
    __ vstm(db_w, r4, s0, s5);
    __ vstm(db_w, r4, s6, s7);

    __ ldm(ia_w, sp, r4.bit() | fp.bit() | pc.bit());

    CodeDesc desc;
    assm.GetCode(&desc);
    Object* code = HEAP->CreateCode(
        desc,
        Code::ComputeFlags(Code::STUB),
        Handle<Object>(HEAP->undefined_value()))->ToObjectChecked();
    CHECK(code->IsCode());
#ifdef DEBUG
    Code::cast(code)->Print();
#endif
    F4 fn = FUNCTION_CAST<F4>(Code::cast(code)->entry());
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

    Object* dummy = CALL_GENERATED_CODE(fn, &d, &f, 0, 0, 0);
    USE(dummy);

    CHECK_EQ(7.7, d.a);
    CHECK_EQ(8.8, d.b);
    CHECK_EQ(1.1, d.c);
    CHECK_EQ(2.2, d.d);
    CHECK_EQ(3.3, d.e);
    CHECK_EQ(4.4, d.f);
    CHECK_EQ(5.5, d.g);
    CHECK_EQ(6.6, d.h);

    CHECK_EQ(7.0, f.a);
    CHECK_EQ(8.0, f.b);
    CHECK_EQ(1.0, f.c);
    CHECK_EQ(2.0, f.d);
    CHECK_EQ(3.0, f.e);
    CHECK_EQ(4.0, f.f);
    CHECK_EQ(5.0, f.g);
    CHECK_EQ(6.0, f.h);
  }
}


TEST(11) {
  // Test instructions using the carry flag.
  InitializeVM();
  v8::HandleScope scope;

  typedef struct {
    int32_t a;
    int32_t b;
    int32_t c;
    int32_t d;
  } I;
  I i;

  i.a = 0xabcd0001;
  i.b = 0xabcd0000;

  Assembler assm(Isolate::Current(), NULL, 0);

  // Test HeapObject untagging.
  __ ldr(r1, MemOperand(r0, OFFSET_OF(I, a)));
  __ mov(r1, Operand(r1, ASR, 1), SetCC);
  __ adc(r1, r1, Operand(r1), LeaveCC, cs);
  __ str(r1, MemOperand(r0, OFFSET_OF(I, a)));

  __ ldr(r2, MemOperand(r0, OFFSET_OF(I, b)));
  __ mov(r2, Operand(r2, ASR, 1), SetCC);
  __ adc(r2, r2, Operand(r2), LeaveCC, cs);
  __ str(r2, MemOperand(r0, OFFSET_OF(I, b)));

  // Test corner cases.
  __ mov(r1, Operand(0xffffffff));
  __ mov(r2, Operand(0));
  __ mov(r3, Operand(r1, ASR, 1), SetCC);  // Set the carry.
  __ adc(r3, r1, Operand(r2));
  __ str(r3, MemOperand(r0, OFFSET_OF(I, c)));

  __ mov(r1, Operand(0xffffffff));
  __ mov(r2, Operand(0));
  __ mov(r3, Operand(r2, ASR, 1), SetCC);  // Unset the carry.
  __ adc(r3, r1, Operand(r2));
  __ str(r3, MemOperand(r0, OFFSET_OF(I, d)));

  __ mov(pc, Operand(lr));

  CodeDesc desc;
  assm.GetCode(&desc);
  Object* code = HEAP->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      Handle<Object>(HEAP->undefined_value()))->ToObjectChecked();
  CHECK(code->IsCode());
#ifdef DEBUG
  Code::cast(code)->Print();
#endif
  F3 f = FUNCTION_CAST<F3>(Code::cast(code)->entry());
  Object* dummy = CALL_GENERATED_CODE(f, &i, 0, 0, 0, 0);
  USE(dummy);

  CHECK_EQ(0xabcd0001, i.a);
  CHECK_EQ(static_cast<int32_t>(0xabcd0000) >> 1, i.b);
  CHECK_EQ(0x00000000, i.c);
  CHECK_EQ(0xffffffff, i.d);
}


TEST(12) {
  // Test chaining of label usages within instructions (issue 1644).
  InitializeVM();
  v8::HandleScope scope;
  Assembler assm(Isolate::Current(), NULL, 0);

  Label target;
  __ b(eq, &target);
  __ b(ne, &target);
  __ bind(&target);
  __ nop();
}

#undef __
