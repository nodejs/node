// Copyright 2009 the V8 project authors. All rights reserved.
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

#include <cstdlib>
#include <iostream>

#include "src/v8.h"

#include "src/base/platform/platform.h"
#include "src/base/utils/random-number-generator.h"
#include "src/double.h"
#include "src/factory.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/ostreams.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

// Test the x64 assembler by compiling some simple functions into
// a buffer and executing them.  These tests do not initialize the
// V8 library, create a context, or use any V8 objects.
// The AMD64 calling convention is used, with the first six arguments
// in RDI, RSI, RDX, RCX, R8, and R9, and floating point arguments in
// the XMM registers.  The return value is in RAX.
// This calling convention is used on Linux, with GCC, and on Mac OS,
// with GCC.  A different convention is used on 64-bit windows,
// where the first four integer arguments are passed in RCX, RDX, R8 and R9.

typedef int (*F0)();
typedef int (*F1)(int64_t x);
typedef int (*F2)(int64_t x, int64_t y);
typedef unsigned (*F3)(double x);
typedef uint64_t (*F4)(uint64_t* x, uint64_t* y);
typedef uint64_t (*F5)(uint64_t x);

#ifdef _WIN64
static const Register arg1 = rcx;
static const Register arg2 = rdx;
#else
static const Register arg1 = rdi;
static const Register arg2 = rsi;
#endif

#define __ masm.

TEST(AssemblerX64ReturnOperation) {
  CcTest::InitializeVM();
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(CcTest::i_isolate(), buffer, static_cast<int>(allocated));

  // Assemble a simple function that copies argument 2 and returns it.
  __ movq(rax, arg2);
  __ nop();
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(CcTest::i_isolate(), &desc);
  // Call the function from C++.
  int result =  FUNCTION_CAST<F2>(buffer)(3, 2);
  CHECK_EQ(2, result);
}


TEST(AssemblerX64StackOperations) {
  CcTest::InitializeVM();
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(CcTest::i_isolate(), buffer, static_cast<int>(allocated));

  // Assemble a simple function that copies argument 2 and returns it.
  // We compile without stack frame pointers, so the gdb debugger shows
  // incorrect stack frames when debugging this function (which has them).
  __ pushq(rbp);
  __ movq(rbp, rsp);
  __ pushq(arg2);  // Value at (rbp - 8)
  __ pushq(arg2);  // Value at (rbp - 16)
  __ pushq(arg1);  // Value at (rbp - 24)
  __ popq(rax);
  __ popq(rax);
  __ popq(rax);
  __ popq(rbp);
  __ nop();
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(CcTest::i_isolate(), &desc);
  // Call the function from C++.
  int result =  FUNCTION_CAST<F2>(buffer)(3, 2);
  CHECK_EQ(2, result);
}


TEST(AssemblerX64ArithmeticOperations) {
  CcTest::InitializeVM();
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(CcTest::i_isolate(), buffer, static_cast<int>(allocated));

  // Assemble a simple function that adds arguments returning the sum.
  __ movq(rax, arg2);
  __ addq(rax, arg1);
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(CcTest::i_isolate(), &desc);
  // Call the function from C++.
  int result =  FUNCTION_CAST<F2>(buffer)(3, 2);
  CHECK_EQ(5, result);
}


TEST(AssemblerX64CmpbOperation) {
  CcTest::InitializeVM();
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(CcTest::i_isolate(), buffer, static_cast<int>(allocated));

  // Assemble a function that compare argument byte returing 1 if equal else 0.
  // On Windows, it compares rcx with rdx which does not require REX prefix;
  // on Linux, it compares rdi with rsi which requires REX prefix.

  Label done;
  __ movq(rax, Immediate(1));
  __ cmpb(arg1, arg2);
  __ j(equal, &done);
  __ movq(rax, Immediate(0));
  __ bind(&done);
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(CcTest::i_isolate(), &desc);
  // Call the function from C++.
  int result =  FUNCTION_CAST<F2>(buffer)(0x1002, 0x2002);
  CHECK_EQ(1, result);
  result =  FUNCTION_CAST<F2>(buffer)(0x1002, 0x2003);
  CHECK_EQ(0, result);
}

TEST(Regression684407) {
  CcTest::InitializeVM();
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(CcTest::i_isolate(), buffer, static_cast<int>(allocated));

  Address before = masm.pc();
  __ cmpl(Operand(arg1, 0),
          Immediate(0, RelocInfo::WASM_FUNCTION_TABLE_SIZE_REFERENCE));
  Address after = masm.pc();
  size_t instruction_size = static_cast<size_t>(after - before);
  // Check that the immediate is not encoded as uint8.
  CHECK_LT(sizeof(uint32_t), instruction_size);
}

TEST(AssemblerX64ImulOperation) {
  CcTest::InitializeVM();
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(CcTest::i_isolate(), buffer, static_cast<int>(allocated));

  // Assemble a simple function that multiplies arguments returning the high
  // word.
  __ movq(rax, arg2);
  __ imulq(arg1);
  __ movq(rax, rdx);
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(CcTest::i_isolate(), &desc);
  // Call the function from C++.
  int result =  FUNCTION_CAST<F2>(buffer)(3, 2);
  CHECK_EQ(0, result);
  result =  FUNCTION_CAST<F2>(buffer)(0x100000000l, 0x100000000l);
  CHECK_EQ(1, result);
  result =  FUNCTION_CAST<F2>(buffer)(-0x100000000l, 0x100000000l);
  CHECK_EQ(-1, result);
}

TEST(AssemblerX64testbwqOperation) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(CcTest::i_isolate(), buffer, static_cast<int>(allocated));

  __ pushq(rbx);
  __ pushq(rdi);
  __ pushq(rsi);
  __ pushq(r12);
  __ pushq(r13);
  __ pushq(r14);
  __ pushq(r15);

  // Assemble a simple function that tests testb and testw
  Label bad;
  Label done;

  // Test immediate testb and testw
  __ movq(rax, Immediate(2));
  __ movq(rbx, Immediate(4));
  __ movq(rcx, Immediate(8));
  __ movq(rdx, Immediate(16));
  __ movq(rsi, Immediate(32));
  __ movq(rdi, Immediate(64));
  __ movq(r10, Immediate(128));
  __ movq(r11, Immediate(0));
  __ movq(r12, Immediate(0));
  __ movq(r13, Immediate(0));
  __ testb(rax, Immediate(2));
  __ j(zero, &bad);
  __ testb(rbx, Immediate(4));
  __ j(zero, &bad);
  __ testb(rcx, Immediate(8));
  __ j(zero, &bad);
  __ testb(rdx, Immediate(16));
  __ j(zero, &bad);
  __ testb(rsi, Immediate(32));
  __ j(zero, &bad);
  __ testb(rdi, Immediate(64));
  __ j(zero, &bad);
  __ testb(r10, Immediate(128));
  __ j(zero, &bad);
  __ testw(rax, Immediate(2));
  __ j(zero, &bad);
  __ testw(rbx, Immediate(4));
  __ j(zero, &bad);
  __ testw(rcx, Immediate(8));
  __ j(zero, &bad);
  __ testw(rdx, Immediate(16));
  __ j(zero, &bad);
  __ testw(rsi, Immediate(32));
  __ j(zero, &bad);
  __ testw(rdi, Immediate(64));
  __ j(zero, &bad);
  __ testw(r10, Immediate(128));
  __ j(zero, &bad);

  // Test reg, reg testb and testw
  __ movq(rax, Immediate(2));
  __ movq(rbx, Immediate(2));
  __ testb(rax, rbx);
  __ j(zero, &bad);
  __ movq(rbx, Immediate(4));
  __ movq(rax, Immediate(4));
  __ testb(rbx, rax);
  __ j(zero, &bad);
  __ movq(rax, Immediate(8));
  __ testb(rcx, rax);
  __ j(zero, &bad);
  __ movq(rax, Immediate(16));
  __ testb(rdx, rax);
  __ j(zero, &bad);
  __ movq(rax, Immediate(32));
  __ testb(rsi, rax);
  __ j(zero, &bad);
  __ movq(rax, Immediate(64));
  __ testb(rdi, rax);
  __ j(zero, &bad);
  __ movq(rax, Immediate(128));
  __ testb(r10, rax);
  __ j(zero, &bad);
  __ movq(rax, Immediate(2));
  __ movq(rbx, Immediate(2));
  __ testw(rax, rbx);
  __ j(zero, &bad);
  __ movq(rbx, Immediate(4));
  __ movq(rax, Immediate(4));
  __ testw(rbx, rax);
  __ j(zero, &bad);
  __ movq(rax, Immediate(8));
  __ testw(rcx, rax);
  __ j(zero, &bad);
  __ movq(rax, Immediate(16));
  __ testw(rdx, rax);
  __ j(zero, &bad);
  __ movq(rax, Immediate(32));
  __ testw(rsi, rax);
  __ j(zero, &bad);
  __ movq(rax, Immediate(64));
  __ testw(rdi, rax);
  __ j(zero, &bad);
  __ movq(rax, Immediate(128));
  __ testw(r10, rax);
  __ j(zero, &bad);

  // Test diffrrent extended register coding combinations.
  __ movq(rax, Immediate(5));
  __ movq(r11, Immediate(5));
  __ testb(r11, rax);
  __ j(zero, &bad);
  __ testb(rax, r11);
  __ j(zero, &bad);
  __ testw(r11, rax);
  __ j(zero, &bad);
  __ testw(rax, r11);
  __ j(zero, &bad);
  __ movq(r11, Immediate(3));
  __ movq(r12, Immediate(3));
  __ movq(rdi, Immediate(3));
  __ testb(r12, rdi);
  __ j(zero, &bad);
  __ testb(rdi, r12);
  __ j(zero, &bad);
  __ testb(r12, r11);
  __ j(zero, &bad);
  __ testb(r11, r12);
  __ j(zero, &bad);
  __ testw(r12, r11);
  __ j(zero, &bad);
  __ testw(r11, r12);
  __ j(zero, &bad);

  // Test sign-extended imediate tests
  __ movq(r11, Immediate(2));
  __ shlq(r11, Immediate(32));
  __ testq(r11, Immediate(-1));
  __ j(zero, &bad);

  // All tests passed
  __ movq(rax, Immediate(1));
  __ jmp(&done);

  __ bind(&bad);
  __ movq(rax, Immediate(0));
  __ bind(&done);

  __ popq(r15);
  __ popq(r14);
  __ popq(r13);
  __ popq(r12);
  __ popq(rsi);
  __ popq(rdi);
  __ popq(rbx);

  __ ret(0);

  CodeDesc desc;
  masm.GetCode(CcTest::i_isolate(), &desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F2>(buffer)(0, 0);
  CHECK_EQ(1, result);
}

TEST(AssemblerX64XchglOperations) {
  CcTest::InitializeVM();
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(CcTest::i_isolate(), buffer, static_cast<int>(allocated));

  __ movq(rax, Operand(arg1, 0));
  __ movq(r11, Operand(arg2, 0));
  __ xchgl(rax, r11);
  __ movq(Operand(arg1, 0), rax);
  __ movq(Operand(arg2, 0), r11);
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(CcTest::i_isolate(), &desc);
  // Call the function from C++.
  uint64_t left = V8_2PART_UINT64_C(0x10000000, 20000000);
  uint64_t right = V8_2PART_UINT64_C(0x30000000, 40000000);
  uint64_t result = FUNCTION_CAST<F4>(buffer)(&left, &right);
  CHECK_EQ(V8_2PART_UINT64_C(0x00000000, 40000000), left);
  CHECK_EQ(V8_2PART_UINT64_C(0x00000000, 20000000), right);
  USE(result);
}


TEST(AssemblerX64OrlOperations) {
  CcTest::InitializeVM();
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(CcTest::i_isolate(), buffer, static_cast<int>(allocated));

  __ movq(rax, Operand(arg2, 0));
  __ orl(Operand(arg1, 0), rax);
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(CcTest::i_isolate(), &desc);
  // Call the function from C++.
  uint64_t left = V8_2PART_UINT64_C(0x10000000, 20000000);
  uint64_t right = V8_2PART_UINT64_C(0x30000000, 40000000);
  uint64_t result = FUNCTION_CAST<F4>(buffer)(&left, &right);
  CHECK_EQ(V8_2PART_UINT64_C(0x10000000, 60000000), left);
  USE(result);
}


TEST(AssemblerX64RollOperations) {
  CcTest::InitializeVM();
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(CcTest::i_isolate(), buffer, static_cast<int>(allocated));

  __ movq(rax, arg1);
  __ roll(rax, Immediate(1));
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(CcTest::i_isolate(), &desc);
  // Call the function from C++.
  uint64_t src = V8_2PART_UINT64_C(0x10000000, C0000000);
  uint64_t result = FUNCTION_CAST<F5>(buffer)(src);
  CHECK_EQ(V8_2PART_UINT64_C(0x00000000, 80000001), result);
}


TEST(AssemblerX64SublOperations) {
  CcTest::InitializeVM();
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(CcTest::i_isolate(), buffer, static_cast<int>(allocated));

  __ movq(rax, Operand(arg2, 0));
  __ subl(Operand(arg1, 0), rax);
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(CcTest::i_isolate(), &desc);
  // Call the function from C++.
  uint64_t left = V8_2PART_UINT64_C(0x10000000, 20000000);
  uint64_t right = V8_2PART_UINT64_C(0x30000000, 40000000);
  uint64_t result = FUNCTION_CAST<F4>(buffer)(&left, &right);
  CHECK_EQ(V8_2PART_UINT64_C(0x10000000, e0000000), left);
  USE(result);
}


TEST(AssemblerX64TestlOperations) {
  CcTest::InitializeVM();
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(CcTest::i_isolate(), buffer, static_cast<int>(allocated));

  // Set rax with the ZF flag of the testl instruction.
  Label done;
  __ movq(rax, Immediate(1));
  __ movq(r11, Operand(arg2, 0));
  __ testl(Operand(arg1, 0), r11);
  __ j(zero, &done, Label::kNear);
  __ movq(rax, Immediate(0));
  __ bind(&done);
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(CcTest::i_isolate(), &desc);
  // Call the function from C++.
  uint64_t left = V8_2PART_UINT64_C(0x10000000, 20000000);
  uint64_t right = V8_2PART_UINT64_C(0x30000000, 00000000);
  uint64_t result = FUNCTION_CAST<F4>(buffer)(&left, &right);
  CHECK_EQ(1u, result);
}

TEST(AssemblerX64TestwOperations) {
  typedef uint16_t (*F)(uint16_t * x);
  CcTest::InitializeVM();
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(CcTest::i_isolate(), buffer, static_cast<int>(allocated));

  // Set rax with the ZF flag of the testl instruction.
  Label done;
  __ movq(rax, Immediate(1));
  __ testw(Operand(arg1, 0), Immediate(0xf0f0));
  __ j(not_zero, &done, Label::kNear);
  __ movq(rax, Immediate(0));
  __ bind(&done);
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(CcTest::i_isolate(), &desc);
  // Call the function from C++.
  uint16_t operand = 0x8000;
  uint16_t result = FUNCTION_CAST<F>(buffer)(&operand);
  CHECK_EQ(1u, result);
}

TEST(AssemblerX64XorlOperations) {
  CcTest::InitializeVM();
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(CcTest::i_isolate(), buffer, static_cast<int>(allocated));

  __ movq(rax, Operand(arg2, 0));
  __ xorl(Operand(arg1, 0), rax);
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(CcTest::i_isolate(), &desc);
  // Call the function from C++.
  uint64_t left = V8_2PART_UINT64_C(0x10000000, 20000000);
  uint64_t right = V8_2PART_UINT64_C(0x30000000, 60000000);
  uint64_t result = FUNCTION_CAST<F4>(buffer)(&left, &right);
  CHECK_EQ(V8_2PART_UINT64_C(0x10000000, 40000000), left);
  USE(result);
}


TEST(AssemblerX64MemoryOperands) {
  CcTest::InitializeVM();
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(CcTest::i_isolate(), buffer, static_cast<int>(allocated));

  // Assemble a simple function that copies argument 2 and returns it.
  __ pushq(rbp);
  __ movq(rbp, rsp);

  __ pushq(arg2);  // Value at (rbp - 8)
  __ pushq(arg2);  // Value at (rbp - 16)
  __ pushq(arg1);  // Value at (rbp - 24)

  const int kStackElementSize = 8;
  __ movq(rax, Operand(rbp, -3 * kStackElementSize));
  __ popq(arg2);
  __ popq(arg2);
  __ popq(arg2);
  __ popq(rbp);
  __ nop();
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(CcTest::i_isolate(), &desc);
  // Call the function from C++.
  int result =  FUNCTION_CAST<F2>(buffer)(3, 2);
  CHECK_EQ(3, result);
}


TEST(AssemblerX64ControlFlow) {
  CcTest::InitializeVM();
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(CcTest::i_isolate(), buffer, static_cast<int>(allocated));

  // Assemble a simple function that copies argument 1 and returns it.
  __ pushq(rbp);

  __ movq(rbp, rsp);
  __ movq(rax, arg1);
  Label target;
  __ jmp(&target);
  __ movq(rax, arg2);
  __ bind(&target);
  __ popq(rbp);
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(CcTest::i_isolate(), &desc);
  // Call the function from C++.
  int result =  FUNCTION_CAST<F2>(buffer)(3, 2);
  CHECK_EQ(3, result);
}


TEST(AssemblerX64LoopImmediates) {
  CcTest::InitializeVM();
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(CcTest::i_isolate(), buffer, static_cast<int>(allocated));

  // Assemble two loops using rax as counter, and verify the ending counts.
  Label Fail;
  __ movq(rax, Immediate(-3));
  Label Loop1_test;
  Label Loop1_body;
  __ jmp(&Loop1_test);
  __ bind(&Loop1_body);
  __ addq(rax, Immediate(7));
  __ bind(&Loop1_test);
  __ cmpq(rax, Immediate(20));
  __ j(less_equal, &Loop1_body);
  // Did the loop terminate with the expected value?
  __ cmpq(rax, Immediate(25));
  __ j(not_equal, &Fail);

  Label Loop2_test;
  Label Loop2_body;
  __ movq(rax, Immediate(0x11FEED00));
  __ jmp(&Loop2_test);
  __ bind(&Loop2_body);
  __ addq(rax, Immediate(-0x1100));
  __ bind(&Loop2_test);
  __ cmpq(rax, Immediate(0x11FE8000));
  __ j(greater, &Loop2_body);
  // Did the loop terminate with the expected value?
  __ cmpq(rax, Immediate(0x11FE7600));
  __ j(not_equal, &Fail);

  __ movq(rax, Immediate(1));
  __ ret(0);
  __ bind(&Fail);
  __ movq(rax, Immediate(0));
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(CcTest::i_isolate(), &desc);
  // Call the function from C++.
  int result =  FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(1, result);
}


TEST(OperandRegisterDependency) {
  int offsets[4] = {0, 1, 0xfed, 0xbeefcad};
  for (int i = 0; i < 4; i++) {
    int offset = offsets[i];
    CHECK(Operand(rax, offset).AddressUsesRegister(rax));
    CHECK(!Operand(rax, offset).AddressUsesRegister(r8));
    CHECK(!Operand(rax, offset).AddressUsesRegister(rcx));

    CHECK(Operand(rax, rax, times_1, offset).AddressUsesRegister(rax));
    CHECK(!Operand(rax, rax, times_1, offset).AddressUsesRegister(r8));
    CHECK(!Operand(rax, rax, times_1, offset).AddressUsesRegister(rcx));

    CHECK(Operand(rax, rcx, times_1, offset).AddressUsesRegister(rax));
    CHECK(Operand(rax, rcx, times_1, offset).AddressUsesRegister(rcx));
    CHECK(!Operand(rax, rcx, times_1, offset).AddressUsesRegister(r8));
    CHECK(!Operand(rax, rcx, times_1, offset).AddressUsesRegister(r9));
    CHECK(!Operand(rax, rcx, times_1, offset).AddressUsesRegister(rdx));
    CHECK(!Operand(rax, rcx, times_1, offset).AddressUsesRegister(rsp));

    CHECK(Operand(rsp, offset).AddressUsesRegister(rsp));
    CHECK(!Operand(rsp, offset).AddressUsesRegister(rax));
    CHECK(!Operand(rsp, offset).AddressUsesRegister(r15));

    CHECK(Operand(rbp, offset).AddressUsesRegister(rbp));
    CHECK(!Operand(rbp, offset).AddressUsesRegister(rax));
    CHECK(!Operand(rbp, offset).AddressUsesRegister(r13));

    CHECK(Operand(rbp, rax, times_1, offset).AddressUsesRegister(rbp));
    CHECK(Operand(rbp, rax, times_1, offset).AddressUsesRegister(rax));
    CHECK(!Operand(rbp, rax, times_1, offset).AddressUsesRegister(rcx));
    CHECK(!Operand(rbp, rax, times_1, offset).AddressUsesRegister(r13));
    CHECK(!Operand(rbp, rax, times_1, offset).AddressUsesRegister(r8));
    CHECK(!Operand(rbp, rax, times_1, offset).AddressUsesRegister(rsp));

    CHECK(Operand(rsp, rbp, times_1, offset).AddressUsesRegister(rsp));
    CHECK(Operand(rsp, rbp, times_1, offset).AddressUsesRegister(rbp));
    CHECK(!Operand(rsp, rbp, times_1, offset).AddressUsesRegister(rax));
    CHECK(!Operand(rsp, rbp, times_1, offset).AddressUsesRegister(r15));
    CHECK(!Operand(rsp, rbp, times_1, offset).AddressUsesRegister(r13));
  }
}


TEST(AssemblerX64LabelChaining) {
  // Test chaining of label usages within instructions (issue 1644).
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  Assembler masm(CcTest::i_isolate(), nullptr, 0);

  Label target;
  __ j(equal, &target);
  __ j(not_equal, &target);
  __ bind(&target);
  __ nop();
}


TEST(AssemblerMultiByteNop) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  byte buffer[1024];
  Isolate* isolate = CcTest::i_isolate();
  Assembler masm(isolate, buffer, sizeof(buffer));
  __ pushq(rbx);
  __ pushq(rcx);
  __ pushq(rdx);
  __ pushq(rdi);
  __ pushq(rsi);
  __ movq(rax, Immediate(1));
  __ movq(rbx, Immediate(2));
  __ movq(rcx, Immediate(3));
  __ movq(rdx, Immediate(4));
  __ movq(rdi, Immediate(5));
  __ movq(rsi, Immediate(6));
  for (int i = 0; i < 16; i++) {
    int before = masm.pc_offset();
    __ Nop(i);
    CHECK_EQ(masm.pc_offset() - before, i);
  }

  Label fail;
  __ cmpq(rax, Immediate(1));
  __ j(not_equal, &fail);
  __ cmpq(rbx, Immediate(2));
  __ j(not_equal, &fail);
  __ cmpq(rcx, Immediate(3));
  __ j(not_equal, &fail);
  __ cmpq(rdx, Immediate(4));
  __ j(not_equal, &fail);
  __ cmpq(rdi, Immediate(5));
  __ j(not_equal, &fail);
  __ cmpq(rsi, Immediate(6));
  __ j(not_equal, &fail);
  __ movq(rax, Immediate(42));
  __ popq(rsi);
  __ popq(rdi);
  __ popq(rdx);
  __ popq(rcx);
  __ popq(rbx);
  __ ret(0);
  __ bind(&fail);
  __ movq(rax, Immediate(13));
  __ popq(rsi);
  __ popq(rdi);
  __ popq(rdx);
  __ popq(rcx);
  __ popq(rbx);
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());

  F0 f = FUNCTION_CAST<F0>(code->entry());
  int res = f();
  CHECK_EQ(42, res);
}


#ifdef __GNUC__
#define ELEMENT_COUNT 4u

void DoSSE2(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();
  byte buffer[1024];

  CHECK(args[0]->IsArray());
  v8::Local<v8::Array> vec = v8::Local<v8::Array>::Cast(args[0]);
  CHECK_EQ(ELEMENT_COUNT, vec->Length());

  Isolate* isolate = CcTest::i_isolate();
  Assembler masm(isolate, buffer, sizeof(buffer));

  // Remove return address from the stack for fix stack frame alignment.
  __ popq(rcx);

  // Store input vector on the stack.
  for (unsigned i = 0; i < ELEMENT_COUNT; i++) {
    __ movl(rax, Immediate(vec->Get(context, i)
                               .ToLocalChecked()
                               ->Int32Value(context)
                               .FromJust()));
    __ shlq(rax, Immediate(0x20));
    __ orq(rax, Immediate(vec->Get(context, ++i)
                              .ToLocalChecked()
                              ->Int32Value(context)
                              .FromJust()));
    __ pushq(rax);
  }

  // Read vector into a xmm register.
  __ xorps(xmm0, xmm0);
  __ movdqa(xmm0, Operand(rsp, 0));
  // Create mask and store it in the return register.
  __ movmskps(rax, xmm0);

  // Remove unused data from the stack.
  __ addq(rsp, Immediate(ELEMENT_COUNT * sizeof(int32_t)));
  // Restore return address.
  __ pushq(rcx);

  __ ret(0);

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());

  F0 f = FUNCTION_CAST<F0>(code->entry());
  int res = f();
  args.GetReturnValue().Set(v8::Integer::New(CcTest::isolate(), res));
}


TEST(StackAlignmentForSSE2) {
  CcTest::InitializeVM();
  CHECK_EQ(0, v8::base::OS::ActivationFrameAlignment() % 16);

  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate);
  global_template->Set(v8_str("do_sse2"),
                       v8::FunctionTemplate::New(isolate, DoSSE2));

  LocalContext env(nullptr, global_template);
  CompileRun(
      "function foo(vec) {"
      "  return do_sse2(vec);"
      "}");

  v8::Local<v8::Object> global_object = env->Global();
  v8::Local<v8::Function> foo = v8::Local<v8::Function>::Cast(
      global_object->Get(env.local(), v8_str("foo")).ToLocalChecked());

  int32_t vec[ELEMENT_COUNT] = { -1, 1, 1, 1 };
  v8::Local<v8::Array> v8_vec = v8::Array::New(isolate, ELEMENT_COUNT);
  for (unsigned i = 0; i < ELEMENT_COUNT; i++) {
    v8_vec->Set(env.local(), i, v8_num(vec[i])).FromJust();
  }

  v8::Local<v8::Value> args[] = { v8_vec };
  v8::Local<v8::Value> result =
      foo->Call(env.local(), global_object, 1, args).ToLocalChecked();

  // The mask should be 0b1000.
  CHECK_EQ(8, result->Int32Value(env.local()).FromJust());
}

#undef ELEMENT_COUNT
#endif  // __GNUC__


TEST(AssemblerX64Extractps) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(SSE4_1)) return;

  v8::HandleScope scope(CcTest::isolate());
  byte buffer[256];
  Isolate* isolate = CcTest::i_isolate();
  Assembler masm(isolate, buffer, sizeof(buffer));
  {
    CpuFeatureScope fscope2(&masm, SSE4_1);
    __ extractps(rax, xmm0, 0x1);
    __ ret(0);
  }

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
#endif

  F3 f = FUNCTION_CAST<F3>(code->entry());
  uint64_t value1 = V8_2PART_UINT64_C(0x12345678, 87654321);
  CHECK_EQ(0x12345678u, f(uint64_to_double(value1)));
  uint64_t value2 = V8_2PART_UINT64_C(0x87654321, 12345678);
  CHECK_EQ(0x87654321u, f(uint64_to_double(value2)));
}


typedef int (*F6)(float x, float y);
TEST(AssemblerX64SSE) {
  CcTest::InitializeVM();

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[256];
  MacroAssembler masm(isolate, buffer, sizeof(buffer),
                      v8::internal::CodeObjectRequired::kYes);
  {
    __ shufps(xmm0, xmm0, 0x0);  // brocast first argument
    __ shufps(xmm1, xmm1, 0x0);  // brocast second argument
    __ movaps(xmm2, xmm1);
    __ addps(xmm2, xmm0);
    __ mulps(xmm2, xmm1);
    __ subps(xmm2, xmm0);
    __ divps(xmm2, xmm1);
    __ cvttss2si(rax, xmm2);
    __ ret(0);
  }

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
#endif

  F6 f = FUNCTION_CAST<F6>(code->entry());
  CHECK_EQ(2, f(1.0, 2.0));
}


typedef int (*F7)(double x, double y, double z);
TEST(AssemblerX64FMA_sd) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(FMA3)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[1024];
  MacroAssembler masm(isolate, buffer, sizeof(buffer),
                      v8::internal::CodeObjectRequired::kYes);
  {
    CpuFeatureScope fscope(&masm, FMA3);
    Label exit;
    // argument in xmm0, xmm1 and xmm2
    // xmm0 * xmm1 + xmm2
    __ movaps(xmm3, xmm0);
    __ mulsd(xmm3, xmm1);
    __ addsd(xmm3, xmm2);  // Expected result in xmm3

    __ subq(rsp, Immediate(kDoubleSize));  // For memory operand
    // vfmadd132sd
    __ movl(rax, Immediate(1));  // Test number
    __ movaps(xmm8, xmm0);
    __ vfmadd132sd(xmm8, xmm2, xmm1);
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmadd213sd
    __ incq(rax);
    __ movaps(xmm8, xmm1);
    __ vfmadd213sd(xmm8, xmm0, xmm2);
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmadd231sd
    __ incq(rax);
    __ movaps(xmm8, xmm2);
    __ vfmadd231sd(xmm8, xmm0, xmm1);
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);

    // vfmadd132sd
    __ incq(rax);
    __ movaps(xmm8, xmm0);
    __ movsd(Operand(rsp, 0), xmm1);
    __ vfmadd132sd(xmm8, xmm2, Operand(rsp, 0));
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmadd213sd
    __ incq(rax);
    __ movaps(xmm8, xmm1);
    __ movsd(Operand(rsp, 0), xmm2);
    __ vfmadd213sd(xmm8, xmm0, Operand(rsp, 0));
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmadd231sd
    __ incq(rax);
    __ movaps(xmm8, xmm2);
    __ movsd(Operand(rsp, 0), xmm1);
    __ vfmadd231sd(xmm8, xmm0, Operand(rsp, 0));
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);

    // xmm0 * xmm1 - xmm2
    __ movaps(xmm3, xmm0);
    __ mulsd(xmm3, xmm1);
    __ subsd(xmm3, xmm2);  // Expected result in xmm3

    // vfmsub132sd
    __ incq(rax);
    __ movaps(xmm8, xmm0);
    __ vfmsub132sd(xmm8, xmm2, xmm1);
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmadd213sd
    __ incq(rax);
    __ movaps(xmm8, xmm1);
    __ vfmsub213sd(xmm8, xmm0, xmm2);
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmsub231sd
    __ incq(rax);
    __ movaps(xmm8, xmm2);
    __ vfmsub231sd(xmm8, xmm0, xmm1);
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);

    // vfmsub132sd
    __ incq(rax);
    __ movaps(xmm8, xmm0);
    __ movsd(Operand(rsp, 0), xmm1);
    __ vfmsub132sd(xmm8, xmm2, Operand(rsp, 0));
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmsub213sd
    __ incq(rax);
    __ movaps(xmm8, xmm1);
    __ movsd(Operand(rsp, 0), xmm2);
    __ vfmsub213sd(xmm8, xmm0, Operand(rsp, 0));
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmsub231sd
    __ incq(rax);
    __ movaps(xmm8, xmm2);
    __ movsd(Operand(rsp, 0), xmm1);
    __ vfmsub231sd(xmm8, xmm0, Operand(rsp, 0));
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);


    // - xmm0 * xmm1 + xmm2
    __ movaps(xmm3, xmm0);
    __ mulsd(xmm3, xmm1);
    __ Move(xmm4, (uint64_t)1 << 63);
    __ xorpd(xmm3, xmm4);
    __ addsd(xmm3, xmm2);  // Expected result in xmm3

    // vfnmadd132sd
    __ incq(rax);
    __ movaps(xmm8, xmm0);
    __ vfnmadd132sd(xmm8, xmm2, xmm1);
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmadd213sd
    __ incq(rax);
    __ movaps(xmm8, xmm1);
    __ vfnmadd213sd(xmm8, xmm0, xmm2);
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfnmadd231sd
    __ incq(rax);
    __ movaps(xmm8, xmm2);
    __ vfnmadd231sd(xmm8, xmm0, xmm1);
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);

    // vfnmadd132sd
    __ incq(rax);
    __ movaps(xmm8, xmm0);
    __ movsd(Operand(rsp, 0), xmm1);
    __ vfnmadd132sd(xmm8, xmm2, Operand(rsp, 0));
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfnmadd213sd
    __ incq(rax);
    __ movaps(xmm8, xmm1);
    __ movsd(Operand(rsp, 0), xmm2);
    __ vfnmadd213sd(xmm8, xmm0, Operand(rsp, 0));
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfnmadd231sd
    __ incq(rax);
    __ movaps(xmm8, xmm2);
    __ movsd(Operand(rsp, 0), xmm1);
    __ vfnmadd231sd(xmm8, xmm0, Operand(rsp, 0));
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);


    // - xmm0 * xmm1 - xmm2
    __ movaps(xmm3, xmm0);
    __ mulsd(xmm3, xmm1);
    __ Move(xmm4, (uint64_t)1 << 63);
    __ xorpd(xmm3, xmm4);
    __ subsd(xmm3, xmm2);  // Expected result in xmm3

    // vfnmsub132sd
    __ incq(rax);
    __ movaps(xmm8, xmm0);
    __ vfnmsub132sd(xmm8, xmm2, xmm1);
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmsub213sd
    __ incq(rax);
    __ movaps(xmm8, xmm1);
    __ vfnmsub213sd(xmm8, xmm0, xmm2);
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfnmsub231sd
    __ incq(rax);
    __ movaps(xmm8, xmm2);
    __ vfnmsub231sd(xmm8, xmm0, xmm1);
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);

    // vfnmsub132sd
    __ incq(rax);
    __ movaps(xmm8, xmm0);
    __ movsd(Operand(rsp, 0), xmm1);
    __ vfnmsub132sd(xmm8, xmm2, Operand(rsp, 0));
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfnmsub213sd
    __ incq(rax);
    __ movaps(xmm8, xmm1);
    __ movsd(Operand(rsp, 0), xmm2);
    __ vfnmsub213sd(xmm8, xmm0, Operand(rsp, 0));
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfnmsub231sd
    __ incq(rax);
    __ movaps(xmm8, xmm2);
    __ movsd(Operand(rsp, 0), xmm1);
    __ vfnmsub231sd(xmm8, xmm0, Operand(rsp, 0));
    __ ucomisd(xmm8, xmm3);
    __ j(not_equal, &exit);


    __ xorl(rax, rax);
    __ bind(&exit);
    __ addq(rsp, Immediate(kDoubleSize));
    __ ret(0);
  }

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
#endif

  F7 f = FUNCTION_CAST<F7>(code->entry());
  CHECK_EQ(0, f(0.000092662107262076, -2.460774966188315, -1.0958787393627414));
}


typedef int (*F8)(float x, float y, float z);
TEST(AssemblerX64FMA_ss) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(FMA3)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[1024];
  MacroAssembler masm(isolate, buffer, sizeof(buffer),
                      v8::internal::CodeObjectRequired::kYes);
  {
    CpuFeatureScope fscope(&masm, FMA3);
    Label exit;
    // arguments in xmm0, xmm1 and xmm2
    // xmm0 * xmm1 + xmm2
    __ movaps(xmm3, xmm0);
    __ mulss(xmm3, xmm1);
    __ addss(xmm3, xmm2);  // Expected result in xmm3

    __ subq(rsp, Immediate(kDoubleSize));  // For memory operand
    // vfmadd132ss
    __ movl(rax, Immediate(1));  // Test number
    __ movaps(xmm8, xmm0);
    __ vfmadd132ss(xmm8, xmm2, xmm1);
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmadd213ss
    __ incq(rax);
    __ movaps(xmm8, xmm1);
    __ vfmadd213ss(xmm8, xmm0, xmm2);
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmadd231ss
    __ incq(rax);
    __ movaps(xmm8, xmm2);
    __ vfmadd231ss(xmm8, xmm0, xmm1);
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);

    // vfmadd132ss
    __ incq(rax);
    __ movaps(xmm8, xmm0);
    __ movss(Operand(rsp, 0), xmm1);
    __ vfmadd132ss(xmm8, xmm2, Operand(rsp, 0));
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmadd213ss
    __ incq(rax);
    __ movaps(xmm8, xmm1);
    __ movss(Operand(rsp, 0), xmm2);
    __ vfmadd213ss(xmm8, xmm0, Operand(rsp, 0));
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmadd231ss
    __ incq(rax);
    __ movaps(xmm8, xmm2);
    __ movss(Operand(rsp, 0), xmm1);
    __ vfmadd231ss(xmm8, xmm0, Operand(rsp, 0));
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);

    // xmm0 * xmm1 - xmm2
    __ movaps(xmm3, xmm0);
    __ mulss(xmm3, xmm1);
    __ subss(xmm3, xmm2);  // Expected result in xmm3

    // vfmsub132ss
    __ incq(rax);
    __ movaps(xmm8, xmm0);
    __ vfmsub132ss(xmm8, xmm2, xmm1);
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmadd213ss
    __ incq(rax);
    __ movaps(xmm8, xmm1);
    __ vfmsub213ss(xmm8, xmm0, xmm2);
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmsub231ss
    __ incq(rax);
    __ movaps(xmm8, xmm2);
    __ vfmsub231ss(xmm8, xmm0, xmm1);
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);

    // vfmsub132ss
    __ incq(rax);
    __ movaps(xmm8, xmm0);
    __ movss(Operand(rsp, 0), xmm1);
    __ vfmsub132ss(xmm8, xmm2, Operand(rsp, 0));
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmsub213ss
    __ incq(rax);
    __ movaps(xmm8, xmm1);
    __ movss(Operand(rsp, 0), xmm2);
    __ vfmsub213ss(xmm8, xmm0, Operand(rsp, 0));
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmsub231ss
    __ incq(rax);
    __ movaps(xmm8, xmm2);
    __ movss(Operand(rsp, 0), xmm1);
    __ vfmsub231ss(xmm8, xmm0, Operand(rsp, 0));
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);


    // - xmm0 * xmm1 + xmm2
    __ movaps(xmm3, xmm0);
    __ mulss(xmm3, xmm1);
    __ Move(xmm4, (uint32_t)1 << 31);
    __ xorps(xmm3, xmm4);
    __ addss(xmm3, xmm2);  // Expected result in xmm3

    // vfnmadd132ss
    __ incq(rax);
    __ movaps(xmm8, xmm0);
    __ vfnmadd132ss(xmm8, xmm2, xmm1);
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmadd213ss
    __ incq(rax);
    __ movaps(xmm8, xmm1);
    __ vfnmadd213ss(xmm8, xmm0, xmm2);
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfnmadd231ss
    __ incq(rax);
    __ movaps(xmm8, xmm2);
    __ vfnmadd231ss(xmm8, xmm0, xmm1);
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);

    // vfnmadd132ss
    __ incq(rax);
    __ movaps(xmm8, xmm0);
    __ movss(Operand(rsp, 0), xmm1);
    __ vfnmadd132ss(xmm8, xmm2, Operand(rsp, 0));
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfnmadd213ss
    __ incq(rax);
    __ movaps(xmm8, xmm1);
    __ movss(Operand(rsp, 0), xmm2);
    __ vfnmadd213ss(xmm8, xmm0, Operand(rsp, 0));
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfnmadd231ss
    __ incq(rax);
    __ movaps(xmm8, xmm2);
    __ movss(Operand(rsp, 0), xmm1);
    __ vfnmadd231ss(xmm8, xmm0, Operand(rsp, 0));
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);


    // - xmm0 * xmm1 - xmm2
    __ movaps(xmm3, xmm0);
    __ mulss(xmm3, xmm1);
    __ Move(xmm4, (uint32_t)1 << 31);
    __ xorps(xmm3, xmm4);
    __ subss(xmm3, xmm2);  // Expected result in xmm3

    // vfnmsub132ss
    __ incq(rax);
    __ movaps(xmm8, xmm0);
    __ vfnmsub132ss(xmm8, xmm2, xmm1);
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfmsub213ss
    __ incq(rax);
    __ movaps(xmm8, xmm1);
    __ vfnmsub213ss(xmm8, xmm0, xmm2);
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfnmsub231ss
    __ incq(rax);
    __ movaps(xmm8, xmm2);
    __ vfnmsub231ss(xmm8, xmm0, xmm1);
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);

    // vfnmsub132ss
    __ incq(rax);
    __ movaps(xmm8, xmm0);
    __ movss(Operand(rsp, 0), xmm1);
    __ vfnmsub132ss(xmm8, xmm2, Operand(rsp, 0));
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfnmsub213ss
    __ incq(rax);
    __ movaps(xmm8, xmm1);
    __ movss(Operand(rsp, 0), xmm2);
    __ vfnmsub213ss(xmm8, xmm0, Operand(rsp, 0));
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);
    // vfnmsub231ss
    __ incq(rax);
    __ movaps(xmm8, xmm2);
    __ movss(Operand(rsp, 0), xmm1);
    __ vfnmsub231ss(xmm8, xmm0, Operand(rsp, 0));
    __ ucomiss(xmm8, xmm3);
    __ j(not_equal, &exit);


    __ xorl(rax, rax);
    __ bind(&exit);
    __ addq(rsp, Immediate(kDoubleSize));
    __ ret(0);
  }

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
#endif

  F8 f = FUNCTION_CAST<F8>(code->entry());
  CHECK_EQ(0, f(9.26621069e-05f, -2.4607749f, -1.09587872f));
}


TEST(AssemblerX64SSE_ss) {
  CcTest::InitializeVM();

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[1024];
  Assembler masm(isolate, buffer, sizeof(buffer));
  {
    Label exit;
    // arguments in xmm0, xmm1 and xmm2
    __ movl(rax, Immediate(0));

    __ movaps(xmm3, xmm0);
    __ maxss(xmm3, xmm1);
    __ ucomiss(xmm3, xmm1);
    __ j(parity_even, &exit);
    __ j(not_equal, &exit);
    __ movl(rax, Immediate(1));

    __ movaps(xmm3, xmm1);
    __ minss(xmm3, xmm2);
    __ ucomiss(xmm3, xmm1);
    __ j(parity_even, &exit);
    __ j(not_equal, &exit);
    __ movl(rax, Immediate(2));

    __ movaps(xmm3, xmm2);
    __ subss(xmm3, xmm1);
    __ ucomiss(xmm3, xmm0);
    __ j(parity_even, &exit);
    __ j(not_equal, &exit);
    __ movl(rax, Immediate(3));

    __ movaps(xmm3, xmm0);
    __ addss(xmm3, xmm1);
    __ ucomiss(xmm3, xmm2);
    __ j(parity_even, &exit);
    __ j(not_equal, &exit);
    __ movl(rax, Immediate(4));

    __ movaps(xmm3, xmm0);
    __ mulss(xmm3, xmm1);
    __ ucomiss(xmm3, xmm1);
    __ j(parity_even, &exit);
    __ j(not_equal, &exit);
    __ movl(rax, Immediate(5));

    __ movaps(xmm3, xmm0);
    __ divss(xmm3, xmm1);
    __ mulss(xmm3, xmm2);
    __ mulss(xmm3, xmm1);
    __ ucomiss(xmm3, xmm2);
    __ j(parity_even, &exit);
    __ j(not_equal, &exit);
    __ movl(rax, Immediate(6));

    // result in eax
    __ bind(&exit);
    __ ret(0);
  }

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
#endif

  F8 f = FUNCTION_CAST<F8>(code->entry());
  int res = f(1.0f, 2.0f, 3.0f);
  PrintF("f(1,2,3) = %d\n", res);
  CHECK_EQ(6, res);
}


TEST(AssemblerX64AVX_ss) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(AVX)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[1024];
  Assembler masm(isolate, buffer, sizeof(buffer));
  {
    CpuFeatureScope avx_scope(&masm, AVX);
    Label exit;
    // arguments in xmm0, xmm1 and xmm2
    __ subq(rsp, Immediate(kDoubleSize * 2));  // For memory operand

    __ movl(rdx, Immediate(0xc2f64000));  // -123.125
    __ vmovd(xmm4, rdx);
    __ vmovss(Operand(rsp, 0), xmm4);
    __ vmovss(xmm5, Operand(rsp, 0));
    __ vmovaps(xmm6, xmm5);
    __ vmovd(rcx, xmm6);
    __ cmpl(rcx, rdx);
    __ movl(rax, Immediate(9));
    __ j(not_equal, &exit);

    __ movl(rax, Immediate(0));
    __ vmaxss(xmm3, xmm0, xmm1);
    __ vucomiss(xmm3, xmm1);
    __ j(parity_even, &exit);
    __ j(not_equal, &exit);
    __ movl(rax, Immediate(1));

    __ vminss(xmm3, xmm1, xmm2);
    __ vucomiss(xmm3, xmm1);
    __ j(parity_even, &exit);
    __ j(not_equal, &exit);
    __ movl(rax, Immediate(2));

    __ vsubss(xmm3, xmm2, xmm1);
    __ vucomiss(xmm3, xmm0);
    __ j(parity_even, &exit);
    __ j(not_equal, &exit);
    __ movl(rax, Immediate(3));

    __ vaddss(xmm3, xmm0, xmm1);
    __ vucomiss(xmm3, xmm2);
    __ j(parity_even, &exit);
    __ j(not_equal, &exit);
    __ movl(rax, Immediate(4));

    __ vmulss(xmm3, xmm0, xmm1);
    __ vucomiss(xmm3, xmm1);
    __ j(parity_even, &exit);
    __ j(not_equal, &exit);
    __ movl(rax, Immediate(5));

    __ vdivss(xmm3, xmm0, xmm1);
    __ vmulss(xmm3, xmm3, xmm2);
    __ vmulss(xmm3, xmm3, xmm1);
    __ vucomiss(xmm3, xmm2);
    __ j(parity_even, &exit);
    __ j(not_equal, &exit);
    __ movl(rax, Immediate(6));

    // result in eax
    __ bind(&exit);
    __ addq(rsp, Immediate(kDoubleSize * 2));
    __ ret(0);
  }

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
#endif

  F8 f = FUNCTION_CAST<F8>(code->entry());
  int res = f(1.0f, 2.0f, 3.0f);
  PrintF("f(1,2,3) = %d\n", res);
  CHECK_EQ(6, res);
}


TEST(AssemblerX64AVX_sd) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(AVX)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[1024];
  Assembler masm(isolate, buffer, sizeof(buffer));
  {
    CpuFeatureScope avx_scope(&masm, AVX);
    Label exit;
    // arguments in xmm0, xmm1 and xmm2
    __ subq(rsp, Immediate(kDoubleSize * 2));  // For memory operand
    __ movl(rax, Immediate(0));

    __ vmaxsd(xmm4, xmm0, xmm1);
    __ vmovsd(Operand(rsp, kDoubleSize), xmm4);
    __ vmovsd(xmm5, Operand(rsp, kDoubleSize));
    __ vmovsd(xmm6, xmm6, xmm5);
    __ vmovapd(xmm3, xmm6);

    // Test vcvtss2sd & vcvtsd2ss
    __ movl(rax, Immediate(9));
    __ movq(rdx, V8_INT64_C(0x426D1A0000000000));
    __ movq(Operand(rsp, 0), rdx);
    __ vcvtsd2ss(xmm6, xmm6, Operand(rsp, 0));
    __ vcvtss2sd(xmm7, xmm6, xmm6);
    __ vcvtsd2ss(xmm8, xmm7, xmm7);
    __ vmovss(Operand(rsp, 0), xmm8);
    __ vcvtss2sd(xmm9, xmm8, Operand(rsp, 0));
    __ vmovq(rcx, xmm9);
    __ cmpq(rcx, rdx);
    __ j(not_equal, &exit);

    // Test vcvttsd2si
    __ movl(rax, Immediate(10));
    __ movl(rdx, Immediate(123));
    __ vcvtlsi2sd(xmm6, xmm6, rdx);
    __ vcvttsd2si(rcx, xmm6);
    __ cmpl(rcx, rdx);
    __ j(not_equal, &exit);
    __ xorl(rcx, rcx);
    __ vmovsd(Operand(rsp, 0), xmm6);
    __ vcvttsd2si(rcx, Operand(rsp, 0));
    __ cmpl(rcx, rdx);
    __ j(not_equal, &exit);

    // Test vcvttsd2siq
    __ movl(rax, Immediate(11));
    __ movq(rdx, V8_INT64_C(0x426D1A94A2000000));  // 1.0e12
    __ vmovq(xmm6, rdx);
    __ vcvttsd2siq(rcx, xmm6);
    __ movq(rdx, V8_INT64_C(1000000000000));
    __ cmpq(rcx, rdx);
    __ j(not_equal, &exit);
    __ xorq(rcx, rcx);
    __ vmovsd(Operand(rsp, 0), xmm6);
    __ vcvttsd2siq(rcx, Operand(rsp, 0));
    __ cmpq(rcx, rdx);
    __ j(not_equal, &exit);

    // Test vmovmskpd
    __ movl(rax, Immediate(12));
    __ movq(rdx, V8_INT64_C(0x426D1A94A2000000));  // 1.0e12
    __ vmovq(xmm6, rdx);
    __ movq(rdx, V8_INT64_C(0xC26D1A94A2000000));  // -1.0e12
    __ vmovq(xmm7, rdx);
    __ shufps(xmm6, xmm7, 0x44);
    __ vmovmskpd(rdx, xmm6);
    __ cmpl(rdx, Immediate(2));
    __ j(not_equal, &exit);

    // Test vpcmpeqd
    __ movq(rdx, V8_UINT64_C(0x0123456789abcdef));
    __ movq(rcx, V8_UINT64_C(0x0123456788888888));
    __ vmovq(xmm6, rdx);
    __ vmovq(xmm7, rcx);
    __ vpcmpeqd(xmm8, xmm6, xmm7);
    __ vmovq(rdx, xmm8);
    __ movq(rcx, V8_UINT64_C(0xffffffff00000000));
    __ cmpq(rcx, rdx);
    __ movl(rax, Immediate(13));
    __ j(not_equal, &exit);

    // Test vpsllq, vpsrlq
    __ movl(rax, Immediate(13));
    __ movq(rdx, V8_UINT64_C(0x0123456789abcdef));
    __ vmovq(xmm6, rdx);
    __ vpsrlq(xmm7, xmm6, 4);
    __ vmovq(rdx, xmm7);
    __ movq(rcx, V8_UINT64_C(0x00123456789abcde));
    __ cmpq(rdx, rcx);
    __ j(not_equal, &exit);
    __ vpsllq(xmm7, xmm6, 12);
    __ vmovq(rdx, xmm7);
    __ movq(rcx, V8_UINT64_C(0x3456789abcdef000));
    __ cmpq(rdx, rcx);
    __ j(not_equal, &exit);

    // Test vandpd, vorpd, vxorpd
    __ movl(rax, Immediate(14));
    __ movl(rdx, Immediate(0x00ff00ff));
    __ movl(rcx, Immediate(0x0f0f0f0f));
    __ vmovd(xmm4, rdx);
    __ vmovd(xmm5, rcx);
    __ vandpd(xmm6, xmm4, xmm5);
    __ vmovd(rdx, xmm6);
    __ cmpl(rdx, Immediate(0x000f000f));
    __ j(not_equal, &exit);
    __ vorpd(xmm6, xmm4, xmm5);
    __ vmovd(rdx, xmm6);
    __ cmpl(rdx, Immediate(0x0fff0fff));
    __ j(not_equal, &exit);
    __ vxorpd(xmm6, xmm4, xmm5);
    __ vmovd(rdx, xmm6);
    __ cmpl(rdx, Immediate(0x0ff00ff0));
    __ j(not_equal, &exit);

    // Test vsqrtsd
    __ movl(rax, Immediate(15));
    __ movq(rdx, V8_UINT64_C(0x4004000000000000));  // 2.5
    __ vmovq(xmm4, rdx);
    __ vmulsd(xmm5, xmm4, xmm4);
    __ vmovsd(Operand(rsp, 0), xmm5);
    __ vsqrtsd(xmm6, xmm5, xmm5);
    __ vmovq(rcx, xmm6);
    __ cmpq(rcx, rdx);
    __ j(not_equal, &exit);
    __ vsqrtsd(xmm7, xmm7, Operand(rsp, 0));
    __ vmovq(rcx, xmm7);
    __ cmpq(rcx, rdx);
    __ j(not_equal, &exit);

    // Test vroundsd
    __ movl(rax, Immediate(16));
    __ movq(rdx, V8_UINT64_C(0x4002000000000000));  // 2.25
    __ vmovq(xmm4, rdx);
    __ vroundsd(xmm5, xmm4, xmm4, kRoundUp);
    __ movq(rcx, V8_UINT64_C(0x4008000000000000));  // 3.0
    __ vmovq(xmm6, rcx);
    __ vucomisd(xmm5, xmm6);
    __ j(not_equal, &exit);

    // Test vcvtlsi2sd
    __ movl(rax, Immediate(17));
    __ movl(rdx, Immediate(6));
    __ movq(rcx, V8_UINT64_C(0x4018000000000000));  // 6.0
    __ vmovq(xmm5, rcx);
    __ vcvtlsi2sd(xmm6, xmm6, rdx);
    __ vucomisd(xmm5, xmm6);
    __ j(not_equal, &exit);
    __ movl(Operand(rsp, 0), rdx);
    __ vcvtlsi2sd(xmm7, xmm7, Operand(rsp, 0));
    __ vucomisd(xmm5, xmm6);
    __ j(not_equal, &exit);

    // Test vcvtqsi2sd
    __ movl(rax, Immediate(18));
    __ movq(rdx, V8_UINT64_C(0x2000000000000000));  // 2 << 0x3c
    __ movq(rcx, V8_UINT64_C(0x43c0000000000000));
    __ vmovq(xmm5, rcx);
    __ vcvtqsi2sd(xmm6, xmm6, rdx);
    __ vucomisd(xmm5, xmm6);
    __ j(not_equal, &exit);

    // Test vcvtsd2si
    __ movl(rax, Immediate(19));
    __ movq(rdx, V8_UINT64_C(0x4018000000000000));  // 6.0
    __ vmovq(xmm5, rdx);
    __ vcvtsd2si(rcx, xmm5);
    __ cmpl(rcx, Immediate(6));
    __ j(not_equal, &exit);

    __ movq(rdx, V8_INT64_C(0x3ff0000000000000));  // 1.0
    __ vmovq(xmm7, rdx);
    __ vmulsd(xmm1, xmm1, xmm7);
    __ movq(Operand(rsp, 0), rdx);
    __ vmovq(xmm6, Operand(rsp, 0));
    __ vmulsd(xmm1, xmm1, xmm6);

    __ vucomisd(xmm3, xmm1);
    __ j(parity_even, &exit);
    __ j(not_equal, &exit);
    __ movl(rax, Immediate(1));

    __ vminsd(xmm3, xmm1, xmm2);
    __ vucomisd(xmm3, xmm1);
    __ j(parity_even, &exit);
    __ j(not_equal, &exit);
    __ movl(rax, Immediate(2));

    __ vsubsd(xmm3, xmm2, xmm1);
    __ vucomisd(xmm3, xmm0);
    __ j(parity_even, &exit);
    __ j(not_equal, &exit);
    __ movl(rax, Immediate(3));

    __ vaddsd(xmm3, xmm0, xmm1);
    __ vucomisd(xmm3, xmm2);
    __ j(parity_even, &exit);
    __ j(not_equal, &exit);
    __ movl(rax, Immediate(4));

    __ vmulsd(xmm3, xmm0, xmm1);
    __ vucomisd(xmm3, xmm1);
    __ j(parity_even, &exit);
    __ j(not_equal, &exit);
    __ movl(rax, Immediate(5));

    __ vdivsd(xmm3, xmm0, xmm1);
    __ vmulsd(xmm3, xmm3, xmm2);
    __ vmulsd(xmm3, xmm3, xmm1);
    __ vucomisd(xmm3, xmm2);
    __ j(parity_even, &exit);
    __ j(not_equal, &exit);
    __ movl(rax, Immediate(6));

    // result in eax
    __ bind(&exit);
    __ addq(rsp, Immediate(kDoubleSize * 2));
    __ ret(0);
  }

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
#endif

  F7 f = FUNCTION_CAST<F7>(code->entry());
  int res = f(1.0, 2.0, 3.0);
  PrintF("f(1,2,3) = %d\n", res);
  CHECK_EQ(6, res);
}


TEST(AssemblerX64BMI1) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(BMI1)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[1024];
  MacroAssembler masm(isolate, buffer, sizeof(buffer),
                      v8::internal::CodeObjectRequired::kYes);
  {
    CpuFeatureScope fscope(&masm, BMI1);
    Label exit;

    __ movq(rcx, V8_UINT64_C(0x1122334455667788));  // source operand
    __ pushq(rcx);                                  // For memory operand

    // andn
    __ movq(rdx, V8_UINT64_C(0x1000000020000000));

    __ movl(rax, Immediate(1));  // Test number
    __ andnq(r8, rdx, rcx);
    __ movq(r9, V8_UINT64_C(0x0122334455667788));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ andnq(r8, rdx, Operand(rsp, 0));
    __ movq(r9, V8_UINT64_C(0x0122334455667788));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ andnl(r8, rdx, rcx);
    __ movq(r9, V8_UINT64_C(0x0000000055667788));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ andnl(r8, rdx, Operand(rsp, 0));
    __ movq(r9, V8_UINT64_C(0x0000000055667788));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    // bextr
    __ movq(rdx, V8_UINT64_C(0x0000000000002808));

    __ incq(rax);
    __ bextrq(r8, rcx, rdx);
    __ movq(r9, V8_UINT64_C(0x0000003344556677));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ bextrq(r8, Operand(rsp, 0), rdx);
    __ movq(r9, V8_UINT64_C(0x0000003344556677));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ bextrl(r8, rcx, rdx);
    __ movq(r9, V8_UINT64_C(0x0000000000556677));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ bextrl(r8, Operand(rsp, 0), rdx);
    __ movq(r9, V8_UINT64_C(0x0000000000556677));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    // blsi
    __ incq(rax);
    __ blsiq(r8, rcx);
    __ movq(r9, V8_UINT64_C(0x0000000000000008));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ blsiq(r8, Operand(rsp, 0));
    __ movq(r9, V8_UINT64_C(0x0000000000000008));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ blsil(r8, rcx);
    __ movq(r9, V8_UINT64_C(0x0000000000000008));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ blsil(r8, Operand(rsp, 0));
    __ movq(r9, V8_UINT64_C(0x0000000000000008));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    // blsmsk
    __ incq(rax);
    __ blsmskq(r8, rcx);
    __ movq(r9, V8_UINT64_C(0x000000000000000f));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ blsmskq(r8, Operand(rsp, 0));
    __ movq(r9, V8_UINT64_C(0x000000000000000f));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ blsmskl(r8, rcx);
    __ movq(r9, V8_UINT64_C(0x000000000000000f));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ blsmskl(r8, Operand(rsp, 0));
    __ movq(r9, V8_UINT64_C(0x000000000000000f));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    // blsr
    __ incq(rax);
    __ blsrq(r8, rcx);
    __ movq(r9, V8_UINT64_C(0x1122334455667780));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ blsrq(r8, Operand(rsp, 0));
    __ movq(r9, V8_UINT64_C(0x1122334455667780));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ blsrl(r8, rcx);
    __ movq(r9, V8_UINT64_C(0x0000000055667780));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ blsrl(r8, Operand(rsp, 0));
    __ movq(r9, V8_UINT64_C(0x0000000055667780));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    // tzcnt
    __ incq(rax);
    __ tzcntq(r8, rcx);
    __ movq(r9, V8_UINT64_C(0x0000000000000003));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ tzcntq(r8, Operand(rsp, 0));
    __ movq(r9, V8_UINT64_C(0x0000000000000003));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ tzcntl(r8, rcx);
    __ movq(r9, V8_UINT64_C(0x0000000000000003));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ tzcntl(r8, Operand(rsp, 0));
    __ movq(r9, V8_UINT64_C(0x0000000000000003));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ xorl(rax, rax);
    __ bind(&exit);
    __ popq(rcx);
    __ ret(0);
  }

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
#endif

  F0 f = FUNCTION_CAST<F0>(code->entry());
  CHECK_EQ(0, f());
}


TEST(AssemblerX64LZCNT) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(LZCNT)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[256];
  MacroAssembler masm(isolate, buffer, sizeof(buffer),
                      v8::internal::CodeObjectRequired::kYes);
  {
    CpuFeatureScope fscope(&masm, LZCNT);
    Label exit;

    __ movq(rcx, V8_UINT64_C(0x1122334455667788));  // source operand
    __ pushq(rcx);                                  // For memory operand

    __ movl(rax, Immediate(1));  // Test number
    __ lzcntq(r8, rcx);
    __ movq(r9, V8_UINT64_C(0x0000000000000003));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ lzcntq(r8, Operand(rsp, 0));
    __ movq(r9, V8_UINT64_C(0x0000000000000003));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ lzcntl(r8, rcx);
    __ movq(r9, V8_UINT64_C(0x0000000000000001));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ lzcntl(r8, Operand(rsp, 0));
    __ movq(r9, V8_UINT64_C(0x0000000000000001));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ xorl(rax, rax);
    __ bind(&exit);
    __ popq(rcx);
    __ ret(0);
  }

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
#endif

  F0 f = FUNCTION_CAST<F0>(code->entry());
  CHECK_EQ(0, f());
}


TEST(AssemblerX64POPCNT) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(POPCNT)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[256];
  MacroAssembler masm(isolate, buffer, sizeof(buffer),
                      v8::internal::CodeObjectRequired::kYes);
  {
    CpuFeatureScope fscope(&masm, POPCNT);
    Label exit;

    __ movq(rcx, V8_UINT64_C(0x1111111111111100));  // source operand
    __ pushq(rcx);                                  // For memory operand

    __ movl(rax, Immediate(1));  // Test number
    __ popcntq(r8, rcx);
    __ movq(r9, V8_UINT64_C(0x000000000000000e));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ popcntq(r8, Operand(rsp, 0));
    __ movq(r9, V8_UINT64_C(0x000000000000000e));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ popcntl(r8, rcx);
    __ movq(r9, V8_UINT64_C(0x0000000000000006));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ popcntl(r8, Operand(rsp, 0));
    __ movq(r9, V8_UINT64_C(0x0000000000000006));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ xorl(rax, rax);
    __ bind(&exit);
    __ popq(rcx);
    __ ret(0);
  }

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
#endif

  F0 f = FUNCTION_CAST<F0>(code->entry());
  CHECK_EQ(0, f());
}


TEST(AssemblerX64BMI2) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(BMI2)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[2048];
  MacroAssembler masm(isolate, buffer, sizeof(buffer),
                      v8::internal::CodeObjectRequired::kYes);
  {
    CpuFeatureScope fscope(&masm, BMI2);
    Label exit;
    __ pushq(rbx);                                  // save rbx
    __ movq(rcx, V8_UINT64_C(0x1122334455667788));  // source operand
    __ pushq(rcx);                                  // For memory operand

    // bzhi
    __ movq(rdx, V8_UINT64_C(0x0000000000000009));

    __ movl(rax, Immediate(1));  // Test number
    __ bzhiq(r8, rcx, rdx);
    __ movq(r9, V8_UINT64_C(0x0000000000000188));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ bzhiq(r8, Operand(rsp, 0), rdx);
    __ movq(r9, V8_UINT64_C(0x0000000000000188));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ bzhil(r8, rcx, rdx);
    __ movq(r9, V8_UINT64_C(0x0000000000000188));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ bzhil(r8, Operand(rsp, 0), rdx);
    __ movq(r9, V8_UINT64_C(0x0000000000000188));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    // mulx
    __ movq(rdx, V8_UINT64_C(0x0000000000001000));

    __ incq(rax);
    __ mulxq(r8, r9, rcx);
    __ movq(rbx, V8_UINT64_C(0x0000000000000112));  // expected result
    __ cmpq(r8, rbx);
    __ j(not_equal, &exit);
    __ movq(rbx, V8_UINT64_C(0x2334455667788000));  // expected result
    __ cmpq(r9, rbx);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ mulxq(r8, r9, Operand(rsp, 0));
    __ movq(rbx, V8_UINT64_C(0x0000000000000112));  // expected result
    __ cmpq(r8, rbx);
    __ j(not_equal, &exit);
    __ movq(rbx, V8_UINT64_C(0x2334455667788000));  // expected result
    __ cmpq(r9, rbx);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ mulxl(r8, r9, rcx);
    __ movq(rbx, V8_UINT64_C(0x0000000000000556));  // expected result
    __ cmpq(r8, rbx);
    __ j(not_equal, &exit);
    __ movq(rbx, V8_UINT64_C(0x0000000067788000));  // expected result
    __ cmpq(r9, rbx);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ mulxl(r8, r9, Operand(rsp, 0));
    __ movq(rbx, V8_UINT64_C(0x0000000000000556));  // expected result
    __ cmpq(r8, rbx);
    __ j(not_equal, &exit);
    __ movq(rbx, V8_UINT64_C(0x0000000067788000));  // expected result
    __ cmpq(r9, rbx);
    __ j(not_equal, &exit);

    // pdep
    __ movq(rdx, V8_UINT64_C(0xfffffffffffffff0));

    __ incq(rax);
    __ pdepq(r8, rdx, rcx);
    __ movq(r9, V8_UINT64_C(0x1122334455667400));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ pdepq(r8, rdx, Operand(rsp, 0));
    __ movq(r9, V8_UINT64_C(0x1122334455667400));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ pdepl(r8, rdx, rcx);
    __ movq(r9, V8_UINT64_C(0x0000000055667400));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ pdepl(r8, rdx, Operand(rsp, 0));
    __ movq(r9, V8_UINT64_C(0x0000000055667400));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    // pext
    __ movq(rdx, V8_UINT64_C(0xfffffffffffffff0));

    __ incq(rax);
    __ pextq(r8, rdx, rcx);
    __ movq(r9, V8_UINT64_C(0x0000000003fffffe));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ pextq(r8, rdx, Operand(rsp, 0));
    __ movq(r9, V8_UINT64_C(0x0000000003fffffe));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ pextl(r8, rdx, rcx);
    __ movq(r9, V8_UINT64_C(0x000000000000fffe));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ pextl(r8, rdx, Operand(rsp, 0));
    __ movq(r9, V8_UINT64_C(0x000000000000fffe));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    // sarx
    __ movq(rdx, V8_UINT64_C(0x0000000000000004));

    __ incq(rax);
    __ sarxq(r8, rcx, rdx);
    __ movq(r9, V8_UINT64_C(0x0112233445566778));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ sarxq(r8, Operand(rsp, 0), rdx);
    __ movq(r9, V8_UINT64_C(0x0112233445566778));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ sarxl(r8, rcx, rdx);
    __ movq(r9, V8_UINT64_C(0x0000000005566778));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ sarxl(r8, Operand(rsp, 0), rdx);
    __ movq(r9, V8_UINT64_C(0x0000000005566778));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    // shlx
    __ movq(rdx, V8_UINT64_C(0x0000000000000004));

    __ incq(rax);
    __ shlxq(r8, rcx, rdx);
    __ movq(r9, V8_UINT64_C(0x1223344556677880));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ shlxq(r8, Operand(rsp, 0), rdx);
    __ movq(r9, V8_UINT64_C(0x1223344556677880));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ shlxl(r8, rcx, rdx);
    __ movq(r9, V8_UINT64_C(0x0000000056677880));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ shlxl(r8, Operand(rsp, 0), rdx);
    __ movq(r9, V8_UINT64_C(0x0000000056677880));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    // shrx
    __ movq(rdx, V8_UINT64_C(0x0000000000000004));

    __ incq(rax);
    __ shrxq(r8, rcx, rdx);
    __ movq(r9, V8_UINT64_C(0x0112233445566778));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ shrxq(r8, Operand(rsp, 0), rdx);
    __ movq(r9, V8_UINT64_C(0x0112233445566778));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ shrxl(r8, rcx, rdx);
    __ movq(r9, V8_UINT64_C(0x0000000005566778));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ shrxl(r8, Operand(rsp, 0), rdx);
    __ movq(r9, V8_UINT64_C(0x0000000005566778));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    // rorx
    __ incq(rax);
    __ rorxq(r8, rcx, 0x4);
    __ movq(r9, V8_UINT64_C(0x8112233445566778));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ rorxq(r8, Operand(rsp, 0), 0x4);
    __ movq(r9, V8_UINT64_C(0x8112233445566778));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ rorxl(r8, rcx, 0x4);
    __ movq(r9, V8_UINT64_C(0x0000000085566778));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ incq(rax);
    __ rorxl(r8, Operand(rsp, 0), 0x4);
    __ movq(r9, V8_UINT64_C(0x0000000085566778));  // expected result
    __ cmpq(r8, r9);
    __ j(not_equal, &exit);

    __ xorl(rax, rax);
    __ bind(&exit);
    __ popq(rcx);
    __ popq(rbx);
    __ ret(0);
  }

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
#endif

  F0 f = FUNCTION_CAST<F0>(code->entry());
  CHECK_EQ(0, f());
}


TEST(AssemblerX64JumpTables1) {
  // Test jump tables with forward jumps.
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  MacroAssembler masm(isolate, nullptr, 0,
                      v8::internal::CodeObjectRequired::kYes);

  const int kNumCases = 512;
  int values[kNumCases];
  isolate->random_number_generator()->NextBytes(values, sizeof(values));
  Label labels[kNumCases];

  Label done, table;
  __ leaq(arg2, Operand(&table));
  __ jmp(Operand(arg2, arg1, times_8, 0));
  __ ud2();
  __ bind(&table);
  for (int i = 0; i < kNumCases; ++i) {
    __ dq(&labels[i]);
  }

  for (int i = 0; i < kNumCases; ++i) {
    __ bind(&labels[i]);
    __ movq(rax, Immediate(values[i]));
    __ jmp(&done);
  }

  __ bind(&done);
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef OBJECT_PRINT
  code->Print(std::cout);
#endif

  F1 f = FUNCTION_CAST<F1>(code->entry());
  for (int i = 0; i < kNumCases; ++i) {
    int res = f(i);
    PrintF("f(%d) = %d\n", i, res);
    CHECK_EQ(values[i], res);
  }
}


TEST(AssemblerX64JumpTables2) {
  // Test jump tables with backwards jumps.
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  MacroAssembler masm(isolate, nullptr, 0,
                      v8::internal::CodeObjectRequired::kYes);

  const int kNumCases = 512;
  int values[kNumCases];
  isolate->random_number_generator()->NextBytes(values, sizeof(values));
  Label labels[kNumCases];

  Label done, table;
  __ leaq(arg2, Operand(&table));
  __ jmp(Operand(arg2, arg1, times_8, 0));
  __ ud2();

  for (int i = 0; i < kNumCases; ++i) {
    __ bind(&labels[i]);
    __ movq(rax, Immediate(values[i]));
    __ jmp(&done);
  }

  __ bind(&done);
  __ ret(0);

  __ bind(&table);
  for (int i = 0; i < kNumCases; ++i) {
    __ dq(&labels[i]);
  }

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef OBJECT_PRINT
  code->Print(std::cout);
#endif

  F1 f = FUNCTION_CAST<F1>(code->entry());
  for (int i = 0; i < kNumCases; ++i) {
    int res = f(i);
    PrintF("f(%d) = %d\n", i, res);
    CHECK_EQ(values[i], res);
  }
}

TEST(AssemblerX64PslldWithXmm15) {
  CcTest::InitializeVM();
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(CcTest::i_isolate(), buffer, static_cast<int>(allocated));

  __ movq(xmm15, arg1);
  __ pslld(xmm15, 1);
  __ movq(rax, xmm15);
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(CcTest::i_isolate(), &desc);
  uint64_t result = FUNCTION_CAST<F5>(buffer)(V8_UINT64_C(0x1122334455667788));
  CHECK_EQ(V8_UINT64_C(0x22446688aaccef10), result);
}

typedef float (*F9)(float x, float y);
TEST(AssemblerX64vmovups) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(AVX)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[256];
  MacroAssembler masm(isolate, buffer, sizeof(buffer),
                      v8::internal::CodeObjectRequired::kYes);
  {
    CpuFeatureScope avx_scope(&masm, AVX);
    __ shufps(xmm0, xmm0, 0x0);  // brocast first argument
    __ shufps(xmm1, xmm1, 0x0);  // brocast second argument
    // copy xmm1 to xmm0 through the stack to test the "vmovups reg, mem".
    __ subq(rsp, Immediate(kSimd128Size));
    __ vmovups(Operand(rsp, 0), xmm1);
    __ vmovups(xmm0, Operand(rsp, 0));
    __ addq(rsp, Immediate(kSimd128Size));

    __ ret(0);
  }

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef OBJECT_PRINT
  OFStream os(stdout);
  code->Print(os);
#endif

  F9 f = FUNCTION_CAST<F9>(code->entry());
  CHECK_EQ(-1.5, f(1.5, -1.5));
}

#undef __

}  // namespace internal
}  // namespace v8
