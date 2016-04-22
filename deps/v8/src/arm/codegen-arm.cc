// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arm/codegen-arm.h"

#if V8_TARGET_ARCH_ARM

#include "src/arm/simulator-arm.h"
#include "src/codegen.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {


#define __ masm.


#if defined(USE_SIMULATOR)
byte* fast_exp_arm_machine_code = nullptr;
double fast_exp_simulator(double x, Isolate* isolate) {
  return Simulator::current(isolate)
      ->CallFPReturnsDouble(fast_exp_arm_machine_code, x, 0);
}
#endif


UnaryMathFunctionWithIsolate CreateExpFunction(Isolate* isolate) {
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(base::OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == nullptr) return nullptr;
  ExternalReference::InitializeMathExpData();

  MacroAssembler masm(isolate, buffer, static_cast<int>(actual_size),
                      CodeObjectRequired::kNo);

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
  DCHECK(!RelocInfo::RequiresRelocation(desc));

  Assembler::FlushICache(isolate, buffer, actual_size);
  base::OS::ProtectCode(buffer, actual_size);

#if !defined(USE_SIMULATOR)
  return FUNCTION_CAST<UnaryMathFunctionWithIsolate>(buffer);
#else
  fast_exp_arm_machine_code = buffer;
  return &fast_exp_simulator;
#endif
}

#if defined(V8_HOST_ARCH_ARM)
MemCopyUint8Function CreateMemCopyUint8Function(Isolate* isolate,
                                                MemCopyUint8Function stub) {
#if defined(USE_SIMULATOR)
  return stub;
#else
  if (!CpuFeatures::IsSupported(UNALIGNED_ACCESSES)) return stub;
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(base::OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == nullptr) return stub;

  MacroAssembler masm(isolate, buffer, static_cast<int>(actual_size),
                      CodeObjectRequired::kNo);

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
  DCHECK(!RelocInfo::RequiresRelocation(desc));

  Assembler::FlushICache(isolate, buffer, actual_size);
  base::OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<MemCopyUint8Function>(buffer);
#endif
}


// Convert 8 to 16. The number of character to copy must be at least 8.
MemCopyUint16Uint8Function CreateMemCopyUint16Uint8Function(
    Isolate* isolate, MemCopyUint16Uint8Function stub) {
#if defined(USE_SIMULATOR)
  return stub;
#else
  if (!CpuFeatures::IsSupported(UNALIGNED_ACCESSES)) return stub;
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(base::OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == nullptr) return stub;

  MacroAssembler masm(isolate, buffer, static_cast<int>(actual_size),
                      CodeObjectRequired::kNo);

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
  masm.GetCode(&desc);

  Assembler::FlushICache(isolate, buffer, actual_size);
  base::OS::ProtectCode(buffer, actual_size);

  return FUNCTION_CAST<MemCopyUint16Uint8Function>(buffer);
#endif
}
#endif

UnaryMathFunctionWithIsolate CreateSqrtFunction(Isolate* isolate) {
#if defined(USE_SIMULATOR)
  return nullptr;
#else
  size_t actual_size;
  byte* buffer =
      static_cast<byte*>(base::OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == nullptr) return nullptr;

  MacroAssembler masm(isolate, buffer, static_cast<int>(actual_size),
                      CodeObjectRequired::kNo);

  __ MovFromFloatParameter(d0);
  __ vsqrt(d0, d0);
  __ MovToFloatResult(d0);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(&desc);
  DCHECK(!RelocInfo::RequiresRelocation(desc));

  Assembler::FlushICache(isolate, buffer, actual_size);
  base::OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<UnaryMathFunctionWithIsolate>(buffer);
#endif
}

#undef __


// -------------------------------------------------------------------------
// Platform-specific RuntimeCallHelper functions.

void StubRuntimeCallHelper::BeforeCall(MacroAssembler* masm) const {
  masm->EnterFrame(StackFrame::INTERNAL);
  DCHECK(!masm->has_frame());
  masm->set_has_frame(true);
}


void StubRuntimeCallHelper::AfterCall(MacroAssembler* masm) const {
  masm->LeaveFrame(StackFrame::INTERNAL);
  DCHECK(masm->has_frame());
  masm->set_has_frame(false);
}


// -------------------------------------------------------------------------
// Code generators

#define __ ACCESS_MASM(masm)

void ElementsTransitionGenerator::GenerateMapChangeElementsTransition(
    MacroAssembler* masm,
    Register receiver,
    Register key,
    Register value,
    Register target_map,
    AllocationSiteMode mode,
    Label* allocation_memento_found) {
  Register scratch_elements = r4;
  DCHECK(!AreAliased(receiver, key, value, target_map,
                     scratch_elements));

  if (mode == TRACK_ALLOCATION_SITE) {
    DCHECK(allocation_memento_found != NULL);
    __ JumpIfJSArrayHasAllocationMemento(
        receiver, scratch_elements, allocation_memento_found);
  }

  // Set transitioned map.
  __ str(target_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ RecordWriteField(receiver,
                      HeapObject::kMapOffset,
                      target_map,
                      r9,
                      kLRHasNotBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}


void ElementsTransitionGenerator::GenerateSmiToDouble(
    MacroAssembler* masm,
    Register receiver,
    Register key,
    Register value,
    Register target_map,
    AllocationSiteMode mode,
    Label* fail) {
  // Register lr contains the return address.
  Label loop, entry, convert_hole, gc_required, only_change_map, done;
  Register elements = r4;
  Register length = r5;
  Register array = r6;
  Register array_end = array;

  // target_map parameter can be clobbered.
  Register scratch1 = target_map;
  Register scratch2 = r9;

  // Verify input registers don't conflict with locals.
  DCHECK(!AreAliased(receiver, key, value, target_map,
                     elements, length, array, scratch2));

  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(receiver, elements, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ CompareRoot(elements, Heap::kEmptyFixedArrayRootIndex);
  __ b(eq, &only_change_map);

  __ push(lr);
  __ ldr(length, FieldMemOperand(elements, FixedArray::kLengthOffset));
  // length: number of elements (smi-tagged)

  // Allocate new FixedDoubleArray.
  // Use lr as a temporary register.
  __ mov(lr, Operand(length, LSL, 2));
  __ add(lr, lr, Operand(FixedDoubleArray::kHeaderSize));
  __ Allocate(lr, array, elements, scratch2, &gc_required, DOUBLE_ALIGNMENT);
  // array: destination FixedDoubleArray, not tagged as heap object.
  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  // r4: source FixedArray.

  // Set destination FixedDoubleArray's length and map.
  __ LoadRoot(scratch2, Heap::kFixedDoubleArrayMapRootIndex);
  __ str(length, MemOperand(array, FixedDoubleArray::kLengthOffset));
  // Update receiver's map.
  __ str(scratch2, MemOperand(array, HeapObject::kMapOffset));

  __ str(target_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ RecordWriteField(receiver,
                      HeapObject::kMapOffset,
                      target_map,
                      scratch2,
                      kLRHasBeenSaved,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  // Replace receiver's backing store with newly created FixedDoubleArray.
  __ add(scratch1, array, Operand(kHeapObjectTag));
  __ str(scratch1, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ RecordWriteField(receiver,
                      JSObject::kElementsOffset,
                      scratch1,
                      scratch2,
                      kLRHasBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);

  // Prepare for conversion loop.
  __ add(scratch1, elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ add(scratch2, array, Operand(FixedDoubleArray::kHeaderSize));
  __ add(array_end, scratch2, Operand(length, LSL, 2));

  // Repurpose registers no longer in use.
  Register hole_lower = elements;
  Register hole_upper = length;

  __ mov(hole_lower, Operand(kHoleNanLower32));
  __ mov(hole_upper, Operand(kHoleNanUpper32));
  // scratch1: begin of source FixedArray element fields, not tagged
  // hole_lower: kHoleNanLower32
  // hole_upper: kHoleNanUpper32
  // array_end: end of destination FixedDoubleArray, not tagged
  // scratch2: begin of FixedDoubleArray element fields, not tagged

  __ b(&entry);

  __ bind(&only_change_map);
  __ str(target_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ RecordWriteField(receiver,
                      HeapObject::kMapOffset,
                      target_map,
                      scratch2,
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
  __ ldr(lr, MemOperand(scratch1, 4, PostIndex));
  // lr: current element
  __ UntagAndJumpIfNotSmi(lr, lr, &convert_hole);

  // Normal smi, convert to double and store.
  __ vmov(s0, lr);
  __ vcvt_f64_s32(d0, s0);
  __ vstr(d0, scratch2, 0);
  __ add(scratch2, scratch2, Operand(8));
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
  __ Strd(hole_lower, hole_upper, MemOperand(scratch2, 8, PostIndex));

  __ bind(&entry);
  __ cmp(scratch2, array_end);
  __ b(lt, &loop);

  __ pop(lr);
  __ bind(&done);
}


void ElementsTransitionGenerator::GenerateDoubleToObject(
    MacroAssembler* masm,
    Register receiver,
    Register key,
    Register value,
    Register target_map,
    AllocationSiteMode mode,
    Label* fail) {
  // Register lr contains the return address.
  Label entry, loop, convert_hole, gc_required, only_change_map;
  Register elements = r4;
  Register array = r6;
  Register length = r5;
  Register scratch = r9;

  // Verify input registers don't conflict with locals.
  DCHECK(!AreAliased(receiver, key, value, target_map,
                     elements, array, length, scratch));

  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(receiver, elements, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ CompareRoot(elements, Heap::kEmptyFixedArrayRootIndex);
  __ b(eq, &only_change_map);

  __ push(lr);
  __ Push(target_map, receiver, key, value);
  __ ldr(length, FieldMemOperand(elements, FixedArray::kLengthOffset));
  // elements: source FixedDoubleArray
  // length: number of elements (smi-tagged)

  // Allocate new FixedArray.
  // Re-use value and target_map registers, as they have been saved on the
  // stack.
  Register array_size = value;
  Register allocate_scratch = target_map;
  __ mov(array_size, Operand(FixedDoubleArray::kHeaderSize));
  __ add(array_size, array_size, Operand(length, LSL, 1));
  __ Allocate(array_size, array, allocate_scratch, scratch, &gc_required,
              NO_ALLOCATION_FLAGS);
  // array: destination FixedArray, not tagged as heap object
  // Set destination FixedDoubleArray's length and map.
  __ LoadRoot(scratch, Heap::kFixedArrayMapRootIndex);
  __ str(length, MemOperand(array, FixedDoubleArray::kLengthOffset));
  __ str(scratch, MemOperand(array, HeapObject::kMapOffset));

  // Prepare for conversion loop.
  Register src_elements = elements;
  Register dst_elements = target_map;
  Register dst_end = length;
  Register heap_number_map = scratch;
  __ add(src_elements, elements,
         Operand(FixedDoubleArray::kHeaderSize - kHeapObjectTag + 4));
  __ add(dst_elements, array, Operand(FixedArray::kHeaderSize));
  __ add(dst_end, dst_elements, Operand(length, LSL, 1));

  // Allocating heap numbers in the loop below can fail and cause a jump to
  // gc_required. We can't leave a partly initialized FixedArray behind,
  // so pessimistically fill it with holes now.
  Label initialization_loop, initialization_loop_entry;
  __ LoadRoot(scratch, Heap::kTheHoleValueRootIndex);
  __ b(&initialization_loop_entry);
  __ bind(&initialization_loop);
  __ str(scratch, MemOperand(dst_elements, kPointerSize, PostIndex));
  __ bind(&initialization_loop_entry);
  __ cmp(dst_elements, dst_end);
  __ b(lt, &initialization_loop);

  __ add(dst_elements, array, Operand(FixedArray::kHeaderSize));
  __ add(array, array, Operand(kHeapObjectTag));
  __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  // Using offsetted addresses in src_elements to fully take advantage of
  // post-indexing.
  // dst_elements: begin of destination FixedArray element fields, not tagged
  // src_elements: begin of source FixedDoubleArray element fields,
  //               not tagged, +4
  // dst_end: end of destination FixedArray, not tagged
  // array: destination FixedArray
  // heap_number_map: heap number map
  __ b(&entry);

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  __ Pop(target_map, receiver, key, value);
  __ pop(lr);
  __ b(fail);

  __ bind(&loop);
  Register upper_bits = key;
  __ ldr(upper_bits, MemOperand(src_elements, 8, PostIndex));
  // upper_bits: current element's upper 32 bit
  // src_elements: address of next element's upper 32 bit
  __ cmp(upper_bits, Operand(kHoleNanUpper32));
  __ b(eq, &convert_hole);

  // Non-hole double, copy value into a heap number.
  Register heap_number = receiver;
  Register scratch2 = value;
  __ AllocateHeapNumber(heap_number, scratch2, lr, heap_number_map,
                        &gc_required);
  // heap_number: new heap number
  __ ldr(scratch2, MemOperand(src_elements, 12, NegOffset));
  __ Strd(scratch2, upper_bits,
          FieldMemOperand(heap_number, HeapNumber::kValueOffset));
  __ mov(scratch2, dst_elements);
  __ str(heap_number, MemOperand(dst_elements, 4, PostIndex));
  __ RecordWrite(array,
                 scratch2,
                 heap_number,
                 kLRHasBeenSaved,
                 kDontSaveFPRegs,
                 EMIT_REMEMBERED_SET,
                 OMIT_SMI_CHECK);
  __ b(&entry);

  // Replace the-hole NaN with the-hole pointer.
  __ bind(&convert_hole);
  __ LoadRoot(scratch2, Heap::kTheHoleValueRootIndex);
  __ str(scratch2, MemOperand(dst_elements, 4, PostIndex));

  __ bind(&entry);
  __ cmp(dst_elements, dst_end);
  __ b(lt, &loop);

  __ Pop(target_map, receiver, key, value);
  // Replace receiver's backing store with newly created and filled FixedArray.
  __ str(array, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ RecordWriteField(receiver,
                      JSObject::kElementsOffset,
                      array,
                      scratch,
                      kLRHasBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ pop(lr);

  __ bind(&only_change_map);
  // Update receiver's map.
  __ str(target_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ RecordWriteField(receiver,
                      HeapObject::kMapOffset,
                      target_map,
                      scratch,
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
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ tst(result, Operand(kShortExternalStringMask));
  __ b(ne, call_runtime);
  __ ldr(string, FieldMemOperand(string, ExternalString::kResourceDataOffset));

  Label one_byte, done;
  __ bind(&check_encoding);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ tst(result, Operand(kStringEncodingMask));
  __ b(ne, &one_byte);
  // Two-byte string.
  __ ldrh(result, MemOperand(string, index, LSL, 1));
  __ jmp(&done);
  __ bind(&one_byte);
  // One-byte string.
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
  DCHECK(!input.is(result));
  DCHECK(!input.is(double_scratch1));
  DCHECK(!input.is(double_scratch2));
  DCHECK(!result.is(double_scratch1));
  DCHECK(!result.is(double_scratch2));
  DCHECK(!double_scratch1.is(double_scratch2));
  DCHECK(!temp1.is(temp2));
  DCHECK(!temp1.is(temp3));
  DCHECK(!temp2.is(temp3));
  DCHECK(ExternalReference::math_exp_constants(0).address() != NULL);
  DCHECK(!masm->serializer_enabled());  // External references not serializable.

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
  DCHECK(*reinterpret_cast<double*>
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

#ifdef DEBUG
// add(r0, pc, Operand(-8))
static const uint32_t kCodeAgePatchFirstInstruction = 0xe24f0008;
#endif

CodeAgingHelper::CodeAgingHelper(Isolate* isolate) {
  USE(isolate);
  DCHECK(young_sequence_.length() == kNoCodeAgeSequenceLength);
  // Since patcher is a large object, allocate it dynamically when needed,
  // to avoid overloading the stack in stress conditions.
  // DONT_FLUSH is used because the CodeAgingHelper is initialized early in
  // the process, before ARM simulator ICache is setup.
  base::SmartPointer<CodePatcher> patcher(
      new CodePatcher(isolate, young_sequence_.start(),
                      young_sequence_.length() / Assembler::kInstrSize,
                      CodePatcher::DONT_FLUSH));
  PredictableCodeSizeScope scope(patcher->masm(), young_sequence_.length());
  patcher->masm()->PushFixedFrame(r1);
  patcher->masm()->nop(ip.code());
  patcher->masm()->add(
      fp, sp, Operand(StandardFrameConstants::kFixedFrameSizeFromFp));
}


#ifdef DEBUG
bool CodeAgingHelper::IsOld(byte* candidate) const {
  return Memory::uint32_at(candidate) == kCodeAgePatchFirstInstruction;
}
#endif


bool Code::IsYoungSequence(Isolate* isolate, byte* sequence) {
  bool result = isolate->code_aging_helper()->IsYoung(sequence);
  DCHECK(result || isolate->code_aging_helper()->IsOld(sequence));
  return result;
}


void Code::GetCodeAgeAndParity(Isolate* isolate, byte* sequence, Age* age,
                               MarkingParity* parity) {
  if (IsYoungSequence(isolate, sequence)) {
    *age = kNoAgeCodeAge;
    *parity = NO_MARKING_PARITY;
  } else {
    Address target_address = Memory::Address_at(
        sequence + (kNoCodeAgeSequenceLength - Assembler::kInstrSize));
    Code* stub = GetCodeFromTargetAddress(target_address);
    GetCodeAgeAndParity(stub, age, parity);
  }
}


void Code::PatchPlatformCodeAge(Isolate* isolate,
                                byte* sequence,
                                Code::Age age,
                                MarkingParity parity) {
  uint32_t young_length = isolate->code_aging_helper()->young_sequence_length();
  if (age == kNoAgeCodeAge) {
    isolate->code_aging_helper()->CopyYoungSequenceTo(sequence);
    Assembler::FlushICache(isolate, sequence, young_length);
  } else {
    Code* stub = GetCodeAgeStub(isolate, age, parity);
    CodePatcher patcher(isolate, sequence,
                        young_length / Assembler::kInstrSize);
    patcher.masm()->add(r0, pc, Operand(-8));
    patcher.masm()->ldr(pc, MemOperand(pc, -4));
    patcher.masm()->emit_code_stub_address(stub);
  }
}


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM
