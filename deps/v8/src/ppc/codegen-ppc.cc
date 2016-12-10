// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ppc/codegen-ppc.h"

#if V8_TARGET_ARCH_PPC

#include <memory>

#include "src/codegen.h"
#include "src/macro-assembler.h"
#include "src/ppc/simulator-ppc.h"

namespace v8 {
namespace internal {


#define __ masm.

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

// Called from C
  __ function_descriptor();

  __ MovFromFloatParameter(d1);
  __ fsqrt(d1, d1);
  __ MovToFloatResult(d1);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(&desc);
  DCHECK(ABI_USES_FUNCTION_DESCRIPTORS || !RelocInfo::RequiresRelocation(desc));

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
    MacroAssembler* masm, Register receiver, Register key, Register value,
    Register target_map, AllocationSiteMode mode,
    Label* allocation_memento_found) {
  Register scratch_elements = r7;
  DCHECK(!AreAliased(receiver, key, value, target_map, scratch_elements));

  if (mode == TRACK_ALLOCATION_SITE) {
    DCHECK(allocation_memento_found != NULL);
    __ JumpIfJSArrayHasAllocationMemento(receiver, scratch_elements, r11,
                                         allocation_memento_found);
  }

  // Set transitioned map.
  __ StoreP(target_map, FieldMemOperand(receiver, HeapObject::kMapOffset), r0);
  __ RecordWriteField(receiver, HeapObject::kMapOffset, target_map, r11,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}


void ElementsTransitionGenerator::GenerateSmiToDouble(
    MacroAssembler* masm, Register receiver, Register key, Register value,
    Register target_map, AllocationSiteMode mode, Label* fail) {
  // lr contains the return address
  Label loop, entry, convert_hole, only_change_map, done;
  Register elements = r7;
  Register length = r8;
  Register array = r9;
  Register array_end = array;

  // target_map parameter can be clobbered.
  Register scratch1 = target_map;
  Register scratch2 = r10;
  Register scratch3 = r11;
  Register scratch4 = r14;

  // Verify input registers don't conflict with locals.
  DCHECK(!AreAliased(receiver, key, value, target_map, elements, length, array,
                     scratch2));

  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(receiver, elements, scratch3, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ LoadP(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ CompareRoot(elements, Heap::kEmptyFixedArrayRootIndex);
  __ beq(&only_change_map);

  __ LoadP(length, FieldMemOperand(elements, FixedArray::kLengthOffset));
  // length: number of elements (smi-tagged)

  // Allocate new FixedDoubleArray.
  __ SmiToDoubleArrayOffset(scratch3, length);
  __ addi(scratch3, scratch3, Operand(FixedDoubleArray::kHeaderSize));
  __ Allocate(scratch3, array, scratch4, scratch2, fail, DOUBLE_ALIGNMENT);
  __ subi(array, array, Operand(kHeapObjectTag));
  // array: destination FixedDoubleArray, not tagged as heap object.
  // elements: source FixedArray.

  // Set destination FixedDoubleArray's length and map.
  __ LoadRoot(scratch2, Heap::kFixedDoubleArrayMapRootIndex);
  __ StoreP(length, MemOperand(array, FixedDoubleArray::kLengthOffset));
  // Update receiver's map.
  __ StoreP(scratch2, MemOperand(array, HeapObject::kMapOffset));

  __ StoreP(target_map, FieldMemOperand(receiver, HeapObject::kMapOffset), r0);
  __ RecordWriteField(receiver, HeapObject::kMapOffset, target_map, scratch2,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  // Replace receiver's backing store with newly created FixedDoubleArray.
  __ addi(scratch1, array, Operand(kHeapObjectTag));
  __ StoreP(scratch1, FieldMemOperand(receiver, JSObject::kElementsOffset), r0);
  __ RecordWriteField(receiver, JSObject::kElementsOffset, scratch1, scratch2,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);

  // Prepare for conversion loop.
  __ addi(scratch1, elements,
          Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ addi(scratch2, array, Operand(FixedDoubleArray::kHeaderSize));
  __ SmiToDoubleArrayOffset(array_end, length);
  __ add(array_end, scratch2, array_end);
// Repurpose registers no longer in use.
#if V8_TARGET_ARCH_PPC64
  Register hole_int64 = elements;
  __ mov(hole_int64, Operand(kHoleNanInt64));
#else
  Register hole_lower = elements;
  Register hole_upper = length;
  __ mov(hole_lower, Operand(kHoleNanLower32));
  __ mov(hole_upper, Operand(kHoleNanUpper32));
#endif
  // scratch1: begin of source FixedArray element fields, not tagged
  // hole_lower: kHoleNanLower32 OR hol_int64
  // hole_upper: kHoleNanUpper32
  // array_end: end of destination FixedDoubleArray, not tagged
  // scratch2: begin of FixedDoubleArray element fields, not tagged

  __ b(&entry);

  __ bind(&only_change_map);
  __ StoreP(target_map, FieldMemOperand(receiver, HeapObject::kMapOffset), r0);
  __ RecordWriteField(receiver, HeapObject::kMapOffset, target_map, scratch2,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ b(&done);

  // Convert and copy elements.
  __ bind(&loop);
  __ LoadP(scratch3, MemOperand(scratch1));
  __ addi(scratch1, scratch1, Operand(kPointerSize));
  // scratch3: current element
  __ UntagAndJumpIfNotSmi(scratch3, scratch3, &convert_hole);

  // Normal smi, convert to double and store.
  __ ConvertIntToDouble(scratch3, d0);
  __ stfd(d0, MemOperand(scratch2, 0));
  __ addi(scratch2, scratch2, Operand(8));
  __ b(&entry);

  // Hole found, store the-hole NaN.
  __ bind(&convert_hole);
  if (FLAG_debug_code) {
    __ LoadP(scratch3, MemOperand(scratch1, -kPointerSize));
    __ CompareRoot(scratch3, Heap::kTheHoleValueRootIndex);
    __ Assert(eq, kObjectFoundInSmiOnlyArray);
  }
#if V8_TARGET_ARCH_PPC64
  __ std(hole_int64, MemOperand(scratch2, 0));
#else
  __ stw(hole_upper, MemOperand(scratch2, Register::kExponentOffset));
  __ stw(hole_lower, MemOperand(scratch2, Register::kMantissaOffset));
#endif
  __ addi(scratch2, scratch2, Operand(8));

  __ bind(&entry);
  __ cmp(scratch2, array_end);
  __ blt(&loop);

  __ bind(&done);
}


void ElementsTransitionGenerator::GenerateDoubleToObject(
    MacroAssembler* masm, Register receiver, Register key, Register value,
    Register target_map, AllocationSiteMode mode, Label* fail) {
  // Register lr contains the return address.
  Label loop, convert_hole, gc_required, only_change_map;
  Register elements = r7;
  Register array = r9;
  Register length = r8;
  Register scratch = r10;
  Register scratch3 = r11;
  Register hole_value = r14;

  // Verify input registers don't conflict with locals.
  DCHECK(!AreAliased(receiver, key, value, target_map, elements, array, length,
                     scratch));

  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(receiver, elements, scratch3, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ LoadP(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ CompareRoot(elements, Heap::kEmptyFixedArrayRootIndex);
  __ beq(&only_change_map);

  __ Push(target_map, receiver, key, value);
  __ LoadP(length, FieldMemOperand(elements, FixedArray::kLengthOffset));
  // elements: source FixedDoubleArray
  // length: number of elements (smi-tagged)

  // Allocate new FixedArray.
  // Re-use value and target_map registers, as they have been saved on the
  // stack.
  Register array_size = value;
  Register allocate_scratch = target_map;
  __ li(array_size, Operand(FixedDoubleArray::kHeaderSize));
  __ SmiToPtrArrayOffset(r0, length);
  __ add(array_size, array_size, r0);
  __ Allocate(array_size, array, allocate_scratch, scratch, &gc_required,
              NO_ALLOCATION_FLAGS);
  // array: destination FixedArray, tagged as heap object
  // Set destination FixedDoubleArray's length and map.
  __ LoadRoot(scratch, Heap::kFixedArrayMapRootIndex);
  __ StoreP(length, FieldMemOperand(array,
            FixedDoubleArray::kLengthOffset), r0);
  __ StoreP(scratch, FieldMemOperand(array, HeapObject::kMapOffset), r0);

  // Prepare for conversion loop.
  Register src_elements = elements;
  Register dst_elements = target_map;
  Register dst_end = length;
  Register heap_number_map = scratch;
  __ addi(src_elements, elements,
          Operand(FixedDoubleArray::kHeaderSize - kHeapObjectTag));
  __ SmiToPtrArrayOffset(length, length);
  __ LoadRoot(hole_value, Heap::kTheHoleValueRootIndex);

  Label initialization_loop, loop_done;
  __ ShiftRightImm(r0, length, Operand(kPointerSizeLog2), SetRC);
  __ beq(&loop_done, cr0);

  // Allocating heap numbers in the loop below can fail and cause a jump to
  // gc_required. We can't leave a partly initialized FixedArray behind,
  // so pessimistically fill it with holes now.
  __ mtctr(r0);
  __ addi(dst_elements, array,
          Operand(FixedArray::kHeaderSize - kHeapObjectTag - kPointerSize));
  __ bind(&initialization_loop);
  __ StorePU(hole_value, MemOperand(dst_elements, kPointerSize));
  __ bdnz(&initialization_loop);

  __ addi(dst_elements, array,
          Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ add(dst_end, dst_elements, length);
  __ LoadRoot(heap_number_map, Heap::kHeapNumberMapRootIndex);
  // Using offsetted addresses in src_elements to fully take advantage of
  // post-indexing.
  // dst_elements: begin of destination FixedArray element fields, not tagged
  // src_elements: begin of source FixedDoubleArray element fields,
  //               not tagged, +4
  // dst_end: end of destination FixedArray, not tagged
  // array: destination FixedArray
  // hole_value: the-hole pointer
  // heap_number_map: heap number map
  __ b(&loop);

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  __ Pop(target_map, receiver, key, value);
  __ b(fail);

  // Replace the-hole NaN with the-hole pointer.
  __ bind(&convert_hole);
  __ StoreP(hole_value, MemOperand(dst_elements));
  __ addi(dst_elements, dst_elements, Operand(kPointerSize));
  __ cmpl(dst_elements, dst_end);
  __ bge(&loop_done);

  __ bind(&loop);
  Register upper_bits = key;
  __ lwz(upper_bits, MemOperand(src_elements, Register::kExponentOffset));
  __ addi(src_elements, src_elements, Operand(kDoubleSize));
  // upper_bits: current element's upper 32 bit
  // src_elements: address of next element's upper 32 bit
  __ Cmpi(upper_bits, Operand(kHoleNanUpper32), r0);
  __ beq(&convert_hole);

  // Non-hole double, copy value into a heap number.
  Register heap_number = receiver;
  Register scratch2 = value;
  __ AllocateHeapNumber(heap_number, scratch2, scratch3, heap_number_map,
                        &gc_required);
  // heap_number: new heap number
#if V8_TARGET_ARCH_PPC64
  __ ld(scratch2, MemOperand(src_elements, -kDoubleSize));
  // subtract tag for std
  __ addi(upper_bits, heap_number, Operand(-kHeapObjectTag));
  __ std(scratch2, MemOperand(upper_bits, HeapNumber::kValueOffset));
#else
  __ lwz(scratch2,
         MemOperand(src_elements, Register::kMantissaOffset - kDoubleSize));
  __ lwz(upper_bits,
         MemOperand(src_elements, Register::kExponentOffset - kDoubleSize));
  __ stw(scratch2, FieldMemOperand(heap_number, HeapNumber::kMantissaOffset));
  __ stw(upper_bits, FieldMemOperand(heap_number, HeapNumber::kExponentOffset));
#endif
  __ mr(scratch2, dst_elements);
  __ StoreP(heap_number, MemOperand(dst_elements));
  __ addi(dst_elements, dst_elements, Operand(kPointerSize));
  __ RecordWrite(array, scratch2, heap_number, kLRHasNotBeenSaved,
                 kDontSaveFPRegs, EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);
  __ cmpl(dst_elements, dst_end);
  __ blt(&loop);
  __ bind(&loop_done);

  __ Pop(target_map, receiver, key, value);
  // Replace receiver's backing store with newly created and filled FixedArray.
  __ StoreP(array, FieldMemOperand(receiver, JSObject::kElementsOffset), r0);
  __ RecordWriteField(receiver, JSObject::kElementsOffset, array, scratch,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);

  __ bind(&only_change_map);
  // Update receiver's map.
  __ StoreP(target_map, FieldMemOperand(receiver, HeapObject::kMapOffset), r0);
  __ RecordWriteField(receiver, HeapObject::kMapOffset, target_map, scratch,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}


// assume ip can be used as a scratch register below
void StringCharLoadGenerator::Generate(MacroAssembler* masm, Register string,
                                       Register index, Register result,
                                       Label* call_runtime) {
  // Fetch the instance type of the receiver into result register.
  __ LoadP(result, FieldMemOperand(string, HeapObject::kMapOffset));
  __ lbz(result, FieldMemOperand(result, Map::kInstanceTypeOffset));

  // We need special handling for indirect strings.
  Label check_sequential;
  __ andi(r0, result, Operand(kIsIndirectStringMask));
  __ beq(&check_sequential, cr0);

  // Dispatch on the indirect string shape: slice or cons.
  Label cons_string;
  __ mov(ip, Operand(kSlicedNotConsMask));
  __ and_(r0, result, ip, SetRC);
  __ beq(&cons_string, cr0);

  // Handle slices.
  Label indirect_string_loaded;
  __ LoadP(result, FieldMemOperand(string, SlicedString::kOffsetOffset));
  __ LoadP(string, FieldMemOperand(string, SlicedString::kParentOffset));
  __ SmiUntag(ip, result);
  __ add(index, index, ip);
  __ b(&indirect_string_loaded);

  // Handle cons strings.
  // Check whether the right hand side is the empty string (i.e. if
  // this is really a flat string in a cons string). If that is not
  // the case we would rather go to the runtime system now to flatten
  // the string.
  __ bind(&cons_string);
  __ LoadP(result, FieldMemOperand(string, ConsString::kSecondOffset));
  __ CompareRoot(result, Heap::kempty_stringRootIndex);
  __ bne(call_runtime);
  // Get the first of the two strings and load its instance type.
  __ LoadP(string, FieldMemOperand(string, ConsString::kFirstOffset));

  __ bind(&indirect_string_loaded);
  __ LoadP(result, FieldMemOperand(string, HeapObject::kMapOffset));
  __ lbz(result, FieldMemOperand(result, Map::kInstanceTypeOffset));

  // Distinguish sequential and external strings. Only these two string
  // representations can reach here (slices and flat cons strings have been
  // reduced to the underlying sequential or external string).
  Label external_string, check_encoding;
  __ bind(&check_sequential);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ andi(r0, result, Operand(kStringRepresentationMask));
  __ bne(&external_string, cr0);

  // Prepare sequential strings
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ addi(string, string,
          Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  __ b(&check_encoding);

  // Handle external strings.
  __ bind(&external_string);
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ andi(r0, result, Operand(kIsIndirectStringMask));
    __ Assert(eq, kExternalStringExpectedButNotFound, cr0);
  }
  // Rule out short external strings.
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ andi(r0, result, Operand(kShortExternalStringMask));
  __ bne(call_runtime, cr0);
  __ LoadP(string,
           FieldMemOperand(string, ExternalString::kResourceDataOffset));

  Label one_byte, done;
  __ bind(&check_encoding);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ andi(r0, result, Operand(kStringEncodingMask));
  __ bne(&one_byte, cr0);
  // Two-byte string.
  __ ShiftLeftImm(result, index, Operand(1));
  __ lhzx(result, MemOperand(string, result));
  __ b(&done);
  __ bind(&one_byte);
  // One-byte string.
  __ lbzx(result, MemOperand(string, index));
  __ bind(&done);
}

#undef __

CodeAgingHelper::CodeAgingHelper(Isolate* isolate) {
  USE(isolate);
  DCHECK(young_sequence_.length() == kNoCodeAgeSequenceLength);
  // Since patcher is a large object, allocate it dynamically when needed,
  // to avoid overloading the stack in stress conditions.
  // DONT_FLUSH is used because the CodeAgingHelper is initialized early in
  // the process, before ARM simulator ICache is setup.
  std::unique_ptr<CodePatcher> patcher(
      new CodePatcher(isolate, young_sequence_.start(),
                      young_sequence_.length() / Assembler::kInstrSize,
                      CodePatcher::DONT_FLUSH));
  PredictableCodeSizeScope scope(patcher->masm(), young_sequence_.length());
  patcher->masm()->PushStandardFrame(r4);
  for (int i = 0; i < kNoCodeAgeSequenceNops; i++) {
    patcher->masm()->nop();
  }
}


#ifdef DEBUG
bool CodeAgingHelper::IsOld(byte* candidate) const {
  return Assembler::IsNop(Assembler::instr_at(candidate));
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
    Code* code = NULL;
    Address target_address =
        Assembler::target_address_at(sequence + kCodeAgingTargetDelta, code);
    Code* stub = GetCodeFromTargetAddress(target_address);
    GetCodeAgeAndParity(stub, age, parity);
  }
}


void Code::PatchPlatformCodeAge(Isolate* isolate, byte* sequence, Code::Age age,
                                MarkingParity parity) {
  uint32_t young_length = isolate->code_aging_helper()->young_sequence_length();
  if (age == kNoAgeCodeAge) {
    isolate->code_aging_helper()->CopyYoungSequenceTo(sequence);
    Assembler::FlushICache(isolate, sequence, young_length);
  } else {
    // FIXED_SEQUENCE
    Code* stub = GetCodeAgeStub(isolate, age, parity);
    CodePatcher patcher(isolate, sequence,
                        young_length / Assembler::kInstrSize);
    Assembler::BlockTrampolinePoolScope block_trampoline_pool(patcher.masm());
    intptr_t target = reinterpret_cast<intptr_t>(stub->instruction_start());
    // Don't use Call -- we need to preserve ip and lr.
    // GenerateMakeCodeYoungAgainCommon for the stub code.
    patcher.masm()->nop();  // marker to detect sequence (see IsOld)
    patcher.masm()->mov(r3, Operand(target));
    patcher.masm()->Jump(r3);
    for (int i = 0; i < kCodeAgingSequenceNops; i++) {
      patcher.masm()->nop();
    }
  }
}
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC
