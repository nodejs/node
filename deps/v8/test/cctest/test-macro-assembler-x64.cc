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

#include <stdlib.h>

#include "src/v8.h"

#include "src/base/platform/platform.h"
#include "src/heap/factory.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/simulator.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace test_macro_assembler_x64 {

// Test the x64 assembler by compiling some simple functions into
// a buffer and executing them.  These tests do not initialize the
// V8 library, create a context, or use any V8 objects.
// The AMD64 calling convention is used, with the first five arguments
// in RSI, RDI, RDX, RCX, R8, and R9, and floating point arguments in
// the XMM registers.  The return value is in RAX.
// This calling convention is used on Linux, with GCC, and on Mac OS,
// with GCC.  A different convention is used on 64-bit windows.

typedef int(F0)();

#define __ masm->

static void EntryCode(MacroAssembler* masm) {
  // Smi constant register is callee save.
  __ pushq(kRootRegister);
  __ InitializeRootRegister();
}

static void ExitCode(MacroAssembler* masm) { __ popq(kRootRegister); }

TEST(Smi) {
  // Check that C++ Smi operations work as expected.
  int64_t test_numbers[] = {
      0, 1, -1, 127, 128, -128, -129, 255, 256, -256, -257,
      Smi::kMaxValue, static_cast<int64_t>(Smi::kMaxValue) + 1,
      Smi::kMinValue, static_cast<int64_t>(Smi::kMinValue) - 1
  };
  int test_number_count = 15;
  for (int i = 0; i < test_number_count; i++) {
    int64_t number = test_numbers[i];
    bool is_valid = Smi::IsValid(number);
    bool is_in_range = number >= Smi::kMinValue && number <= Smi::kMaxValue;
    CHECK_EQ(is_in_range, is_valid);
    if (is_valid) {
      Smi* smi_from_intptr = Smi::FromIntptr(number);
      if (static_cast<int>(number) == number) {  // Is a 32-bit int.
        Smi* smi_from_int = Smi::FromInt(static_cast<int32_t>(number));
        CHECK_EQ(smi_from_int, smi_from_intptr);
      }
      int64_t smi_value = smi_from_intptr->value();
      CHECK_EQ(number, smi_value);
    }
  }
}


static void TestMoveSmi(MacroAssembler* masm, Label* exit, int id, Smi* value) {
  __ movl(rax, Immediate(id));
  __ Move(rcx, value);
  __ Set(rdx, reinterpret_cast<intptr_t>(value));
  __ cmpq(rcx, rdx);
  __ j(not_equal, exit);
}


// Test that we can move a Smi value literally into a register.
TEST(SmiMove) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  MacroAssembler assembler(isolate, buffer, static_cast<int>(allocated),
                           v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assembler;  // Create a pointer for the __ macro.
  EntryCode(masm);
  Label exit;

  TestMoveSmi(masm, &exit, 1, Smi::kZero);
  TestMoveSmi(masm, &exit, 2, Smi::FromInt(127));
  TestMoveSmi(masm, &exit, 3, Smi::FromInt(128));
  TestMoveSmi(masm, &exit, 4, Smi::FromInt(255));
  TestMoveSmi(masm, &exit, 5, Smi::FromInt(256));
  TestMoveSmi(masm, &exit, 6, Smi::FromInt(Smi::kMaxValue));
  TestMoveSmi(masm, &exit, 7, Smi::FromInt(-1));
  TestMoveSmi(masm, &exit, 8, Smi::FromInt(-128));
  TestMoveSmi(masm, &exit, 9, Smi::FromInt(-129));
  TestMoveSmi(masm, &exit, 10, Smi::FromInt(-256));
  TestMoveSmi(masm, &exit, 11, Smi::FromInt(-257));
  TestMoveSmi(masm, &exit, 12, Smi::FromInt(Smi::kMinValue));

  __ xorq(rax, rax);  // Success.
  __ bind(&exit);
  ExitCode(masm);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(isolate, &desc);
  MakeAssemblerBufferExecutable(buffer, allocated);
  // Call the function from C++.
  auto f = GeneratedCode<F0>::FromAddress(CcTest::i_isolate(), buffer);
  int result = f.Call();
  CHECK_EQ(0, result);
}


void TestSmiCompare(MacroAssembler* masm, Label* exit, int id, int x, int y) {
  __ Move(rcx, Smi::FromInt(x));
  __ movq(r8, rcx);
  __ Move(rdx, Smi::FromInt(y));
  __ movq(r9, rdx);
  __ SmiCompare(rcx, rdx);
  if (x < y) {
    __ movl(rax, Immediate(id + 1));
    __ j(greater_equal, exit);
  } else if (x > y) {
    __ movl(rax, Immediate(id + 2));
    __ j(less_equal, exit);
  } else {
    CHECK_EQ(x, y);
    __ movl(rax, Immediate(id + 3));
    __ j(not_equal, exit);
  }
  __ movl(rax, Immediate(id + 4));
  __ cmpq(rcx, r8);
  __ j(not_equal, exit);
  __ incq(rax);
  __ cmpq(rdx, r9);
  __ j(not_equal, exit);

  if (x != y) {
    __ SmiCompare(rdx, rcx);
    if (y < x) {
      __ movl(rax, Immediate(id + 9));
      __ j(greater_equal, exit);
    } else {
      CHECK(y > x);
      __ movl(rax, Immediate(id + 10));
      __ j(less_equal, exit);
    }
  } else {
    __ cmpq(rcx, rcx);
    __ movl(rax, Immediate(id + 11));
    __ j(not_equal, exit);
    __ incq(rax);
    __ cmpq(rcx, r8);
    __ j(not_equal, exit);
  }
}


// Test that we can compare smis for equality (and more).
TEST(SmiCompare) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  MacroAssembler assembler(isolate, buffer, static_cast<int>(allocated),
                           v8::internal::CodeObjectRequired::kYes);

  MacroAssembler* masm = &assembler;
  EntryCode(masm);
  Label exit;

  TestSmiCompare(masm, &exit, 0x10, 0, 0);
  TestSmiCompare(masm, &exit, 0x20, 0, 1);
  TestSmiCompare(masm, &exit, 0x30, 1, 0);
  TestSmiCompare(masm, &exit, 0x40, 1, 1);
  TestSmiCompare(masm, &exit, 0x50, 0, -1);
  TestSmiCompare(masm, &exit, 0x60, -1, 0);
  TestSmiCompare(masm, &exit, 0x70, -1, -1);
  TestSmiCompare(masm, &exit, 0x80, 0, Smi::kMinValue);
  TestSmiCompare(masm, &exit, 0x90, Smi::kMinValue, 0);
  TestSmiCompare(masm, &exit, 0xA0, 0, Smi::kMaxValue);
  TestSmiCompare(masm, &exit, 0xB0, Smi::kMaxValue, 0);
  TestSmiCompare(masm, &exit, 0xC0, -1, Smi::kMinValue);
  TestSmiCompare(masm, &exit, 0xD0, Smi::kMinValue, -1);
  TestSmiCompare(masm, &exit, 0xE0, -1, Smi::kMaxValue);
  TestSmiCompare(masm, &exit, 0xF0, Smi::kMaxValue, -1);
  TestSmiCompare(masm, &exit, 0x100, Smi::kMinValue, Smi::kMinValue);
  TestSmiCompare(masm, &exit, 0x110, Smi::kMinValue, Smi::kMaxValue);
  TestSmiCompare(masm, &exit, 0x120, Smi::kMaxValue, Smi::kMinValue);
  TestSmiCompare(masm, &exit, 0x130, Smi::kMaxValue, Smi::kMaxValue);

  __ xorq(rax, rax);  // Success.
  __ bind(&exit);
  ExitCode(masm);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(isolate, &desc);
  MakeAssemblerBufferExecutable(buffer, allocated);
  // Call the function from C++.
  auto f = GeneratedCode<F0>::FromAddress(CcTest::i_isolate(), buffer);
  int result = f.Call();
  CHECK_EQ(0, result);
}



TEST(Integer32ToSmi) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  MacroAssembler assembler(isolate, buffer, static_cast<int>(allocated),
                           v8::internal::CodeObjectRequired::kYes);

  MacroAssembler* masm = &assembler;
  EntryCode(masm);
  Label exit;

  __ movq(rax, Immediate(1));  // Test number.
  __ movl(rcx, Immediate(0));
  __ Integer32ToSmi(rcx, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::kZero));
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);

  __ movq(rax, Immediate(2));  // Test number.
  __ movl(rcx, Immediate(1024));
  __ Integer32ToSmi(rcx, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(1024)));
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);

  __ movq(rax, Immediate(3));  // Test number.
  __ movl(rcx, Immediate(-1));
  __ Integer32ToSmi(rcx, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(-1)));
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);

  __ movq(rax, Immediate(4));  // Test number.
  __ movl(rcx, Immediate(Smi::kMaxValue));
  __ Integer32ToSmi(rcx, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(Smi::kMaxValue)));
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);

  __ movq(rax, Immediate(5));  // Test number.
  __ movl(rcx, Immediate(Smi::kMinValue));
  __ Integer32ToSmi(rcx, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(Smi::kMinValue)));
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);

  // Different target register.

  __ movq(rax, Immediate(6));  // Test number.
  __ movl(rcx, Immediate(0));
  __ Integer32ToSmi(r8, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::kZero));
  __ cmpq(r8, rdx);
  __ j(not_equal, &exit);

  __ movq(rax, Immediate(7));  // Test number.
  __ movl(rcx, Immediate(1024));
  __ Integer32ToSmi(r8, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(1024)));
  __ cmpq(r8, rdx);
  __ j(not_equal, &exit);

  __ movq(rax, Immediate(8));  // Test number.
  __ movl(rcx, Immediate(-1));
  __ Integer32ToSmi(r8, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(-1)));
  __ cmpq(r8, rdx);
  __ j(not_equal, &exit);

  __ movq(rax, Immediate(9));  // Test number.
  __ movl(rcx, Immediate(Smi::kMaxValue));
  __ Integer32ToSmi(r8, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(Smi::kMaxValue)));
  __ cmpq(r8, rdx);
  __ j(not_equal, &exit);

  __ movq(rax, Immediate(10));  // Test number.
  __ movl(rcx, Immediate(Smi::kMinValue));
  __ Integer32ToSmi(r8, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(Smi::kMinValue)));
  __ cmpq(r8, rdx);
  __ j(not_equal, &exit);


  __ xorq(rax, rax);  // Success.
  __ bind(&exit);
  ExitCode(masm);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(isolate, &desc);
  MakeAssemblerBufferExecutable(buffer, allocated);
  // Call the function from C++.
  auto f = GeneratedCode<F0>::FromAddress(CcTest::i_isolate(), buffer);
  int result = f.Call();
  CHECK_EQ(0, result);
}

TEST(SmiCheck) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  MacroAssembler assembler(isolate, buffer, static_cast<int>(allocated),
                           v8::internal::CodeObjectRequired::kYes);

  MacroAssembler* masm = &assembler;
  EntryCode(masm);
  Label exit;
  Condition cond;

  __ movl(rax, Immediate(1));  // Test number.

  // CheckSmi

  __ movl(rcx, Immediate(0));
  __ Integer32ToSmi(rcx, rcx);
  cond = masm->CheckSmi(rcx);
  __ j(NegateCondition(cond), &exit);

  __ incq(rax);
  __ xorq(rcx, Immediate(kSmiTagMask));
  cond = masm->CheckSmi(rcx);
  __ j(cond, &exit);

  __ incq(rax);
  __ movl(rcx, Immediate(-1));
  __ Integer32ToSmi(rcx, rcx);
  cond = masm->CheckSmi(rcx);
  __ j(NegateCondition(cond), &exit);

  __ incq(rax);
  __ xorq(rcx, Immediate(kSmiTagMask));
  cond = masm->CheckSmi(rcx);
  __ j(cond, &exit);

  __ incq(rax);
  __ movl(rcx, Immediate(Smi::kMaxValue));
  __ Integer32ToSmi(rcx, rcx);
  cond = masm->CheckSmi(rcx);
  __ j(NegateCondition(cond), &exit);

  __ incq(rax);
  __ xorq(rcx, Immediate(kSmiTagMask));
  cond = masm->CheckSmi(rcx);
  __ j(cond, &exit);

  __ incq(rax);
  __ movl(rcx, Immediate(Smi::kMinValue));
  __ Integer32ToSmi(rcx, rcx);
  cond = masm->CheckSmi(rcx);
  __ j(NegateCondition(cond), &exit);

  __ incq(rax);
  __ xorq(rcx, Immediate(kSmiTagMask));
  cond = masm->CheckSmi(rcx);
  __ j(cond, &exit);

  // Success
  __ xorq(rax, rax);

  __ bind(&exit);
  ExitCode(masm);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(isolate, &desc);
  MakeAssemblerBufferExecutable(buffer, allocated);
  // Call the function from C++.
  auto f = GeneratedCode<F0>::FromAddress(CcTest::i_isolate(), buffer);
  int result = f.Call();
  CHECK_EQ(0, result);
}

void TestSmiIndex(MacroAssembler* masm, Label* exit, int id, int x) {
  __ movl(rax, Immediate(id));

  for (int i = 0; i < 8; i++) {
    __ Move(rcx, Smi::FromInt(x));
    SmiIndex index = masm->SmiToIndex(rdx, rcx, i);
    CHECK(index.reg == rcx || index.reg == rdx);
    __ shlq(index.reg, Immediate(index.scale));
    __ Set(r8, static_cast<intptr_t>(x) << i);
    __ cmpq(index.reg, r8);
    __ j(not_equal, exit);
    __ incq(rax);
    __ Move(rcx, Smi::FromInt(x));
    index = masm->SmiToIndex(rcx, rcx, i);
    CHECK(index.reg == rcx);
    __ shlq(rcx, Immediate(index.scale));
    __ Set(r8, static_cast<intptr_t>(x) << i);
    __ cmpq(rcx, r8);
    __ j(not_equal, exit);
    __ incq(rax);
  }
}

TEST(SmiIndex) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  MacroAssembler assembler(isolate, buffer, static_cast<int>(allocated),
                           v8::internal::CodeObjectRequired::kYes);

  MacroAssembler* masm = &assembler;
  EntryCode(masm);
  Label exit;

  TestSmiIndex(masm, &exit, 0x10, 0);
  TestSmiIndex(masm, &exit, 0x20, 1);
  TestSmiIndex(masm, &exit, 0x30, 100);
  TestSmiIndex(masm, &exit, 0x40, 1000);
  TestSmiIndex(masm, &exit, 0x50, Smi::kMaxValue);

  __ xorq(rax, rax);  // Success.
  __ bind(&exit);
  ExitCode(masm);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(isolate, &desc);
  MakeAssemblerBufferExecutable(buffer, allocated);
  // Call the function from C++.
  auto f = GeneratedCode<F0>::FromAddress(CcTest::i_isolate(), buffer);
  int result = f.Call();
  CHECK_EQ(0, result);
}

TEST(OperandOffset) {
  uint32_t data[256];
  for (uint32_t i = 0; i < 256; i++) { data[i] = i * 0x01010101; }

  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  MacroAssembler assembler(isolate, buffer, static_cast<int>(allocated),
                           v8::internal::CodeObjectRequired::kYes);

  MacroAssembler* masm = &assembler;
  Label exit;

  EntryCode(masm);
  __ pushq(r13);
  __ pushq(r14);
  __ pushq(rbx);
  __ pushq(rbp);
  __ pushq(Immediate(0x100));  // <-- rbp
  __ movq(rbp, rsp);
  __ pushq(Immediate(0x101));
  __ pushq(Immediate(0x102));
  __ pushq(Immediate(0x103));
  __ pushq(Immediate(0x104));
  __ pushq(Immediate(0x105));  // <-- rbx
  __ pushq(Immediate(0x106));
  __ pushq(Immediate(0x107));
  __ pushq(Immediate(0x108));
  __ pushq(Immediate(0x109));  // <-- rsp
  // rbp = rsp[9]
  // r15 = rsp[3]
  // rbx = rsp[5]
  // r13 = rsp[7]
  __ leaq(r14, Operand(rsp, 3 * kPointerSize));
  __ leaq(r13, Operand(rbp, -3 * kPointerSize));
  __ leaq(rbx, Operand(rbp, -5 * kPointerSize));
  __ movl(rcx, Immediate(2));
  __ Move(r8, reinterpret_cast<Address>(&data[128]), RelocInfo::NONE);
  __ movl(rax, Immediate(1));

  Operand sp0 = Operand(rsp, 0);

  // Test 1.
  __ movl(rdx, sp0);  // Sanity check.
  __ cmpl(rdx, Immediate(0x109));
  __ j(not_equal, &exit);
  __ incq(rax);

  // Test 2.
  // Zero to non-zero displacement.
  __ movl(rdx, Operand(sp0, 2 * kPointerSize));
  __ cmpl(rdx, Immediate(0x107));
  __ j(not_equal, &exit);
  __ incq(rax);

  Operand sp2 = Operand(rsp, 2 * kPointerSize);

  // Test 3.
  __ movl(rdx, sp2);  // Sanity check.
  __ cmpl(rdx, Immediate(0x107));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(sp2, 2 * kPointerSize));
  __ cmpl(rdx, Immediate(0x105));
  __ j(not_equal, &exit);
  __ incq(rax);

  // Non-zero to zero displacement.
  __ movl(rdx, Operand(sp2, -2 * kPointerSize));
  __ cmpl(rdx, Immediate(0x109));
  __ j(not_equal, &exit);
  __ incq(rax);

  Operand sp2c2 = Operand(rsp, rcx, times_pointer_size, 2 * kPointerSize);

  // Test 6.
  __ movl(rdx, sp2c2);  // Sanity check.
  __ cmpl(rdx, Immediate(0x105));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(sp2c2, 2 * kPointerSize));
  __ cmpl(rdx, Immediate(0x103));
  __ j(not_equal, &exit);
  __ incq(rax);

  // Non-zero to zero displacement.
  __ movl(rdx, Operand(sp2c2, -2 * kPointerSize));
  __ cmpl(rdx, Immediate(0x107));
  __ j(not_equal, &exit);
  __ incq(rax);


  Operand bp0 = Operand(rbp, 0);

  // Test 9.
  __ movl(rdx, bp0);  // Sanity check.
  __ cmpl(rdx, Immediate(0x100));
  __ j(not_equal, &exit);
  __ incq(rax);

  // Zero to non-zero displacement.
  __ movl(rdx, Operand(bp0, -2 * kPointerSize));
  __ cmpl(rdx, Immediate(0x102));
  __ j(not_equal, &exit);
  __ incq(rax);

  Operand bp2 = Operand(rbp, -2 * kPointerSize);

  // Test 11.
  __ movl(rdx, bp2);  // Sanity check.
  __ cmpl(rdx, Immediate(0x102));
  __ j(not_equal, &exit);
  __ incq(rax);

  // Non-zero to zero displacement.
  __ movl(rdx, Operand(bp2, 2 * kPointerSize));
  __ cmpl(rdx, Immediate(0x100));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(bp2, -2 * kPointerSize));
  __ cmpl(rdx, Immediate(0x104));
  __ j(not_equal, &exit);
  __ incq(rax);

  Operand bp2c4 = Operand(rbp, rcx, times_pointer_size, -4 * kPointerSize);

  // Test 14:
  __ movl(rdx, bp2c4);  // Sanity check.
  __ cmpl(rdx, Immediate(0x102));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(bp2c4, 2 * kPointerSize));
  __ cmpl(rdx, Immediate(0x100));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(bp2c4, -2 * kPointerSize));
  __ cmpl(rdx, Immediate(0x104));
  __ j(not_equal, &exit);
  __ incq(rax);

  Operand bx0 = Operand(rbx, 0);

  // Test 17.
  __ movl(rdx, bx0);  // Sanity check.
  __ cmpl(rdx, Immediate(0x105));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(bx0, 5 * kPointerSize));
  __ cmpl(rdx, Immediate(0x100));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(bx0, -4 * kPointerSize));
  __ cmpl(rdx, Immediate(0x109));
  __ j(not_equal, &exit);
  __ incq(rax);

  Operand bx2 = Operand(rbx, 2 * kPointerSize);

  // Test 20.
  __ movl(rdx, bx2);  // Sanity check.
  __ cmpl(rdx, Immediate(0x103));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(bx2, 2 * kPointerSize));
  __ cmpl(rdx, Immediate(0x101));
  __ j(not_equal, &exit);
  __ incq(rax);

  // Non-zero to zero displacement.
  __ movl(rdx, Operand(bx2, -2 * kPointerSize));
  __ cmpl(rdx, Immediate(0x105));
  __ j(not_equal, &exit);
  __ incq(rax);

  Operand bx2c2 = Operand(rbx, rcx, times_pointer_size, -2 * kPointerSize);

  // Test 23.
  __ movl(rdx, bx2c2);  // Sanity check.
  __ cmpl(rdx, Immediate(0x105));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(bx2c2, 2 * kPointerSize));
  __ cmpl(rdx, Immediate(0x103));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(bx2c2, -2 * kPointerSize));
  __ cmpl(rdx, Immediate(0x107));
  __ j(not_equal, &exit);
  __ incq(rax);

  Operand r80 = Operand(r8, 0);

  // Test 26.
  __ movl(rdx, r80);  // Sanity check.
  __ cmpl(rdx, Immediate(0x80808080));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(r80, -8 * kIntSize));
  __ cmpl(rdx, Immediate(0x78787878));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(r80, 8 * kIntSize));
  __ cmpl(rdx, Immediate(0x88888888));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(r80, -64 * kIntSize));
  __ cmpl(rdx, Immediate(0x40404040));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(r80, 64 * kIntSize));
  __ cmpl(rdx, Immediate(0xC0C0C0C0));
  __ j(not_equal, &exit);
  __ incq(rax);

  Operand r88 = Operand(r8, 8 * kIntSize);

  // Test 31.
  __ movl(rdx, r88);  // Sanity check.
  __ cmpl(rdx, Immediate(0x88888888));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(r88, -8 * kIntSize));
  __ cmpl(rdx, Immediate(0x80808080));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(r88, 8 * kIntSize));
  __ cmpl(rdx, Immediate(0x90909090));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(r88, -64 * kIntSize));
  __ cmpl(rdx, Immediate(0x48484848));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(r88, 64 * kIntSize));
  __ cmpl(rdx, Immediate(0xC8C8C8C8));
  __ j(not_equal, &exit);
  __ incq(rax);


  Operand r864 = Operand(r8, 64 * kIntSize);

  // Test 36.
  __ movl(rdx, r864);  // Sanity check.
  __ cmpl(rdx, Immediate(0xC0C0C0C0));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(r864, -8 * kIntSize));
  __ cmpl(rdx, Immediate(0xB8B8B8B8));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(r864, 8 * kIntSize));
  __ cmpl(rdx, Immediate(0xC8C8C8C8));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(r864, -64 * kIntSize));
  __ cmpl(rdx, Immediate(0x80808080));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(r864, 32 * kIntSize));
  __ cmpl(rdx, Immediate(0xE0E0E0E0));
  __ j(not_equal, &exit);
  __ incq(rax);

  // 32-bit offset to 8-bit offset.
  __ movl(rdx, Operand(r864, -60 * kIntSize));
  __ cmpl(rdx, Immediate(0x84848484));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(r864, 60 * kIntSize));
  __ cmpl(rdx, Immediate(0xFCFCFCFC));
  __ j(not_equal, &exit);
  __ incq(rax);

  // Test unaligned offsets.

  // Test 43.
  __ movl(rdx, Operand(r80, 2));
  __ cmpl(rdx, Immediate(0x81818080));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(r80, -2));
  __ cmpl(rdx, Immediate(0x80807F7F));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(r80, 126));
  __ cmpl(rdx, Immediate(0xA0A09F9F));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(r80, -126));
  __ cmpl(rdx, Immediate(0x61616060));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(r80, 254));
  __ cmpl(rdx, Immediate(0xC0C0BFBF));
  __ j(not_equal, &exit);
  __ incq(rax);

  __ movl(rdx, Operand(r80, -254));
  __ cmpl(rdx, Immediate(0x41414040));
  __ j(not_equal, &exit);
  __ incq(rax);

  // Success.

  __ movl(rax, Immediate(0));
  __ bind(&exit);
  __ leaq(rsp, Operand(rbp, kPointerSize));
  __ popq(rbp);
  __ popq(rbx);
  __ popq(r14);
  __ popq(r13);
  ExitCode(masm);
  __ ret(0);


  CodeDesc desc;
  masm->GetCode(isolate, &desc);
  MakeAssemblerBufferExecutable(buffer, allocated);
  // Call the function from C++.
  auto f = GeneratedCode<F0>::FromAddress(CcTest::i_isolate(), buffer);
  int result = f.Call();
  CHECK_EQ(0, result);
}


TEST(LoadAndStoreWithRepresentation) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  MacroAssembler assembler(isolate, buffer, static_cast<int>(allocated),
                           v8::internal::CodeObjectRequired::kYes);

  MacroAssembler* masm = &assembler;  // Create a pointer for the __ macro.
  EntryCode(masm);
  __ subq(rsp, Immediate(1 * kPointerSize));
  Label exit;

  // Test 1.
  __ movq(rax, Immediate(1));  // Test number.
  __ movq(Operand(rsp, 0 * kPointerSize), Immediate(0));
  __ movq(rcx, Immediate(-1));
  __ Store(Operand(rsp, 0 * kPointerSize), rcx, Representation::UInteger8());
  __ movq(rcx, Operand(rsp, 0 * kPointerSize));
  __ movl(rdx, Immediate(255));
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);
  __ Load(rdx, Operand(rsp, 0 * kPointerSize), Representation::UInteger8());
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);

  // Test 2.
  __ movq(rax, Immediate(2));  // Test number.
  __ movq(Operand(rsp, 0 * kPointerSize), Immediate(0));
  __ Set(rcx, V8_2PART_UINT64_C(0xDEADBEAF, 12345678));
  __ Store(Operand(rsp, 0 * kPointerSize), rcx, Representation::Smi());
  __ movq(rcx, Operand(rsp, 0 * kPointerSize));
  __ Set(rdx, V8_2PART_UINT64_C(0xDEADBEAF, 12345678));
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);
  __ Load(rdx, Operand(rsp, 0 * kPointerSize), Representation::Smi());
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);

  // Test 3.
  __ movq(rax, Immediate(3));  // Test number.
  __ movq(Operand(rsp, 0 * kPointerSize), Immediate(0));
  __ movq(rcx, Immediate(-1));
  __ Store(Operand(rsp, 0 * kPointerSize), rcx, Representation::Integer32());
  __ movq(rcx, Operand(rsp, 0 * kPointerSize));
  __ movl(rdx, Immediate(-1));
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);
  __ Load(rdx, Operand(rsp, 0 * kPointerSize), Representation::Integer32());
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);

  // Test 4.
  __ movq(rax, Immediate(4));  // Test number.
  __ movq(Operand(rsp, 0 * kPointerSize), Immediate(0));
  __ movl(rcx, Immediate(0x44332211));
  __ Store(Operand(rsp, 0 * kPointerSize), rcx, Representation::HeapObject());
  __ movq(rcx, Operand(rsp, 0 * kPointerSize));
  __ movl(rdx, Immediate(0x44332211));
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);
  __ Load(rdx, Operand(rsp, 0 * kPointerSize), Representation::HeapObject());
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);

  // Test 5.
  __ movq(rax, Immediate(5));  // Test number.
  __ movq(Operand(rsp, 0 * kPointerSize), Immediate(0));
  __ Set(rcx, V8_2PART_UINT64_C(0x12345678, DEADBEAF));
  __ Store(Operand(rsp, 0 * kPointerSize), rcx, Representation::Tagged());
  __ movq(rcx, Operand(rsp, 0 * kPointerSize));
  __ Set(rdx, V8_2PART_UINT64_C(0x12345678, DEADBEAF));
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);
  __ Load(rdx, Operand(rsp, 0 * kPointerSize), Representation::Tagged());
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);

  // Test 6.
  __ movq(rax, Immediate(6));  // Test number.
  __ movq(Operand(rsp, 0 * kPointerSize), Immediate(0));
  __ Set(rcx, V8_2PART_UINT64_C(0x11223344, 55667788));
  __ Store(Operand(rsp, 0 * kPointerSize), rcx, Representation::External());
  __ movq(rcx, Operand(rsp, 0 * kPointerSize));
  __ Set(rdx, V8_2PART_UINT64_C(0x11223344, 55667788));
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);
  __ Load(rdx, Operand(rsp, 0 * kPointerSize), Representation::External());
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);

  // Test 7.
  __ movq(rax, Immediate(7));  // Test number.
  __ movq(Operand(rsp, 0 * kPointerSize), Immediate(0));
  __ movq(rcx, Immediate(-1));
  __ Store(Operand(rsp, 0 * kPointerSize), rcx, Representation::Integer8());
  __ movq(rcx, Operand(rsp, 0 * kPointerSize));
  __ movl(rdx, Immediate(255));
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);
  __ Load(rdx, Operand(rsp, 0 * kPointerSize), Representation::Integer8());
  __ movq(rcx, Immediate(-1));
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);

  // Test 8.
  __ movq(rax, Immediate(8));  // Test number.
  __ movq(Operand(rsp, 0 * kPointerSize), Immediate(0));
  __ movq(rcx, Immediate(-1));
  __ Store(Operand(rsp, 0 * kPointerSize), rcx, Representation::Integer16());
  __ movq(rcx, Operand(rsp, 0 * kPointerSize));
  __ movl(rdx, Immediate(65535));
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);
  __ Load(rdx, Operand(rsp, 0 * kPointerSize), Representation::Integer16());
  __ movq(rcx, Immediate(-1));
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);

  // Test 9.
  __ movq(rax, Immediate(9));  // Test number.
  __ movq(Operand(rsp, 0 * kPointerSize), Immediate(0));
  __ movq(rcx, Immediate(-1));
  __ Store(Operand(rsp, 0 * kPointerSize), rcx, Representation::UInteger16());
  __ movq(rcx, Operand(rsp, 0 * kPointerSize));
  __ movl(rdx, Immediate(65535));
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);
  __ Load(rdx, Operand(rsp, 0 * kPointerSize), Representation::UInteger16());
  __ cmpq(rcx, rdx);
  __ j(not_equal, &exit);

  __ xorq(rax, rax);  // Success.
  __ bind(&exit);
  __ addq(rsp, Immediate(1 * kPointerSize));
  ExitCode(masm);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(isolate, &desc);
  MakeAssemblerBufferExecutable(buffer, allocated);
  // Call the function from C++.
  auto f = GeneratedCode<F0>::FromAddress(CcTest::i_isolate(), buffer);
  int result = f.Call();
  CHECK_EQ(0, result);
}

void TestFloat32x4Abs(MacroAssembler* masm, Label* exit, float x, float y,
                      float z, float w) {
  __ subq(rsp, Immediate(kSimd128Size));

  __ Move(xmm1, x);
  __ Movss(Operand(rsp, 0 * kFloatSize), xmm1);
  __ Move(xmm2, y);
  __ Movss(Operand(rsp, 1 * kFloatSize), xmm2);
  __ Move(xmm3, z);
  __ Movss(Operand(rsp, 2 * kFloatSize), xmm3);
  __ Move(xmm4, w);
  __ Movss(Operand(rsp, 3 * kFloatSize), xmm4);
  __ Movups(xmm0, Operand(rsp, 0));

  __ Absps(xmm0);
  __ Movups(Operand(rsp, 0), xmm0);

  __ incq(rax);
  __ Move(xmm1, fabsf(x));
  __ Ucomiss(xmm1, Operand(rsp, 0 * kFloatSize));
  __ j(not_equal, exit);
  __ incq(rax);
  __ Move(xmm2, fabsf(y));
  __ Ucomiss(xmm2, Operand(rsp, 1 * kFloatSize));
  __ j(not_equal, exit);
  __ incq(rax);
  __ Move(xmm3, fabsf(z));
  __ Ucomiss(xmm3, Operand(rsp, 2 * kFloatSize));
  __ j(not_equal, exit);
  __ incq(rax);
  __ Move(xmm4, fabsf(w));
  __ Ucomiss(xmm4, Operand(rsp, 3 * kFloatSize));
  __ j(not_equal, exit);

  __ addq(rsp, Immediate(kSimd128Size));
}

void TestFloat32x4Neg(MacroAssembler* masm, Label* exit, float x, float y,
                      float z, float w) {
  __ subq(rsp, Immediate(kSimd128Size));

  __ Move(xmm1, x);
  __ Movss(Operand(rsp, 0 * kFloatSize), xmm1);
  __ Move(xmm2, y);
  __ Movss(Operand(rsp, 1 * kFloatSize), xmm2);
  __ Move(xmm3, z);
  __ Movss(Operand(rsp, 2 * kFloatSize), xmm3);
  __ Move(xmm4, w);
  __ Movss(Operand(rsp, 3 * kFloatSize), xmm4);
  __ Movups(xmm0, Operand(rsp, 0));

  __ Negps(xmm0);
  __ Movups(Operand(rsp, 0), xmm0);

  __ incq(rax);
  __ Move(xmm1, -x);
  __ Ucomiss(xmm1, Operand(rsp, 0 * kFloatSize));
  __ j(not_equal, exit);
  __ incq(rax);
  __ Move(xmm2, -y);
  __ Ucomiss(xmm2, Operand(rsp, 1 * kFloatSize));
  __ j(not_equal, exit);
  __ incq(rax);
  __ Move(xmm3, -z);
  __ Ucomiss(xmm3, Operand(rsp, 2 * kFloatSize));
  __ j(not_equal, exit);
  __ incq(rax);
  __ Move(xmm4, -w);
  __ Ucomiss(xmm4, Operand(rsp, 3 * kFloatSize));
  __ j(not_equal, exit);

  __ addq(rsp, Immediate(kSimd128Size));
}

void TestFloat64x2Abs(MacroAssembler* masm, Label* exit, double x, double y) {
  __ subq(rsp, Immediate(kSimd128Size));

  __ Move(xmm1, x);
  __ Movsd(Operand(rsp, 0 * kDoubleSize), xmm1);
  __ Move(xmm2, y);
  __ Movsd(Operand(rsp, 1 * kDoubleSize), xmm2);
  __ movupd(xmm0, Operand(rsp, 0));

  __ Abspd(xmm0);
  __ movupd(Operand(rsp, 0), xmm0);

  __ incq(rax);
  __ Move(xmm1, fabs(x));
  __ Ucomisd(xmm1, Operand(rsp, 0 * kDoubleSize));
  __ j(not_equal, exit);
  __ incq(rax);
  __ Move(xmm2, fabs(y));
  __ Ucomisd(xmm2, Operand(rsp, 1 * kDoubleSize));
  __ j(not_equal, exit);

  __ addq(rsp, Immediate(kSimd128Size));
}

void TestFloat64x2Neg(MacroAssembler* masm, Label* exit, double x, double y) {
  __ subq(rsp, Immediate(kSimd128Size));

  __ Move(xmm1, x);
  __ Movsd(Operand(rsp, 0 * kDoubleSize), xmm1);
  __ Move(xmm2, y);
  __ Movsd(Operand(rsp, 1 * kDoubleSize), xmm2);
  __ movupd(xmm0, Operand(rsp, 0));

  __ Negpd(xmm0);
  __ movupd(Operand(rsp, 0), xmm0);

  __ incq(rax);
  __ Move(xmm1, -x);
  __ Ucomisd(xmm1, Operand(rsp, 0 * kDoubleSize));
  __ j(not_equal, exit);
  __ incq(rax);
  __ Move(xmm2, -y);
  __ Ucomisd(xmm2, Operand(rsp, 1 * kDoubleSize));
  __ j(not_equal, exit);

  __ addq(rsp, Immediate(kSimd128Size));
}

TEST(SIMDMacros) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  MacroAssembler assembler(isolate, buffer, static_cast<int>(allocated),
                           v8::internal::CodeObjectRequired::kYes);

  MacroAssembler* masm = &assembler;
  EntryCode(masm);
  Label exit;

  __ xorq(rax, rax);
  TestFloat32x4Abs(masm, &exit, 1.5, -1.5, 0.5, -0.5);
  TestFloat32x4Neg(masm, &exit, 1.5, -1.5, 0.5, -0.5);
  TestFloat64x2Abs(masm, &exit, 1.75, -1.75);
  TestFloat64x2Neg(masm, &exit, 1.75, -1.75);

  __ xorq(rax, rax);  // Success.
  __ bind(&exit);
  ExitCode(masm);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(isolate, &desc);
  MakeAssemblerBufferExecutable(buffer, allocated);
  // Call the function from C++.
  auto f = GeneratedCode<F0>::FromAddress(CcTest::i_isolate(), buffer);
  int result = f.Call();
  CHECK_EQ(0, result);
}

#undef __

}  // namespace test_macro_assembler_x64
}  // namespace internal
}  // namespace v8
