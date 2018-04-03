// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_ARM

#include <memory>

#include "src/arm/assembler-arm-inl.h"
#include "src/arm/simulator-arm.h"
#include "src/codegen.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {

#define __ masm.

#if defined(V8_HOST_ARCH_ARM)

MemCopyUint8Function CreateMemCopyUint8Function(Isolate* isolate,
                                                MemCopyUint8Function stub) {
#if defined(USE_SIMULATOR)
  return stub;
#else
  size_t allocated = 0;
  byte* buffer = AllocatePage(isolate->heap()->GetRandomMmapAddr(), &allocated);
  if (buffer == nullptr) return stub;

  MacroAssembler masm(isolate, buffer, static_cast<int>(allocated),
                      CodeObjectRequired::kNo);

  Register dest = r0;
  Register src = r1;
  Register chars = r2;
  Register temp1 = r3;
  Label less_4;

  if (CpuFeatures::IsSupported(NEON)) {
    CpuFeatureScope scope(&masm, NEON);
    Label loop, less_256, less_128, less_64, less_32, _16_or_less, _8_or_less;
    Label size_less_than_8;
    __ pld(MemOperand(src, 0));

    __ cmp(chars, Operand(8));
    __ b(lt, &size_less_than_8);
    __ cmp(chars, Operand(32));
    __ b(lt, &less_32);
    if (CpuFeatures::dcache_line_size() == 32) {
      __ pld(MemOperand(src, 32));
    }
    __ cmp(chars, Operand(64));
    __ b(lt, &less_64);
    __ pld(MemOperand(src, 64));
    if (CpuFeatures::dcache_line_size() == 32) {
      __ pld(MemOperand(src, 96));
    }
    __ cmp(chars, Operand(128));
    __ b(lt, &less_128);
    __ pld(MemOperand(src, 128));
    if (CpuFeatures::dcache_line_size() == 32) {
      __ pld(MemOperand(src, 160));
    }
    __ pld(MemOperand(src, 192));
    if (CpuFeatures::dcache_line_size() == 32) {
      __ pld(MemOperand(src, 224));
    }
    __ cmp(chars, Operand(256));
    __ b(lt, &less_256);
    __ sub(chars, chars, Operand(256));

    __ bind(&loop);
    __ pld(MemOperand(src, 256));
    __ vld1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(src, PostIndex));
    if (CpuFeatures::dcache_line_size() == 32) {
      __ pld(MemOperand(src, 256));
    }
    __ vld1(Neon8, NeonListOperand(d4, 4), NeonMemOperand(src, PostIndex));
    __ sub(chars, chars, Operand(64), SetCC);
    __ vst1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(dest, PostIndex));
    __ vst1(Neon8, NeonListOperand(d4, 4), NeonMemOperand(dest, PostIndex));
    __ b(ge, &loop);
    __ add(chars, chars, Operand(256));

    __ bind(&less_256);
    __ vld1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(src, PostIndex));
    __ vld1(Neon8, NeonListOperand(d4, 4), NeonMemOperand(src, PostIndex));
    __ sub(chars, chars, Operand(128));
    __ vst1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(dest, PostIndex));
    __ vst1(Neon8, NeonListOperand(d4, 4), NeonMemOperand(dest, PostIndex));
    __ vld1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(src, PostIndex));
    __ vld1(Neon8, NeonListOperand(d4, 4), NeonMemOperand(src, PostIndex));
    __ vst1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(dest, PostIndex));
    __ vst1(Neon8, NeonListOperand(d4, 4), NeonMemOperand(dest, PostIndex));
    __ cmp(chars, Operand(64));
    __ b(lt, &less_64);

    __ bind(&less_128);
    __ vld1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(src, PostIndex));
    __ vld1(Neon8, NeonListOperand(d4, 4), NeonMemOperand(src, PostIndex));
    __ sub(chars, chars, Operand(64));
    __ vst1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(dest, PostIndex));
    __ vst1(Neon8, NeonListOperand(d4, 4), NeonMemOperand(dest, PostIndex));

    __ bind(&less_64);
    __ cmp(chars, Operand(32));
    __ b(lt, &less_32);
    __ vld1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(src, PostIndex));
    __ vst1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(dest, PostIndex));
    __ sub(chars, chars, Operand(32));

    __ bind(&less_32);
    __ cmp(chars, Operand(16));
    __ b(le, &_16_or_less);
    __ vld1(Neon8, NeonListOperand(d0, 2), NeonMemOperand(src, PostIndex));
    __ vst1(Neon8, NeonListOperand(d0, 2), NeonMemOperand(dest, PostIndex));
    __ sub(chars, chars, Operand(16));

    __ bind(&_16_or_less);
    __ cmp(chars, Operand(8));
    __ b(le, &_8_or_less);
    __ vld1(Neon8, NeonListOperand(d0), NeonMemOperand(src, PostIndex));
    __ vst1(Neon8, NeonListOperand(d0), NeonMemOperand(dest, PostIndex));
    __ sub(chars, chars, Operand(8));

    // Do a last copy which may overlap with the previous copy (up to 8 bytes).
    __ bind(&_8_or_less);
    __ rsb(chars, chars, Operand(8));
    __ sub(src, src, Operand(chars));
    __ sub(dest, dest, Operand(chars));
    __ vld1(Neon8, NeonListOperand(d0), NeonMemOperand(src));
    __ vst1(Neon8, NeonListOperand(d0), NeonMemOperand(dest));

    __ Ret();

    __ bind(&size_less_than_8);

    __ bic(temp1, chars, Operand(0x3), SetCC);
    __ b(&less_4, eq);
    __ ldr(temp1, MemOperand(src, 4, PostIndex));
    __ str(temp1, MemOperand(dest, 4, PostIndex));
  } else {
    UseScratchRegisterScope temps(&masm);
    Register temp2 = temps.Acquire();
    Label loop;

    __ bic(temp2, chars, Operand(0x3), SetCC);
    __ b(&less_4, eq);
    __ add(temp2, dest, temp2);

    __ bind(&loop);
    __ ldr(temp1, MemOperand(src, 4, PostIndex));
    __ str(temp1, MemOperand(dest, 4, PostIndex));
    __ cmp(dest, temp2);
    __ b(&loop, ne);
  }

  __ bind(&less_4);
  __ mov(chars, Operand(chars, LSL, 31), SetCC);
  // bit0 => Z (ne), bit1 => C (cs)
  __ ldrh(temp1, MemOperand(src, 2, PostIndex), cs);
  __ strh(temp1, MemOperand(dest, 2, PostIndex), cs);
  __ ldrb(temp1, MemOperand(src), ne);
  __ strb(temp1, MemOperand(dest), ne);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  DCHECK(!RelocInfo::RequiresRelocation(isolate, desc));

  Assembler::FlushICache(isolate, buffer, allocated);
  CHECK(SetPermissions(buffer, allocated, PageAllocator::kReadExecute));
  return FUNCTION_CAST<MemCopyUint8Function>(buffer);
#endif
}


// Convert 8 to 16. The number of character to copy must be at least 8.
MemCopyUint16Uint8Function CreateMemCopyUint16Uint8Function(
    Isolate* isolate, MemCopyUint16Uint8Function stub) {
#if defined(USE_SIMULATOR)
  return stub;
#else
  size_t allocated = 0;
  byte* buffer = AllocatePage(isolate->heap()->GetRandomMmapAddr(), &allocated);
  if (buffer == nullptr) return stub;

  MacroAssembler masm(isolate, buffer, static_cast<int>(allocated),
                      CodeObjectRequired::kNo);

  Register dest = r0;
  Register src = r1;
  Register chars = r2;
  if (CpuFeatures::IsSupported(NEON)) {
    CpuFeatureScope scope(&masm, NEON);
    Register temp = r3;
    Label loop;

    __ bic(temp, chars, Operand(0x7));
    __ sub(chars, chars, Operand(temp));
    __ add(temp, dest, Operand(temp, LSL, 1));

    __ bind(&loop);
    __ vld1(Neon8, NeonListOperand(d0), NeonMemOperand(src, PostIndex));
    __ vmovl(NeonU8, q0, d0);
    __ vst1(Neon16, NeonListOperand(d0, 2), NeonMemOperand(dest, PostIndex));
    __ cmp(dest, temp);
    __ b(&loop, ne);

    // Do a last copy which will overlap with the previous copy (1 to 8 bytes).
    __ rsb(chars, chars, Operand(8));
    __ sub(src, src, Operand(chars));
    __ sub(dest, dest, Operand(chars, LSL, 1));
    __ vld1(Neon8, NeonListOperand(d0), NeonMemOperand(src));
    __ vmovl(NeonU8, q0, d0);
    __ vst1(Neon16, NeonListOperand(d0, 2), NeonMemOperand(dest));
    __ Ret();
  } else {
    UseScratchRegisterScope temps(&masm);

    Register temp1 = r3;
    Register temp2 = temps.Acquire();
    Register temp3 = lr;
    Register temp4 = r4;
    Label loop;
    Label not_two;

    __ Push(lr, r4);
    __ bic(temp2, chars, Operand(0x3));
    __ add(temp2, dest, Operand(temp2, LSL, 1));

    __ bind(&loop);
    __ ldr(temp1, MemOperand(src, 4, PostIndex));
    __ uxtb16(temp3, temp1);
    __ uxtb16(temp4, temp1, 8);
    __ pkhbt(temp1, temp3, Operand(temp4, LSL, 16));
    __ str(temp1, MemOperand(dest));
    __ pkhtb(temp1, temp4, Operand(temp3, ASR, 16));
    __ str(temp1, MemOperand(dest, 4));
    __ add(dest, dest, Operand(8));
    __ cmp(dest, temp2);
    __ b(&loop, ne);

    __ mov(chars, Operand(chars, LSL, 31), SetCC);  // bit0 => ne, bit1 => cs
    __ b(&not_two, cc);
    __ ldrh(temp1, MemOperand(src, 2, PostIndex));
    __ uxtb(temp3, temp1, 8);
    __ mov(temp3, Operand(temp3, LSL, 16));
    __ uxtab(temp3, temp3, temp1);
    __ str(temp3, MemOperand(dest, 4, PostIndex));
    __ bind(&not_two);
    __ ldrb(temp1, MemOperand(src), ne);
    __ strh(temp1, MemOperand(dest), ne);
    __ Pop(pc, r4);
  }

  CodeDesc desc;
  masm.GetCode(isolate, &desc);

  Assembler::FlushICache(isolate, buffer, allocated);
  CHECK(SetPermissions(buffer, allocated, PageAllocator::kReadExecute));
  return FUNCTION_CAST<MemCopyUint16Uint8Function>(buffer);
#endif
}
#endif

UnaryMathFunctionWithIsolate CreateSqrtFunction(Isolate* isolate) {
#if defined(USE_SIMULATOR)
  return nullptr;
#else
  size_t allocated = 0;
  byte* buffer = AllocatePage(isolate->heap()->GetRandomMmapAddr(), &allocated);
  if (buffer == nullptr) return nullptr;

  MacroAssembler masm(isolate, buffer, static_cast<int>(allocated),
                      CodeObjectRequired::kNo);

  __ MovFromFloatParameter(d0);
  __ vsqrt(d0, d0);
  __ MovToFloatResult(d0);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  DCHECK(!RelocInfo::RequiresRelocation(isolate, desc));

  Assembler::FlushICache(isolate, buffer, allocated);
  CHECK(SetPermissions(buffer, allocated, PageAllocator::kReadExecute));
  return FUNCTION_CAST<UnaryMathFunctionWithIsolate>(buffer);
#endif
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM
