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

#include "v8.h"

#include "macro-assembler.h"
#include "factory.h"
#include "platform.h"
#include "serialize.h"
#include "cctest.h"

using v8::internal::byte;
using v8::internal::OS;
using v8::internal::Assembler;
using v8::internal::Condition;
using v8::internal::MacroAssembler;
using v8::internal::HandleScope;
using v8::internal::Operand;
using v8::internal::Immediate;
using v8::internal::SmiIndex;
using v8::internal::Label;
using v8::internal::RelocInfo;
using v8::internal::rax;
using v8::internal::rbx;
using v8::internal::rsi;
using v8::internal::rdi;
using v8::internal::rcx;
using v8::internal::rdx;
using v8::internal::rbp;
using v8::internal::rsp;
using v8::internal::r8;
using v8::internal::r9;
using v8::internal::r11;
using v8::internal::r12;  // Remember: r12..r15 are callee save!
using v8::internal::r13;
using v8::internal::r14;
using v8::internal::r15;
using v8::internal::FUNCTION_CAST;
using v8::internal::CodeDesc;
using v8::internal::less_equal;
using v8::internal::not_equal;
using v8::internal::not_zero;
using v8::internal::greater;
using v8::internal::greater_equal;
using v8::internal::carry;
using v8::internal::not_carry;
using v8::internal::negative;
using v8::internal::positive;
using v8::internal::Smi;
using v8::internal::kSmiTagMask;
using v8::internal::kSmiValueSize;

// Test the x64 assembler by compiling some simple functions into
// a buffer and executing them.  These tests do not initialize the
// V8 library, create a context, or use any V8 objects.
// The AMD64 calling convention is used, with the first five arguments
// in RSI, RDI, RDX, RCX, R8, and R9, and floating point arguments in
// the XMM registers.  The return value is in RAX.
// This calling convention is used on Linux, with GCC, and on Mac OS,
// with GCC.  A different convention is used on 64-bit windows.

typedef int (*F0)();

#define __ masm->

TEST(Smi) {
  // Check that C++ Smi operations work as expected.
  intptr_t test_numbers[] = {
      0, 1, -1, 127, 128, -128, -129, 255, 256, -256, -257,
      Smi::kMaxValue, static_cast<intptr_t>(Smi::kMaxValue) + 1,
      Smi::kMinValue, static_cast<intptr_t>(Smi::kMinValue) - 1
  };
  int test_number_count = 15;
  for (int i = 0; i < test_number_count; i++) {
    intptr_t number = test_numbers[i];
    bool is_valid = Smi::IsValid(number);
    bool is_in_range = number >= Smi::kMinValue && number <= Smi::kMaxValue;
    CHECK_EQ(is_in_range, is_valid);
    if (is_valid) {
      Smi* smi_from_intptr = Smi::FromIntptr(number);
      if (static_cast<int>(number) == number) {  // Is a 32-bit int.
        Smi* smi_from_int = Smi::FromInt(static_cast<int32_t>(number));
        CHECK_EQ(smi_from_int, smi_from_intptr);
      }
      int smi_value = smi_from_intptr->value();
      CHECK_EQ(number, static_cast<intptr_t>(smi_value));
    }
  }
}


static void TestMoveSmi(MacroAssembler* masm, Label* exit, int id, Smi* value) {
  __ movl(rax, Immediate(id));
  __ Move(rcx, Smi::FromInt(0));
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(0)));
  __ cmpq(rcx, rdx);
  __ j(not_equal, exit);
}


// Test that we can move a Smi value literally into a register.
TEST(SmiMove) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                   &actual_size,
                                                   true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));
  MacroAssembler* masm = &assembler;  // Create a pointer for the __ macro.
  masm->set_allow_stub_calls(false);
  Label exit;

  TestMoveSmi(masm, &exit, 1, Smi::FromInt(0));
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

  __ xor_(rax, rax);  // Success.
  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
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
    ASSERT_EQ(x, y);
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
      ASSERT(y > x);
      __ movl(rax, Immediate(id + 10));
      __ j(less_equal, exit);
    }
  } else {
    __ SmiCompare(rcx, rcx);
    __ movl(rax, Immediate(id + 11));
    __ j(not_equal, exit);
    __ incq(rax);
    __ cmpq(rcx, r8);
    __ j(not_equal, exit);
  }
}


// Test that we can compare smis for equality (and more).
TEST(SmiCompare) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                      &actual_size,
                                      true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
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

  __ xor_(rax, rax);  // Success.
  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}



TEST(Integer32ToSmi) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
  Label exit;

  __ movq(rax, Immediate(1));  // Test number.
  __ movl(rcx, Immediate(0));
  __ Integer32ToSmi(rcx, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(0)));
  __ SmiCompare(rcx, rdx);
  __ j(not_equal, &exit);

  __ movq(rax, Immediate(2));  // Test number.
  __ movl(rcx, Immediate(1024));
  __ Integer32ToSmi(rcx, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(1024)));
  __ SmiCompare(rcx, rdx);
  __ j(not_equal, &exit);

  __ movq(rax, Immediate(3));  // Test number.
  __ movl(rcx, Immediate(-1));
  __ Integer32ToSmi(rcx, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(-1)));
  __ SmiCompare(rcx, rdx);
  __ j(not_equal, &exit);

  __ movq(rax, Immediate(4));  // Test number.
  __ movl(rcx, Immediate(Smi::kMaxValue));
  __ Integer32ToSmi(rcx, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(Smi::kMaxValue)));
  __ SmiCompare(rcx, rdx);
  __ j(not_equal, &exit);

  __ movq(rax, Immediate(5));  // Test number.
  __ movl(rcx, Immediate(Smi::kMinValue));
  __ Integer32ToSmi(rcx, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(Smi::kMinValue)));
  __ SmiCompare(rcx, rdx);
  __ j(not_equal, &exit);

  // Different target register.

  __ movq(rax, Immediate(6));  // Test number.
  __ movl(rcx, Immediate(0));
  __ Integer32ToSmi(r8, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(0)));
  __ SmiCompare(r8, rdx);
  __ j(not_equal, &exit);

  __ movq(rax, Immediate(7));  // Test number.
  __ movl(rcx, Immediate(1024));
  __ Integer32ToSmi(r8, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(1024)));
  __ SmiCompare(r8, rdx);
  __ j(not_equal, &exit);

  __ movq(rax, Immediate(8));  // Test number.
  __ movl(rcx, Immediate(-1));
  __ Integer32ToSmi(r8, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(-1)));
  __ SmiCompare(r8, rdx);
  __ j(not_equal, &exit);

  __ movq(rax, Immediate(9));  // Test number.
  __ movl(rcx, Immediate(Smi::kMaxValue));
  __ Integer32ToSmi(r8, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(Smi::kMaxValue)));
  __ SmiCompare(r8, rdx);
  __ j(not_equal, &exit);

  __ movq(rax, Immediate(10));  // Test number.
  __ movl(rcx, Immediate(Smi::kMinValue));
  __ Integer32ToSmi(r8, rcx);
  __ Set(rdx, reinterpret_cast<intptr_t>(Smi::FromInt(Smi::kMinValue)));
  __ SmiCompare(r8, rdx);
  __ j(not_equal, &exit);


  __ xor_(rax, rax);  // Success.
  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}


void TestI64PlusConstantToSmi(MacroAssembler* masm,
                              Label* exit,
                              int id,
                              int64_t x,
                              int y) {
  int64_t result = x + y;
  ASSERT(Smi::IsValid(result));
  __ movl(rax, Immediate(id));
  __ Move(r8, Smi::FromInt(static_cast<int>(result)));
  __ movq(rcx, x, RelocInfo::NONE);
  __ movq(r11, rcx);
  __ Integer64PlusConstantToSmi(rdx, rcx, y);
  __ SmiCompare(rdx, r8);
  __ j(not_equal, exit);

  __ incq(rax);
  __ SmiCompare(r11, rcx);
  __ j(not_equal, exit);

  __ incq(rax);
  __ Integer64PlusConstantToSmi(rcx, rcx, y);
  __ SmiCompare(rcx, r8);
  __ j(not_equal, exit);
}


TEST(Integer64PlusConstantToSmi) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
  Label exit;

  int64_t twice_max = static_cast<int64_t>(Smi::kMaxValue) * 2;

  TestI64PlusConstantToSmi(masm, &exit, 0x10, 0, 0);
  TestI64PlusConstantToSmi(masm, &exit, 0x20, 0, 1);
  TestI64PlusConstantToSmi(masm, &exit, 0x30, 1, 0);
  TestI64PlusConstantToSmi(masm, &exit, 0x40, Smi::kMaxValue - 5, 5);
  TestI64PlusConstantToSmi(masm, &exit, 0x50, Smi::kMinValue + 5, 5);
  TestI64PlusConstantToSmi(masm, &exit, 0x60, twice_max, -Smi::kMaxValue);
  TestI64PlusConstantToSmi(masm, &exit, 0x70, -twice_max, Smi::kMaxValue);
  TestI64PlusConstantToSmi(masm, &exit, 0x80, 0, Smi::kMinValue);
  TestI64PlusConstantToSmi(masm, &exit, 0x90, 0, Smi::kMaxValue);
  TestI64PlusConstantToSmi(masm, &exit, 0xA0, Smi::kMinValue, 0);
  TestI64PlusConstantToSmi(masm, &exit, 0xB0, Smi::kMaxValue, 0);
  TestI64PlusConstantToSmi(masm, &exit, 0xC0, twice_max, Smi::kMinValue);

  __ xor_(rax, rax);  // Success.
  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}


TEST(SmiCheck) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                   &actual_size,
                                                   true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
  Label exit;
  Condition cond;

  __ movl(rax, Immediate(1));  // Test number.

  // CheckSmi

  __ movl(rcx, Immediate(0));
  __ Integer32ToSmi(rcx, rcx);
  cond = masm->CheckSmi(rcx);
  __ j(NegateCondition(cond), &exit);

  __ incq(rax);
  __ xor_(rcx, Immediate(kSmiTagMask));
  cond = masm->CheckSmi(rcx);
  __ j(cond, &exit);

  __ incq(rax);
  __ movl(rcx, Immediate(-1));
  __ Integer32ToSmi(rcx, rcx);
  cond = masm->CheckSmi(rcx);
  __ j(NegateCondition(cond), &exit);

  __ incq(rax);
  __ xor_(rcx, Immediate(kSmiTagMask));
  cond = masm->CheckSmi(rcx);
  __ j(cond, &exit);

  __ incq(rax);
  __ movl(rcx, Immediate(Smi::kMaxValue));
  __ Integer32ToSmi(rcx, rcx);
  cond = masm->CheckSmi(rcx);
  __ j(NegateCondition(cond), &exit);

  __ incq(rax);
  __ xor_(rcx, Immediate(kSmiTagMask));
  cond = masm->CheckSmi(rcx);
  __ j(cond, &exit);

  __ incq(rax);
  __ movl(rcx, Immediate(Smi::kMinValue));
  __ Integer32ToSmi(rcx, rcx);
  cond = masm->CheckSmi(rcx);
  __ j(NegateCondition(cond), &exit);

  __ incq(rax);
  __ xor_(rcx, Immediate(kSmiTagMask));
  cond = masm->CheckSmi(rcx);
  __ j(cond, &exit);

  // CheckPositiveSmi

  __ incq(rax);
  __ movl(rcx, Immediate(0));
  __ Integer32ToSmi(rcx, rcx);
  cond = masm->CheckPositiveSmi(rcx);  // Zero counts as positive.
  __ j(NegateCondition(cond), &exit);

  __ incq(rax);
  __ xor_(rcx, Immediate(kSmiTagMask));
  cond = masm->CheckPositiveSmi(rcx);  // "zero" non-smi.
  __ j(cond, &exit);

  __ incq(rax);
  __ movq(rcx, Immediate(-1));
  __ Integer32ToSmi(rcx, rcx);
  cond = masm->CheckPositiveSmi(rcx);  // Negative smis are not positive.
  __ j(cond, &exit);

  __ incq(rax);
  __ movq(rcx, Immediate(Smi::kMinValue));
  __ Integer32ToSmi(rcx, rcx);
  cond = masm->CheckPositiveSmi(rcx);  // Most negative smi is not positive.
  __ j(cond, &exit);

  __ incq(rax);
  __ xor_(rcx, Immediate(kSmiTagMask));
  cond = masm->CheckPositiveSmi(rcx);  // "Negative" non-smi.
  __ j(cond, &exit);

  __ incq(rax);
  __ movq(rcx, Immediate(Smi::kMaxValue));
  __ Integer32ToSmi(rcx, rcx);
  cond = masm->CheckPositiveSmi(rcx);  // Most positive smi is positive.
  __ j(NegateCondition(cond), &exit);

  __ incq(rax);
  __ xor_(rcx, Immediate(kSmiTagMask));
  cond = masm->CheckPositiveSmi(rcx);  // "Positive" non-smi.
  __ j(cond, &exit);

  // CheckIsMinSmi

  __ incq(rax);
  __ movq(rcx, Immediate(Smi::kMaxValue));
  __ Integer32ToSmi(rcx, rcx);
  cond = masm->CheckIsMinSmi(rcx);
  __ j(cond, &exit);

  __ incq(rax);
  __ movq(rcx, Immediate(0));
  __ Integer32ToSmi(rcx, rcx);
  cond = masm->CheckIsMinSmi(rcx);
  __ j(cond, &exit);

  __ incq(rax);
  __ movq(rcx, Immediate(Smi::kMinValue));
  __ Integer32ToSmi(rcx, rcx);
  cond = masm->CheckIsMinSmi(rcx);
  __ j(NegateCondition(cond), &exit);

  __ incq(rax);
  __ movq(rcx, Immediate(Smi::kMinValue + 1));
  __ Integer32ToSmi(rcx, rcx);
  cond = masm->CheckIsMinSmi(rcx);
  __ j(cond, &exit);

  // CheckBothSmi

  __ incq(rax);
  __ movq(rcx, Immediate(Smi::kMaxValue));
  __ Integer32ToSmi(rcx, rcx);
  __ movq(rdx, Immediate(Smi::kMinValue));
  __ Integer32ToSmi(rdx, rdx);
  cond = masm->CheckBothSmi(rcx, rdx);
  __ j(NegateCondition(cond), &exit);

  __ incq(rax);
  __ xor_(rcx, Immediate(kSmiTagMask));
  cond = masm->CheckBothSmi(rcx, rdx);
  __ j(cond, &exit);

  __ incq(rax);
  __ xor_(rdx, Immediate(kSmiTagMask));
  cond = masm->CheckBothSmi(rcx, rdx);
  __ j(cond, &exit);

  __ incq(rax);
  __ xor_(rcx, Immediate(kSmiTagMask));
  cond = masm->CheckBothSmi(rcx, rdx);
  __ j(cond, &exit);

  __ incq(rax);
  cond = masm->CheckBothSmi(rcx, rcx);
  __ j(NegateCondition(cond), &exit);

  __ incq(rax);
  cond = masm->CheckBothSmi(rdx, rdx);
  __ j(cond, &exit);

  // CheckInteger32ValidSmiValue
  __ incq(rax);
  __ movq(rcx, Immediate(0));
  cond = masm->CheckInteger32ValidSmiValue(rax);
  __ j(NegateCondition(cond), &exit);

  __ incq(rax);
  __ movq(rcx, Immediate(-1));
  cond = masm->CheckInteger32ValidSmiValue(rax);
  __ j(NegateCondition(cond), &exit);

  __ incq(rax);
  __ movq(rcx, Immediate(Smi::kMaxValue));
  cond = masm->CheckInteger32ValidSmiValue(rax);
  __ j(NegateCondition(cond), &exit);

  __ incq(rax);
  __ movq(rcx, Immediate(Smi::kMinValue));
  cond = masm->CheckInteger32ValidSmiValue(rax);
  __ j(NegateCondition(cond), &exit);

  // Success
  __ xor_(rax, rax);

  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}



void TestSmiNeg(MacroAssembler* masm, Label* exit, int id, int x) {
  __ Move(rcx, Smi::FromInt(x));
  __ movq(r11, rcx);
  if (x == Smi::kMinValue || x == 0) {
    // Negation fails.
    __ movl(rax, Immediate(id + 8));
    __ SmiNeg(r9, rcx, exit);

    __ incq(rax);
    __ SmiCompare(r11, rcx);
    __ j(not_equal, exit);

    __ incq(rax);
    __ SmiNeg(rcx, rcx, exit);

    __ incq(rax);
    __ SmiCompare(r11, rcx);
    __ j(not_equal, exit);
  } else {
    Label smi_ok, smi_ok2;
    int result = -x;
    __ movl(rax, Immediate(id));
    __ Move(r8, Smi::FromInt(result));

    __ SmiNeg(r9, rcx, &smi_ok);
    __ jmp(exit);
    __ bind(&smi_ok);
    __ incq(rax);
    __ SmiCompare(r9, r8);
    __ j(not_equal, exit);

    __ incq(rax);
    __ SmiCompare(r11, rcx);
    __ j(not_equal, exit);

    __ incq(rax);
    __ SmiNeg(rcx, rcx, &smi_ok2);
    __ jmp(exit);
    __ bind(&smi_ok2);
    __ incq(rax);
    __ SmiCompare(rcx, r8);
    __ j(not_equal, exit);
  }
}


TEST(SmiNeg) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                      &actual_size,
                                      true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
  Label exit;

  TestSmiNeg(masm, &exit, 0x10, 0);
  TestSmiNeg(masm, &exit, 0x20, 1);
  TestSmiNeg(masm, &exit, 0x30, -1);
  TestSmiNeg(masm, &exit, 0x40, 127);
  TestSmiNeg(masm, &exit, 0x50, 65535);
  TestSmiNeg(masm, &exit, 0x60, Smi::kMinValue);
  TestSmiNeg(masm, &exit, 0x70, Smi::kMaxValue);
  TestSmiNeg(masm, &exit, 0x80, -Smi::kMaxValue);

  __ xor_(rax, rax);  // Success.
  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}




static void SmiAddTest(MacroAssembler* masm,
                       Label* exit,
                       int id,
                       int first,
                       int second) {
  __ movl(rcx, Immediate(first));
  __ Integer32ToSmi(rcx, rcx);
  __ movl(rdx, Immediate(second));
  __ Integer32ToSmi(rdx, rdx);
  __ movl(r8, Immediate(first + second));
  __ Integer32ToSmi(r8, r8);

  __ movl(rax, Immediate(id));  // Test number.
  __ SmiAdd(r9, rcx, rdx, exit);
  __ SmiCompare(r9, r8);
  __ j(not_equal, exit);

  __ incq(rax);
  __ SmiAdd(rcx, rcx, rdx, exit);                              \
  __ SmiCompare(rcx, r8);
  __ j(not_equal, exit);

  __ movl(rcx, Immediate(first));
  __ Integer32ToSmi(rcx, rcx);

  __ incq(rax);
  __ SmiAddConstant(r9, rcx, Smi::FromInt(second));
  __ SmiCompare(r9, r8);
  __ j(not_equal, exit);

  __ SmiAddConstant(rcx, rcx, Smi::FromInt(second));
  __ SmiCompare(rcx, r8);
  __ j(not_equal, exit);

  __ movl(rcx, Immediate(first));
  __ Integer32ToSmi(rcx, rcx);

  __ incq(rax);
  __ SmiAddConstant(r9, rcx, Smi::FromInt(second), exit);
  __ SmiCompare(r9, r8);
  __ j(not_equal, exit);

  __ incq(rax);
  __ SmiAddConstant(rcx, rcx, Smi::FromInt(second), exit);
  __ SmiCompare(rcx, r8);
  __ j(not_equal, exit);
}

TEST(SmiAdd) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
  Label exit;

  // No-overflow tests.
  SmiAddTest(masm, &exit, 0x10, 1, 2);
  SmiAddTest(masm, &exit, 0x20, 1, -2);
  SmiAddTest(masm, &exit, 0x30, -1, 2);
  SmiAddTest(masm, &exit, 0x40, -1, -2);
  SmiAddTest(masm, &exit, 0x50, 0x1000, 0x2000);
  SmiAddTest(masm, &exit, 0x60, Smi::kMinValue, 5);
  SmiAddTest(masm, &exit, 0x70, Smi::kMaxValue, -5);
  SmiAddTest(masm, &exit, 0x80, Smi::kMaxValue, Smi::kMinValue);

  __ xor_(rax, rax);  // Success.
  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}


static void SmiSubTest(MacroAssembler* masm,
                      Label* exit,
                      int id,
                      int first,
                      int second) {
  __ Move(rcx, Smi::FromInt(first));
  __ Move(rdx, Smi::FromInt(second));
  __ Move(r8, Smi::FromInt(first - second));

  __ movl(rax, Immediate(id));  // Test 0.
  __ SmiSub(r9, rcx, rdx, exit);
  __ SmiCompare(r9, r8);
  __ j(not_equal, exit);

  __ incq(rax);  // Test 1.
  __ SmiSub(rcx, rcx, rdx, exit);
  __ SmiCompare(rcx, r8);
  __ j(not_equal, exit);

  __ Move(rcx, Smi::FromInt(first));

  __ incq(rax);  // Test 2.
  __ SmiSubConstant(r9, rcx, Smi::FromInt(second));
  __ SmiCompare(r9, r8);
  __ j(not_equal, exit);

  __ incq(rax);  // Test 3.
  __ SmiSubConstant(rcx, rcx, Smi::FromInt(second));
  __ SmiCompare(rcx, r8);
  __ j(not_equal, exit);

  __ Move(rcx, Smi::FromInt(first));

  __ incq(rax);  // Test 4.
  __ SmiSubConstant(r9, rcx, Smi::FromInt(second), exit);
  __ SmiCompare(r9, r8);
  __ j(not_equal, exit);

  __ incq(rax);  // Test 5.
  __ SmiSubConstant(rcx, rcx, Smi::FromInt(second), exit);
  __ SmiCompare(rcx, r8);
  __ j(not_equal, exit);
}

static void SmiSubOverflowTest(MacroAssembler* masm,
                               Label* exit,
                               int id,
                               int x) {
  // Subtracts a Smi from x so that the subtraction overflows.
  ASSERT(x != -1);  // Can't overflow by subtracting a Smi.
  int y_max = (x < 0) ? (Smi::kMaxValue + 0) : (Smi::kMinValue + 0);
  int y_min = (x < 0) ? (Smi::kMaxValue + x + 2) : (Smi::kMinValue + x);

  __ movl(rax, Immediate(id));
  __ Move(rcx, Smi::FromInt(x));
  __ movq(r11, rcx);  // Store original Smi value of x in r11.
  __ Move(rdx, Smi::FromInt(y_min));
  {
    Label overflow_ok;
    __ SmiSub(r9, rcx, rdx, &overflow_ok);
    __ jmp(exit);
    __ bind(&overflow_ok);
    __ incq(rax);
    __ SmiCompare(rcx, r11);
    __ j(not_equal, exit);
  }

  {
    Label overflow_ok;
    __ incq(rax);
    __ SmiSub(rcx, rcx, rdx, &overflow_ok);
    __ jmp(exit);
    __ bind(&overflow_ok);
    __ incq(rax);
    __ SmiCompare(rcx, r11);
    __ j(not_equal, exit);
  }

  __ movq(rcx, r11);
  {
    Label overflow_ok;
    __ incq(rax);
    __ SmiSubConstant(r9, rcx, Smi::FromInt(y_min), &overflow_ok);
    __ jmp(exit);
    __ bind(&overflow_ok);
    __ incq(rax);
    __ SmiCompare(rcx, r11);
    __ j(not_equal, exit);
  }

  {
    Label overflow_ok;
    __ incq(rax);
    __ SmiSubConstant(rcx, rcx, Smi::FromInt(y_min), &overflow_ok);
    __ jmp(exit);
    __ bind(&overflow_ok);
    __ incq(rax);
    __ SmiCompare(rcx, r11);
    __ j(not_equal, exit);
  }

  __ Move(rdx, Smi::FromInt(y_max));

  {
    Label overflow_ok;
    __ incq(rax);
    __ SmiSub(r9, rcx, rdx, &overflow_ok);
    __ jmp(exit);
    __ bind(&overflow_ok);
    __ incq(rax);
    __ SmiCompare(rcx, r11);
    __ j(not_equal, exit);
  }

  {
    Label overflow_ok;
    __ incq(rax);
    __ SmiSub(rcx, rcx, rdx, &overflow_ok);
    __ jmp(exit);
    __ bind(&overflow_ok);
    __ incq(rax);
    __ SmiCompare(rcx, r11);
    __ j(not_equal, exit);
  }

  __ movq(rcx, r11);
  {
    Label overflow_ok;
    __ incq(rax);
    __ SmiSubConstant(r9, rcx, Smi::FromInt(y_max), &overflow_ok);
    __ jmp(exit);
    __ bind(&overflow_ok);
    __ incq(rax);
    __ SmiCompare(rcx, r11);
    __ j(not_equal, exit);
  }

  {
    Label overflow_ok;
    __ incq(rax);
    __ SmiSubConstant(rcx, rcx, Smi::FromInt(y_max), &overflow_ok);
    __ jmp(exit);
    __ bind(&overflow_ok);
    __ incq(rax);
    __ SmiCompare(rcx, r11);
    __ j(not_equal, exit);
  }
}


TEST(SmiSub) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize * 2,
                                      &actual_size,
                                      true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
  Label exit;

  SmiSubTest(masm, &exit, 0x10, 1, 2);
  SmiSubTest(masm, &exit, 0x20, 1, -2);
  SmiSubTest(masm, &exit, 0x30, -1, 2);
  SmiSubTest(masm, &exit, 0x40, -1, -2);
  SmiSubTest(masm, &exit, 0x50, 0x1000, 0x2000);
  SmiSubTest(masm, &exit, 0x60, Smi::kMinValue, -5);
  SmiSubTest(masm, &exit, 0x70, Smi::kMaxValue, 5);
  SmiSubTest(masm, &exit, 0x80, -Smi::kMaxValue, Smi::kMinValue);
  SmiSubTest(masm, &exit, 0x90, 0, Smi::kMaxValue);

  SmiSubOverflowTest(masm, &exit, 0xA0, 1);
  SmiSubOverflowTest(masm, &exit, 0xB0, 1024);
  SmiSubOverflowTest(masm, &exit, 0xC0, Smi::kMaxValue);
  SmiSubOverflowTest(masm, &exit, 0xD0, -2);
  SmiSubOverflowTest(masm, &exit, 0xE0, -42000);
  SmiSubOverflowTest(masm, &exit, 0xF0, Smi::kMinValue);
  SmiSubOverflowTest(masm, &exit, 0x100, 0);

  __ xor_(rax, rax);  // Success.
  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}



void TestSmiMul(MacroAssembler* masm, Label* exit, int id, int x, int y) {
  int64_t result = static_cast<int64_t>(x) * static_cast<int64_t>(y);
  bool negative_zero = (result == 0) && (x < 0 || y < 0);
  __ Move(rcx, Smi::FromInt(x));
  __ movq(r11, rcx);
  __ Move(rdx, Smi::FromInt(y));
  if (Smi::IsValid(result) && !negative_zero) {
    __ movl(rax, Immediate(id));
    __ Move(r8, Smi::FromIntptr(result));
    __ SmiMul(r9, rcx, rdx, exit);
    __ incq(rax);
    __ SmiCompare(r11, rcx);
    __ j(not_equal, exit);
    __ incq(rax);
    __ SmiCompare(r9, r8);
    __ j(not_equal, exit);

    __ incq(rax);
    __ SmiMul(rcx, rcx, rdx, exit);
    __ SmiCompare(rcx, r8);
    __ j(not_equal, exit);
  } else {
    __ movl(rax, Immediate(id + 8));
    Label overflow_ok, overflow_ok2;
    __ SmiMul(r9, rcx, rdx, &overflow_ok);
    __ jmp(exit);
    __ bind(&overflow_ok);
    __ incq(rax);
    __ SmiCompare(r11, rcx);
    __ j(not_equal, exit);
    __ incq(rax);
    __ SmiMul(rcx, rcx, rdx, &overflow_ok2);
    __ jmp(exit);
    __ bind(&overflow_ok2);
    // 31-bit version doesn't preserve rcx on failure.
    // __ incq(rax);
    // __ SmiCompare(r11, rcx);
    // __ j(not_equal, exit);
  }
}


TEST(SmiMul) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
  Label exit;

  TestSmiMul(masm, &exit, 0x10, 0, 0);
  TestSmiMul(masm, &exit, 0x20, -1, 0);
  TestSmiMul(masm, &exit, 0x30, 0, -1);
  TestSmiMul(masm, &exit, 0x40, -1, -1);
  TestSmiMul(masm, &exit, 0x50, 0x10000, 0x10000);
  TestSmiMul(masm, &exit, 0x60, 0x10000, 0xffff);
  TestSmiMul(masm, &exit, 0x70, 0x10000, 0xffff);
  TestSmiMul(masm, &exit, 0x80, Smi::kMaxValue, -1);
  TestSmiMul(masm, &exit, 0x90, Smi::kMaxValue, -2);
  TestSmiMul(masm, &exit, 0xa0, Smi::kMaxValue, 2);
  TestSmiMul(masm, &exit, 0xb0, (Smi::kMaxValue / 2), 2);
  TestSmiMul(masm, &exit, 0xc0, (Smi::kMaxValue / 2) + 1, 2);
  TestSmiMul(masm, &exit, 0xd0, (Smi::kMinValue / 2), 2);
  TestSmiMul(masm, &exit, 0xe0, (Smi::kMinValue / 2) - 1, 2);

  __ xor_(rax, rax);  // Success.
  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}


void TestSmiDiv(MacroAssembler* masm, Label* exit, int id, int x, int y) {
  bool division_by_zero = (y == 0);
  bool negative_zero = (x == 0 && y < 0);
#ifdef V8_TARGET_ARCH_X64
  bool overflow = (x == Smi::kMinValue && y < 0);  // Safe approx. used.
#else
  bool overflow = (x == Smi::kMinValue && y == -1);
#endif
  bool fraction = !division_by_zero && !overflow && (x % y != 0);
  __ Move(r11, Smi::FromInt(x));
  __ Move(r12, Smi::FromInt(y));
  if (!fraction && !overflow && !negative_zero && !division_by_zero) {
    // Division succeeds
    __ movq(rcx, r11);
    __ movq(r15, Immediate(id));
    int result = x / y;
    __ Move(r8, Smi::FromInt(result));
    __ SmiDiv(r9, rcx, r12, exit);
    // Might have destroyed rcx and r12.
    __ incq(r15);
    __ SmiCompare(r9, r8);
    __ j(not_equal, exit);

    __ incq(r15);
    __ movq(rcx, r11);
    __ Move(r12, Smi::FromInt(y));
    __ SmiCompare(rcx, r11);
    __ j(not_equal, exit);

    __ incq(r15);
    __ SmiDiv(rcx, rcx, r12, exit);

    __ incq(r15);
    __ SmiCompare(rcx, r8);
    __ j(not_equal, exit);
  } else {
    // Division fails.
    __ movq(r15, Immediate(id + 8));

    Label fail_ok, fail_ok2;
    __ movq(rcx, r11);
    __ SmiDiv(r9, rcx, r12, &fail_ok);
    __ jmp(exit);
    __ bind(&fail_ok);

    __ incq(r15);
    __ SmiCompare(rcx, r11);
    __ j(not_equal, exit);

    __ incq(r15);
    __ SmiDiv(rcx, rcx, r12, &fail_ok2);
    __ jmp(exit);
    __ bind(&fail_ok2);

    __ incq(r15);
    __ SmiCompare(rcx, r11);
    __ j(not_equal, exit);
  }
}


TEST(SmiDiv) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize * 2,
                                      &actual_size,
                                      true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
  Label exit;

  __ push(r12);
  __ push(r15);
  TestSmiDiv(masm, &exit, 0x10, 1, 1);
  TestSmiDiv(masm, &exit, 0x20, 1, 0);
  TestSmiDiv(masm, &exit, 0x30, -1, 0);
  TestSmiDiv(masm, &exit, 0x40, 0, 1);
  TestSmiDiv(masm, &exit, 0x50, 0, -1);
  TestSmiDiv(masm, &exit, 0x60, 4, 2);
  TestSmiDiv(masm, &exit, 0x70, -4, 2);
  TestSmiDiv(masm, &exit, 0x80, 4, -2);
  TestSmiDiv(masm, &exit, 0x90, -4, -2);
  TestSmiDiv(masm, &exit, 0xa0, 3, 2);
  TestSmiDiv(masm, &exit, 0xb0, 3, 4);
  TestSmiDiv(masm, &exit, 0xc0, 1, Smi::kMaxValue);
  TestSmiDiv(masm, &exit, 0xd0, -1, Smi::kMaxValue);
  TestSmiDiv(masm, &exit, 0xe0, Smi::kMaxValue, 1);
  TestSmiDiv(masm, &exit, 0xf0, Smi::kMaxValue, Smi::kMaxValue);
  TestSmiDiv(masm, &exit, 0x100, Smi::kMaxValue, -Smi::kMaxValue);
  TestSmiDiv(masm, &exit, 0x110, Smi::kMaxValue, -1);
  TestSmiDiv(masm, &exit, 0x120, Smi::kMinValue, 1);
  TestSmiDiv(masm, &exit, 0x130, Smi::kMinValue, Smi::kMinValue);
  TestSmiDiv(masm, &exit, 0x140, Smi::kMinValue, -1);

  __ xor_(r15, r15);  // Success.
  __ bind(&exit);
  __ movq(rax, r15);
  __ pop(r15);
  __ pop(r12);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}


void TestSmiMod(MacroAssembler* masm, Label* exit, int id, int x, int y) {
  bool division_by_zero = (y == 0);
  bool division_overflow = (x == Smi::kMinValue) && (y == -1);
  bool fraction = !division_by_zero && !division_overflow && ((x % y) != 0);
  bool negative_zero = (!fraction && x < 0);
  __ Move(rcx, Smi::FromInt(x));
  __ movq(r11, rcx);
  __ Move(r12, Smi::FromInt(y));
  if (!division_overflow && !negative_zero && !division_by_zero) {
    // Modulo succeeds
    __ movq(r15, Immediate(id));
    int result = x % y;
    __ Move(r8, Smi::FromInt(result));
    __ SmiMod(r9, rcx, r12, exit);

    __ incq(r15);
    __ SmiCompare(r9, r8);
    __ j(not_equal, exit);

    __ incq(r15);
    __ SmiCompare(rcx, r11);
    __ j(not_equal, exit);

    __ incq(r15);
    __ SmiMod(rcx, rcx, r12, exit);

    __ incq(r15);
    __ SmiCompare(rcx, r8);
    __ j(not_equal, exit);
  } else {
    // Modulo fails.
    __ movq(r15, Immediate(id + 8));

    Label fail_ok, fail_ok2;
    __ SmiMod(r9, rcx, r12, &fail_ok);
    __ jmp(exit);
    __ bind(&fail_ok);

    __ incq(r15);
    __ SmiCompare(rcx, r11);
    __ j(not_equal, exit);

    __ incq(r15);
    __ SmiMod(rcx, rcx, r12, &fail_ok2);
    __ jmp(exit);
    __ bind(&fail_ok2);

    __ incq(r15);
    __ SmiCompare(rcx, r11);
    __ j(not_equal, exit);
  }
}


TEST(SmiMod) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize * 2,
                                      &actual_size,
                                      true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
  Label exit;

  __ push(r12);
  __ push(r15);
  TestSmiMod(masm, &exit, 0x10, 1, 1);
  TestSmiMod(masm, &exit, 0x20, 1, 0);
  TestSmiMod(masm, &exit, 0x30, -1, 0);
  TestSmiMod(masm, &exit, 0x40, 0, 1);
  TestSmiMod(masm, &exit, 0x50, 0, -1);
  TestSmiMod(masm, &exit, 0x60, 4, 2);
  TestSmiMod(masm, &exit, 0x70, -4, 2);
  TestSmiMod(masm, &exit, 0x80, 4, -2);
  TestSmiMod(masm, &exit, 0x90, -4, -2);
  TestSmiMod(masm, &exit, 0xa0, 3, 2);
  TestSmiMod(masm, &exit, 0xb0, 3, 4);
  TestSmiMod(masm, &exit, 0xc0, 1, Smi::kMaxValue);
  TestSmiMod(masm, &exit, 0xd0, -1, Smi::kMaxValue);
  TestSmiMod(masm, &exit, 0xe0, Smi::kMaxValue, 1);
  TestSmiMod(masm, &exit, 0xf0, Smi::kMaxValue, Smi::kMaxValue);
  TestSmiMod(masm, &exit, 0x100, Smi::kMaxValue, -Smi::kMaxValue);
  TestSmiMod(masm, &exit, 0x110, Smi::kMaxValue, -1);
  TestSmiMod(masm, &exit, 0x120, Smi::kMinValue, 1);
  TestSmiMod(masm, &exit, 0x130, Smi::kMinValue, Smi::kMinValue);
  TestSmiMod(masm, &exit, 0x140, Smi::kMinValue, -1);

  __ xor_(r15, r15);  // Success.
  __ bind(&exit);
  __ movq(rax, r15);
  __ pop(r15);
  __ pop(r12);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}


void TestSmiIndex(MacroAssembler* masm, Label* exit, int id, int x) {
  __ movl(rax, Immediate(id));

  for (int i = 0; i < 8; i++) {
    __ Move(rcx, Smi::FromInt(x));
    SmiIndex index = masm->SmiToIndex(rdx, rcx, i);
    ASSERT(index.reg.is(rcx) || index.reg.is(rdx));
    __ shl(index.reg, Immediate(index.scale));
    __ Set(r8, static_cast<intptr_t>(x) << i);
    __ SmiCompare(index.reg, r8);
    __ j(not_equal, exit);
    __ incq(rax);
    __ Move(rcx, Smi::FromInt(x));
    index = masm->SmiToIndex(rcx, rcx, i);
    ASSERT(index.reg.is(rcx));
    __ shl(rcx, Immediate(index.scale));
    __ Set(r8, static_cast<intptr_t>(x) << i);
    __ SmiCompare(rcx, r8);
    __ j(not_equal, exit);
    __ incq(rax);

    __ Move(rcx, Smi::FromInt(x));
    index = masm->SmiToNegativeIndex(rdx, rcx, i);
    ASSERT(index.reg.is(rcx) || index.reg.is(rdx));
    __ shl(index.reg, Immediate(index.scale));
    __ Set(r8, static_cast<intptr_t>(-x) << i);
    __ SmiCompare(index.reg, r8);
    __ j(not_equal, exit);
    __ incq(rax);
    __ Move(rcx, Smi::FromInt(x));
    index = masm->SmiToNegativeIndex(rcx, rcx, i);
    ASSERT(index.reg.is(rcx));
    __ shl(rcx, Immediate(index.scale));
    __ Set(r8, static_cast<intptr_t>(-x) << i);
    __ SmiCompare(rcx, r8);
    __ j(not_equal, exit);
    __ incq(rax);
  }
}

TEST(SmiIndex) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize * 2,
                                      &actual_size,
                                      true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
  Label exit;

  TestSmiIndex(masm, &exit, 0x10, 0);
  TestSmiIndex(masm, &exit, 0x20, 1);
  TestSmiIndex(masm, &exit, 0x30, 100);
  TestSmiIndex(masm, &exit, 0x40, 1000);
  TestSmiIndex(masm, &exit, 0x50, Smi::kMaxValue);

  __ xor_(rax, rax);  // Success.
  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}


void TestSelectNonSmi(MacroAssembler* masm, Label* exit, int id, int x, int y) {
  __ movl(rax, Immediate(id));
  __ Move(rcx, Smi::FromInt(x));
  __ Move(rdx, Smi::FromInt(y));
  __ xor_(rdx, Immediate(kSmiTagMask));
  __ SelectNonSmi(r9, rcx, rdx, exit);

  __ incq(rax);
  __ SmiCompare(r9, rdx);
  __ j(not_equal, exit);

  __ incq(rax);
  __ Move(rcx, Smi::FromInt(x));
  __ Move(rdx, Smi::FromInt(y));
  __ xor_(rcx, Immediate(kSmiTagMask));
  __ SelectNonSmi(r9, rcx, rdx, exit);

  __ incq(rax);
  __ SmiCompare(r9, rcx);
  __ j(not_equal, exit);

  __ incq(rax);
  Label fail_ok;
  __ Move(rcx, Smi::FromInt(x));
  __ Move(rdx, Smi::FromInt(y));
  __ xor_(rcx, Immediate(kSmiTagMask));
  __ xor_(rdx, Immediate(kSmiTagMask));
  __ SelectNonSmi(r9, rcx, rdx, &fail_ok);
  __ jmp(exit);
  __ bind(&fail_ok);
}


TEST(SmiSelectNonSmi) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                      &actual_size,
                                      true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);  // Avoid inline checks.
  Label exit;

  TestSelectNonSmi(masm, &exit, 0x10, 0, 0);
  TestSelectNonSmi(masm, &exit, 0x20, 0, 1);
  TestSelectNonSmi(masm, &exit, 0x30, 1, 0);
  TestSelectNonSmi(masm, &exit, 0x40, 0, -1);
  TestSelectNonSmi(masm, &exit, 0x50, -1, 0);
  TestSelectNonSmi(masm, &exit, 0x60, -1, -1);
  TestSelectNonSmi(masm, &exit, 0x70, 1, 1);
  TestSelectNonSmi(masm, &exit, 0x80, Smi::kMinValue, Smi::kMaxValue);
  TestSelectNonSmi(masm, &exit, 0x90, Smi::kMinValue, Smi::kMinValue);

  __ xor_(rax, rax);  // Success.
  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}


void TestSmiAnd(MacroAssembler* masm, Label* exit, int id, int x, int y) {
  int result = x & y;

  __ movl(rax, Immediate(id));

  __ Move(rcx, Smi::FromInt(x));
  __ movq(r11, rcx);
  __ Move(rdx, Smi::FromInt(y));
  __ Move(r8, Smi::FromInt(result));
  __ SmiAnd(r9, rcx, rdx);
  __ SmiCompare(r8, r9);
  __ j(not_equal, exit);

  __ incq(rax);
  __ SmiCompare(r11, rcx);
  __ j(not_equal, exit);

  __ incq(rax);
  __ SmiAnd(rcx, rcx, rdx);
  __ SmiCompare(r8, rcx);
  __ j(not_equal, exit);

  __ movq(rcx, r11);
  __ incq(rax);
  __ SmiAndConstant(r9, rcx, Smi::FromInt(y));
  __ SmiCompare(r8, r9);
  __ j(not_equal, exit);

  __ incq(rax);
  __ SmiCompare(r11, rcx);
  __ j(not_equal, exit);

  __ incq(rax);
  __ SmiAndConstant(rcx, rcx, Smi::FromInt(y));
  __ SmiCompare(r8, rcx);
  __ j(not_equal, exit);
}


TEST(SmiAnd) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                      &actual_size,
                                      true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
  Label exit;

  TestSmiAnd(masm, &exit, 0x10, 0, 0);
  TestSmiAnd(masm, &exit, 0x20, 0, 1);
  TestSmiAnd(masm, &exit, 0x30, 1, 0);
  TestSmiAnd(masm, &exit, 0x40, 0, -1);
  TestSmiAnd(masm, &exit, 0x50, -1, 0);
  TestSmiAnd(masm, &exit, 0x60, -1, -1);
  TestSmiAnd(masm, &exit, 0x70, 1, 1);
  TestSmiAnd(masm, &exit, 0x80, Smi::kMinValue, Smi::kMaxValue);
  TestSmiAnd(masm, &exit, 0x90, Smi::kMinValue, Smi::kMinValue);
  TestSmiAnd(masm, &exit, 0xA0, Smi::kMinValue, -1);
  TestSmiAnd(masm, &exit, 0xB0, Smi::kMinValue, -1);

  __ xor_(rax, rax);  // Success.
  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}


void TestSmiOr(MacroAssembler* masm, Label* exit, int id, int x, int y) {
  int result = x | y;

  __ movl(rax, Immediate(id));

  __ Move(rcx, Smi::FromInt(x));
  __ movq(r11, rcx);
  __ Move(rdx, Smi::FromInt(y));
  __ Move(r8, Smi::FromInt(result));
  __ SmiOr(r9, rcx, rdx);
  __ SmiCompare(r8, r9);
  __ j(not_equal, exit);

  __ incq(rax);
  __ SmiCompare(r11, rcx);
  __ j(not_equal, exit);

  __ incq(rax);
  __ SmiOr(rcx, rcx, rdx);
  __ SmiCompare(r8, rcx);
  __ j(not_equal, exit);

  __ movq(rcx, r11);
  __ incq(rax);
  __ SmiOrConstant(r9, rcx, Smi::FromInt(y));
  __ SmiCompare(r8, r9);
  __ j(not_equal, exit);

  __ incq(rax);
  __ SmiCompare(r11, rcx);
  __ j(not_equal, exit);

  __ incq(rax);
  __ SmiOrConstant(rcx, rcx, Smi::FromInt(y));
  __ SmiCompare(r8, rcx);
  __ j(not_equal, exit);
}


TEST(SmiOr) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                      &actual_size,
                                      true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
  Label exit;

  TestSmiOr(masm, &exit, 0x10, 0, 0);
  TestSmiOr(masm, &exit, 0x20, 0, 1);
  TestSmiOr(masm, &exit, 0x30, 1, 0);
  TestSmiOr(masm, &exit, 0x40, 0, -1);
  TestSmiOr(masm, &exit, 0x50, -1, 0);
  TestSmiOr(masm, &exit, 0x60, -1, -1);
  TestSmiOr(masm, &exit, 0x70, 1, 1);
  TestSmiOr(masm, &exit, 0x80, Smi::kMinValue, Smi::kMaxValue);
  TestSmiOr(masm, &exit, 0x90, Smi::kMinValue, Smi::kMinValue);
  TestSmiOr(masm, &exit, 0xA0, Smi::kMinValue, -1);
  TestSmiOr(masm, &exit, 0xB0, 0x05555555, 0x01234567);
  TestSmiOr(masm, &exit, 0xC0, 0x05555555, 0x0fedcba9);
  TestSmiOr(masm, &exit, 0xD0, Smi::kMinValue, -1);

  __ xor_(rax, rax);  // Success.
  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}


void TestSmiXor(MacroAssembler* masm, Label* exit, int id, int x, int y) {
  int result = x ^ y;

  __ movl(rax, Immediate(id));

  __ Move(rcx, Smi::FromInt(x));
  __ movq(r11, rcx);
  __ Move(rdx, Smi::FromInt(y));
  __ Move(r8, Smi::FromInt(result));
  __ SmiXor(r9, rcx, rdx);
  __ SmiCompare(r8, r9);
  __ j(not_equal, exit);

  __ incq(rax);
  __ SmiCompare(r11, rcx);
  __ j(not_equal, exit);

  __ incq(rax);
  __ SmiXor(rcx, rcx, rdx);
  __ SmiCompare(r8, rcx);
  __ j(not_equal, exit);

  __ movq(rcx, r11);
  __ incq(rax);
  __ SmiXorConstant(r9, rcx, Smi::FromInt(y));
  __ SmiCompare(r8, r9);
  __ j(not_equal, exit);

  __ incq(rax);
  __ SmiCompare(r11, rcx);
  __ j(not_equal, exit);

  __ incq(rax);
  __ SmiXorConstant(rcx, rcx, Smi::FromInt(y));
  __ SmiCompare(r8, rcx);
  __ j(not_equal, exit);
}


TEST(SmiXor) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                      &actual_size,
                                      true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
  Label exit;

  TestSmiXor(masm, &exit, 0x10, 0, 0);
  TestSmiXor(masm, &exit, 0x20, 0, 1);
  TestSmiXor(masm, &exit, 0x30, 1, 0);
  TestSmiXor(masm, &exit, 0x40, 0, -1);
  TestSmiXor(masm, &exit, 0x50, -1, 0);
  TestSmiXor(masm, &exit, 0x60, -1, -1);
  TestSmiXor(masm, &exit, 0x70, 1, 1);
  TestSmiXor(masm, &exit, 0x80, Smi::kMinValue, Smi::kMaxValue);
  TestSmiXor(masm, &exit, 0x90, Smi::kMinValue, Smi::kMinValue);
  TestSmiXor(masm, &exit, 0xA0, Smi::kMinValue, -1);
  TestSmiXor(masm, &exit, 0xB0, 0x5555555, 0x01234567);
  TestSmiXor(masm, &exit, 0xC0, 0x5555555, 0x0fedcba9);
  TestSmiXor(masm, &exit, 0xD0, Smi::kMinValue, -1);

  __ xor_(rax, rax);  // Success.
  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}


void TestSmiNot(MacroAssembler* masm, Label* exit, int id, int x) {
  int result = ~x;
  __ movl(rax, Immediate(id));

  __ Move(r8, Smi::FromInt(result));
  __ Move(rcx, Smi::FromInt(x));
  __ movq(r11, rcx);

  __ SmiNot(r9, rcx);
  __ SmiCompare(r9, r8);
  __ j(not_equal, exit);

  __ incq(rax);
  __ SmiCompare(r11, rcx);
  __ j(not_equal, exit);

  __ incq(rax);
  __ SmiNot(rcx, rcx);
  __ SmiCompare(rcx, r8);
  __ j(not_equal, exit);
}


TEST(SmiNot) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                      &actual_size,
                                      true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
  Label exit;

  TestSmiNot(masm, &exit, 0x10, 0);
  TestSmiNot(masm, &exit, 0x20, 1);
  TestSmiNot(masm, &exit, 0x30, -1);
  TestSmiNot(masm, &exit, 0x40, 127);
  TestSmiNot(masm, &exit, 0x50, 65535);
  TestSmiNot(masm, &exit, 0x60, Smi::kMinValue);
  TestSmiNot(masm, &exit, 0x70, Smi::kMaxValue);
  TestSmiNot(masm, &exit, 0x80, 0x05555555);

  __ xor_(rax, rax);  // Success.
  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}


void TestSmiShiftLeft(MacroAssembler* masm, Label* exit, int id, int x) {
  const int shifts[] = { 0, 1, 7, 24, kSmiValueSize - 1};
  const int kNumShifts = 5;
  __ movl(rax, Immediate(id));
  for (int i = 0; i < kNumShifts; i++) {
    // rax == id + i * 10.
    int shift = shifts[i];
    int result = x << shift;
    if (Smi::IsValid(result)) {
      __ Move(r8, Smi::FromInt(result));
      __ Move(rcx, Smi::FromInt(x));
      __ SmiShiftLeftConstant(r9, rcx, shift, exit);

      __ incq(rax);
      __ SmiCompare(r9, r8);
      __ j(not_equal, exit);

      __ incq(rax);
      __ Move(rcx, Smi::FromInt(x));
      __ SmiShiftLeftConstant(rcx, rcx, shift, exit);

      __ incq(rax);
      __ SmiCompare(rcx, r8);
      __ j(not_equal, exit);

      __ incq(rax);
      __ Move(rdx, Smi::FromInt(x));
      __ Move(rcx, Smi::FromInt(shift));
      __ SmiShiftLeft(r9, rdx, rcx, exit);

      __ incq(rax);
      __ SmiCompare(r9, r8);
      __ j(not_equal, exit);

      __ incq(rax);
      __ Move(rdx, Smi::FromInt(x));
      __ Move(r11, Smi::FromInt(shift));
      __ SmiShiftLeft(r9, rdx, r11, exit);

      __ incq(rax);
      __ SmiCompare(r9, r8);
      __ j(not_equal, exit);

      __ incq(rax);
      __ Move(rdx, Smi::FromInt(x));
      __ Move(r11, Smi::FromInt(shift));
      __ SmiShiftLeft(rdx, rdx, r11, exit);

      __ incq(rax);
      __ SmiCompare(rdx, r8);
      __ j(not_equal, exit);

      __ incq(rax);
    } else {
      // Cannot happen with long smis.
      Label fail_ok;
      __ Move(rcx, Smi::FromInt(x));
      __ movq(r11, rcx);
      __ SmiShiftLeftConstant(r9, rcx, shift, &fail_ok);
      __ jmp(exit);
      __ bind(&fail_ok);

      __ incq(rax);
      __ SmiCompare(rcx, r11);
      __ j(not_equal, exit);

      __ incq(rax);
      Label fail_ok2;
      __ SmiShiftLeftConstant(rcx, rcx, shift, &fail_ok2);
      __ jmp(exit);
      __ bind(&fail_ok2);

      __ incq(rax);
      __ SmiCompare(rcx, r11);
      __ j(not_equal, exit);

      __ incq(rax);
      __ Move(r8, Smi::FromInt(shift));
      Label fail_ok3;
      __ SmiShiftLeft(r9, rcx, r8, &fail_ok3);
      __ jmp(exit);
      __ bind(&fail_ok3);

      __ incq(rax);
      __ SmiCompare(rcx, r11);
      __ j(not_equal, exit);

      __ incq(rax);
      __ Move(r8, Smi::FromInt(shift));
      __ movq(rdx, r11);
      Label fail_ok4;
      __ SmiShiftLeft(rdx, rdx, r8, &fail_ok4);
      __ jmp(exit);
      __ bind(&fail_ok4);

      __ incq(rax);
      __ SmiCompare(rdx, r11);
      __ j(not_equal, exit);

      __ addq(rax, Immediate(3));
    }
  }
}


TEST(SmiShiftLeft) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize * 3,
                                      &actual_size,
                                      true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
  Label exit;

  TestSmiShiftLeft(masm, &exit, 0x10, 0);
  TestSmiShiftLeft(masm, &exit, 0x50, 1);
  TestSmiShiftLeft(masm, &exit, 0x90, 127);
  TestSmiShiftLeft(masm, &exit, 0xD0, 65535);
  TestSmiShiftLeft(masm, &exit, 0x110, Smi::kMaxValue);
  TestSmiShiftLeft(masm, &exit, 0x150, Smi::kMinValue);
  TestSmiShiftLeft(masm, &exit, 0x190, -1);

  __ xor_(rax, rax);  // Success.
  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}


void TestSmiShiftLogicalRight(MacroAssembler* masm,
                              Label* exit,
                              int id,
                              int x) {
  const int shifts[] = { 0, 1, 7, 24, kSmiValueSize - 1};
  const int kNumShifts = 5;
  __ movl(rax, Immediate(id));
  for (int i = 0; i < kNumShifts; i++) {
    int shift = shifts[i];
    intptr_t result = static_cast<unsigned int>(x) >> shift;
    if (Smi::IsValid(result)) {
      __ Move(r8, Smi::FromInt(static_cast<int>(result)));
      __ Move(rcx, Smi::FromInt(x));
      __ SmiShiftLogicalRightConstant(r9, rcx, shift, exit);

      __ incq(rax);
      __ SmiCompare(r9, r8);
      __ j(not_equal, exit);

      __ incq(rax);
      __ Move(rdx, Smi::FromInt(x));
      __ Move(rcx, Smi::FromInt(shift));
      __ SmiShiftLogicalRight(r9, rdx, rcx, exit);

      __ incq(rax);
      __ SmiCompare(r9, r8);
      __ j(not_equal, exit);

      __ incq(rax);
      __ Move(rdx, Smi::FromInt(x));
      __ Move(r11, Smi::FromInt(shift));
      __ SmiShiftLogicalRight(r9, rdx, r11, exit);

      __ incq(rax);
      __ SmiCompare(r9, r8);
      __ j(not_equal, exit);

      __ incq(rax);
    } else {
      // Cannot happen with long smis.
      Label fail_ok;
      __ Move(rcx, Smi::FromInt(x));
      __ movq(r11, rcx);
      __ SmiShiftLogicalRightConstant(r9, rcx, shift, &fail_ok);
      __ jmp(exit);
      __ bind(&fail_ok);

      __ incq(rax);
      __ SmiCompare(rcx, r11);
      __ j(not_equal, exit);

      __ incq(rax);
      __ Move(r8, Smi::FromInt(shift));
      Label fail_ok3;
      __ SmiShiftLogicalRight(r9, rcx, r8, &fail_ok3);
      __ jmp(exit);
      __ bind(&fail_ok3);

      __ incq(rax);
      __ SmiCompare(rcx, r11);
      __ j(not_equal, exit);

      __ addq(rax, Immediate(3));
    }
  }
}


TEST(SmiShiftLogicalRight) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize * 2,
                                      &actual_size,
                                      true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
  Label exit;

  TestSmiShiftLogicalRight(masm, &exit, 0x10, 0);
  TestSmiShiftLogicalRight(masm, &exit, 0x30, 1);
  TestSmiShiftLogicalRight(masm, &exit, 0x50, 127);
  TestSmiShiftLogicalRight(masm, &exit, 0x70, 65535);
  TestSmiShiftLogicalRight(masm, &exit, 0x90, Smi::kMaxValue);
  TestSmiShiftLogicalRight(masm, &exit, 0xB0, Smi::kMinValue);
  TestSmiShiftLogicalRight(masm, &exit, 0xD0, -1);

  __ xor_(rax, rax);  // Success.
  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}


void TestSmiShiftArithmeticRight(MacroAssembler* masm,
                                 Label* exit,
                                 int id,
                                 int x) {
  const int shifts[] = { 0, 1, 7, 24, kSmiValueSize - 1};
  const int kNumShifts = 5;
  __ movl(rax, Immediate(id));
  for (int i = 0; i < kNumShifts; i++) {
    int shift = shifts[i];
    // Guaranteed arithmetic shift.
    int result = (x < 0) ? ~((~x) >> shift) : (x >> shift);
    __ Move(r8, Smi::FromInt(result));
    __ Move(rcx, Smi::FromInt(x));
    __ SmiShiftArithmeticRightConstant(rcx, rcx, shift);

    __ SmiCompare(rcx, r8);
    __ j(not_equal, exit);

    __ incq(rax);
    __ Move(rdx, Smi::FromInt(x));
    __ Move(r11, Smi::FromInt(shift));
    __ SmiShiftArithmeticRight(rdx, rdx, r11);

    __ SmiCompare(rdx, r8);
    __ j(not_equal, exit);

    __ incq(rax);
  }
}


TEST(SmiShiftArithmeticRight) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize * 2,
                                      &actual_size,
                                      true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
  Label exit;

  TestSmiShiftArithmeticRight(masm, &exit, 0x10, 0);
  TestSmiShiftArithmeticRight(masm, &exit, 0x20, 1);
  TestSmiShiftArithmeticRight(masm, &exit, 0x30, 127);
  TestSmiShiftArithmeticRight(masm, &exit, 0x40, 65535);
  TestSmiShiftArithmeticRight(masm, &exit, 0x50, Smi::kMaxValue);
  TestSmiShiftArithmeticRight(masm, &exit, 0x60, Smi::kMinValue);
  TestSmiShiftArithmeticRight(masm, &exit, 0x70, -1);

  __ xor_(rax, rax);  // Success.
  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}


void TestPositiveSmiPowerUp(MacroAssembler* masm, Label* exit, int id, int x) {
  ASSERT(x >= 0);
  int powers[] = { 0, 1, 2, 3, 8, 16, 24, 31 };
  int power_count = 8;
  __ movl(rax, Immediate(id));
  for (int i = 0; i  < power_count; i++) {
    int power = powers[i];
    intptr_t result = static_cast<intptr_t>(x) << power;
    __ Set(r8, result);
    __ Move(rcx, Smi::FromInt(x));
    __ movq(r11, rcx);
    __ PositiveSmiTimesPowerOfTwoToInteger64(rdx, rcx, power);
    __ SmiCompare(rdx, r8);
    __ j(not_equal, exit);
    __ incq(rax);
    __ SmiCompare(r11, rcx);  // rcx unchanged.
    __ j(not_equal, exit);
    __ incq(rax);
    __ PositiveSmiTimesPowerOfTwoToInteger64(rcx, rcx, power);
    __ SmiCompare(rdx, r8);
    __ j(not_equal, exit);
    __ incq(rax);
  }
}


TEST(PositiveSmiTimesPowerOfTwoToInteger64) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize * 2,
                                      &actual_size,
                                      true));
  CHECK(buffer);
  HandleScope handles;
  MacroAssembler assembler(buffer, static_cast<int>(actual_size));

  MacroAssembler* masm = &assembler;
  masm->set_allow_stub_calls(false);
  Label exit;

  TestPositiveSmiPowerUp(masm, &exit, 0x20, 0);
  TestPositiveSmiPowerUp(masm, &exit, 0x40, 1);
  TestPositiveSmiPowerUp(masm, &exit, 0x60, 127);
  TestPositiveSmiPowerUp(masm, &exit, 0x80, 128);
  TestPositiveSmiPowerUp(masm, &exit, 0xA0, 255);
  TestPositiveSmiPowerUp(masm, &exit, 0xC0, 256);
  TestPositiveSmiPowerUp(masm, &exit, 0x100, 65535);
  TestPositiveSmiPowerUp(masm, &exit, 0x120, 65536);
  TestPositiveSmiPowerUp(masm, &exit, 0x140, Smi::kMaxValue);

  __ xor_(rax, rax);  // Success.
  __ bind(&exit);
  __ ret(0);

  CodeDesc desc;
  masm->GetCode(&desc);
  // Call the function from C++.
  int result = FUNCTION_CAST<F0>(buffer)();
  CHECK_EQ(0, result);
}


#undef __
