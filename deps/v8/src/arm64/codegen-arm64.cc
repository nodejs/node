// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arm64/codegen-arm64.h"

#if V8_TARGET_ARCH_ARM64

#include "src/arm64/simulator-arm64.h"
#include "src/codegen.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

UnaryMathFunctionWithIsolate CreateSqrtFunction(Isolate* isolate) {
  return nullptr;
}


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

void ElementsTransitionGenerator::GenerateMapChangeElementsTransition(
    MacroAssembler* masm,
    Register receiver,
    Register key,
    Register value,
    Register target_map,
    AllocationSiteMode mode,
    Label* allocation_memento_found) {
  ASM_LOCATION(
      "ElementsTransitionGenerator::GenerateMapChangeElementsTransition");
  DCHECK(!AreAliased(receiver, key, value, target_map));

  if (mode == TRACK_ALLOCATION_SITE) {
    DCHECK(allocation_memento_found != NULL);
    __ JumpIfJSArrayHasAllocationMemento(receiver, x10, x11,
                                         allocation_memento_found);
  }

  // Set transitioned map.
  __ Str(target_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ RecordWriteField(receiver,
                      HeapObject::kMapOffset,
                      target_map,
                      x10,
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
  ASM_LOCATION("ElementsTransitionGenerator::GenerateSmiToDouble");
  Label gc_required, only_change_map;
  Register elements = x4;
  Register length = x5;
  Register array_size = x6;
  Register array = x7;

  Register scratch = x6;

  // Verify input registers don't conflict with locals.
  DCHECK(!AreAliased(receiver, key, value, target_map,
                     elements, length, array_size, array));

  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(receiver, x10, x11, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ Ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ JumpIfRoot(elements, Heap::kEmptyFixedArrayRootIndex, &only_change_map);

  __ Push(lr);
  __ Ldrsw(length, UntagSmiFieldMemOperand(elements,
                                           FixedArray::kLengthOffset));

  // Allocate new FixedDoubleArray.
  __ Lsl(array_size, length, kDoubleSizeLog2);
  __ Add(array_size, array_size, FixedDoubleArray::kHeaderSize);
  __ Allocate(array_size, array, x10, x11, &gc_required, DOUBLE_ALIGNMENT);
  // Register array is non-tagged heap object.

  // Set the destination FixedDoubleArray's length and map.
  Register map_root = array_size;
  __ LoadRoot(map_root, Heap::kFixedDoubleArrayMapRootIndex);
  __ SmiTag(x11, length);
  __ Str(x11, FieldMemOperand(array, FixedDoubleArray::kLengthOffset));
  __ Str(map_root, FieldMemOperand(array, HeapObject::kMapOffset));

  __ Str(target_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ RecordWriteField(receiver, HeapObject::kMapOffset, target_map, scratch,
                      kLRHasBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);

  // Replace receiver's backing store with newly created FixedDoubleArray.
  __ Move(x10, array);
  __ Str(array, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ RecordWriteField(receiver, JSObject::kElementsOffset, x10, scratch,
                      kLRHasBeenSaved, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);

  // Prepare for conversion loop.
  Register src_elements = x10;
  Register dst_elements = x11;
  Register dst_end = x12;
  __ Add(src_elements, elements, FixedArray::kHeaderSize - kHeapObjectTag);
  __ Add(dst_elements, array, FixedDoubleArray::kHeaderSize - kHeapObjectTag);
  __ Add(dst_end, dst_elements, Operand(length, LSL, kDoubleSizeLog2));

  FPRegister nan_d = d1;
  __ Fmov(nan_d, rawbits_to_double(kHoleNanInt64));

  Label entry, done;
  __ B(&entry);

  __ Bind(&only_change_map);
  __ Str(target_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ RecordWriteField(receiver, HeapObject::kMapOffset, target_map, scratch,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ B(&done);

  // Call into runtime if GC is required.
  __ Bind(&gc_required);
  __ Pop(lr);
  __ B(fail);

  // Iterate over the array, copying and coverting smis to doubles. If an
  // element is non-smi, write a hole to the destination.
  {
    Label loop;
    __ Bind(&loop);
    __ Ldr(x13, MemOperand(src_elements, kPointerSize, PostIndex));
    __ SmiUntagToDouble(d0, x13, kSpeculativeUntag);
    __ Tst(x13, kSmiTagMask);
    __ Fcsel(d0, d0, nan_d, eq);
    __ Str(d0, MemOperand(dst_elements, kDoubleSize, PostIndex));

    __ Bind(&entry);
    __ Cmp(dst_elements, dst_end);
    __ B(lt, &loop);
  }

  __ Pop(lr);
  __ Bind(&done);
}


void ElementsTransitionGenerator::GenerateDoubleToObject(
    MacroAssembler* masm,
    Register receiver,
    Register key,
    Register value,
    Register target_map,
    AllocationSiteMode mode,
    Label* fail) {
  ASM_LOCATION("ElementsTransitionGenerator::GenerateDoubleToObject");
  Register elements = x4;
  Register array_size = x6;
  Register array = x7;
  Register length = x5;

  // Verify input registers don't conflict with locals.
  DCHECK(!AreAliased(receiver, key, value, target_map,
                     elements, array_size, array, length));

  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(receiver, x10, x11, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  Label only_change_map;

  __ Ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ JumpIfRoot(elements, Heap::kEmptyFixedArrayRootIndex, &only_change_map);

  __ Push(lr);
  // TODO(all): These registers may not need to be pushed. Examine
  // RecordWriteStub and check whether it's needed.
  __ Push(target_map, receiver, key, value);
  __ Ldrsw(length, UntagSmiFieldMemOperand(elements,
                                           FixedArray::kLengthOffset));
  // Allocate new FixedArray.
  Label gc_required;
  __ Mov(array_size, FixedDoubleArray::kHeaderSize);
  __ Add(array_size, array_size, Operand(length, LSL, kPointerSizeLog2));
  __ Allocate(array_size, array, x10, x11, &gc_required, NO_ALLOCATION_FLAGS);

  // Set destination FixedDoubleArray's length and map.
  Register map_root = array_size;
  __ LoadRoot(map_root, Heap::kFixedArrayMapRootIndex);
  __ SmiTag(x11, length);
  __ Str(x11, FieldMemOperand(array, FixedDoubleArray::kLengthOffset));
  __ Str(map_root, FieldMemOperand(array, HeapObject::kMapOffset));

  // Prepare for conversion loop.
  Register src_elements = x10;
  Register dst_elements = x11;
  Register dst_end = x12;
  Register the_hole = x14;
  __ LoadRoot(the_hole, Heap::kTheHoleValueRootIndex);
  __ Add(src_elements, elements,
         FixedDoubleArray::kHeaderSize - kHeapObjectTag);
  __ Add(dst_elements, array, FixedArray::kHeaderSize - kHeapObjectTag);
  __ Add(dst_end, dst_elements, Operand(length, LSL, kPointerSizeLog2));

  // Allocating heap numbers in the loop below can fail and cause a jump to
  // gc_required. We can't leave a partly initialized FixedArray behind,
  // so pessimistically fill it with holes now.
  Label initialization_loop, initialization_loop_entry;
  __ B(&initialization_loop_entry);
  __ bind(&initialization_loop);
  __ Str(the_hole, MemOperand(dst_elements, kPointerSize, PostIndex));
  __ bind(&initialization_loop_entry);
  __ Cmp(dst_elements, dst_end);
  __ B(lt, &initialization_loop);

  __ Add(dst_elements, array, FixedArray::kHeaderSize - kHeapObjectTag);

  Register heap_num_map = x15;
  __ LoadRoot(heap_num_map, Heap::kHeapNumberMapRootIndex);

  Label entry;
  __ B(&entry);

  // Call into runtime if GC is required.
  __ Bind(&gc_required);
  __ Pop(value, key, receiver, target_map);
  __ Pop(lr);
  __ B(fail);

  {
    Label loop, convert_hole;
    __ Bind(&loop);
    __ Ldr(x13, MemOperand(src_elements, kPointerSize, PostIndex));
    __ Cmp(x13, kHoleNanInt64);
    __ B(eq, &convert_hole);

    // Non-hole double, copy value into a heap number.
    Register heap_num = length;
    Register scratch = array_size;
    Register scratch2 = elements;
    __ AllocateHeapNumber(heap_num, &gc_required, scratch, scratch2,
                          x13, heap_num_map);
    __ Mov(x13, dst_elements);
    __ Str(heap_num, MemOperand(dst_elements, kPointerSize, PostIndex));
    __ RecordWrite(array, x13, heap_num, kLRHasBeenSaved, kDontSaveFPRegs,
                   EMIT_REMEMBERED_SET, OMIT_SMI_CHECK);

    __ B(&entry);

    // Replace the-hole NaN with the-hole pointer.
    __ Bind(&convert_hole);
    __ Str(the_hole, MemOperand(dst_elements, kPointerSize, PostIndex));

    __ Bind(&entry);
    __ Cmp(dst_elements, dst_end);
    __ B(lt, &loop);
  }

  __ Pop(value, key, receiver, target_map);
  // Replace receiver's backing store with newly created and filled FixedArray.
  __ Str(array, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ RecordWriteField(receiver, JSObject::kElementsOffset, array, x13,
                      kLRHasBeenSaved, kDontSaveFPRegs, EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ Pop(lr);

  __ Bind(&only_change_map);
  __ Str(target_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ RecordWriteField(receiver, HeapObject::kMapOffset, target_map, x13,
                      kLRHasNotBeenSaved, kDontSaveFPRegs, OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}


CodeAgingHelper::CodeAgingHelper(Isolate* isolate) {
  USE(isolate);
  DCHECK(young_sequence_.length() == kNoCodeAgeSequenceLength);
  // The sequence of instructions that is patched out for aging code is the
  // following boilerplate stack-building prologue that is found both in
  // FUNCTION and OPTIMIZED_FUNCTION code:
  PatchingAssembler patcher(isolate, young_sequence_.start(),
                            young_sequence_.length() / kInstructionSize);
  // The young sequence is the frame setup code for FUNCTION code types. It is
  // generated by FullCodeGenerator::Generate.
  MacroAssembler::EmitFrameSetupForCodeAgePatching(&patcher);

#ifdef DEBUG
  const int length = kCodeAgeStubEntryOffset / kInstructionSize;
  DCHECK(old_sequence_.length() >= kCodeAgeStubEntryOffset);
  PatchingAssembler patcher_old(isolate, old_sequence_.start(), length);
  MacroAssembler::EmitCodeAgeSequence(&patcher_old, NULL);
#endif
}


#ifdef DEBUG
bool CodeAgingHelper::IsOld(byte* candidate) const {
  return memcmp(candidate, old_sequence_.start(), kCodeAgeStubEntryOffset) == 0;
}
#endif


bool Code::IsYoungSequence(Isolate* isolate, byte* sequence) {
  return MacroAssembler::IsYoungSequence(isolate, sequence);
}


void Code::GetCodeAgeAndParity(Isolate* isolate, byte* sequence, Age* age,
                               MarkingParity* parity) {
  if (IsYoungSequence(isolate, sequence)) {
    *age = kNoAgeCodeAge;
    *parity = NO_MARKING_PARITY;
  } else {
    byte* target = sequence + kCodeAgeStubEntryOffset;
    Code* stub = GetCodeFromTargetAddress(Memory::Address_at(target));
    GetCodeAgeAndParity(stub, age, parity);
  }
}


void Code::PatchPlatformCodeAge(Isolate* isolate,
                                byte* sequence,
                                Code::Age age,
                                MarkingParity parity) {
  PatchingAssembler patcher(isolate, sequence,
                            kNoCodeAgeSequenceLength / kInstructionSize);
  if (age == kNoAgeCodeAge) {
    MacroAssembler::EmitFrameSetupForCodeAgePatching(&patcher);
  } else {
    Code * stub = GetCodeAgeStub(isolate, age, parity);
    MacroAssembler::EmitCodeAgeSequence(&patcher, stub);
  }
}


void StringCharLoadGenerator::Generate(MacroAssembler* masm,
                                       Register string,
                                       Register index,
                                       Register result,
                                       Label* call_runtime) {
  DCHECK(string.Is64Bits() && index.Is32Bits() && result.Is64Bits());
  // Fetch the instance type of the receiver into result register.
  __ Ldr(result, FieldMemOperand(string, HeapObject::kMapOffset));
  __ Ldrb(result, FieldMemOperand(result, Map::kInstanceTypeOffset));

  // We need special handling for indirect strings.
  Label check_sequential;
  __ TestAndBranchIfAllClear(result, kIsIndirectStringMask, &check_sequential);

  // Dispatch on the indirect string shape: slice or cons.
  Label cons_string;
  __ TestAndBranchIfAllClear(result, kSlicedNotConsMask, &cons_string);

  // Handle slices.
  Label indirect_string_loaded;
  __ Ldr(result.W(),
         UntagSmiFieldMemOperand(string, SlicedString::kOffsetOffset));
  __ Ldr(string, FieldMemOperand(string, SlicedString::kParentOffset));
  __ Add(index, index, result.W());
  __ B(&indirect_string_loaded);

  // Handle cons strings.
  // Check whether the right hand side is the empty string (i.e. if
  // this is really a flat string in a cons string). If that is not
  // the case we would rather go to the runtime system now to flatten
  // the string.
  __ Bind(&cons_string);
  __ Ldr(result, FieldMemOperand(string, ConsString::kSecondOffset));
  __ JumpIfNotRoot(result, Heap::kempty_stringRootIndex, call_runtime);
  // Get the first of the two strings and load its instance type.
  __ Ldr(string, FieldMemOperand(string, ConsString::kFirstOffset));

  __ Bind(&indirect_string_loaded);
  __ Ldr(result, FieldMemOperand(string, HeapObject::kMapOffset));
  __ Ldrb(result, FieldMemOperand(result, Map::kInstanceTypeOffset));

  // Distinguish sequential and external strings. Only these two string
  // representations can reach here (slices and flat cons strings have been
  // reduced to the underlying sequential or external string).
  Label external_string, check_encoding;
  __ Bind(&check_sequential);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ TestAndBranchIfAnySet(result, kStringRepresentationMask, &external_string);

  // Prepare sequential strings
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ Add(string, string, SeqTwoByteString::kHeaderSize - kHeapObjectTag);
  __ B(&check_encoding);

  // Handle external strings.
  __ Bind(&external_string);
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ Tst(result, kIsIndirectStringMask);
    __ Assert(eq, kExternalStringExpectedButNotFound);
  }
  // Rule out short external strings.
  STATIC_ASSERT(kShortExternalStringTag != 0);
  // TestAndBranchIfAnySet can emit Tbnz. Do not use it because call_runtime
  // can be bound far away in deferred code.
  __ Tst(result, kShortExternalStringMask);
  __ B(ne, call_runtime);
  __ Ldr(string, FieldMemOperand(string, ExternalString::kResourceDataOffset));

  Label one_byte, done;
  __ Bind(&check_encoding);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ TestAndBranchIfAnySet(result, kStringEncodingMask, &one_byte);
  // Two-byte string.
  __ Ldrh(result, MemOperand(string, index, SXTW, 1));
  __ B(&done);
  __ Bind(&one_byte);
  // One-byte string.
  __ Ldrb(result, MemOperand(string, index, SXTW));
  __ Bind(&done);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64
