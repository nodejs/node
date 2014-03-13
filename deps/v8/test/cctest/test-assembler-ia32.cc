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

#include "v8.h"

#include "disassembler.h"
#include "factory.h"
#include "macro-assembler.h"
#include "platform.h"
#include "serialize.h"
#include "cctest.h"

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
  Object* code = isolate->heap()->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      Handle<Code>())->ToObjectChecked();
  CHECK(code->IsCode());
#ifdef OBJECT_PRINT
  Code::cast(code)->Print();
#endif
  F2 f = FUNCTION_CAST<F2>(Code::cast(code)->entry());
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
  Object* code = isolate->heap()->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      Handle<Code>())->ToObjectChecked();
  CHECK(code->IsCode());
#ifdef OBJECT_PRINT
  Code::cast(code)->Print();
#endif
  F1 f = FUNCTION_CAST<F1>(Code::cast(code)->entry());
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
  Object* code = isolate->heap()->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      Handle<Code>())->ToObjectChecked();
  CHECK(code->IsCode());
#ifdef OBJECT_PRINT
  Code::cast(code)->Print();
#endif
  F1 f = FUNCTION_CAST<F1>(Code::cast(code)->entry());
  int res = f(10);
  ::printf("f() = %d\n", res);
  CHECK_EQ(3628800, res);
}


typedef int (*F3)(float x);

TEST(AssemblerIa323) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(SSE2)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);

  v8::internal::byte buffer[256];
  Assembler assm(isolate, buffer, sizeof buffer);

  CHECK(CpuFeatures::IsSupported(SSE2));
  { CpuFeatureScope fscope(&assm, SSE2);
    __ cvttss2si(eax, Operand(esp, 4));
    __ ret(0);
  }

  CodeDesc desc;
  assm.GetCode(&desc);
  Code* code = Code::cast(isolate->heap()->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      Handle<Code>())->ToObjectChecked());
  // don't print the code - our disassembler can't handle cvttss2si
  // instead print bytes
  Disassembler::Dump(stdout,
                     code->instruction_start(),
                     code->instruction_start() + code->instruction_size());
  F3 f = FUNCTION_CAST<F3>(code->entry());
  int res = f(static_cast<float>(-3.1415));
  ::printf("f() = %d\n", res);
  CHECK_EQ(-3, res);
}


typedef int (*F4)(double x);

TEST(AssemblerIa324) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(SSE2)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);

  v8::internal::byte buffer[256];
  Assembler assm(isolate, buffer, sizeof buffer);

  CHECK(CpuFeatures::IsSupported(SSE2));
  CpuFeatureScope fscope(&assm, SSE2);
  __ cvttsd2si(eax, Operand(esp, 4));
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  Code* code = Code::cast(isolate->heap()->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      Handle<Code>())->ToObjectChecked());
  // don't print the code - our disassembler can't handle cvttsd2si
  // instead print bytes
  Disassembler::Dump(stdout,
                     code->instruction_start(),
                     code->instruction_start() + code->instruction_size());
  F4 f = FUNCTION_CAST<F4>(code->entry());
  int res = f(2.718281828);
  ::printf("f() = %d\n", res);
  CHECK_EQ(2, res);
}


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
  Code* code = Code::cast(isolate->heap()->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      Handle<Code>())->ToObjectChecked());
  F0 f = FUNCTION_CAST<F0>(code->entry());
  int res = f();
  CHECK_EQ(42, res);
}


typedef double (*F5)(double x, double y);

TEST(AssemblerIa326) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(SSE2)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[256];
  Assembler assm(isolate, buffer, sizeof buffer);

  CpuFeatureScope fscope(&assm, SSE2);
  __ movsd(xmm0, Operand(esp, 1 * kPointerSize));
  __ movsd(xmm1, Operand(esp, 3 * kPointerSize));
  __ addsd(xmm0, xmm1);
  __ mulsd(xmm0, xmm1);
  __ subsd(xmm0, xmm1);
  __ divsd(xmm0, xmm1);
  // Copy xmm0 to st(0) using eight bytes of stack.
  __ sub(esp, Immediate(8));
  __ movsd(Operand(esp, 0), xmm0);
  __ fld_d(Operand(esp, 0));
  __ add(esp, Immediate(8));
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);
  Code* code = Code::cast(isolate->heap()->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      Handle<Code>())->ToObjectChecked());
#ifdef DEBUG
  ::printf("\n---\n");
  // don't print the code - our disassembler can't handle SSE instructions
  // instead print bytes
  Disassembler::Dump(stdout,
                     code->instruction_start(),
                     code->instruction_start() + code->instruction_size());
#endif
  F5 f = FUNCTION_CAST<F5>(code->entry());
  double res = f(2.2, 1.1);
  ::printf("f() = %f\n", res);
  CHECK(2.29 < res && res < 2.31);
}


typedef double (*F6)(int x);

TEST(AssemblerIa328) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(SSE2)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[256];
  Assembler assm(isolate, buffer, sizeof buffer);
  CpuFeatureScope fscope(&assm, SSE2);
  __ mov(eax, Operand(esp, 4));
  __ cvtsi2sd(xmm0, eax);
  // Copy xmm0 to st(0) using eight bytes of stack.
  __ sub(esp, Immediate(8));
  __ movsd(Operand(esp, 0), xmm0);
  __ fld_d(Operand(esp, 0));
  __ add(esp, Immediate(8));
  __ ret(0);
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
  F6 f = FUNCTION_CAST<F6>(Code::cast(code)->entry());
  double res = f(12);

  ::printf("f() = %f\n", res);
  CHECK(11.99 < res && res < 12.001);
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
  Code* code = Code::cast(isolate->heap()->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      Handle<Code>())->ToObjectChecked());
  CHECK(code->IsCode());
#ifdef OBJECT_PRINT
  Code::cast(code)->Print();
#endif

  F7 f = FUNCTION_CAST<F7>(Code::cast(code)->entry());
  CHECK_EQ(kLess, f(1.1, 2.2));
  CHECK_EQ(kEqual, f(2.2, 2.2));
  CHECK_EQ(kGreater, f(3.3, 2.2));
  CHECK_EQ(kNaN, f(OS::nan_value(), 1.1));
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
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);

  CHECK(args[0]->IsArray());
  v8::Local<v8::Array> vec = v8::Local<v8::Array>::Cast(args[0]);
  CHECK_EQ(ELEMENT_COUNT, vec->Length());

  v8::internal::byte buffer[256];
  Assembler assm(isolate, buffer, sizeof buffer);

  ASSERT(CpuFeatures::IsSupported(SSE2));
  CpuFeatureScope fscope(&assm, SSE2);

  // Remove return address from the stack for fix stack frame alignment.
  __ pop(ecx);

  // Store input vector on the stack.
  for (int i = 0; i < ELEMENT_COUNT; ++i) {
    __ push(Immediate(vec->Get(i)->Int32Value()));
  }

  // Read vector into a xmm register.
  __ pxor(xmm0, xmm0);
  __ movdqa(xmm0, Operand(esp, 0));
  // Create mask and store it in the return register.
  __ movmskps(eax, xmm0);

  // Remove unused data from the stack.
  __ add(esp, Immediate(ELEMENT_COUNT * sizeof(int32_t)));
  // Restore return address.
  __ push(ecx);

  __ ret(0);

  CodeDesc desc;
  assm.GetCode(&desc);

  Object* code = isolate->heap()->CreateCode(
      desc,
      Code::ComputeFlags(Code::STUB),
      Handle<Code>())->ToObjectChecked();
  CHECK(code->IsCode());

  F0 f = FUNCTION_CAST<F0>(Code::cast(code)->entry());
  int res = f();
  args.GetReturnValue().Set(v8::Integer::New(CcTest::isolate(), res));
}


TEST(StackAlignmentForSSE2) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(SSE2)) return;

  CHECK_EQ(0, OS::ActivationFrameAlignment() % 16);

  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate);
  global_template->Set(v8_str("do_sse2"),
                       v8::FunctionTemplate::New(isolate, DoSSE2));

  LocalContext env(NULL, global_template);
  CompileRun(
      "function foo(vec) {"
      "  return do_sse2(vec);"
      "}");

  v8::Local<v8::Object> global_object = env->Global();
  v8::Local<v8::Function> foo =
      v8::Local<v8::Function>::Cast(global_object->Get(v8_str("foo")));

  int32_t vec[ELEMENT_COUNT] = { -1, 1, 1, 1 };
  v8::Local<v8::Array> v8_vec = v8::Array::New(isolate, ELEMENT_COUNT);
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


TEST(AssemblerIa32Extractps) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(SSE2) ||
      !CpuFeatures::IsSupported(SSE4_1)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[256];
  MacroAssembler assm(isolate, buffer, sizeof buffer);
  { CpuFeatureScope fscope2(&assm, SSE2);
    CpuFeatureScope fscope41(&assm, SSE4_1);
    __ movsd(xmm1, Operand(esp, 4));
    __ extractps(eax, xmm1, 0x1);
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

  F4 f = FUNCTION_CAST<F4>(Code::cast(code)->entry());
  uint64_t value1 = V8_2PART_UINT64_C(0x12345678, 87654321);
  CHECK_EQ(0x12345678, f(uint64_to_double(value1)));
  uint64_t value2 = V8_2PART_UINT64_C(0x87654321, 12345678);
  CHECK_EQ(0x87654321, f(uint64_to_double(value2)));
}


typedef int (*F8)(float x, float y);
TEST(AssemblerIa32SSE) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(SSE2)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[256];
  MacroAssembler assm(isolate, buffer, sizeof buffer);
  {
    CpuFeatureScope fscope(&assm, SSE2);
    __ movss(xmm0, Operand(esp, kPointerSize));
    __ movss(xmm1, Operand(esp, 2 * kPointerSize));
    __ shufps(xmm0, xmm0, 0x0);
    __ shufps(xmm1, xmm1, 0x0);
    __ movaps(xmm2, xmm1);
    __ addps(xmm2, xmm0);
    __ mulps(xmm2, xmm1);
    __ subps(xmm2, xmm0);
    __ divps(xmm2, xmm1);
    __ cvttss2si(eax, xmm2);
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

  F8 f = FUNCTION_CAST<F8>(Code::cast(code)->entry());
  CHECK_EQ(2, f(1.0, 2.0));
}


#undef __
