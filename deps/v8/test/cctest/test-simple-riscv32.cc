// Copyright 2021 the V8 project authors. All rights reserved.
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

#include <iostream>

#include "src/base/utils/random-number-generator.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/diagnostics/disassembler.h"
#include "src/execution/simulator.h"
#include "src/heap/factory.h"
#include "src/init/v8.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

// Define these function prototypes to match JSEntryFunction in execution.cc.
// TODO(mips64): Refine these signatures per test case.
using F1 = void*(int x, int p1, int p2, int p3, int p4);
using F2 = void*(int x, int y, int p2, int p3, int p4);
using F3 = void*(void* p, int p1, int p2, int p3, int p4);
using F4 = void*(int64_t x, int64_t y, int64_t p2, int64_t p3, int64_t p4);
using F5 = void*(void* p0, void* p1, int p2, int p3, int p4);

#define __ assm.

TEST(RISCV_SIMPLE0) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assm(isolate, v8::internal::CodeObjectRequired::kYes);

  // Addition.
  __ add(a0, a0, a1);
  __ jr(ra);

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
  auto f = GeneratedCode<F2>::FromCode(*code);
  int32_t res = reinterpret_cast<int32_t>(f.Call(0xAB0, 0xC, 0, 0, 0));
  CHECK_EQ(0xABCL, res);
}

TEST(RISCV_SIMPLE1) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assm(isolate, v8::internal::CodeObjectRequired::kYes);

  // Addition.
  __ addi(a0, a0, -1);
  __ jr(ra);

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
  auto f = GeneratedCode<F1>::FromCode(*code);
  int32_t res = reinterpret_cast<int32_t>(f.Call(100, 0, 0, 0, 0));
  CHECK_EQ(99L, res);
}

// Loop 100 times, adding loop counter to result
TEST(RISCV_SIMPLE2) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assm(isolate, v8::internal::CodeObjectRequired::kYes);
  Label L, C;
  // input a0, result a1
  __ mv(a1, a0);
  __ RV_li(a0, 0);
  __ j(&C);

  __ bind(&L);

  __ add(a0, a0, a1);
  __ addi(a1, a1, -1);

  __ bind(&C);
  __ bgtz(a1, &L);
  __ jr(ra);

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
#ifdef DEBUG
  code->Print();
#endif
  auto f = GeneratedCode<F1>::FromCode(*code);
  int32_t res = reinterpret_cast<int32_t>(f.Call(100, 0, 0, 0, 0));
  CHECK_EQ(5050, res);
}

// Test part of Load and Store
TEST(RISCV_SIMPLE3) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assm(isolate, v8::internal::CodeObjectRequired::kYes);

  __ sb(a0, sp, -4);
  __ lb(a0, sp, -4);
  __ jr(ra);

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
  auto f = GeneratedCode<F1>::FromCode(*code);
  int32_t res = reinterpret_cast<int32_t>(f.Call(255, 0, 0, 0, 0));
  CHECK_EQ(-1, res);
}

// Test loading immediates of various sizes
TEST(LI) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assm(isolate, v8::internal::CodeObjectRequired::kYes);
  Label error;

  // Load 0
  __ RV_li(a0, 0l);
  __ bnez(a0, &error);

  // Load small number (<12 bits)
  __ RV_li(a1, 5);
  __ RV_li(a2, -5);
  __ add(a0, a1, a2);
  __ bnez(a0, &error);

  // Load medium number (13-32 bits)
  __ RV_li(a1, 124076833);
  __ RV_li(a2, -124076833);
  __ add(a0, a1, a2);
  __ bnez(a0, &error);

  __ mv(a0, zero_reg);
  __ jr(ra);

  __ bind(&error);
  __ jr(ra);

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
  auto f = GeneratedCode<F1>::FromCode(*code);
  int32_t res = reinterpret_cast<int32_t>(f.Call(0xDEADBEEF, 0, 0, 0, 0));
  CHECK_EQ(0L, res);
}

TEST(LI_CONST) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

  MacroAssembler assm(isolate, v8::internal::CodeObjectRequired::kYes);
  Label error;

  // Load 0
  __ li_constant(a0, 0l);
  __ bnez(a0, &error);

  // Load small number (<12 bits)
  __ li_constant(a1, 5);
  __ li_constant(a2, -5);
  __ add(a0, a1, a2);
  __ bnez(a0, &error);

  // Load medium number (13-32 bits)
  __ li_constant(a1, 124076833);
  __ li_constant(a2, -124076833);
  __ add(a0, a1, a2);
  __ bnez(a0, &error);

  __ mv(a0, zero_reg);
  __ jr(ra);

  __ bind(&error);
  __ jr(ra);

  CodeDesc desc;
  assm.GetCode(isolate, &desc);
  Handle<Code> code =
      Factory::CodeBuilder(isolate, desc, CodeKind::FOR_TESTING).Build();
  auto f = GeneratedCode<F1>::FromCode(*code);
  int32_t res = reinterpret_cast<int32_t>(f.Call(0xDEADBEEF, 0, 0, 0, 0));
  CHECK_EQ(0L, res);
}

#undef __

}  // namespace internal
}  // namespace v8
