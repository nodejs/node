// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_X87

#include "src/codegen.h"
#include "src/heap/heap.h"
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


UnaryMathFunction CreateExpFunction() {
  // No SSE2 support
  return &std::exp;
}


UnaryMathFunction CreateSqrtFunction() {
  // No SSE2 support
  return &std::sqrt;
}


// Helper functions for CreateMemMoveFunction.
#undef __
#define __ ACCESS_MASM(masm)

enum Direction { FORWARD, BACKWARD };
enum Alignment { MOVE_ALIGNED, MOVE_UNALIGNED };


void MemMoveEmitPopAndReturn(MacroAssembler* masm) {
  __ pop(esi);
  __ pop(edi);
  __ ret(0);
}


#undef __
#define __ masm.


class LabelConverter {
 public:
  explicit LabelConverter(byte* buffer) : buffer_(buffer) {}
  int32_t address(Label* l) const {
    return reinterpret_cast<int32_t>(buffer_) + l->pos();
  }
 private:
  byte* buffer_;
};


MemMoveFunction CreateMemMoveFunction() {
  size_t actual_size;
  // Allocate buffer in executable space.
  byte* buffer =
      static_cast<byte*>(base::OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == NULL) return NULL;
  MacroAssembler masm(NULL, buffer, static_cast<int>(actual_size));
  LabelConverter conv(buffer);

  // Generated code is put into a fixed, unmovable buffer, and not into
  // the V8 heap. We can't, and don't, refer to any relocatable addresses
  // (e.g. the JavaScript nan-object).

  // 32-bit C declaration function calls pass arguments on stack.

  // Stack layout:
  // esp[12]: Third argument, size.
  // esp[8]: Second argument, source pointer.
  // esp[4]: First argument, destination pointer.
  // esp[0]: return address

  const int kDestinationOffset = 1 * kPointerSize;
  const int kSourceOffset = 2 * kPointerSize;
  const int kSizeOffset = 3 * kPointerSize;

  int stack_offset = 0;  // Update if we change the stack height.

  Label backward, backward_much_overlap;
  Label forward_much_overlap, small_size, medium_size, pop_and_return;
  __ push(edi);
  __ push(esi);
  stack_offset += 2 * kPointerSize;
  Register dst = edi;
  Register src = esi;
  Register count = ecx;
  __ mov(dst, Operand(esp, stack_offset + kDestinationOffset));
  __ mov(src, Operand(esp, stack_offset + kSourceOffset));
  __ mov(count, Operand(esp, stack_offset + kSizeOffset));

  __ cmp(dst, src);
  __ j(equal, &pop_and_return);

  // No SSE2.
  Label forward;
  __ cmp(count, 0);
  __ j(equal, &pop_and_return);
  __ cmp(dst, src);
  __ j(above, &backward);
  __ jmp(&forward);
  {
    // Simple forward copier.
    Label forward_loop_1byte, forward_loop_4byte;
    __ bind(&forward_loop_4byte);
    __ mov(eax, Operand(src, 0));
    __ sub(count, Immediate(4));
    __ add(src, Immediate(4));
    __ mov(Operand(dst, 0), eax);
    __ add(dst, Immediate(4));
    __ bind(&forward);  // Entry point.
    __ cmp(count, 3);
    __ j(above, &forward_loop_4byte);
    __ bind(&forward_loop_1byte);
    __ cmp(count, 0);
    __ j(below_equal, &pop_and_return);
    __ mov_b(eax, Operand(src, 0));
    __ dec(count);
    __ inc(src);
    __ mov_b(Operand(dst, 0), eax);
    __ inc(dst);
    __ jmp(&forward_loop_1byte);
  }
  {
    // Simple backward copier.
    Label backward_loop_1byte, backward_loop_4byte, entry_shortcut;
    __ bind(&backward);
    __ add(src, count);
    __ add(dst, count);
    __ cmp(count, 3);
    __ j(below_equal, &entry_shortcut);

    __ bind(&backward_loop_4byte);
    __ sub(src, Immediate(4));
    __ sub(count, Immediate(4));
    __ mov(eax, Operand(src, 0));
    __ sub(dst, Immediate(4));
    __ mov(Operand(dst, 0), eax);
    __ cmp(count, 3);
    __ j(above, &backward_loop_4byte);
    __ bind(&backward_loop_1byte);
    __ cmp(count, 0);
    __ j(below_equal, &pop_and_return);
    __ bind(&entry_shortcut);
    __ dec(src);
    __ dec(count);
    __ mov_b(eax, Operand(src, 0));
    __ dec(dst);
    __ mov_b(Operand(dst, 0), eax);
    __ jmp(&backward_loop_1byte);
  }

  __ bind(&pop_and_return);
  MemMoveEmitPopAndReturn(&masm);

  CodeDesc desc;
  masm.GetCode(&desc);
  DCHECK(!RelocInfo::RequiresRelocation(desc));
  CpuFeatures::FlushICache(buffer, actual_size);
  base::OS::ProtectCode(buffer, actual_size);
  // TODO(jkummerow): It would be nice to register this code creation event
  // with the PROFILE / GDBJIT system.
  return FUNCTION_CAST<MemMoveFunction>(buffer);
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
  Register scratch = edi;
  DCHECK(!AreAliased(receiver, key, value, target_map, scratch));

  if (mode == TRACK_ALLOCATION_SITE) {
    DCHECK(allocation_memento_found != NULL);
    __ JumpIfJSArrayHasAllocationMemento(
        receiver, scratch, allocation_memento_found);
  }

  // Set transitioned map.
  __ mov(FieldOperand(receiver, HeapObject::kMapOffset), target_map);
  __ RecordWriteField(receiver,
                      HeapObject::kMapOffset,
                      target_map,
                      scratch,
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
  DCHECK(receiver.is(edx));
  DCHECK(key.is(ecx));
  DCHECK(value.is(eax));
  DCHECK(target_map.is(ebx));

  Label loop, entry, convert_hole, gc_required, only_change_map;

  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(edx, edi, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ mov(edi, FieldOperand(edx, JSObject::kElementsOffset));
  __ cmp(edi, Immediate(masm->isolate()->factory()->empty_fixed_array()));
  __ j(equal, &only_change_map);

  __ push(eax);
  __ push(ebx);

  __ mov(edi, FieldOperand(edi, FixedArray::kLengthOffset));

  // Allocate new FixedDoubleArray.
  // edx: receiver
  // edi: length of source FixedArray (smi-tagged)
  AllocationFlags flags =
      static_cast<AllocationFlags>(TAG_OBJECT | DOUBLE_ALIGNMENT);
  __ Allocate(FixedDoubleArray::kHeaderSize, times_8, edi,
              REGISTER_VALUE_IS_SMI, eax, ebx, no_reg, &gc_required, flags);

  // eax: destination FixedDoubleArray
  // edi: number of elements
  // edx: receiver
  __ mov(FieldOperand(eax, HeapObject::kMapOffset),
         Immediate(masm->isolate()->factory()->fixed_double_array_map()));
  __ mov(FieldOperand(eax, FixedDoubleArray::kLengthOffset), edi);
  __ mov(esi, FieldOperand(edx, JSObject::kElementsOffset));
  // Replace receiver's backing store with newly created FixedDoubleArray.
  __ mov(FieldOperand(edx, JSObject::kElementsOffset), eax);
  __ mov(ebx, eax);
  __ RecordWriteField(edx,
                      JSObject::kElementsOffset,
                      ebx,
                      edi,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);

  __ mov(edi, FieldOperand(esi, FixedArray::kLengthOffset));

  // Prepare for conversion loop.
  ExternalReference canonical_the_hole_nan_reference =
      ExternalReference::address_of_the_hole_nan();
  __ jmp(&entry);

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  // Restore registers before jumping into runtime.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  __ pop(ebx);
  __ pop(eax);
  __ jmp(fail);

  // Convert and copy elements
  // esi: source FixedArray
  __ bind(&loop);
  __ mov(ebx, FieldOperand(esi, edi, times_2, FixedArray::kHeaderSize));
  // ebx: current element from source
  // edi: index of current element
  __ JumpIfNotSmi(ebx, &convert_hole);

  // Normal smi, convert it to double and store.
  __ SmiUntag(ebx);
  __ push(ebx);
  __ fild_s(Operand(esp, 0));
  __ pop(ebx);
  __ fstp_d(FieldOperand(eax, edi, times_4, FixedDoubleArray::kHeaderSize));
  __ jmp(&entry);

  // Found hole, store hole_nan_as_double instead.
  __ bind(&convert_hole);

  if (FLAG_debug_code) {
    __ cmp(ebx, masm->isolate()->factory()->the_hole_value());
    __ Assert(equal, kObjectFoundInSmiOnlyArray);
  }

  __ fld_d(Operand::StaticVariable(canonical_the_hole_nan_reference));
  __ fstp_d(FieldOperand(eax, edi, times_4, FixedDoubleArray::kHeaderSize));

  __ bind(&entry);
  __ sub(edi, Immediate(Smi::FromInt(1)));
  __ j(not_sign, &loop);

  __ pop(ebx);
  __ pop(eax);

  // Restore esi.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));

  __ bind(&only_change_map);
  // eax: value
  // ebx: target map
  // Set transitioned map.
  __ mov(FieldOperand(edx, HeapObject::kMapOffset), ebx);
  __ RecordWriteField(edx,
                      HeapObject::kMapOffset,
                      ebx,
                      edi,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
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
  DCHECK(receiver.is(edx));
  DCHECK(key.is(ecx));
  DCHECK(value.is(eax));
  DCHECK(target_map.is(ebx));

  Label loop, entry, convert_hole, gc_required, only_change_map, success;

  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(edx, edi, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ mov(edi, FieldOperand(edx, JSObject::kElementsOffset));
  __ cmp(edi, Immediate(masm->isolate()->factory()->empty_fixed_array()));
  __ j(equal, &only_change_map);

  __ push(eax);
  __ push(edx);
  __ push(ebx);

  __ mov(ebx, FieldOperand(edi, FixedDoubleArray::kLengthOffset));

  // Allocate new FixedArray.
  // ebx: length of source FixedDoubleArray (smi-tagged)
  __ lea(edi, Operand(ebx, times_2, FixedArray::kHeaderSize));
  __ Allocate(edi, eax, esi, no_reg, &gc_required, TAG_OBJECT);

  // eax: destination FixedArray
  // ebx: number of elements
  __ mov(FieldOperand(eax, HeapObject::kMapOffset),
         Immediate(masm->isolate()->factory()->fixed_array_map()));
  __ mov(FieldOperand(eax, FixedArray::kLengthOffset), ebx);
  __ mov(edi, FieldOperand(edx, JSObject::kElementsOffset));

  __ jmp(&entry);

  // ebx: target map
  // edx: receiver
  // Set transitioned map.
  __ bind(&only_change_map);
  __ mov(FieldOperand(edx, HeapObject::kMapOffset), ebx);
  __ RecordWriteField(edx,
                      HeapObject::kMapOffset,
                      ebx,
                      edi,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ jmp(&success);

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
  __ pop(ebx);
  __ pop(edx);
  __ pop(eax);
  __ jmp(fail);

  // Box doubles into heap numbers.
  // edi: source FixedDoubleArray
  // eax: destination FixedArray
  __ bind(&loop);
  // ebx: index of current element (smi-tagged)
  uint32_t offset = FixedDoubleArray::kHeaderSize + sizeof(kHoleNanLower32);
  __ cmp(FieldOperand(edi, ebx, times_4, offset), Immediate(kHoleNanUpper32));
  __ j(equal, &convert_hole);

  // Non-hole double, copy value into a heap number.
  __ AllocateHeapNumber(edx, esi, no_reg, &gc_required);
  // edx: new heap number
  __ mov(esi, FieldOperand(edi, ebx, times_4, FixedDoubleArray::kHeaderSize));
  __ mov(FieldOperand(edx, HeapNumber::kValueOffset), esi);
  __ mov(esi, FieldOperand(edi, ebx, times_4, offset));
  __ mov(FieldOperand(edx, HeapNumber::kValueOffset + kPointerSize), esi);
  __ mov(FieldOperand(eax, ebx, times_2, FixedArray::kHeaderSize), edx);
  __ mov(esi, ebx);
  __ RecordWriteArray(eax,
                      edx,
                      esi,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ jmp(&entry, Label::kNear);

  // Replace the-hole NaN with the-hole pointer.
  __ bind(&convert_hole);
  __ mov(FieldOperand(eax, ebx, times_2, FixedArray::kHeaderSize),
         masm->isolate()->factory()->the_hole_value());

  __ bind(&entry);
  __ sub(ebx, Immediate(Smi::FromInt(1)));
  __ j(not_sign, &loop);

  __ pop(ebx);
  __ pop(edx);
  // ebx: target map
  // edx: receiver
  // Set transitioned map.
  __ mov(FieldOperand(edx, HeapObject::kMapOffset), ebx);
  __ RecordWriteField(edx,
                      HeapObject::kMapOffset,
                      ebx,
                      edi,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  // Replace receiver's backing store with newly created and filled FixedArray.
  __ mov(FieldOperand(edx, JSObject::kElementsOffset), eax);
  __ RecordWriteField(edx,
                      JSObject::kElementsOffset,
                      eax,
                      edi,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);

  // Restore registers.
  __ pop(eax);
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));

  __ bind(&success);
}


void StringCharLoadGenerator::Generate(MacroAssembler* masm,
                                       Factory* factory,
                                       Register string,
                                       Register index,
                                       Register result,
                                       Label* call_runtime) {
  // Fetch the instance type of the receiver into result register.
  __ mov(result, FieldOperand(string, HeapObject::kMapOffset));
  __ movzx_b(result, FieldOperand(result, Map::kInstanceTypeOffset));

  // We need special handling for indirect strings.
  Label check_sequential;
  __ test(result, Immediate(kIsIndirectStringMask));
  __ j(zero, &check_sequential, Label::kNear);

  // Dispatch on the indirect string shape: slice or cons.
  Label cons_string;
  __ test(result, Immediate(kSlicedNotConsMask));
  __ j(zero, &cons_string, Label::kNear);

  // Handle slices.
  Label indirect_string_loaded;
  __ mov(result, FieldOperand(string, SlicedString::kOffsetOffset));
  __ SmiUntag(result);
  __ add(index, result);
  __ mov(string, FieldOperand(string, SlicedString::kParentOffset));
  __ jmp(&indirect_string_loaded, Label::kNear);

  // Handle cons strings.
  // Check whether the right hand side is the empty string (i.e. if
  // this is really a flat string in a cons string). If that is not
  // the case we would rather go to the runtime system now to flatten
  // the string.
  __ bind(&cons_string);
  __ cmp(FieldOperand(string, ConsString::kSecondOffset),
         Immediate(factory->empty_string()));
  __ j(not_equal, call_runtime);
  __ mov(string, FieldOperand(string, ConsString::kFirstOffset));

  __ bind(&indirect_string_loaded);
  __ mov(result, FieldOperand(string, HeapObject::kMapOffset));
  __ movzx_b(result, FieldOperand(result, Map::kInstanceTypeOffset));

  // Distinguish sequential and external strings. Only these two string
  // representations can reach here (slices and flat cons strings have been
  // reduced to the underlying sequential or external string).
  Label seq_string;
  __ bind(&check_sequential);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ test(result, Immediate(kStringRepresentationMask));
  __ j(zero, &seq_string, Label::kNear);

  // Handle external strings.
  Label ascii_external, done;
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ test(result, Immediate(kIsIndirectStringMask));
    __ Assert(zero, kExternalStringExpectedButNotFound);
  }
  // Rule out short external strings.
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ test_b(result, kShortExternalStringMask);
  __ j(not_zero, call_runtime);
  // Check encoding.
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ test_b(result, kStringEncodingMask);
  __ mov(result, FieldOperand(string, ExternalString::kResourceDataOffset));
  __ j(not_equal, &ascii_external, Label::kNear);
  // Two-byte string.
  __ movzx_w(result, Operand(result, index, times_2, 0));
  __ jmp(&done, Label::kNear);
  __ bind(&ascii_external);
  // Ascii string.
  __ movzx_b(result, Operand(result, index, times_1, 0));
  __ jmp(&done, Label::kNear);

  // Dispatch on the encoding: ASCII or two-byte.
  Label ascii;
  __ bind(&seq_string);
  STATIC_ASSERT((kStringEncodingMask & kOneByteStringTag) != 0);
  STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);
  __ test(result, Immediate(kStringEncodingMask));
  __ j(not_zero, &ascii, Label::kNear);

  // Two-byte string.
  // Load the two-byte character code into the result register.
  __ movzx_w(result, FieldOperand(string,
                                  index,
                                  times_2,
                                  SeqTwoByteString::kHeaderSize));
  __ jmp(&done, Label::kNear);

  // Ascii string.
  // Load the byte into the result register.
  __ bind(&ascii);
  __ movzx_b(result, FieldOperand(string,
                                  index,
                                  times_1,
                                  SeqOneByteString::kHeaderSize));
  __ bind(&done);
}


#undef __


CodeAgingHelper::CodeAgingHelper() {
  DCHECK(young_sequence_.length() == kNoCodeAgeSequenceLength);
  CodePatcher patcher(young_sequence_.start(), young_sequence_.length());
  patcher.masm()->push(ebp);
  patcher.masm()->mov(ebp, esp);
  patcher.masm()->push(esi);
  patcher.masm()->push(edi);
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
    CpuFeatures::FlushICache(sequence, young_length);
  } else {
    Code* stub = GetCodeAgeStub(isolate, age, parity);
    CodePatcher patcher(sequence, young_length);
    patcher.masm()->call(stub->instruction_start(), RelocInfo::NONE32);
  }
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X87
