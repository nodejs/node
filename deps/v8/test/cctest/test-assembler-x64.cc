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

using namespace v8::internal;

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
typedef int (*F3)(double x);
typedef int64_t (*F4)(int64_t* x, int64_t* y);
typedef int64_t (*F5)(int64_t x);

#ifdef _WIN64
static const Register arg1 = rcx;
static const Register arg2 = rdx;
#else
static const Register arg1 = rdi;
static const Register arg2 = rsi;
#endif

#define __ assm.


TEST(AssemblerX64ReturnOperation) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  Assembler assm(CcTest::i_isolate(), buffer, static_cast<int>(actual_size));

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
  Assembler assm(CcTest::i_isolate(), buffer, static_cast<int>(actual_size));

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
  Assembler assm(CcTest::i_isolate(), buffer, static_cast<int>(actual_size));

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
  Assembler assm(CcTest::i_isolate(), buffer, static_cast<int>(actual_size));

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


TEST(AssemblerX64XchglOperations) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  Assembler assm(CcTest::i_isolate(), buffer, static_cast<int>(actual_size));

  __ movq(rax, Operand(arg1, 0));
  __ movq(rbx, Operand(arg2, 0));
  __ xchgl(rax, rbx);
  __ movq(Operand(arg1, 0), rax);
  __ movq(Operand(arg2, 0), rbx);
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  // Call the function from C++.
  int64_t left   = V8_2PART_UINT64_C(0x10000000, 20000000);
  int64_t right  = V8_2PART_UINT64_C(0x30000000, 40000000);
  int64_t result = FUNCTION_CAST<F4>(buffer)(&left, &right);
  CHECK_EQ(V8_2PART_UINT64_C(0x00000000, 40000000), left);
  CHECK_EQ(V8_2PART_UINT64_C(0x00000000, 20000000), right);
  USE(result);
}


TEST(AssemblerX64OrlOperations) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  Assembler assm(CcTest::i_isolate(), buffer, static_cast<int>(actual_size));

  __ movq(rax, Operand(arg2, 0));
  __ orl(Operand(arg1, 0), rax);
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  // Call the function from C++.
  int64_t left   = V8_2PART_UINT64_C(0x10000000, 20000000);
  int64_t right  = V8_2PART_UINT64_C(0x30000000, 40000000);
  int64_t result = FUNCTION_CAST<F4>(buffer)(&left, &right);
  CHECK_EQ(V8_2PART_UINT64_C(0x10000000, 60000000), left);
  USE(result);
}


TEST(AssemblerX64RollOperations) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  Assembler assm(CcTest::i_isolate(), buffer, static_cast<int>(actual_size));

  __ movq(rax, arg1);
  __ roll(rax, Immediate(1));
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  // Call the function from C++.
  int64_t src    = V8_2PART_UINT64_C(0x10000000, C0000000);
  int64_t result = FUNCTION_CAST<F5>(buffer)(src);
  CHECK_EQ(V8_2PART_UINT64_C(0x00000000, 80000001), result);
}


TEST(AssemblerX64SublOperations) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  Assembler assm(CcTest::i_isolate(), buffer, static_cast<int>(actual_size));

  __ movq(rax, Operand(arg2, 0));
  __ subl(Operand(arg1, 0), rax);
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  // Call the function from C++.
  int64_t left   = V8_2PART_UINT64_C(0x10000000, 20000000);
  int64_t right  = V8_2PART_UINT64_C(0x30000000, 40000000);
  int64_t result = FUNCTION_CAST<F4>(buffer)(&left, &right);
  CHECK_EQ(V8_2PART_UINT64_C(0x10000000, e0000000), left);
  USE(result);
}


TEST(AssemblerX64TestlOperations) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  Assembler assm(CcTest::i_isolate(), buffer, static_cast<int>(actual_size));

  // Set rax with the ZF flag of the testl instruction.
  Label done;
  __ movq(rax, Immediate(1));
  __ movq(rbx, Operand(arg2, 0));
  __ testl(Operand(arg1, 0), rbx);
  __ j(zero, &done, Label::kNear);
  __ movq(rax, Immediate(0));
  __ bind(&done);
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  // Call the function from C++.
  int64_t left   = V8_2PART_UINT64_C(0x10000000, 20000000);
  int64_t right  = V8_2PART_UINT64_C(0x30000000, 00000000);
  int64_t result = FUNCTION_CAST<F4>(buffer)(&left, &right);
  CHECK_EQ(static_cast<int64_t>(1), result);
}


TEST(AssemblerX64XorlOperations) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  Assembler assm(CcTest::i_isolate(), buffer, static_cast<int>(actual_size));

  __ movq(rax, Operand(arg2, 0));
  __ xorl(Operand(arg1, 0), rax);
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  // Call the function from C++.
  int64_t left   = V8_2PART_UINT64_C(0x10000000, 20000000);
  int64_t right  = V8_2PART_UINT64_C(0x30000000, 60000000);
  int64_t result = FUNCTION_CAST<F4>(buffer)(&left, &right);
  CHECK_EQ(V8_2PART_UINT64_C(0x10000000, 40000000), left);
  USE(result);
}


TEST(AssemblerX64MemoryOperands) {
  // Allocate an executable page of memory.
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  Assembler assm(CcTest::i_isolate(), buffer, static_cast<int>(actual_size));

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
  Assembler assm(CcTest::i_isolate(), buffer, static_cast<int>(actual_size));

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
  Assembler assm(CcTest::i_isolate(), buffer, static_cast<int>(actual_size));
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
  Assembler assm(CcTest::i_isolate(), NULL, 0);

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
      Handle<Code>())->ToObjectChecked());
  CHECK(code->IsCode());

  F0 f = FUNCTION_CAST<F0>(code->entry());
  int res = f();
  CHECK_EQ(42, res);
}


#ifdef __GNUC__
#define ELEMENT_COUNT 4

void DoSSE2(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(CcTest::isolate());
  byte buffer[1024];

  CHECK(args[0]->IsArray());
  v8::Local<v8::Array> vec = v8::Local<v8::Array>::Cast(args[0]);
  CHECK_EQ(ELEMENT_COUNT, vec->Length());

  Isolate* isolate = CcTest::i_isolate();
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
      Handle<Code>())->ToObjectChecked());
  CHECK(code->IsCode());

  F0 f = FUNCTION_CAST<F0>(code->entry());
  int res = f();
  args.GetReturnValue().Set(v8::Integer::New(res));
}


TEST(StackAlignmentForSSE2) {
  CcTest::InitializeVM();
  CHECK_EQ(0, OS::ActivationFrameAlignment() % 16);

  v8::Isolate* isolate = CcTest::isolate();
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


TEST(AssemblerX64Extractps) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(SSE4_1)) return;

  v8::HandleScope scope(CcTest::isolate());
  byte buffer[256];
  Isolate* isolate = CcTest::i_isolate();
  Assembler assm(isolate, buffer, sizeof(buffer));
  { CpuFeatureScope fscope2(&assm, SSE4_1);
    __ extractps(rax, xmm0, 0x1);
    __ ret(0);
  }

  CodeDesc desc;
  assm.GetCode(&desc);
  Code* code = Code::cast(isolate->heap()->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      Handle<Code>())->ToObjectChecked());
  CHECK(code->IsCode());
#ifdef OBJECT_PRINT
  Code::cast(code)->Print();
#endif

  F3 f = FUNCTION_CAST<F3>(Code::cast(code)->entry());
  uint64_t value1 = V8_2PART_UINT64_C(0x12345678, 87654321);
  CHECK_EQ(0x12345678, f(uint64_to_double(value1)));
  uint64_t value2 = V8_2PART_UINT64_C(0x87654321, 12345678);
  CHECK_EQ(0x87654321, f(uint64_to_double(value2)));
}


#undef __
