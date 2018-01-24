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

#include "src/arm/simulator-arm.h"
#include "src/assembler-inl.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/v8.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {
namespace test_macro_assembler_arm {

typedef void* (*F)(int x, int y, int p2, int p3, int p4);

#define __ masm->

typedef Object* (*F3)(void* p0, int p1, int p2, int p3, int p4);
typedef int (*F5)(void*, void*, void*, void*, void*);

TEST(LoadAndStoreWithRepresentation) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);

  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  MacroAssembler assembler(isolate, buffer, static_cast<int>(allocated),
                           v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assembler;  // Create a pointer for the __ macro.

  __ sub(sp, sp, Operand(1 * kPointerSize));
  Label exit;

  // Test 1.
  __ mov(r0, Operand(1));  // Test number.
  __ mov(r1, Operand(0));
  __ str(r1, MemOperand(sp, 0 * kPointerSize));
  __ mov(r2, Operand(-1));
  __ Store(r2, MemOperand(sp, 0 * kPointerSize), Representation::UInteger8());
  __ ldr(r3, MemOperand(sp, 0 * kPointerSize));
  __ mov(r2, Operand(255));
  __ cmp(r3, r2);
  __ b(ne, &exit);
  __ mov(r2, Operand(255));
  __ Load(r3, MemOperand(sp, 0 * kPointerSize), Representation::UInteger8());
  __ cmp(r3, r2);
  __ b(ne, &exit);

  // Test 2.
  __ mov(r0, Operand(2));  // Test number.
  __ mov(r1, Operand(0));
  __ str(r1, MemOperand(sp, 0 * kPointerSize));
  __ mov(r2, Operand(-1));
  __ Store(r2, MemOperand(sp, 0 * kPointerSize), Representation::Integer8());
  __ ldr(r3, MemOperand(sp, 0 * kPointerSize));
  __ mov(r2, Operand(255));
  __ cmp(r3, r2);
  __ b(ne, &exit);
  __ mov(r2, Operand(-1));
  __ Load(r3, MemOperand(sp, 0 * kPointerSize), Representation::Integer8());
  __ cmp(r3, r2);
  __ b(ne, &exit);

  // Test 3.
  __ mov(r0, Operand(3));  // Test number.
  __ mov(r1, Operand(0));
  __ str(r1, MemOperand(sp, 0 * kPointerSize));
  __ mov(r2, Operand(-1));
  __ Store(r2, MemOperand(sp, 0 * kPointerSize), Representation::UInteger16());
  __ ldr(r3, MemOperand(sp, 0 * kPointerSize));
  __ mov(r2, Operand(65535));
  __ cmp(r3, r2);
  __ b(ne, &exit);
  __ mov(r2, Operand(65535));
  __ Load(r3, MemOperand(sp, 0 * kPointerSize), Representation::UInteger16());
  __ cmp(r3, r2);
  __ b(ne, &exit);

  // Test 4.
  __ mov(r0, Operand(4));  // Test number.
  __ mov(r1, Operand(0));
  __ str(r1, MemOperand(sp, 0 * kPointerSize));
  __ mov(r2, Operand(-1));
  __ Store(r2, MemOperand(sp, 0 * kPointerSize), Representation::Integer16());
  __ ldr(r3, MemOperand(sp, 0 * kPointerSize));
  __ mov(r2, Operand(65535));
  __ cmp(r3, r2);
  __ b(ne, &exit);
  __ mov(r2, Operand(-1));
  __ Load(r3, MemOperand(sp, 0 * kPointerSize), Representation::Integer16());
  __ cmp(r3, r2);
  __ b(ne, &exit);

  __ mov(r0, Operand(0));  // Success.
  __ bind(&exit);
  __ add(sp, sp, Operand(1 * kPointerSize));
  __ bx(lr);

  CodeDesc desc;
  masm->GetCode(isolate, &desc);
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());

  // Call the function from C++.
  F5 f = FUNCTION_CAST<F5>(code->entry());
  CHECK(!CALL_GENERATED_CODE(isolate, f, 0, 0, 0, 0, 0));
}

TEST(ExtractLane) {
  if (!CpuFeatures::IsSupported(NEON)) return;

  Isolate* isolate = CcTest::i_isolate();
  HandleScope handles(isolate);

  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  MacroAssembler assembler(isolate, buffer, static_cast<int>(allocated),
                           v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assembler;  // Create a pointer for the __ macro.

  typedef struct {
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
  } T;
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
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  F3 f = FUNCTION_CAST<F3>(code->entry());
  Object* dummy = CALL_GENERATED_CODE(isolate, f, &t, 0, 0, 0, 0);
  USE(dummy);
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

  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  MacroAssembler assembler(isolate, buffer, static_cast<int>(allocated),
                           v8::internal::CodeObjectRequired::kYes);
  MacroAssembler* masm = &assembler;  // Create a pointer for the __ macro.

  typedef struct {
    int32_t i32x4_low[4];
    int32_t i32x4_high[4];
    int16_t i16x8_low[8];
    int16_t i16x8_high[8];
    int8_t i8x16_low[16];
    int8_t i8x16_high[16];
    int32_t f32x4_low[4];
    int32_t f32x4_high[4];
  } T;
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
  Handle<Code> code =
      isolate->factory()->NewCode(desc, Code::STUB, Handle<Code>());
#ifdef DEBUG
  OFStream os(stdout);
  code->Print(os);
#endif
  F3 f = FUNCTION_CAST<F3>(code->entry());
  Object* dummy = CALL_GENERATED_CODE(isolate, f, &t, 0, 0, 0, 0);
  USE(dummy);
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
