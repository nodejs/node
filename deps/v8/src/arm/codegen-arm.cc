// Copyright 2012 the V8 project authors. All rights reserved.
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

#include "v8.h"

#if V8_TARGET_ARCH_ARM

#include "codegen.h"
#include "macro-assembler.h"
#include "simulator-arm.h"

namespace v8 {
namespace internal {


UnaryMathFunction CreateTranscendentalFunction(TranscendentalCache::Type type) {
  switch (type) {
    case TranscendentalCache::SIN: return &sin;
    case TranscendentalCache::COS: return &cos;
    case TranscendentalCache::TAN: return &tan;
    case TranscendentalCache::LOG: return &log;
    default: UNIMPLEMENTED();
  }
  return NULL;
}


#define __ masm.


#if defined(USE_SIMULATOR)
byte* fast_exp_arm_machine_code = NULL;
double fast_exp_simulator(double x) {
  return Simulator::current(Isolate::Current())->CallFPReturnsDouble(
      fast_exp_arm_machine_code, x, 0);
}
#endif


UnaryMathFunction CreateExpFunction() {
  if (!FLAG_fast_math) return &exp;
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == NULL) return &exp;
  ExternalReference::InitializeMathExpData();

  MacroAssembler masm(NULL, buffer, static_cast<int>(actual_size));

  {
    DwVfpRegister input = d0;
    DwVfpRegister result = d1;
    DwVfpRegister double_scratch1 = d2;
    DwVfpRegister double_scratch2 = d3;
    Register temp1 = r4;
    Register temp2 = r5;
    Register temp3 = r6;

    if (masm.use_eabi_hardfloat()) {
      // Input value is in d0 anyway, nothing to do.
    } else {
      __ vmov(input, r0, r1);
    }
    __ Push(temp3, temp2, temp1);
    MathExpGenerator::EmitMathExp(
        &masm, input, result, double_scratch1, double_scratch2,
        temp1, temp2, temp3);
    __ Pop(temp3, temp2, temp1);
    if (masm.use_eabi_hardfloat()) {
      __ vmov(d0, result);
    } else {
      __ vmov(r0, r1, result);
    }
    __ Ret();
  }

  CodeDesc desc;
  masm.GetCode(&desc);
  ASSERT(!RelocInfo::RequiresRelocation(desc));

  CPU::FlushICache(buffer, actual_size);
  OS::ProtectCode(buffer, actual_size);

#if !defined(USE_SIMULATOR)
  return FUNCTION_CAST<UnaryMathFunction>(buffer);
#else
  fast_exp_arm_machine_code = buffer;
  return &fast_exp_simulator;
#endif
}

#if defined(V8_HOST_ARCH_ARM)
OS::MemCopyUint8Function CreateMemCopyUint8Function(
      OS::MemCopyUint8Function stub) {
#if defined(USE_SIMULATOR)
  return stub;
#else
  if (Serializer::enabled() || !CpuFeatures::IsSupported(UNALIGNED_ACCESSES)) {
    return stub;
  }
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == NULL) return stub;

  MacroAssembler masm(NULL, buffer, static_cast<int>(actual_size));

  Register dest = r0;
  Register src = r1;
  Register chars = r2;
  Register temp1 = r3;
  Label less_4;

  if (CpuFeatures::IsSupported(NEON)) {
    Label loop, less_256, less_128, less_64, less_32, _16_or_less, _8_or_less;
    Label size_less_than_8;
    __ pld(MemOperand(src, 0));

    __ cmp(chars, Operand(8));
    __ b(lt, &size_less_than_8);
    __ cmp(chars, Operand(32));
    __ b(lt, &less_32);
    if (CpuFeatures::cache_line_size() == 32) {
      __ pld(MemOperand(src, 32));
    }
    __ cmp(chars, Operand(64));
    __ b(lt, &less_64);
    __ pld(MemOperand(src, 64));
    if (CpuFeatures::cache_line_size() == 32) {
      __ pld(MemOperand(src, 96));
    }
    __ cmp(chars, Operand(128));
    __ b(lt, &less_128);
    __ pld(MemOperand(src, 128));
    if (CpuFeatures::cache_line_size() == 32) {
      __ pld(MemOperand(src, 160));
    }
    __ pld(MemOperand(src, 192));
    if (CpuFeatures::cache_line_size() == 32) {
      __ pld(MemOperand(src, 224));
    }
    __ cmp(chars, Operand(256));
    __ b(lt, &less_256);
    __ sub(chars, chars, Operand(256));

    __ bind(&loop);
    __ pld(MemOperand(src, 256));
    __ vld1(Neon8, NeonListOperand(d0, 4), NeonMemOperand(src, PostIndex));
    if (CpuFeatures::cache_line_size() == 32) {
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
    Register temp2 = ip;
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
  masm.GetCode(&desc);
  ASSERT(!RelocInfo::RequiresRelocation(desc));

  CPU::FlushICache(buffer, actual_size);
  OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<OS::MemCopyUint8Function>(buffer);
#endif
}


// Convert 8 to 16. The number of character to copy must be at least 8.
OS::MemCopyUint16Uint8Function CreateMemCopyUint16Uint8Function(
      OS::MemCopyUint16Uint8Function stub) {
#if defined(USE_SIMULATOR)
  return stub;
#else
  if (Serializer::enabled() || !CpuFeatures::IsSupported(UNALIGNED_ACCESSES)) {
    return stub;
  }
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == NULL) return stub;

  MacroAssembler masm(NULL, buffer, static_cast<int>(actual_size));

  Register dest = r0;
  Register src = r1;
  Register chars = r2;
  if (CpuFeatures::IsSupported(NEON)) {
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
    Register temp1 = r3;
    Register temp2 = ip;
    Register temp3 = lr;
    Register temp4 = r4;
    Label loop;
    Label not_two;

    __ Push(lr, r4);
    __ bic(temp2, chars, Operand(0x3));
    __ add(temp2, dest, Operand(temp2, LSL, 1));

    __ bind(&loop);
    __ ldr(temp1, MemOperand(src, 4, PostIndex));
    __ uxtb16(temp3, Operand(temp1, ROR, 0));
    __ uxtb16(temp4, Operand(temp1, ROR, 8));
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
    __ uxtb(temp3, Operand(temp1, ROR, 8));
    __ mov(temp3, Operand(temp3, LSL, 16));
    __ uxtab(temp3, temp3, Operand(temp1, ROR, 0));
    __ str(temp3, MemOperand(dest, 4, PostIndex));
    __ bind(&not_two);
    __ ldrb(temp1, MemOperand(src), ne);
    __ strh(temp1, MemOperand(dest), ne);
    __ Pop(pc, r4);
  }

  CodeDesc desc;
  masm.GetCode(&desc);

  CPU::FlushICache(buffer, actual_size);
  OS::ProtectCode(buffer, actual_size);

  return FUNCTION_CAST<OS::MemCopyUint16Uint8Function>(buffer);
#endif
}
#endif

#undef __


UnaryMathFunction CreateSqrtFunction() {
  return &sqrt;
}


// -------------------------------------------------------------------------
// Platform-specific RuntimeCallHelper functions.

void StubRuntimeCallHelper::BeforeCall(MacroAssembler* masm) const {
  masm->EnterFrame(StackFrame::INTERNAL);
  ASSERT(!masm->has_frame());
  masm->set_has_frame(true);
}


void StubRuntimeCallHelper::AfterCall(MacroAssembler* masm) const {
  masm->LeaveFrame(StackFrame::INTERNAL);
  ASSERT(masm->has_frame());
  masm->set_has_frame(false);
}


// -------------------------------------------------------------------------
// Code generators

#define __ ACCESS_MASM(masm)

void ElementsTransitionGenerator::GenerateMapChangeElementsTransition(
    MacroAssembler* masm, AllocationSiteMode mode,
    Label* allocation_memento_found) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : key
  //  -- r2    : receiver
  //  -- lr    : return address
  //  -- r3    : target map, scratch for subsequent call
  //  -- r4    : scratch (elements)
  // -----------------------------------
  if (mode == TRACK_ALLOCATION_SITE) {
    ASSERT(allocation_memento_found != NULL);
    __ JumpIfJSArrayHasAllocationMemento(r2, r4, allocation_memento_found);
  }

  // Set transitioned map.
  __ str(r3, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ RecordWriteField(r2,
                      HeapObject::kMapOffset,
                      r3,
                      r9,
                      kLRHasNotBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}


void ElementsTransitionGenerator::GenerateSmiToDouble(
    MacroAssembler* masm, AllocationSiteMode mode, Label* fail) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : key
  //  -- r2    : receiver
  //  -- lr    : return address
  //  -- r3    : target map, scratch for subsequent call
  //  -- r4    : scratch (elements)
  // -----------------------------------
  Label loop, entry, convert_hole, gc_required, only_change_map, done;

  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(r2, r4, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ ldr(r4, FieldMemOperand(r2, JSObject::kElementsOffset));
  __ CompareRoot(r4, Heap::kEmptyFixedArrayRootIndex);
  __ b(eq, &only_change_map);

  __ push(lr);
  __ ldr(r5, FieldMemOperand(r4, FixedArray::kLengthOffset));
  // r5: number of elements (smi-tagged)

  // Allocate new FixedDoubleArray.
  // Use lr as a temporary register.
  __ mov(lr, Operand(r5, LSL, 2));
  __ add(lr, lr, Operand(FixedDoubleArray::kHeaderSize));
  __ Allocate(lr, r6, r4, r9, &gc_required, DOUBLE_ALIGNMENT);
  // r6: destination FixedDoubleArray, not tagged as heap object.
  __ ldr(r4, FieldMemOperand(r2, JSObject::kElementsOffset));
  // r4: source FixedArray.

  // Set destination FixedDoubleArray's length and map.
  __ LoadRoot(r9, Heap::kFixedDoubleArrayMapRootIndex);
  __ str(r5, MemOperand(r6, FixedDoubleArray::kLengthOffset));
  // Update receiver's map.
  __ str(r9, MemOperand(r6, HeapObject::kMapOffset));

  __ str(r3, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ RecordWriteField(r2,
                      HeapObject::kMapOffset,
                      r3,
                      r9,
                      kLRHasBeenSaved,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  // Replace receiver's backing store with newly created FixedDoubleArray.
  __ add(r3, r6, Operand(kHeapObjectTag));
  __ str(r3, FieldMemOperand(r2, JSObject::kElementsOffset));
  __ RecordWriteField(r2,
                      JSObject::kElementsOffset,
                      r3,
                      r9,
                      kLRHasBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);

  // Prepare for conversion loop.
  __ add(r3, r4, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ add(r9, r6, Operand(FixedDoubleArray::kHeaderSize));
  __ add(r6, r9, Operand(r5, LSL, 2));
  __ mov(r4, Operand(kHoleNanLower32));
  __ mov(r5, Operand(kHoleNanUpper32));
  // r3: begin of source FixedArray element fields, not tagged
  // r4: kHoleNanLower32
  // r5: kHoleNanUpper32
  // r6: end of destination FixedDoubleArray, not tagged
  // r9: begin of FixedDoubleArray element fields, not tagged

  __ b(&entry);

  __ bind(&only_change_map);
  __ str(r3, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ RecordWriteField(r2,
                      HeapObject::kMapOffset,
                      r3,
                      r9,
                      kLRHasNotBeenSaved,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ b(&done);

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  __ pop(lr);
  __ b(fail);

  // Convert and copy elements.
  __ bind(&loop);
  __ ldr(lr, MemOperand(r3, 4, PostIndex));
  // lr: current element
  __ UntagAndJumpIfNotSmi(lr, lr, &convert_hole);

  // Normal smi, convert to double and store.
  __ vmov(s0, lr);
  __ vcvt_f64_s32(d0, s0);
  __ vstr(d0, r9, 0);
  __ add(r9, r9, Operand(8));
  __ b(&entry);

  // Hole found, store the-hole NaN.
  __ bind(&convert_hole);
  if (FLAG_debug_code) {
    // Restore a "smi-untagged" heap object.
    __ SmiTag(lr);
    __ orr(lr, lr, Operand(1));
    __ CompareRoot(lr, Heap::kTheHoleValueRootIndex);
    __ Assert(eq, kObjectFoundInSmiOnlyArray);
  }
  __ Strd(r4, r5, MemOperand(r9, 8, PostIndex));

  __ bind(&entry);
  __ cmp(r9, r6);
  __ b(lt, &loop);

  __ pop(lr);
  __ bind(&done);
}


void ElementsTransitionGenerator::GenerateDoubleToObject(
    MacroAssembler* masm, AllocationSiteMode mode, Label* fail) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : key
  //  -- r2    : receiver
  //  -- lr    : return address
  //  -- r3    : target map, scratch for subsequent call
  //  -- r4    : scratch (elements)
  // -----------------------------------
  Label entry, loop, convert_hole, gc_required, only_change_map;

  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(r2, r4, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ ldr(r4, FieldMemOperand(r2, JSObject::kElementsOffset));
  __ CompareRoot(r4, Heap::kEmptyFixedArrayRootIndex);
  __ b(eq, &only_change_map);

  __ push(lr);
  __ Push(r3, r2, r1, r0);
  __ ldr(r5, FieldMemOperand(r4, FixedArray::kLengthOffset));
  // r4: source FixedDoubleArray
  // r5: number of elements (smi-tagged)

  // Allocate new FixedArray.
  __ mov(r0, Operand(FixedDoubleArray::kHeaderSize));
  __ add(r0, r0, Operand(r5, LSL, 1));
  __ Allocate(r0, r6, r3, r9, &gc_required, NO_ALLOCATION_FLAGS);
  // r6: destination FixedArray, not tagged as heap object
  // Set destination FixedDoubleArray's length and map.
  __ LoadRoot(r9, Heap::kFixedArrayMapRootIndex);
  __ str(r5, MemOperand(r6, FixedDoubleArray::kLengthOffset));
  __ str(r9, MemOperand(r6, HeapObject::kMapOffset));

  // Prepare for conversion loop.
  __ add(r4, r4, Operand(FixedDoubleArray::kHeaderSize - kHeapObjectTag + 4));
  __ add(r3, r6, Operand(FixedArray::kHeaderSize));
  __ add(r6, r6, Operand(kHeapObjectTag));
  __ add(r5, r3, Operand(r5, LSL, 1));
  __ LoadRoot(r9, Heap::kHeapNumberMapRootIndex);
  // Using offsetted addresses in r4 to fully take advantage of post-indexing.
  // r3: begin of destination FixedArray element fields, not tagged
  // r4: begin of source FixedDoubleArray element fields, not tagged, +4
  // r5: end of destination FixedArray, not tagged
  // r6: destination FixedArray
  // r9: heap number map
  __ b(&entry);

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  __ Pop(r3, r2, r1, r0);
  __ pop(lr);
  __ b(fail);

  __ bind(&loop);
  __ ldr(r1, MemOperand(r4, 8, PostIndex));
  // r1: current element's upper 32 bit
  // r4: address of next element's upper 32 bit
  __ cmp(r1, Operand(kHoleNanUpper32));
  __ b(eq, &convert_hole);

  // Non-hole double, copy value into a heap number.
  __ AllocateHeapNumber(r2, r0, lr, r9, &gc_required);
  // r2: new heap number
  __ ldr(r0, MemOperand(r4, 12, NegOffset));
  __ Strd(r0, r1, FieldMemOperand(r2, HeapNumber::kValueOffset));
  __ mov(r0, r3);
  __ str(r2, MemOperand(r3, 4, PostIndex));
  __ RecordWrite(r6,
                 r0,
                 r2,
                 kLRHasBeenSaved,
                 kDontSaveFPRegs,
                 EMIT_REMEMBERED_SET,
                 OMIT_SMI_CHECK);
  __ b(&entry);

  // Replace the-hole NaN with the-hole pointer.
  __ bind(&convert_hole);
  __ LoadRoot(r0, Heap::kTheHoleValueRootIndex);
  __ str(r0, MemOperand(r3, 4, PostIndex));

  __ bind(&entry);
  __ cmp(r3, r5);
  __ b(lt, &loop);

  __ Pop(r3, r2, r1, r0);
  // Replace receiver's backing store with newly created and filled FixedArray.
  __ str(r6, FieldMemOperand(r2, JSObject::kElementsOffset));
  __ RecordWriteField(r2,
                      JSObject::kElementsOffset,
                      r6,
                      r9,
                      kLRHasBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ pop(lr);

  __ bind(&only_change_map);
  // Update receiver's map.
  __ str(r3, FieldMemOperand(r2, HeapObject::kMapOffset));
  __ RecordWriteField(r2,
                      HeapObject::kMapOffset,
                      r3,
                      r9,
                      kLRHasNotBeenSaved,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}


void StringCharLoadGenerator::Generate(MacroAssembler* masm,
                                       Register string,
                                       Register index,
                                       Register result,
                                       Label* call_runtime) {
  // Fetch the instance type of the receiver into result register.
  __ ldr(result, FieldMemOperand(string, HeapObject::kMapOffset));
  __ ldrb(result, FieldMemOperand(result, Map::kInstanceTypeOffset));

  // We need special handling for indirect strings.
  Label check_sequential;
  __ tst(result, Operand(kIsIndirectStringMask));
  __ b(eq, &check_sequential);

  // Dispatch on the indirect string shape: slice or cons.
  Label cons_string;
  __ tst(result, Operand(kSlicedNotConsMask));
  __ b(eq, &cons_string);

  // Handle slices.
  Label indirect_string_loaded;
  __ ldr(result, FieldMemOperand(string, SlicedString::kOffsetOffset));
  __ ldr(string, FieldMemOperand(string, SlicedString::kParentOffset));
  __ add(index, index, Operand::SmiUntag(result));
  __ jmp(&indirect_string_loaded);

  // Handle cons strings.
  // Check whether the right hand side is the empty string (i.e. if
  // this is really a flat string in a cons string). If that is not
  // the case we would rather go to the runtime system now to flatten
  // the string.
  __ bind(&cons_string);
  __ ldr(result, FieldMemOperand(string, ConsString::kSecondOffset));
  __ CompareRoot(result, Heap::kempty_stringRootIndex);
  __ b(ne, call_runtime);
  // Get the first of the two strings and load its instance type.
  __ ldr(string, FieldMemOperand(string, ConsString::kFirstOffset));

  __ bind(&indirect_string_loaded);
  __ ldr(result, FieldMemOperand(string, HeapObject::kMapOffset));
  __ ldrb(result, FieldMemOperand(result, Map::kInstanceTypeOffset));

  // Distinguish sequential and external strings. Only these two string
  // representations can reach here (slices and flat cons strings have been
  // reduced to the underlying sequential or external string).
  Label external_string, check_encoding;
  __ bind(&check_sequential);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ tst(result, Operand(kStringRepresentationMask));
  __ b(ne, &external_string);

  // Prepare sequential strings
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ add(string,
         string,
         Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  __ jmp(&check_encoding);

  // Handle external strings.
  __ bind(&external_string);
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ tst(result, Operand(kIsIndirectStringMask));
    __ Assert(eq, kExternalStringExpectedButNotFound);
  }
  // Rule out short external strings.
  STATIC_CHECK(kShortExternalStringTag != 0);
  __ tst(result, Operand(kShortExternalStringMask));
  __ b(ne, call_runtime);
  __ ldr(string, FieldMemOperand(string, ExternalString::kResourceDataOffset));

  Label ascii, done;
  __ bind(&check_encoding);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ tst(result, Operand(kStringEncodingMask));
  __ b(ne, &ascii);
  // Two-byte string.
  __ ldrh(result, MemOperand(string, index, LSL, 1));
  __ jmp(&done);
  __ bind(&ascii);
  // Ascii string.
  __ ldrb(result, MemOperand(string, index));
  __ bind(&done);
}


static MemOperand ExpConstant(int index, Register base) {
  return MemOperand(base, index * kDoubleSize);
}


void MathExpGenerator::EmitMathExp(MacroAssembler* masm,
                                   DwVfpRegister input,
                                   DwVfpRegister result,
                                   DwVfpRegister double_scratch1,
                                   DwVfpRegister double_scratch2,
                                   Register temp1,
                                   Register temp2,
                                   Register temp3) {
  ASSERT(!input.is(result));
  ASSERT(!input.is(double_scratch1));
  ASSERT(!input.is(double_scratch2));
  ASSERT(!result.is(double_scratch1));
  ASSERT(!result.is(double_scratch2));
  ASSERT(!double_scratch1.is(double_scratch2));
  ASSERT(!temp1.is(temp2));
  ASSERT(!temp1.is(temp3));
  ASSERT(!temp2.is(temp3));
  ASSERT(ExternalReference::math_exp_constants(0).address() != NULL);

  Label zero, infinity, done;

  __ mov(temp3, Operand(ExternalReference::math_exp_constants(0)));

  __ vldr(double_scratch1, ExpConstant(0, temp3));
  __ VFPCompareAndSetFlags(double_scratch1, input);
  __ b(ge, &zero);

  __ vldr(double_scratch2, ExpConstant(1, temp3));
  __ VFPCompareAndSetFlags(input, double_scratch2);
  __ b(ge, &infinity);

  __ vldr(double_scratch1, ExpConstant(3, temp3));
  __ vldr(result, ExpConstant(4, temp3));
  __ vmul(double_scratch1, double_scratch1, input);
  __ vadd(double_scratch1, double_scratch1, result);
  __ VmovLow(temp2, double_scratch1);
  __ vsub(double_scratch1, double_scratch1, result);
  __ vldr(result, ExpConstant(6, temp3));
  __ vldr(double_scratch2, ExpConstant(5, temp3));
  __ vmul(double_scratch1, double_scratch1, double_scratch2);
  __ vsub(double_scratch1, double_scratch1, input);
  __ vsub(result, result, double_scratch1);
  __ vmul(double_scratch2, double_scratch1, double_scratch1);
  __ vmul(result, result, double_scratch2);
  __ vldr(double_scratch2, ExpConstant(7, temp3));
  __ vmul(result, result, double_scratch2);
  __ vsub(result, result, double_scratch1);
  // Mov 1 in double_scratch2 as math_exp_constants_array[8] == 1.
  ASSERT(*reinterpret_cast<double*>
         (ExternalReference::math_exp_constants(8).address()) == 1);
  __ vmov(double_scratch2, 1);
  __ vadd(result, result, double_scratch2);
  __ mov(temp1, Operand(temp2, LSR, 11));
  __ Ubfx(temp2, temp2, 0, 11);
  __ add(temp1, temp1, Operand(0x3ff));

  // Must not call ExpConstant() after overwriting temp3!
  __ mov(temp3, Operand(ExternalReference::math_exp_log_table()));
  __ add(temp3, temp3, Operand(temp2, LSL, 3));
  __ ldm(ia, temp3, temp2.bit() | temp3.bit());
  // The first word is loaded is the lower number register.
  if (temp2.code() < temp3.code()) {
    __ orr(temp1, temp3, Operand(temp1, LSL, 20));
    __ vmov(double_scratch1, temp2, temp1);
  } else {
    __ orr(temp1, temp2, Operand(temp1, LSL, 20));
    __ vmov(double_scratch1, temp3, temp1);
  }
  __ vmul(result, result, double_scratch1);
  __ b(&done);

  __ bind(&zero);
  __ vmov(result, kDoubleRegZero);
  __ b(&done);

  __ bind(&infinity);
  __ vldr(result, ExpConstant(2, temp3));

  __ bind(&done);
}

#undef __

// add(r0, pc, Operand(-8))
static const uint32_t kCodeAgePatchFirstInstruction = 0xe24f0008;

static byte* GetNoCodeAgeSequence(uint32_t* length) {
  // The sequence of instructions that is patched out for aging code is the
  // following boilerplate stack-building prologue that is found in FUNCTIONS
  static bool initialized = false;
  static uint32_t sequence[kNoCodeAgeSequenceLength];
  byte* byte_sequence = reinterpret_cast<byte*>(sequence);
  *length = kNoCodeAgeSequenceLength * Assembler::kInstrSize;
  if (!initialized) {
    CodePatcher patcher(byte_sequence, kNoCodeAgeSequenceLength);
    PredictableCodeSizeScope scope(patcher.masm(), *length);
    patcher.masm()->stm(db_w, sp, r1.bit() | cp.bit() | fp.bit() | lr.bit());
    patcher.masm()->nop(ip.code());
    patcher.masm()->add(fp, sp, Operand(2 * kPointerSize));
    initialized = true;
  }
  return byte_sequence;
}


bool Code::IsYoungSequence(byte* sequence) {
  uint32_t young_length;
  byte* young_sequence = GetNoCodeAgeSequence(&young_length);
  bool result = !memcmp(sequence, young_sequence, young_length);
  ASSERT(result ||
         Memory::uint32_at(sequence) == kCodeAgePatchFirstInstruction);
  return result;
}


void Code::GetCodeAgeAndParity(byte* sequence, Age* age,
                               MarkingParity* parity) {
  if (IsYoungSequence(sequence)) {
    *age = kNoAgeCodeAge;
    *parity = NO_MARKING_PARITY;
  } else {
    Address target_address = Memory::Address_at(
        sequence + Assembler::kInstrSize * (kNoCodeAgeSequenceLength - 1));
    Code* stub = GetCodeFromTargetAddress(target_address);
    GetCodeAgeAndParity(stub, age, parity);
  }
}


void Code::PatchPlatformCodeAge(Isolate* isolate,
                                byte* sequence,
                                Code::Age age,
                                MarkingParity parity) {
  uint32_t young_length;
  byte* young_sequence = GetNoCodeAgeSequence(&young_length);
  if (age == kNoAgeCodeAge) {
    CopyBytes(sequence, young_sequence, young_length);
    CPU::FlushICache(sequence, young_length);
  } else {
    Code* stub = GetCodeAgeStub(isolate, age, parity);
    CodePatcher patcher(sequence, young_length / Assembler::kInstrSize);
    patcher.masm()->add(r0, pc, Operand(-8));
    patcher.masm()->ldr(pc, MemOperand(pc, -4));
    patcher.masm()->dd(reinterpret_cast<uint32_t>(stub->instruction_start()));
  }
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM
