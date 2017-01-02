// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/x64/codegen-x64.h"

#if V8_TARGET_ARCH_X64

#include "src/codegen.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {

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


#define __ masm.


UnaryMathFunctionWithIsolate CreateSqrtFunction(Isolate* isolate) {
  size_t actual_size;
  // Allocate buffer in executable space.
  byte* buffer =
      static_cast<byte*>(base::OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == nullptr) return nullptr;

  MacroAssembler masm(isolate, buffer, static_cast<int>(actual_size),
                      CodeObjectRequired::kNo);
  // xmm0: raw double input.
  // Move double input into registers.
  __ Sqrtsd(xmm0, xmm0);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(&desc);
  DCHECK(!RelocInfo::RequiresRelocation(desc));

  Assembler::FlushICache(isolate, buffer, actual_size);
  base::OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<UnaryMathFunctionWithIsolate>(buffer);
}

#undef __

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
  // Return address is on the stack.
  Register scratch = rdi;
  DCHECK(!AreAliased(receiver, key, value, target_map, scratch));

  if (mode == TRACK_ALLOCATION_SITE) {
    DCHECK(allocation_memento_found != NULL);
    __ JumpIfJSArrayHasAllocationMemento(
        receiver, scratch, allocation_memento_found);
  }

  // Set transitioned map.
  __ movp(FieldOperand(receiver, HeapObject::kMapOffset), target_map);
  __ RecordWriteField(receiver,
                      HeapObject::kMapOffset,
                      target_map,
                      scratch,
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
  // Return address is on the stack.
  DCHECK(receiver.is(rdx));
  DCHECK(key.is(rcx));
  DCHECK(value.is(rax));
  DCHECK(target_map.is(rbx));

  // The fail label is not actually used since we do not allocate.
  Label allocated, new_backing_store, only_change_map, done;

  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(rdx, rdi, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ movp(r8, FieldOperand(rdx, JSObject::kElementsOffset));
  __ CompareRoot(r8, Heap::kEmptyFixedArrayRootIndex);
  __ j(equal, &only_change_map);

  __ SmiToInteger32(r9, FieldOperand(r8, FixedDoubleArray::kLengthOffset));
  if (kPointerSize == kDoubleSize) {
    // Check backing store for COW-ness. For COW arrays we have to
    // allocate a new backing store.
    __ CompareRoot(FieldOperand(r8, HeapObject::kMapOffset),
                   Heap::kFixedCOWArrayMapRootIndex);
    __ j(equal, &new_backing_store);
  } else {
    // For x32 port we have to allocate a new backing store as SMI size is
    // not equal with double size.
    DCHECK(kDoubleSize == 2 * kPointerSize);
    __ jmp(&new_backing_store);
  }

  // Check if the backing store is in new-space. If not, we need to allocate
  // a new one since the old one is in pointer-space.
  // If in new space, we can reuse the old backing store because it is
  // the same size.
  __ JumpIfNotInNewSpace(r8, rdi, &new_backing_store);

  __ movp(r14, r8);  // Destination array equals source array.

  // r8 : source FixedArray
  // r9 : elements array length
  // r14: destination FixedDoubleArray
  // Set backing store's map
  __ LoadRoot(rdi, Heap::kFixedDoubleArrayMapRootIndex);
  __ movp(FieldOperand(r14, HeapObject::kMapOffset), rdi);

  __ bind(&allocated);
  // Set transitioned map.
  __ movp(FieldOperand(rdx, HeapObject::kMapOffset), rbx);
  __ RecordWriteField(rdx,
                      HeapObject::kMapOffset,
                      rbx,
                      rdi,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);

  // Convert smis to doubles and holes to hole NaNs.  The Array's length
  // remains unchanged.
  STATIC_ASSERT(FixedDoubleArray::kLengthOffset == FixedArray::kLengthOffset);
  STATIC_ASSERT(FixedDoubleArray::kHeaderSize == FixedArray::kHeaderSize);

  Label loop, entry, convert_hole;
  __ movq(r15, bit_cast<int64_t, uint64_t>(kHoleNanInt64));
  // r15: the-hole NaN
  __ jmp(&entry);

  // Allocate new backing store.
  __ bind(&new_backing_store);
  __ leap(rdi, Operand(r9, times_8, FixedArray::kHeaderSize));
  __ Allocate(rdi, r14, r11, r15, fail, NO_ALLOCATION_FLAGS);
  // Set backing store's map
  __ LoadRoot(rdi, Heap::kFixedDoubleArrayMapRootIndex);
  __ movp(FieldOperand(r14, HeapObject::kMapOffset), rdi);
  // Set receiver's backing store.
  __ movp(FieldOperand(rdx, JSObject::kElementsOffset), r14);
  __ movp(r11, r14);
  __ RecordWriteField(rdx,
                      JSObject::kElementsOffset,
                      r11,
                      r15,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  // Set backing store's length.
  __ Integer32ToSmi(r11, r9);
  __ movp(FieldOperand(r14, FixedDoubleArray::kLengthOffset), r11);
  __ jmp(&allocated);

  __ bind(&only_change_map);
  // Set transitioned map.
  __ movp(FieldOperand(rdx, HeapObject::kMapOffset), rbx);
  __ RecordWriteField(rdx,
                      HeapObject::kMapOffset,
                      rbx,
                      rdi,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ jmp(&done);

  // Conversion loop.
  __ bind(&loop);
  __ movp(rbx,
          FieldOperand(r8, r9, times_pointer_size, FixedArray::kHeaderSize));
  // r9 : current element's index
  // rbx: current element (smi-tagged)
  __ JumpIfNotSmi(rbx, &convert_hole);
  __ SmiToInteger32(rbx, rbx);
  __ Cvtlsi2sd(kScratchDoubleReg, rbx);
  __ Movsd(FieldOperand(r14, r9, times_8, FixedDoubleArray::kHeaderSize),
           kScratchDoubleReg);
  __ jmp(&entry);
  __ bind(&convert_hole);

  if (FLAG_debug_code) {
    __ CompareRoot(rbx, Heap::kTheHoleValueRootIndex);
    __ Assert(equal, kObjectFoundInSmiOnlyArray);
  }

  __ movq(FieldOperand(r14, r9, times_8, FixedDoubleArray::kHeaderSize), r15);
  __ bind(&entry);
  __ decp(r9);
  __ j(not_sign, &loop);

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
  // Return address is on the stack.
  DCHECK(receiver.is(rdx));
  DCHECK(key.is(rcx));
  DCHECK(value.is(rax));
  DCHECK(target_map.is(rbx));

  Label loop, entry, convert_hole, gc_required, only_change_map;

  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(rdx, rdi, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ movp(r8, FieldOperand(rdx, JSObject::kElementsOffset));
  __ CompareRoot(r8, Heap::kEmptyFixedArrayRootIndex);
  __ j(equal, &only_change_map);

  __ Push(rsi);
  __ Push(rax);

  __ movp(r8, FieldOperand(rdx, JSObject::kElementsOffset));
  __ SmiToInteger32(r9, FieldOperand(r8, FixedDoubleArray::kLengthOffset));
  // r8 : source FixedDoubleArray
  // r9 : number of elements
  __ leap(rdi, Operand(r9, times_pointer_size, FixedArray::kHeaderSize));
  __ Allocate(rdi, r11, r14, r15, &gc_required, NO_ALLOCATION_FLAGS);
  // r11: destination FixedArray
  __ LoadRoot(rdi, Heap::kFixedArrayMapRootIndex);
  __ movp(FieldOperand(r11, HeapObject::kMapOffset), rdi);
  __ Integer32ToSmi(r14, r9);
  __ movp(FieldOperand(r11, FixedArray::kLengthOffset), r14);

  // Prepare for conversion loop.
  __ movq(rsi, bit_cast<int64_t, uint64_t>(kHoleNanInt64));
  __ LoadRoot(rdi, Heap::kTheHoleValueRootIndex);
  // rsi: the-hole NaN
  // rdi: pointer to the-hole

  // Allocating heap numbers in the loop below can fail and cause a jump to
  // gc_required. We can't leave a partly initialized FixedArray behind,
  // so pessimistically fill it with holes now.
  Label initialization_loop, initialization_loop_entry;
  __ jmp(&initialization_loop_entry, Label::kNear);
  __ bind(&initialization_loop);
  __ movp(FieldOperand(r11, r9, times_pointer_size, FixedArray::kHeaderSize),
          rdi);
  __ bind(&initialization_loop_entry);
  __ decp(r9);
  __ j(not_sign, &initialization_loop);

  __ SmiToInteger32(r9, FieldOperand(r8, FixedDoubleArray::kLengthOffset));
  __ jmp(&entry);

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  __ Pop(rax);
  __ Pop(rsi);
  __ jmp(fail);

  // Box doubles into heap numbers.
  __ bind(&loop);
  __ movq(r14, FieldOperand(r8,
                            r9,
                            times_8,
                            FixedDoubleArray::kHeaderSize));
  // r9 : current element's index
  // r14: current element
  __ cmpq(r14, rsi);
  __ j(equal, &convert_hole);

  // Non-hole double, copy value into a heap number.
  __ AllocateHeapNumber(rax, r15, &gc_required);
  // rax: new heap number
  __ movq(FieldOperand(rax, HeapNumber::kValueOffset), r14);
  __ movp(FieldOperand(r11,
                       r9,
                       times_pointer_size,
                       FixedArray::kHeaderSize),
          rax);
  __ movp(r15, r9);
  __ RecordWriteArray(r11,
                      rax,
                      r15,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ jmp(&entry, Label::kNear);

  // Replace the-hole NaN with the-hole pointer.
  __ bind(&convert_hole);
  __ movp(FieldOperand(r11,
                       r9,
                       times_pointer_size,
                       FixedArray::kHeaderSize),
          rdi);

  __ bind(&entry);
  __ decp(r9);
  __ j(not_sign, &loop);

  // Replace receiver's backing store with newly created and filled FixedArray.
  __ movp(FieldOperand(rdx, JSObject::kElementsOffset), r11);
  __ RecordWriteField(rdx,
                      JSObject::kElementsOffset,
                      r11,
                      r15,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ Pop(rax);
  __ Pop(rsi);

  __ bind(&only_change_map);
  // Set transitioned map.
  __ movp(FieldOperand(rdx, HeapObject::kMapOffset), rbx);
  __ RecordWriteField(rdx,
                      HeapObject::kMapOffset,
                      rbx,
                      rdi,
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
  __ movp(result, FieldOperand(string, HeapObject::kMapOffset));
  __ movzxbl(result, FieldOperand(result, Map::kInstanceTypeOffset));

  // We need special handling for indirect strings.
  Label check_sequential;
  __ testb(result, Immediate(kIsIndirectStringMask));
  __ j(zero, &check_sequential, Label::kNear);

  // Dispatch on the indirect string shape: slice or cons.
  Label cons_string;
  __ testb(result, Immediate(kSlicedNotConsMask));
  __ j(zero, &cons_string, Label::kNear);

  // Handle slices.
  Label indirect_string_loaded;
  __ SmiToInteger32(result, FieldOperand(string, SlicedString::kOffsetOffset));
  __ addp(index, result);
  __ movp(string, FieldOperand(string, SlicedString::kParentOffset));
  __ jmp(&indirect_string_loaded, Label::kNear);

  // Handle cons strings.
  // Check whether the right hand side is the empty string (i.e. if
  // this is really a flat string in a cons string). If that is not
  // the case we would rather go to the runtime system now to flatten
  // the string.
  __ bind(&cons_string);
  __ CompareRoot(FieldOperand(string, ConsString::kSecondOffset),
                 Heap::kempty_stringRootIndex);
  __ j(not_equal, call_runtime);
  __ movp(string, FieldOperand(string, ConsString::kFirstOffset));

  __ bind(&indirect_string_loaded);
  __ movp(result, FieldOperand(string, HeapObject::kMapOffset));
  __ movzxbl(result, FieldOperand(result, Map::kInstanceTypeOffset));

  // Distinguish sequential and external strings. Only these two string
  // representations can reach here (slices and flat cons strings have been
  // reduced to the underlying sequential or external string).
  Label seq_string;
  __ bind(&check_sequential);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ testb(result, Immediate(kStringRepresentationMask));
  __ j(zero, &seq_string, Label::kNear);

  // Handle external strings.
  Label one_byte_external, done;
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ testb(result, Immediate(kIsIndirectStringMask));
    __ Assert(zero, kExternalStringExpectedButNotFound);
  }
  // Rule out short external strings.
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ testb(result, Immediate(kShortExternalStringTag));
  __ j(not_zero, call_runtime);
  // Check encoding.
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ testb(result, Immediate(kStringEncodingMask));
  __ movp(result, FieldOperand(string, ExternalString::kResourceDataOffset));
  __ j(not_equal, &one_byte_external, Label::kNear);
  // Two-byte string.
  __ movzxwl(result, Operand(result, index, times_2, 0));
  __ jmp(&done, Label::kNear);
  __ bind(&one_byte_external);
  // One-byte string.
  __ movzxbl(result, Operand(result, index, times_1, 0));
  __ jmp(&done, Label::kNear);

  // Dispatch on the encoding: one-byte or two-byte.
  Label one_byte;
  __ bind(&seq_string);
  STATIC_ASSERT((kStringEncodingMask & kOneByteStringTag) != 0);
  STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);
  __ testb(result, Immediate(kStringEncodingMask));
  __ j(not_zero, &one_byte, Label::kNear);

  // Two-byte string.
  // Load the two-byte character code into the result register.
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
  __ movzxwl(result, FieldOperand(string,
                                  index,
                                  times_2,
                                  SeqTwoByteString::kHeaderSize));
  __ jmp(&done, Label::kNear);

  // One-byte string.
  // Load the byte into the result register.
  __ bind(&one_byte);
  __ movzxbl(result, FieldOperand(string,
                                  index,
                                  times_1,
                                  SeqOneByteString::kHeaderSize));
  __ bind(&done);
}

#undef __


CodeAgingHelper::CodeAgingHelper(Isolate* isolate) {
  USE(isolate);
  DCHECK(young_sequence_.length() == kNoCodeAgeSequenceLength);
  // The sequence of instructions that is patched out for aging code is the
  // following boilerplate stack-building prologue that is found both in
  // FUNCTION and OPTIMIZED_FUNCTION code:
  CodePatcher patcher(isolate, young_sequence_.start(),
                      young_sequence_.length());
  patcher.masm()->pushq(rbp);
  patcher.masm()->movp(rbp, rsp);
  patcher.masm()->Push(rsi);
  patcher.masm()->Push(rdi);
}


#ifdef DEBUG
bool CodeAgingHelper::IsOld(byte* candidate) const {
  return *candidate == kCallOpcode;
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
    sequence++;  // Skip the kCallOpcode byte
    Address target_address = sequence + *reinterpret_cast<int*>(sequence) +
        Assembler::kCallTargetAddressOffset;
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
    CodePatcher patcher(isolate, sequence, young_length);
    patcher.masm()->call(stub->instruction_start());
    patcher.masm()->Nop(
        kNoCodeAgeSequenceLength - Assembler::kShortCallInstructionLength);
  }
}


Operand StackArgumentsAccessor::GetArgumentOperand(int index) {
  DCHECK(index >= 0);
  int receiver = (receiver_mode_ == ARGUMENTS_CONTAIN_RECEIVER) ? 1 : 0;
  int displacement_to_last_argument = base_reg_.is(rsp) ?
      kPCOnStackSize : kFPOnStackSize + kPCOnStackSize;
  displacement_to_last_argument += extra_displacement_to_last_argument_;
  if (argument_count_reg_.is(no_reg)) {
    // argument[0] is at base_reg_ + displacement_to_last_argument +
    // (argument_count_immediate_ + receiver - 1) * kPointerSize.
    DCHECK(argument_count_immediate_ + receiver > 0);
    return Operand(base_reg_, displacement_to_last_argument +
        (argument_count_immediate_ + receiver - 1 - index) * kPointerSize);
  } else {
    // argument[0] is at base_reg_ + displacement_to_last_argument +
    // argument_count_reg_ * times_pointer_size + (receiver - 1) * kPointerSize.
    return Operand(base_reg_, argument_count_reg_, times_pointer_size,
        displacement_to_last_argument + (receiver - 1 - index) * kPointerSize);
  }
}


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64
