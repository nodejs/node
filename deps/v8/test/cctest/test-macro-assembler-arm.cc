// Copyright 2013 the V8 project authors. All rights reserved.
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

#include "src/codegen/assembler-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/execution/simulator.h"
#include "src/init/v8.h"
#include "src/objects/objects-inl.h"
#include "src/utils/ostreams.h"
#include "test/cctest/cctest.h"
#include "test/common/assembler-tester.h"

namespace v8 {
namespace internal {
namespace test_macro_assembler_arm {

using F = void*(int x, int y, int p2, int p3, int p4);

#define __ masm->

using F3 = void*(void* p0, int p1, int p2, int p3, int p4);
using F5 = int(void*, void*, void*, void*, void*);

TEST(ExtractLane) {
  if (!CpuFeatures::IsSupported(NEON)) return;

  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);

  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler assembler(isolate, v8::internal::CodeObjectRequired::kYes,
                           buffer->CreateView());
  MacroAssembler* masm = &assembler;  // Create a pointer for the __ macro.

  struct T {
    int32_t i32x4_low[4];
    int32_t i32x4_high[4];
    int32_t i16x8_low[8];
    int32_t i16x8_high[8];
    int32_t i8x16_low[16];
    int32_t i8x16_high[16];
    int32_t f32x4_low[4];
    int32_t f32x4_high[4];
    int32_t i8x16_low_d[16];
    int32_t i8x16_high_d[16];
  };
  T t;

  __ stm(db_w, sp, r4.bit() | r5.bit() | lr.bit());

  for (int i = 0; i < 4; i++) {
    __ mov(r4, Operand(i));
    __ vdup(Neon32, q1, r4);
    __ ExtractLane(r5, q1, NeonS32, i);
    __ str(r5, MemOperand(r0, offsetof(T, i32x4_low) + 4 * i));
    SwVfpRegister si = SwVfpRegister::from_code(i);
    __ ExtractLane(si, q1, i);
    __ vstr(si, r0, offsetof(T, f32x4_low) + 4 * i);
  }

  for (int i = 0; i < 8; i++) {
    __ mov(r4, Operand(i));
    __ vdup(Neon16, q1, r4);
    __ ExtractLane(r5, q1, NeonS16, i);
    __ str(r5, MemOperand(r0, offsetof(T, i16x8_low) + 4 * i));
  }

  for (int i = 0; i < 16; i++) {
    __ mov(r4, Operand(i));
    __ vdup(Neon8, q1, r4);
    __ ExtractLane(r5, q1, NeonS8, i);
    __ str(r5, MemOperand(r0, offsetof(T, i8x16_low) + 4 * i));
  }

  for (int i = 0; i < 8; i++) {
    __ mov(r4, Operand(i));
    __ vdup(Neon8, q1, r4);  // q1 = d2,d3
    __ ExtractLane(r5, d2, NeonS8, i);
    __ str(r5, MemOperand(r0, offsetof(T, i8x16_low_d) + 4 * i));
    __ ExtractLane(r5, d3, NeonS8, i);
    __ str(r5, MemOperand(r0, offsetof(T, i8x16_low_d) + 4 * (i + 8)));
  }

  if (CpuFeatures::IsSupported(VFP32DREGS)) {
    for (int i = 0; i < 4; i++) {
      __ mov(r4, Operand(-i));
      __ vdup(Neon32, q15, r4);
      __ ExtractLane(r5, q15, NeonS32, i);
      __ str(r5, MemOperand(r0, offsetof(T, i32x4_high) + 4 * i));
      SwVfpRegister si = SwVfpRegister::from_code(i);
      __ ExtractLane(si, q15, i);
      __ vstr(si, r0, offsetof(T, f32x4_high) + 4 * i);
    }

    for (int i = 0; i < 8; i++) {
      __ mov(r4, Operand(-i));
      __ vdup(Neon16, q15, r4);
      __ ExtractLane(r5, q15, NeonS16, i);
      __ str(r5, MemOperand(r0, offsetof(T, i16x8_high) + 4 * i));
    }

    for (int i = 0; i < 16; i++) {
      __ mov(r4, Operand(-i));
      __ vdup(Neon8, q15, r4);
      __ ExtractLane(r5, q15, NeonS8, i);
      __ str(r5, MemOperand(r0, offsetof(T, i8x16_high) + 4 * i));
    }

    for (int i = 0; i < 8; i++) {
      __ mov(r4, Operand(-i));
      __ vdup(Neon8, q15, r4);  // q1 = d30,d31
      __ ExtractLane(r5, d30, NeonS8, i);
      __ str(r5, MemOperand(r0, offsetof(T, i8x16_high_d) + 4 * i));
      __ ExtractLane(r5, d31, NeonS8, i);
      __ str(r5, MemOperand(r0, offsetof(T, i8x16_high_d) + 4 * (i + 8)));
    }
  }

  __ ldm(ia_w, sp, r4.bit() | r5.bit() | pc.bit());

  CodeDesc desc;
  masm->GetCode(isolate, &desc);
  Handle<Code> code = Factory::CodeBuilder(isolate, desc, Code::STUB).Build();
#ifdef DEBUG
  StdoutStream os;
  code->Print(os);
#endif
  auto f = GeneratedCode<F3>::FromCode(*code);
  f.Call(&t, 0, 0, 0, 0);
  for (int i = 0; i < 4; i++) {
    CHECK_EQ(i, t.i32x4_low[i]);
    CHECK_EQ(i, t.f32x4_low[i]);
  }
  for (int i = 0; i < 8; i++) {
    CHECK_EQ(i, t.i16x8_low[i]);
  }
  for (int i = 0; i < 16; i++) {
    CHECK_EQ(i, t.i8x16_low[i]);
  }
  for (int i = 0; i < 8; i++) {
    CHECK_EQ(i, t.i8x16_low_d[i]);
    CHECK_EQ(i, t.i8x16_low_d[i + 8]);
  }
  if (CpuFeatures::IsSupported(VFP32DREGS)) {
    for (int i = 0; i < 4; i++) {
      CHECK_EQ(-i, t.i32x4_high[i]);
      CHECK_EQ(-i, t.f32x4_high[i]);
    }
    for (int i = 0; i < 8; i++) {
      CHECK_EQ(-i, t.i16x8_high[i]);
    }
    for (int i = 0; i < 16; i++) {
      CHECK_EQ(-i, t.i8x16_high[i]);
    }
    for (int i = 0; i < 8; i++) {
      CHECK_EQ(-i, t.i8x16_high_d[i]);
      CHECK_EQ(-i, t.i8x16_high_d[i + 8]);
    }
  }
}

TEST(ReplaceLane) {
  if (!CpuFeatures::IsSupported(NEON)) return;

  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);

  auto buffer = AllocateAssemblerBuffer();
  MacroAssembler assembler(isolate, v8::internal::CodeObjectRequired::kYes,
                           buffer->CreateView());
  MacroAssembler* masm = &assembler;  // Create a pointer for the __ macro.

  struct T {
    int32_t i32x4_low[4];
    int32_t i32x4_high[4];
    int16_t i16x8_low[8];
    int16_t i16x8_high[8];
    int8_t i8x16_low[16];
    int8_t i8x16_high[16];
    int32_t f32x4_low[4];
    int32_t f32x4_high[4];
  };
  T t;

  __ stm(db_w, sp, r4.bit() | r5.bit() | r6.bit() | r7.bit() | lr.bit());

  __ veor(q0, q0, q0);  // Zero
  __ veor(q1, q1, q1);  // Zero
  for (int i = 0; i < 4; i++) {
    __ mov(r4, Operand(i));
    __ ReplaceLane(q0, q0, r4, NeonS32, i);
    SwVfpRegister si = SwVfpRegister::from_code(i);
    __ vmov(si, r4);
    __ ReplaceLane(q1, q1, si, i);
  }
  __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(T, i32x4_low))));
  __ vst1(Neon8, NeonListOperand(q0), NeonMemOperand(r4));
  __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(T, f32x4_low))));
  __ vst1(Neon8, NeonListOperand(q1), NeonMemOperand(r4));

  __ veor(q0, q0, q0);  // Zero
  for (int i = 0; i < 8; i++) {
    __ mov(r4, Operand(i));
    __ ReplaceLane(q0, q0, r4, NeonS16, i);
  }
  __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(T, i16x8_low))));
  __ vst1(Neon8, NeonListOperand(q0), NeonMemOperand(r4));

  __ veor(q0, q0, q0);  // Zero
  for (int i = 0; i < 16; i++) {
    __ mov(r4, Operand(i));
    __ ReplaceLane(q0, q0, r4, NeonS8, i);
  }
  __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(T, i8x16_low))));
  __ vst1(Neon8, NeonListOperand(q0), NeonMemOperand(r4));

  if (CpuFeatures::IsSupported(VFP32DREGS)) {
    __ veor(q14, q14, q14);  // Zero
    __ veor(q15, q15, q15);  // Zero
    for (int i = 0; i < 4; i++) {
      __ mov(r4, Operand(-i));
      __ ReplaceLane(q14, q14, r4, NeonS32, i);
      SwVfpRegister si = SwVfpRegister::from_code(i);
      __ vmov(si, r4);
      __ ReplaceLane(q15, q15, si, i);
    }
    __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(T, i32x4_high))));
    __ vst1(Neon8, NeonListOperand(q14), NeonMemOperand(r4));
    __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(T, f32x4_high))));
    __ vst1(Neon8, NeonListOperand(q15), NeonMemOperand(r4));

    __ veor(q14, q14, q14);  // Zero
    for (int i = 0; i < 8; i++) {
      __ mov(r4, Operand(-i));
      __ ReplaceLane(q14, q14, r4, NeonS16, i);
    }
    __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(T, i16x8_high))));
    __ vst1(Neon8, NeonListOperand(q14), NeonMemOperand(r4));

    __ veor(q14, q14, q14);  // Zero
    for (int i = 0; i < 16; i++) {
      __ mov(r4, Operand(-i));
      __ ReplaceLane(q14, q14, r4, NeonS8, i);
    }
    __ add(r4, r0, Operand(static_cast<int32_t>(offsetof(T, i8x16_high))));
    __ vst1(Neon8, NeonListOperand(q14), NeonMemOperand(r4));
  }

  __ ldm(ia_w, sp, r4.bit() | r5.bit() | r6.bit() | r7.bit() | pc.bit());

  CodeDesc desc;
  masm->GetCode(isolate, &desc);
  Handle<Code> code = Factory::CodeBuilder(isolate, desc, Code::STUB).Build();
#ifdef DEBUG
  StdoutStream os;
  code->Print(os);
#endif
  auto f = GeneratedCode<F3>::FromCode(*code);
  f.Call(&t, 0, 0, 0, 0);
  for (int i = 0; i < 4; i++) {
    CHECK_EQ(i, t.i32x4_low[i]);
    CHECK_EQ(i, t.f32x4_low[i]);
  }
  for (int i = 0; i < 8; i++) {
    CHECK_EQ(i, t.i16x8_low[i]);
  }
  for (int i = 0; i < 16; i++) {
    CHECK_EQ(i, t.i8x16_low[i]);
  }
  if (CpuFeatures::IsSupported(VFP32DREGS)) {
    for (int i = 0; i < 4; i++) {
      CHECK_EQ(-i, t.i32x4_high[i]);
      CHECK_EQ(-i, t.f32x4_high[i]);
    }
    for (int i = 0; i < 8; i++) {
      CHECK_EQ(-i, t.i16x8_high[i]);
    }
    for (int i = 0; i < 16; i++) {
      CHECK_EQ(-i, t.i8x16_high[i]);
    }
  }
}

#undef __

}  // namespace test_macro_assembler_arm
}  // namespace internal
}  // namespace v8
