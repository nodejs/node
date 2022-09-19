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

#include "include/v8-function.h"
#include "src/base/platform/platform.h"
#include "src/base/utils/random-number-generator.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/diagnostics/disassembler.h"
#include "src/heap/factory.h"
#include "src/init/v8.h"
#include "src/utils/ostreams.h"
#include "test/cctest/cctest.h"
#include "test/common/assembler-tester.h"

namespace v8 {
namespace internal {

using F0 = int();
using F1 = int(int x);
using F2 = int(int x, int y);

#define __ assm.

TEST(AssemblerIa320) {
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);

  v8::internal::byte buffer[256];
  Assembler assm(AssemblerOptions{},
                 ExternalAssemblerBuffer(buffer, sizeof buffer));

  __ mov(eax, Operand(esp, 4));
  __ add(eax, Operand(esp, 8));
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif
  auto f = GeneratedCode<F2>::FromCode(*code);
  auto res = f.Call(3, 4);
  ::printf("f() = %d\n", res);
  CHECK_EQ(7, res);
}


TEST(AssemblerIa321) {
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);

  v8::internal::byte buffer[256];
  Assembler assm(AssemblerOptions{},
                 ExternalAssemblerBuffer(buffer, sizeof buffer));
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
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif
  auto f = GeneratedCode<F1>::FromCode(*code);
  int res = f.Call(100);
  ::printf("f() = %d\n", res);
  CHECK_EQ(5050, res);
}


TEST(AssemblerIa322) {
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);

  v8::internal::byte buffer[256];
  Assembler assm(AssemblerOptions{},
                 ExternalAssemblerBuffer(buffer, sizeof buffer));
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
  __ jmp(kNullAddress, RelocInfo::RUNTIME_ENTRY);

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif
  auto f = GeneratedCode<F1>::FromCode(*code);
  int res = f.Call(10);
  ::printf("f() = %d\n", res);
  CHECK_EQ(3628800, res);
}

using F3 = int(float x);

TEST(AssemblerIa323) {
  CcTest::InitializeVM();

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);

  v8::internal::byte buffer[256];
  Assembler assm(AssemblerOptions{},
                 ExternalAssemblerBuffer(buffer, sizeof buffer));

  __ cvttss2si(eax, Operand(esp, 4));
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif
  auto f = GeneratedCode<F3>::FromCode(*code);
  int res = f.Call(-3.1415f);
  ::printf("f() = %d\n", res);
  CHECK_EQ(-3, res);
}

using F4 = int(double x);

TEST(AssemblerIa324) {
  CcTest::InitializeVM();

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);

  v8::internal::byte buffer[256];
  Assembler assm(AssemblerOptions{},
                 ExternalAssemblerBuffer(buffer, sizeof buffer));

  __ cvttsd2si(eax, Operand(esp, 4));
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif
  auto f = GeneratedCode<F4>::FromCode(*code);
  int res = f.Call(2.718281828);
  ::printf("f() = %d\n", res);
  CHECK_EQ(2, res);
}


static int baz = 42;
TEST(AssemblerIa325) {
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);

  v8::internal::byte buffer[256];
  Assembler assm(AssemblerOptions{},
                 ExternalAssemblerBuffer(buffer, sizeof buffer));

  __ mov(eax, Operand(reinterpret_cast<intptr_t>(&baz), RelocInfo::NO_INFO));
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
  auto f = GeneratedCode<F0>::FromCode(*code);
  int res = f.Call();
  CHECK_EQ(42, res);
}

using F5 = double(double x, double y);

TEST(AssemblerIa326) {
  CcTest::InitializeVM();

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[256];
  Assembler assm(AssemblerOptions{},
                 ExternalAssemblerBuffer(buffer, sizeof buffer));

  __ movsd(xmm0, Operand(esp, 1 * kSystemPointerSize));
  __ movsd(xmm1, Operand(esp, 3 * kSystemPointerSize));
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
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif
  auto f = GeneratedCode<F5>::FromCode(*code);
  double res = f.Call(2.2, 1.1);
  ::printf("f() = %f\n", res);
  CHECK(2.29 < res && res < 2.31);
}

using F6 = double(int x);

TEST(AssemblerIa328) {
  CcTest::InitializeVM();

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[256];
  Assembler assm(AssemblerOptions{},
                 ExternalAssemblerBuffer(buffer, sizeof buffer));
  __ mov(eax, Operand(esp, 4));
  __ cvtsi2sd(xmm0, eax);
  // Copy xmm0 to st(0) using eight bytes of stack.
  __ sub(esp, Immediate(8));
  __ movsd(Operand(esp, 0), xmm0);
  __ fld_d(Operand(esp, 0));
  __ add(esp, Immediate(8));
  __ ret(0);
  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif
  auto f = GeneratedCode<F6>::FromCode(*code);
  double res = f.Call(12);

  ::printf("f() = %f\n", res);
  CHECK(11.99 < res && res < 12.001);
}

TEST(AssemblerIa3210) {
  // Test chaining of label usages within instructions (issue 1644).
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  Assembler assm(AssemblerOptions{});

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
  Assembler assm(AssemblerOptions{},
                 ExternalAssemblerBuffer(buffer, sizeof(buffer)));
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
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
  CHECK(code->IsCode());

  auto f = GeneratedCode<F0>::FromCode(*code);
  int res = f.Call();
  CHECK_EQ(42, res);
}


#ifdef __GNUC__
#define ELEMENT_COUNT 4u

void DoSSE2(const v8::FunctionCallbackInfo<v8::Value>& args) {
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::Local<v8::Context> context = CcTest::isolate()->GetCurrentContext();

  CHECK(args[0]->IsArray());
  v8::Local<v8::Array> vec = v8::Local<v8::Array>::Cast(args[0]);
  CHECK_EQ(ELEMENT_COUNT, vec->Length());

  v8::internal::byte buffer[256];
  Assembler assm(AssemblerOptions{},
                 ExternalAssemblerBuffer(buffer, sizeof buffer));

  // Remove return address from the stack for fix stack frame alignment.
  __ pop(ecx);

  // Store input vector on the stack.
  for (unsigned i = 0; i < ELEMENT_COUNT; ++i) {
    __ push(Immediate(
        vec->Get(context, i).ToLocalChecked()->Int32Value(context).FromJust()));
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
  assm.GetCode(isolate, &desc);

  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();

  auto f = GeneratedCode<F0>::FromCode(*code);
  int res = f.Call();
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


TEST(AssemblerIa32Extractps) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(SSE4_1)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[256];
  MacroAssembler assm(isolate, v8::internal::CodeObjectRequired::kYes,
                      ExternalAssemblerBuffer(buffer, sizeof(buffer)));
  { CpuFeatureScope fscope41(&assm, SSE4_1);
    __ movsd(xmm1, Operand(esp, 4));
    __ extractps(eax, xmm1, 0x1);
    __ ret(0);
  }

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif

  auto f = GeneratedCode<F4>::FromCode(*code);
  uint64_t value1 = 0x1234'5678'8765'4321;
  CHECK_EQ(0x12345678, f.Call(base::uint64_to_double(value1)));
  uint64_t value2 = 0x8765'4321'1234'5678;
  CHECK_EQ(static_cast<int>(0x87654321),
           f.Call(base::uint64_to_double(value2)));
}

using F8 = int(float x, float y);
TEST(AssemblerIa32SSE) {
  CcTest::InitializeVM();

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[256];
  MacroAssembler assm(isolate, v8::internal::CodeObjectRequired::kYes,
                      ExternalAssemblerBuffer(buffer, sizeof(buffer)));
  {
    __ movss(xmm0, Operand(esp, kSystemPointerSize));
    __ movss(xmm1, Operand(esp, 2 * kSystemPointerSize));
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
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif

  auto f = GeneratedCode<F8>::FromCode(*code);
  CHECK_EQ(2, f.Call(1.0, 2.0));
}

TEST(AssemblerIa32SSE3) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(SSE3)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[256];
  MacroAssembler assm(isolate, v8::internal::CodeObjectRequired::kYes,
                      ExternalAssemblerBuffer(buffer, sizeof(buffer)));
  {
    CpuFeatureScope fscope(&assm, SSE3);
    __ movss(xmm0, Operand(esp, kSystemPointerSize));
    __ movss(xmm1, Operand(esp, 2 * kSystemPointerSize));
    __ shufps(xmm0, xmm0, 0x0);
    __ shufps(xmm1, xmm1, 0x0);
    __ haddps(xmm1, xmm0);
    __ cvttss2si(eax, xmm1);
    __ ret(0);
  }

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif

  auto f = GeneratedCode<F8>::FromCode(*code);
  CHECK_EQ(4, f.Call(1.0, 2.0));
}

using F9 = int(double x, double y, double z);
TEST(AssemblerX64FMA_sd) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(FMA3)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[1024];
  MacroAssembler assm(isolate, v8::internal::CodeObjectRequired::kYes,
                      ExternalAssemblerBuffer(buffer, sizeof(buffer)));
  {
    CpuFeatureScope fscope(&assm, FMA3);
    Label exit;
    __ movsd(xmm0, Operand(esp, 1 * kSystemPointerSize));
    __ movsd(xmm1, Operand(esp, 3 * kSystemPointerSize));
    __ movsd(xmm2, Operand(esp, 5 * kSystemPointerSize));
    // argument in xmm0, xmm1 and xmm2
    // xmm0 * xmm1 + xmm2
    __ movaps(xmm3, xmm0);
    __ mulsd(xmm3, xmm1);
    __ addsd(xmm3, xmm2);  // Expected result in xmm3

    __ AllocateStackSpace(kDoubleSize);  // For memory operand
    // vfmadd132sd
    __ mov(eax, Immediate(1));  // Test number
    __ movaps(xmm4, xmm0);
    __ vfmadd132sd(xmm4, xmm2, xmm1);
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmadd213sd
    __ inc(eax);
    __ movaps(xmm4, xmm1);
    __ vfmadd213sd(xmm4, xmm0, xmm2);
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmadd231sd
    __ inc(eax);
    __ movaps(xmm4, xmm2);
    __ vfmadd231sd(xmm4, xmm0, xmm1);
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);

    // vfmadd132sd
    __ inc(eax);
    __ movaps(xmm4, xmm0);
    __ movsd(Operand(esp, 0), xmm1);
    __ vfmadd132sd(xmm4, xmm2, Operand(esp, 0));
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmadd213sd
    __ inc(eax);
    __ movaps(xmm4, xmm1);
    __ movsd(Operand(esp, 0), xmm2);
    __ vfmadd213sd(xmm4, xmm0, Operand(esp, 0));
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmadd231sd
    __ inc(eax);
    __ movaps(xmm4, xmm2);
    __ movsd(Operand(esp, 0), xmm1);
    __ vfmadd231sd(xmm4, xmm0, Operand(esp, 0));
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);

    // xmm0 * xmm1 - xmm2
    __ movaps(xmm3, xmm0);
    __ mulsd(xmm3, xmm1);
    __ subsd(xmm3, xmm2);  // Expected result in xmm3

    // vfmsub132sd
    __ inc(eax);
    __ movaps(xmm4, xmm0);
    __ vfmsub132sd(xmm4, xmm2, xmm1);
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmadd213sd
    __ inc(eax);
    __ movaps(xmm4, xmm1);
    __ vfmsub213sd(xmm4, xmm0, xmm2);
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmsub231sd
    __ inc(eax);
    __ movaps(xmm4, xmm2);
    __ vfmsub231sd(xmm4, xmm0, xmm1);
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);

    // vfmsub132sd
    __ inc(eax);
    __ movaps(xmm4, xmm0);
    __ movsd(Operand(esp, 0), xmm1);
    __ vfmsub132sd(xmm4, xmm2, Operand(esp, 0));
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmsub213sd
    __ inc(eax);
    __ movaps(xmm4, xmm1);
    __ movsd(Operand(esp, 0), xmm2);
    __ vfmsub213sd(xmm4, xmm0, Operand(esp, 0));
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmsub231sd
    __ inc(eax);
    __ movaps(xmm4, xmm2);
    __ movsd(Operand(esp, 0), xmm1);
    __ vfmsub231sd(xmm4, xmm0, Operand(esp, 0));
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);


    // - xmm0 * xmm1 + xmm2
    __ movaps(xmm3, xmm0);
    __ mulsd(xmm3, xmm1);
    __ Move(xmm4, (uint64_t)1 << 63);
    __ xorpd(xmm3, xmm4);
    __ addsd(xmm3, xmm2);  // Expected result in xmm3

    // vfnmadd132sd
    __ inc(eax);
    __ movaps(xmm4, xmm0);
    __ vfnmadd132sd(xmm4, xmm2, xmm1);
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmadd213sd
    __ inc(eax);
    __ movaps(xmm4, xmm1);
    __ vfnmadd213sd(xmm4, xmm0, xmm2);
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfnmadd231sd
    __ inc(eax);
    __ movaps(xmm4, xmm2);
    __ vfnmadd231sd(xmm4, xmm0, xmm1);
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);

    // vfnmadd132sd
    __ inc(eax);
    __ movaps(xmm4, xmm0);
    __ movsd(Operand(esp, 0), xmm1);
    __ vfnmadd132sd(xmm4, xmm2, Operand(esp, 0));
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfnmadd213sd
    __ inc(eax);
    __ movaps(xmm4, xmm1);
    __ movsd(Operand(esp, 0), xmm2);
    __ vfnmadd213sd(xmm4, xmm0, Operand(esp, 0));
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfnmadd231sd
    __ inc(eax);
    __ movaps(xmm4, xmm2);
    __ movsd(Operand(esp, 0), xmm1);
    __ vfnmadd231sd(xmm4, xmm0, Operand(esp, 0));
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);


    // - xmm0 * xmm1 - xmm2
    __ movaps(xmm3, xmm0);
    __ mulsd(xmm3, xmm1);
    __ Move(xmm4, (uint64_t)1 << 63);
    __ xorpd(xmm3, xmm4);
    __ subsd(xmm3, xmm2);  // Expected result in xmm3

    // vfnmsub132sd
    __ inc(eax);
    __ movaps(xmm4, xmm0);
    __ vfnmsub132sd(xmm4, xmm2, xmm1);
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmsub213sd
    __ inc(eax);
    __ movaps(xmm4, xmm1);
    __ vfnmsub213sd(xmm4, xmm0, xmm2);
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfnmsub231sd
    __ inc(eax);
    __ movaps(xmm4, xmm2);
    __ vfnmsub231sd(xmm4, xmm0, xmm1);
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);

    // vfnmsub132sd
    __ inc(eax);
    __ movaps(xmm4, xmm0);
    __ movsd(Operand(esp, 0), xmm1);
    __ vfnmsub132sd(xmm4, xmm2, Operand(esp, 0));
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfnmsub213sd
    __ inc(eax);
    __ movaps(xmm4, xmm1);
    __ movsd(Operand(esp, 0), xmm2);
    __ vfnmsub213sd(xmm4, xmm0, Operand(esp, 0));
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfnmsub231sd
    __ inc(eax);
    __ movaps(xmm4, xmm2);
    __ movsd(Operand(esp, 0), xmm1);
    __ vfnmsub231sd(xmm4, xmm0, Operand(esp, 0));
    __ ucomisd(xmm4, xmm3);
    __ j(not_equal, &exit);


    __ xor_(eax, eax);
    __ bind(&exit);
    __ add(esp, Immediate(kDoubleSize));
    __ ret(0);
  }

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif

  auto f = GeneratedCode<F9>::FromCode(*code);
  CHECK_EQ(
      0, f.Call(0.000092662107262076, -2.460774966188315, -1.0958787393627414));
}

using F10 = int(float x, float y, float z);
TEST(AssemblerX64FMA_ss) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(FMA3)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[1024];
  MacroAssembler assm(isolate, v8::internal::CodeObjectRequired::kYes,
                      ExternalAssemblerBuffer(buffer, sizeof(buffer)));
  {
    CpuFeatureScope fscope(&assm, FMA3);
    Label exit;
    __ movss(xmm0, Operand(esp, 1 * kSystemPointerSize));
    __ movss(xmm1, Operand(esp, 2 * kSystemPointerSize));
    __ movss(xmm2, Operand(esp, 3 * kSystemPointerSize));
    // arguments in xmm0, xmm1 and xmm2
    // xmm0 * xmm1 + xmm2
    __ movaps(xmm3, xmm0);
    __ mulss(xmm3, xmm1);
    __ addss(xmm3, xmm2);  // Expected result in xmm3

    __ AllocateStackSpace(kDoubleSize);  // For memory operand
    // vfmadd132ss
    __ mov(eax, Immediate(1));  // Test number
    __ movaps(xmm4, xmm0);
    __ vfmadd132ss(xmm4, xmm2, xmm1);
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmadd213ss
    __ inc(eax);
    __ movaps(xmm4, xmm1);
    __ vfmadd213ss(xmm4, xmm0, xmm2);
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmadd231ss
    __ inc(eax);
    __ movaps(xmm4, xmm2);
    __ vfmadd231ss(xmm4, xmm0, xmm1);
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);

    // vfmadd132ss
    __ inc(eax);
    __ movaps(xmm4, xmm0);
    __ movss(Operand(esp, 0), xmm1);
    __ vfmadd132ss(xmm4, xmm2, Operand(esp, 0));
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmadd213ss
    __ inc(eax);
    __ movaps(xmm4, xmm1);
    __ movss(Operand(esp, 0), xmm2);
    __ vfmadd213ss(xmm4, xmm0, Operand(esp, 0));
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmadd231ss
    __ inc(eax);
    __ movaps(xmm4, xmm2);
    __ movss(Operand(esp, 0), xmm1);
    __ vfmadd231ss(xmm4, xmm0, Operand(esp, 0));
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);

    // xmm0 * xmm1 - xmm2
    __ movaps(xmm3, xmm0);
    __ mulss(xmm3, xmm1);
    __ subss(xmm3, xmm2);  // Expected result in xmm3

    // vfmsub132ss
    __ inc(eax);
    __ movaps(xmm4, xmm0);
    __ vfmsub132ss(xmm4, xmm2, xmm1);
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmadd213ss
    __ inc(eax);
    __ movaps(xmm4, xmm1);
    __ vfmsub213ss(xmm4, xmm0, xmm2);
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmsub231ss
    __ inc(eax);
    __ movaps(xmm4, xmm2);
    __ vfmsub231ss(xmm4, xmm0, xmm1);
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);

    // vfmsub132ss
    __ inc(eax);
    __ movaps(xmm4, xmm0);
    __ movss(Operand(esp, 0), xmm1);
    __ vfmsub132ss(xmm4, xmm2, Operand(esp, 0));
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmsub213ss
    __ inc(eax);
    __ movaps(xmm4, xmm1);
    __ movss(Operand(esp, 0), xmm2);
    __ vfmsub213ss(xmm4, xmm0, Operand(esp, 0));
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmsub231ss
    __ inc(eax);
    __ movaps(xmm4, xmm2);
    __ movss(Operand(esp, 0), xmm1);
    __ vfmsub231ss(xmm4, xmm0, Operand(esp, 0));
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);


    // - xmm0 * xmm1 + xmm2
    __ movaps(xmm3, xmm0);
    __ mulss(xmm3, xmm1);
    __ Move(xmm4, (uint32_t)1 << 31);
    __ xorps(xmm3, xmm4);
    __ addss(xmm3, xmm2);  // Expected result in xmm3

    // vfnmadd132ss
    __ inc(eax);
    __ movaps(xmm4, xmm0);
    __ vfnmadd132ss(xmm4, xmm2, xmm1);
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmadd213ss
    __ inc(eax);
    __ movaps(xmm4, xmm1);
    __ vfnmadd213ss(xmm4, xmm0, xmm2);
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfnmadd231ss
    __ inc(eax);
    __ movaps(xmm4, xmm2);
    __ vfnmadd231ss(xmm4, xmm0, xmm1);
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);

    // vfnmadd132ss
    __ inc(eax);
    __ movaps(xmm4, xmm0);
    __ movss(Operand(esp, 0), xmm1);
    __ vfnmadd132ss(xmm4, xmm2, Operand(esp, 0));
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfnmadd213ss
    __ inc(eax);
    __ movaps(xmm4, xmm1);
    __ movss(Operand(esp, 0), xmm2);
    __ vfnmadd213ss(xmm4, xmm0, Operand(esp, 0));
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfnmadd231ss
    __ inc(eax);
    __ movaps(xmm4, xmm2);
    __ movss(Operand(esp, 0), xmm1);
    __ vfnmadd231ss(xmm4, xmm0, Operand(esp, 0));
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);


    // - xmm0 * xmm1 - xmm2
    __ movaps(xmm3, xmm0);
    __ mulss(xmm3, xmm1);
    __ Move(xmm4, (uint32_t)1 << 31);
    __ xorps(xmm3, xmm4);
    __ subss(xmm3, xmm2);  // Expected result in xmm3

    // vfnmsub132ss
    __ inc(eax);
    __ movaps(xmm4, xmm0);
    __ vfnmsub132ss(xmm4, xmm2, xmm1);
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfmsub213ss
    __ inc(eax);
    __ movaps(xmm4, xmm1);
    __ vfnmsub213ss(xmm4, xmm0, xmm2);
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfnmsub231ss
    __ inc(eax);
    __ movaps(xmm4, xmm2);
    __ vfnmsub231ss(xmm4, xmm0, xmm1);
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);

    // vfnmsub132ss
    __ inc(eax);
    __ movaps(xmm4, xmm0);
    __ movss(Operand(esp, 0), xmm1);
    __ vfnmsub132ss(xmm4, xmm2, Operand(esp, 0));
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfnmsub213ss
    __ inc(eax);
    __ movaps(xmm4, xmm1);
    __ movss(Operand(esp, 0), xmm2);
    __ vfnmsub213ss(xmm4, xmm0, Operand(esp, 0));
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);
    // vfnmsub231ss
    __ inc(eax);
    __ movaps(xmm4, xmm2);
    __ movss(Operand(esp, 0), xmm1);
    __ vfnmsub231ss(xmm4, xmm0, Operand(esp, 0));
    __ ucomiss(xmm4, xmm3);
    __ j(not_equal, &exit);


    __ xor_(eax, eax);
    __ bind(&exit);
    __ add(esp, Immediate(kDoubleSize));
    __ ret(0);
  }

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif

  auto f = GeneratedCode<F10>::FromCode(*code);
  CHECK_EQ(0, f.Call(9.26621069e-05f, -2.4607749f, -1.09587872f));
}


TEST(AssemblerIa32BMI1) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(BMI1)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[1024];
  MacroAssembler assm(isolate, v8::internal::CodeObjectRequired::kYes,
                      ExternalAssemblerBuffer(buffer, sizeof(buffer)));
  {
    CpuFeatureScope fscope(&assm, BMI1);
    Label exit;

    __ push(ebx);                         // save ebx
    __ mov(ecx, Immediate(0x55667788u));  // source operand
    __ push(ecx);                         // For memory operand

    // andn
    __ mov(edx, Immediate(0x20000000u));

    __ mov(eax, Immediate(1));  // Test number
    __ andn(ebx, edx, ecx);
    __ cmp(ebx, Immediate(0x55667788u));  // expected result
    __ j(not_equal, &exit);

    __ inc(eax);
    __ andn(ebx, edx, Operand(esp, 0));
    __ cmp(ebx, Immediate(0x55667788u));  // expected result
    __ j(not_equal, &exit);

    // bextr
    __ mov(edx, Immediate(0x00002808u));

    __ inc(eax);
    __ bextr(ebx, ecx, edx);
    __ cmp(ebx, Immediate(0x00556677u));  // expected result
    __ j(not_equal, &exit);

    __ inc(eax);
    __ bextr(ebx, Operand(esp, 0), edx);
    __ cmp(ebx, Immediate(0x00556677u));  // expected result
    __ j(not_equal, &exit);

    // blsi
    __ inc(eax);
    __ blsi(ebx, ecx);
    __ cmp(ebx, Immediate(0x00000008u));  // expected result
    __ j(not_equal, &exit);

    __ inc(eax);
    __ blsi(ebx, Operand(esp, 0));
    __ cmp(ebx, Immediate(0x00000008u));  // expected result
    __ j(not_equal, &exit);

    // blsmsk
    __ inc(eax);
    __ blsmsk(ebx, ecx);
    __ cmp(ebx, Immediate(0x0000000Fu));  // expected result
    __ j(not_equal, &exit);

    __ inc(eax);
    __ blsmsk(ebx, Operand(esp, 0));
    __ cmp(ebx, Immediate(0x0000000Fu));  // expected result
    __ j(not_equal, &exit);

    // blsr
    __ inc(eax);
    __ blsr(ebx, ecx);
    __ cmp(ebx, Immediate(0x55667780u));  // expected result
    __ j(not_equal, &exit);

    __ inc(eax);
    __ blsr(ebx, Operand(esp, 0));
    __ cmp(ebx, Immediate(0x55667780u));  // expected result
    __ j(not_equal, &exit);

    // tzcnt
    __ inc(eax);
    __ tzcnt(ebx, ecx);
    __ cmp(ebx, Immediate(3));  // expected result
    __ j(not_equal, &exit);

    __ inc(eax);
    __ tzcnt(ebx, Operand(esp, 0));
    __ cmp(ebx, Immediate(3));  // expected result
    __ j(not_equal, &exit);

    __ xor_(eax, eax);
    __ bind(&exit);
    __ pop(ecx);
    __ pop(ebx);
    __ ret(0);
  }

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif

  auto f = GeneratedCode<F0>::FromCode(*code);
  CHECK_EQ(0, f.Call());
}


TEST(AssemblerIa32LZCNT) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(LZCNT)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[256];
  MacroAssembler assm(isolate, v8::internal::CodeObjectRequired::kYes,
                      ExternalAssemblerBuffer(buffer, sizeof(buffer)));
  {
    CpuFeatureScope fscope(&assm, LZCNT);
    Label exit;

    __ push(ebx);                         // save ebx
    __ mov(ecx, Immediate(0x55667788u));  // source operand
    __ push(ecx);                         // For memory operand

    __ mov(eax, Immediate(1));  // Test number
    __ lzcnt(ebx, ecx);
    __ cmp(ebx, Immediate(1));  // expected result
    __ j(not_equal, &exit);

    __ inc(eax);
    __ lzcnt(ebx, Operand(esp, 0));
    __ cmp(ebx, Immediate(1));  // expected result
    __ j(not_equal, &exit);

    __ xor_(eax, eax);
    __ bind(&exit);
    __ pop(ecx);
    __ pop(ebx);
    __ ret(0);
  }

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif

  auto f = GeneratedCode<F0>::FromCode(*code);
  CHECK_EQ(0, f.Call());
}


TEST(AssemblerIa32POPCNT) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(POPCNT)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[256];
  MacroAssembler assm(isolate, v8::internal::CodeObjectRequired::kYes,
                      ExternalAssemblerBuffer(buffer, sizeof(buffer)));
  {
    CpuFeatureScope fscope(&assm, POPCNT);
    Label exit;

    __ push(ebx);                         // save ebx
    __ mov(ecx, Immediate(0x11111100u));  // source operand
    __ push(ecx);                         // For memory operand

    __ mov(eax, Immediate(1));  // Test number
    __ popcnt(ebx, ecx);
    __ cmp(ebx, Immediate(6));  // expected result
    __ j(not_equal, &exit);

    __ inc(eax);
    __ popcnt(ebx, Operand(esp, 0));
    __ cmp(ebx, Immediate(6));  // expected result
    __ j(not_equal, &exit);

    __ xor_(eax, eax);
    __ bind(&exit);
    __ pop(ecx);
    __ pop(ebx);
    __ ret(0);
  }

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif

  auto f = GeneratedCode<F0>::FromCode(*code);
  CHECK_EQ(0, f.Call());
}


TEST(AssemblerIa32BMI2) {
  CcTest::InitializeVM();
  if (!CpuFeatures::IsSupported(BMI2)) return;

  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  v8::internal::byte buffer[2048];
  MacroAssembler assm(isolate, v8::internal::CodeObjectRequired::kYes,
                      ExternalAssemblerBuffer(buffer, sizeof(buffer)));
  {
    CpuFeatureScope fscope(&assm, BMI2);
    Label exit;

    __ push(ebx);                         // save ebx
    __ push(esi);                         // save esi
    __ mov(ecx, Immediate(0x55667788u));  // source operand
    __ push(ecx);                         // For memory operand

    // bzhi
    __ mov(edx, Immediate(9));

    __ mov(eax, Immediate(1));  // Test number
    __ bzhi(ebx, ecx, edx);
    __ cmp(ebx, Immediate(0x00000188u));  // expected result
    __ j(not_equal, &exit);

    __ inc(eax);
    __ bzhi(ebx, Operand(esp, 0), edx);
    __ cmp(ebx, Immediate(0x00000188u));  // expected result
    __ j(not_equal, &exit);

    // mulx
    __ mov(edx, Immediate(0x00001000u));

    __ inc(eax);
    __ mulx(ebx, esi, ecx);
    __ cmp(ebx, Immediate(0x00000556u));  // expected result
    __ j(not_equal, &exit);
    __ cmp(esi, Immediate(0x67788000u));  // expected result
    __ j(not_equal, &exit);

    __ inc(eax);
    __ mulx(ebx, esi, Operand(esp, 0));
    __ cmp(ebx, Immediate(0x00000556u));  // expected result
    __ j(not_equal, &exit);
    __ cmp(esi, Immediate(0x67788000u));  // expected result
    __ j(not_equal, &exit);

    // pdep
    __ mov(edx, Immediate(0xFFFFFFF0u));

    __ inc(eax);
    __ pdep(ebx, edx, ecx);
    __ cmp(ebx, Immediate(0x55667400u));  // expected result
    __ j(not_equal, &exit);

    __ inc(eax);
    __ pdep(ebx, edx, Operand(esp, 0));
    __ cmp(ebx, Immediate(0x55667400u));  // expected result
    __ j(not_equal, &exit);

    // pext
    __ mov(edx, Immediate(0xFFFFFFF0u));

    __ inc(eax);
    __ pext(ebx, edx, ecx);
    __ cmp(ebx, Immediate(0x0000FFFEu));  // expected result
    __ j(not_equal, &exit);

    __ inc(eax);
    __ pext(ebx, edx, Operand(esp, 0));
    __ cmp(ebx, Immediate(0x0000FFFEu));  // expected result
    __ j(not_equal, &exit);

    // sarx
    __ mov(edx, Immediate(4));

    __ inc(eax);
    __ sarx(ebx, ecx, edx);
    __ cmp(ebx, Immediate(0x05566778u));  // expected result
    __ j(not_equal, &exit);

    __ inc(eax);
    __ sarx(ebx, Operand(esp, 0), edx);
    __ cmp(ebx, Immediate(0x05566778u));  // expected result
    __ j(not_equal, &exit);

    // shlx
    __ mov(edx, Immediate(4));

    __ inc(eax);
    __ shlx(ebx, ecx, edx);
    __ cmp(ebx, Immediate(0x56677880u));  // expected result
    __ j(not_equal, &exit);

    __ inc(eax);
    __ shlx(ebx, Operand(esp, 0), edx);
    __ cmp(ebx, Immediate(0x56677880u));  // expected result
    __ j(not_equal, &exit);

    // shrx
    __ mov(edx, Immediate(4));

    __ inc(eax);
    __ shrx(ebx, ecx, edx);
    __ cmp(ebx, Immediate(0x05566778u));  // expected result
    __ j(not_equal, &exit);

    __ inc(eax);
    __ shrx(ebx, Operand(esp, 0), edx);
    __ cmp(ebx, Immediate(0x05566778u));  // expected result
    __ j(not_equal, &exit);

    // rorx
    __ inc(eax);
    __ rorx(ebx, ecx, 0x4);
    __ cmp(ebx, Immediate(0x85566778u));  // expected result
    __ j(not_equal, &exit);

    __ inc(eax);
    __ rorx(ebx, Operand(esp, 0), 0x4);
    __ cmp(ebx, Immediate(0x85566778u));  // expected result
    __ j(not_equal, &exit);

    __ xor_(eax, eax);
    __ bind(&exit);
    __ pop(ecx);
    __ pop(esi);
    __ pop(ebx);
    __ ret(0);
  }

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif

  auto f = GeneratedCode<F0>::FromCode(*code);
  CHECK_EQ(0, f.Call());
}


TEST(AssemblerIa32JumpTables1) {
  // Test jump tables with forward jumps.
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  Assembler assm(AssemblerOptions{});

  const int kNumCases = 512;
  int values[kNumCases];
  isolate->random_number_generator()->NextBytes(values, sizeof(values));
  Label labels[kNumCases];

  Label done, table;
  __ mov(eax, Operand(esp, 4));
  __ jmp(Operand::JumpTable(eax, times_system_pointer_size, &table));
  __ ud2();
  __ bind(&table);
  for (int i = 0; i < kNumCases; ++i) {
    __ dd(&labels[i]);
  }

  for (int i = 0; i < kNumCases; ++i) {
    __ bind(&labels[i]);
    __ mov(eax, Immediate(values[i]));
    __ jmp(&done);
  }

  __ bind(&done);
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif
  auto f = GeneratedCode<F1>::FromCode(*code);
  for (int i = 0; i < kNumCases; ++i) {
    int res = f.Call(i);
    ::printf("f(%d) = %d\n", i, res);
    CHECK_EQ(values[i], res);
  }
}


TEST(AssemblerIa32JumpTables2) {
  // Test jump tables with backward jumps.
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  Assembler assm(AssemblerOptions{});

  const int kNumCases = 512;
  int values[kNumCases];
  isolate->random_number_generator()->NextBytes(values, sizeof(values));
  Label labels[kNumCases];

  Label done, table;
  __ mov(eax, Operand(esp, 4));
  __ jmp(Operand::JumpTable(eax, times_system_pointer_size, &table));
  __ ud2();

  for (int i = 0; i < kNumCases; ++i) {
    __ bind(&labels[i]);
    __ mov(eax, Immediate(values[i]));
    __ jmp(&done);
  }

  __ bind(&table);
  for (int i = 0; i < kNumCases; ++i) {
    __ dd(&labels[i]);
  }

  __ bind(&done);
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif
  auto f = GeneratedCode<F1>::FromCode(*code);
  for (int i = 0; i < kNumCases; ++i) {
    int res = f.Call(i);
    ::printf("f(%d) = %d\n", i, res);
    CHECK_EQ(values[i], res);
  }
}

TEST(Regress621926) {
  // Bug description:
  // The opcodes for cmpw r/m16, r16 and cmpw r16, r/m16 were swapped.
  // This was causing non-commutative comparisons to produce the wrong result.
  CcTest::InitializeVM();
  Isolate* isolate = reinterpret_cast<Isolate*>(CcTest::isolate());
  HandleScope scope(isolate);
  Assembler assm(AssemblerOptions{});

  uint16_t a = 42;

  Label fail;
  __ push(ebx);
  __ mov(ebx, Immediate(reinterpret_cast<intptr_t>(&a)));
  __ mov(eax, Immediate(41));
  __ cmpw(eax, Operand(ebx, 0));
  __ j(above_equal, &fail);
  __ cmpw(Operand(ebx, 0), eax);
  __ j(below_equal, &fail);
  __ mov(eax, 1);
  __ pop(ebx);
  __ ret(0);
  __ bind(&fail);
  __ mov(eax, 0);
  __ pop(ebx);
  __ ret(0);

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();

#ifdef OBJECT_PRINT
  StdoutStream os;
  code->Print(os);
#endif

  auto f = GeneratedCode<F0>::FromCode(*code);
  CHECK_EQ(1, f.Call());
}

TEST(DeoptExitSizeIsFixed) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);
  v8::internal::byte buffer[256];
  MacroAssembler masm(isolate, v8::internal::CodeObjectRequired::kYes,
                      ExternalAssemblerBuffer(buffer, sizeof(buffer)));

  STATIC_ASSERT(static_cast<int>(kFirstDeoptimizeKind) == 0);
  for (int i = 0; i < kDeoptimizeKindCount; i++) {
    DeoptimizeKind kind = static_cast<DeoptimizeKind>(i);
    Label before_exit;
    masm.bind(&before_exit);
    Builtin target = Deoptimizer::GetDeoptimizationEntry(kind);
    masm.CallForDeoptimization(target, 42, &before_exit, kind, &before_exit,
                               nullptr);
    CHECK_EQ(masm.SizeOfCodeGeneratedSince(&before_exit),
             kind == DeoptimizeKind::kLazy ? Deoptimizer::kLazyDeoptExitSize
                                           : Deoptimizer::kEagerDeoptExitSize);
  }
}

TEST(CpuFeatures_ProbeImpl) {
  // Support for a newer extension implies support for the older extensions.
  CHECK_IMPLIES(CpuFeatures::IsSupported(FMA3), CpuFeatures::IsSupported(AVX));
  CHECK_IMPLIES(CpuFeatures::IsSupported(AVX2), CpuFeatures::IsSupported(AVX));
  CHECK_IMPLIES(CpuFeatures::IsSupported(AVX),
                CpuFeatures::IsSupported(SSE4_2));
  CHECK_IMPLIES(CpuFeatures::IsSupported(SSE4_2),
                CpuFeatures::IsSupported(SSE4_1));
  CHECK_IMPLIES(CpuFeatures::IsSupported(SSE4_1),
                CpuFeatures::IsSupported(SSSE3));
  CHECK_IMPLIES(CpuFeatures::IsSupported(SSSE3),
                CpuFeatures::IsSupported(SSE3));

  // Check the reverse, if an older extension is not supported, a newer
  // extension cannot be supported.
  CHECK_IMPLIES(!CpuFeatures::IsSupported(SSE3),
                !CpuFeatures::IsSupported(SSSE3));
  CHECK_IMPLIES(!CpuFeatures::IsSupported(SSSE3),
                !CpuFeatures::IsSupported(SSE4_1));
  CHECK_IMPLIES(!CpuFeatures::IsSupported(SSE4_1),
                !CpuFeatures::IsSupported(SSE4_2));
  CHECK_IMPLIES(!CpuFeatures::IsSupported(SSE4_2),
                !CpuFeatures::IsSupported(AVX));
  CHECK_IMPLIES(!CpuFeatures::IsSupported(AVX),
                !CpuFeatures::IsSupported(AVX2));
  CHECK_IMPLIES(!CpuFeatures::IsSupported(AVX),
                !CpuFeatures::IsSupported(FMA3));
}

#undef __

}  // namespace internal
}  // namespace v8
