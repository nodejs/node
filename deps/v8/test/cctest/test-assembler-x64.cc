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

using v8::internal::Assembler;
using v8::internal::Code;
using v8::internal::CodeDesc;
using v8::internal::FUNCTION_CAST;
using v8::internal::Immediate;
using v8::internal::Isolate;
using v8::internal::Label;
using v8::internal::OS;
using v8::internal::Operand;
using v8::internal::byte;
using v8::internal::greater;
using v8::internal::less_equal;
using v8::internal::equal;
using v8::internal::not_equal;
using v8::internal::r13;
using v8::internal::r15;
using v8::internal::r8;
using v8::internal::r9;
using v8::internal::rax;
using v8::internal::rbx;
using v8::internal::rbp;
using v8::internal::rcx;
using v8::internal::rdi;
using v8::internal::rdx;
using v8::internal::rsi;
using v8::internal::rsp;
using v8::internal::times_1;
using v8::internal::xmm0;

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

#ifdef _WIN64
static const v8::internal::Register arg1 = rcx;
static const v8::internal::Register arg2 = rdx;
#else
static const v8::internal::Register arg1 = rdi;
static const v8::internal::Register arg2 = rsi;
#endif

#define __ assm.


TEST(AssemblerX64ReturnOperation) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  Assembler assm(Isolate::Current(), buffer, static_cast<int>(actual_size));

  // Assemble a simple function that copies argument 2 and returns it.
  __ movq(rax, arg2);
  __ nop();
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  // Call the function from C++.
  int result =  FUNCTION_CAST<F2>(buffer)(3, 2);
  CHECK_EQ(2, result);
}


TEST(AssemblerX64StackOperations) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  Assembler assm(Isolate::Current(), buffer, static_cast<int>(actual_size));

  // Assemble a simple function that copies argument 2 and returns it.
  // We compile without stack frame pointers, so the gdb debugger shows
  // incorrect stack frames when debugging this function (which has them).
  __ push(rbp);
  __ movq(rbp, rsp);
  __ push(arg2);  // Value at (rbp - 8)
  __ push(arg2);  // Value at (rbp - 16)
  __ push(arg1);  // Value at (rbp - 24)
  __ pop(rax);
  __ pop(rax);
  __ pop(rax);
  __ pop(rbp);
  __ nop();
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  // Call the function from C++.
  int result =  FUNCTION_CAST<F2>(buffer)(3, 2);
  CHECK_EQ(2, result);
}


TEST(AssemblerX64ArithmeticOperations) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  Assembler assm(Isolate::Current(), buffer, static_cast<int>(actual_size));

  // Assemble a simple function that adds arguments returning the sum.
  __ movq(rax, arg2);
  __ addq(rax, arg1);
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  // Call the function from C++.
  int result =  FUNCTION_CAST<F2>(buffer)(3, 2);
  CHECK_EQ(5, result);
}


TEST(AssemblerX64ImulOperation) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  Assembler assm(Isolate::Current(), buffer, static_cast<int>(actual_size));

  // Assemble a simple function that multiplies arguments returning the high
  // word.
  __ movq(rax, arg2);
  __ imul(arg1);
  __ movq(rax, rdx);
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  // Call the function from C++.
  int result =  FUNCTION_CAST<F2>(buffer)(3, 2);
  CHECK_EQ(0, result);
  result =  FUNCTION_CAST<F2>(buffer)(0x100000000l, 0x100000000l);
  CHECK_EQ(1, result);
  result =  FUNCTION_CAST<F2>(buffer)(-0x100000000l, 0x100000000l);
  CHECK_EQ(-1, result);
}


TEST(AssemblerX64MemoryOperands) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  Assembler assm(Isolate::Current(), buffer, static_cast<int>(actual_size));

  // Assemble a simple function that copies argument 2 and returns it.
  __ push(rbp);
  __ movq(rbp, rsp);

  __ push(arg2);  // Value at (rbp - 8)
  __ push(arg2);  // Value at (rbp - 16)
  __ push(arg1);  // Value at (rbp - 24)

  const int kStackElementSize = 8;
  __ movq(rax, Operand(rbp, -3 * kStackElementSize));
  __ pop(arg2);
  __ pop(arg2);
  __ pop(arg2);
  __ pop(rbp);
  __ nop();
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  // Call the function from C++.
  int result =  FUNCTION_CAST<F2>(buffer)(3, 2);
  CHECK_EQ(3, result);
}


TEST(AssemblerX64ControlFlow) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  Assembler assm(Isolate::Current(), buffer, static_cast<int>(actual_size));

  // Assemble a simple function that copies argument 1 and returns it.
  __ push(rbp);

  __ movq(rbp, rsp);
  __ movq(rax, arg1);
  Label target;
  __ jmp(&target);
  __ movq(rax, arg2);
  __ bind(&target);
  __ pop(rbp);
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  // Call the function from C++.
  int result =  FUNCTION_CAST<F2>(buffer)(3, 2);
  CHECK_EQ(3, result);
}


TEST(AssemblerX64LoopImmediates) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  Assembler assm(Isolate::Current(), buffer, static_cast<int>(actual_size));
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
  assm.GetCode(&desc);
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
  Assembler assm(Isolate::Current(), NULL, 0);

  Label target;
  __ j(equal, &target);
  __ j(not_equal, &target);
  __ bind(&target);
  __ nop();
}


TEST(AssemblerMultiByteNop) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::internal::byte buffer[1024];
  Isolate* isolate = Isolate::Current();
  Assembler assm(isolate, buffer, sizeof(buffer));
  __ push(rbx);
  __ push(rcx);
  __ push(rdx);
  __ push(rdi);
  __ push(rsi);
  __ movq(rax, Immediate(1));
  __ movq(rbx, Immediate(2));
  __ movq(rcx, Immediate(3));
  __ movq(rdx, Immediate(4));
  __ movq(rdi, Immediate(5));
  __ movq(rsi, Immediate(6));
  for (int i = 0; i < 16; i++) {
    int before = assm.pc_offset();
    __ Nop(i);
    CHECK_EQ(assm.pc_offset() - before, i);
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
  __ pop(rsi);
  __ pop(rdi);
  __ pop(rdx);
  __ pop(rcx);
  __ pop(rbx);
  __ ret(0);
  __ bind(&fail);
  __ movq(rax, Immediate(13));
  __ pop(rsi);
  __ pop(rdi);
  __ pop(rdx);
  __ pop(rcx);
  __ pop(rbx);
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  Code* code = Code::cast(isolate->heap()->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      v8::internal::Handle<Code>())->ToObjectChecked());
  CHECK(code->IsCode());

  F0 f = FUNCTION_CAST<F0>(code->entry());
  int res = f();
  CHECK_EQ(42, res);
}


#ifdef __GNUC__
#define ELEMENT_COUNT 4

void DoSSE2(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CcTest::InitializeVM();
  v8::HandleScope scope(CcTest::isolate());
  v8::internal::byte buffer[1024];

  CHECK(args[0]->IsArray());
  v8::Local<v8::Array> vec = v8::Local<v8::Array>::Cast(args[0]);
  CHECK_EQ(ELEMENT_COUNT, vec->Length());

  Isolate* isolate = Isolate::Current();
  Assembler assm(isolate, buffer, sizeof(buffer));

  // Remove return address from the stack for fix stack frame alignment.
  __ pop(rcx);

  // Store input vector on the stack.
  for (int i = 0; i < ELEMENT_COUNT; i++) {
    __ movl(rax, Immediate(vec->Get(i)->Int32Value()));
    __ shl(rax, Immediate(0x20));
    __ or_(rax, Immediate(vec->Get(++i)->Int32Value()));
    __ push(rax);
  }

  // Read vector into a xmm register.
  __ xorps(xmm0, xmm0);
  __ movdqa(xmm0, Operand(rsp, 0));
  // Create mask and store it in the return register.
  __ movmskps(rax, xmm0);

  // Remove unused data from the stack.
  __ addq(rsp, Immediate(ELEMENT_COUNT * sizeof(int32_t)));
  // Restore return address.
  __ push(rcx);

  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  Code* code = Code::cast(isolate->heap()->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      v8::internal::Handle<Code>())->ToObjectChecked());
  CHECK(code->IsCode());

  F0 f = FUNCTION_CAST<F0>(code->entry());
  int res = f();
  args.GetReturnValue().Set(v8::Integer::New(res));
}


TEST(StackAlignmentForSSE2) {
  CHECK_EQ(0, OS::ActivationFrameAlignment() % 16);

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::ObjectTemplate> global_template = v8::ObjectTemplate::New();
  global_template->Set(v8_str("do_sse2"), v8::FunctionTemplate::New(DoSSE2));

  LocalContext env(NULL, global_template);
  CompileRun(
      "function foo(vec) {"
      "  return do_sse2(vec);"
      "}");

  v8::Local<v8::Object> global_object = env->Global();
  v8::Local<v8::Function> foo =
      v8::Local<v8::Function>::Cast(global_object->Get(v8_str("foo")));

  int32_t vec[ELEMENT_COUNT] = { -1, 1, 1, 1 };
  v8::Local<v8::Array> v8_vec = v8::Array::New(ELEMENT_COUNT);
  for (int i = 0; i < ELEMENT_COUNT; i++) {
    v8_vec->Set(i, v8_num(vec[i]));
  }

  v8::Local<v8::Value> args[] = { v8_vec };
  v8::Local<v8::Value> result = foo->Call(global_object, 1, args);

  // The mask should be 0b1000.
  CHECK_EQ(8, result->Int32Value());
}

#undef ELEMENT_COUNT
#endif  // __GNUC__


#undef __
