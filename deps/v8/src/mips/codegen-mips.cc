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

#if defined(V8_TARGET_ARCH_MIPS)

#include "codegen.h"
#include "macro-assembler.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

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

void ElementsTransitionGenerator::GenerateMapChangeElementsTransition(
    MacroAssembler* masm) {
  // ----------- S t a t e -------------
  //  -- a0    : value
  //  -- a1    : key
  //  -- a2    : receiver
  //  -- ra    : return address
  //  -- a3    : target map, scratch for subsequent call
  //  -- t0    : scratch (elements)
  // -----------------------------------
  // Set transitioned map.
  __ sw(a3, FieldMemOperand(a2, HeapObject::kMapOffset));
  __ RecordWriteField(a2,
                      HeapObject::kMapOffset,
                      a3,
                      t5,
                      kRAHasNotBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}


void ElementsTransitionGenerator::GenerateSmiToDouble(
    MacroAssembler* masm, Label* fail) {
  // ----------- S t a t e -------------
  //  -- a0    : value
  //  -- a1    : key
  //  -- a2    : receiver
  //  -- ra    : return address
  //  -- a3    : target map, scratch for subsequent call
  //  -- t0    : scratch (elements)
  // -----------------------------------
  Label loop, entry, convert_hole, gc_required, only_change_map, done;
  bool fpu_supported = CpuFeatures::IsSupported(FPU);

  Register scratch = t6;

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ lw(t0, FieldMemOperand(a2, JSObject::kElementsOffset));
  __ LoadRoot(at, Heap::kEmptyFixedArrayRootIndex);
  __ Branch(&only_change_map, eq, at, Operand(t0));

  __ push(ra);
  __ lw(t1, FieldMemOperand(t0, FixedArray::kLengthOffset));
  // t0: source FixedArray
  // t1: number of elements (smi-tagged)

  // Allocate new FixedDoubleArray.
  __ sll(scratch, t1, 2);
  __ Addu(scratch, scratch, FixedDoubleArray::kHeaderSize);
  __ AllocateInNewSpace(scratch, t2, t3, t5, &gc_required, NO_ALLOCATION_FLAGS);
  // t2: destination FixedDoubleArray, not tagged as heap object
  // Set destination FixedDoubleArray's length and map.
  __ LoadRoot(t5, Heap::kFixedDoubleArrayMapRootIndex);
  __ sw(t1, MemOperand(t2, FixedDoubleArray::kLengthOffset));
  __ sw(t5, MemOperand(t2, HeapObject::kMapOffset));
  // Update receiver's map.

  __ sw(a3, FieldMemOperand(a2, HeapObject::kMapOffset));
  __ RecordWriteField(a2,
                      HeapObject::kMapOffset,
                      a3,
                      t5,
                      kRAHasBeenSaved,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  // Replace receiver's backing store with newly created FixedDoubleArray.
  __ Addu(a3, t2, Operand(kHeapObjectTag));
  __ sw(a3, FieldMemOperand(a2, JSObject::kElementsOffset));
  __ RecordWriteField(a2,
                      JSObject::kElementsOffset,
                      a3,
                      t5,
                      kRAHasBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);


  // Prepare for conversion loop.
  __ Addu(a3, t0, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ Addu(t3, t2, Operand(FixedDoubleArray::kHeaderSize));
  __ sll(t2, t1, 2);
  __ Addu(t2, t2, t3);
  __ li(t0, Operand(kHoleNanLower32));
  __ li(t1, Operand(kHoleNanUpper32));
  // t0: kHoleNanLower32
  // t1: kHoleNanUpper32
  // t2: end of destination FixedDoubleArray, not tagged
  // t3: begin of FixedDoubleArray element fields, not tagged

  if (!fpu_supported) __ Push(a1, a0);

  __ Branch(&entry);

  __ bind(&only_change_map);
  __ sw(a3, FieldMemOperand(a2, HeapObject::kMapOffset));
  __ RecordWriteField(a2,
                      HeapObject::kMapOffset,
                      a3,
                      t5,
                      kRAHasBeenSaved,
                      kDontSaveFPRegs,
                      OMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ Branch(&done);

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  __ pop(ra);
  __ Branch(fail);

  // Convert and copy elements.
  __ bind(&loop);
  __ lw(t5, MemOperand(a3));
  __ Addu(a3, a3, kIntSize);
  // t5: current element
  __ UntagAndJumpIfNotSmi(t5, t5, &convert_hole);

  // Normal smi, convert to double and store.
  if (fpu_supported) {
    CpuFeatures::Scope scope(FPU);
    __ mtc1(t5, f0);
    __ cvt_d_w(f0, f0);
    __ sdc1(f0, MemOperand(t3));
    __ Addu(t3, t3, kDoubleSize);
  } else {
    FloatingPointHelper::ConvertIntToDouble(masm,
                                            t5,
                                            FloatingPointHelper::kCoreRegisters,
                                            f0,
                                            a0,
                                            a1,
                                            t7,
                                            f0);
    __ sw(a0, MemOperand(t3));  // mantissa
    __ sw(a1, MemOperand(t3, kIntSize));  // exponent
    __ Addu(t3, t3, kDoubleSize);
  }
  __ Branch(&entry);

  // Hole found, store the-hole NaN.
  __ bind(&convert_hole);
  if (FLAG_debug_code) {
    // Restore a "smi-untagged" heap object.
    __ SmiTag(t5);
    __ Or(t5, t5, Operand(1));
    __ LoadRoot(at, Heap::kTheHoleValueRootIndex);
    __ Assert(eq, "object found in smi-only array", at, Operand(t5));
  }
  __ sw(t0, MemOperand(t3));  // mantissa
  __ sw(t1, MemOperand(t3, kIntSize));  // exponent
  __ Addu(t3, t3, kDoubleSize);

  __ bind(&entry);
  __ Branch(&loop, lt, t3, Operand(t2));

  if (!fpu_supported) __ Pop(a1, a0);
  __ pop(ra);
  __ bind(&done);
}


void ElementsTransitionGenerator::GenerateDoubleToObject(
    MacroAssembler* masm, Label* fail) {
  // ----------- S t a t e -------------
  //  -- a0    : value
  //  -- a1    : key
  //  -- a2    : receiver
  //  -- ra    : return address
  //  -- a3    : target map, scratch for subsequent call
  //  -- t0    : scratch (elements)
  // -----------------------------------
  Label entry, loop, convert_hole, gc_required, only_change_map;

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ lw(t0, FieldMemOperand(a2, JSObject::kElementsOffset));
  __ LoadRoot(at, Heap::kEmptyFixedArrayRootIndex);
  __ Branch(&only_change_map, eq, at, Operand(t0));

  __ MultiPush(a0.bit() | a1.bit() | a2.bit() | a3.bit() | ra.bit());

  __ lw(t1, FieldMemOperand(t0, FixedArray::kLengthOffset));
  // t0: source FixedArray
  // t1: number of elements (smi-tagged)

  // Allocate new FixedArray.
  __ sll(a0, t1, 1);
  __ Addu(a0, a0, FixedDoubleArray::kHeaderSize);
  __ AllocateInNewSpace(a0, t2, t3, t5, &gc_required, NO_ALLOCATION_FLAGS);
  // t2: destination FixedArray, not tagged as heap object
  // Set destination FixedDoubleArray's length and map.
  __ LoadRoot(t5, Heap::kFixedArrayMapRootIndex);
  __ sw(t1, MemOperand(t2, FixedDoubleArray::kLengthOffset));
  __ sw(t5, MemOperand(t2, HeapObject::kMapOffset));

  // Prepare for conversion loop.
  __ Addu(t0, t0, Operand(FixedDoubleArray::kHeaderSize - kHeapObjectTag + 4));
  __ Addu(a3, t2, Operand(FixedArray::kHeaderSize));
  __ Addu(t2, t2, Operand(kHeapObjectTag));
  __ sll(t1, t1, 1);
  __ Addu(t1, a3, t1);
  __ LoadRoot(t3, Heap::kTheHoleValueRootIndex);
  __ LoadRoot(t5, Heap::kHeapNumberMapRootIndex);
  // Using offsetted addresses.
  // a3: begin of destination FixedArray element fields, not tagged
  // t0: begin of source FixedDoubleArray element fields, not tagged, +4
  // t1: end of destination FixedArray, not tagged
  // t2: destination FixedArray
  // t3: the-hole pointer
  // t5: heap number map
  __ Branch(&entry);

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  __ MultiPop(a0.bit() | a1.bit() | a2.bit() | a3.bit() | ra.bit());

  __ Branch(fail);

  __ bind(&loop);
  __ lw(a1, MemOperand(t0));
  __ Addu(t0, t0, kDoubleSize);
  // a1: current element's upper 32 bit
  // t0: address of next element's upper 32 bit
  __ Branch(&convert_hole, eq, a1, Operand(kHoleNanUpper32));

  // Non-hole double, copy value into a heap number.
  __ AllocateHeapNumber(a2, a0, t6, t5, &gc_required);
  // a2: new heap number
  __ lw(a0, MemOperand(t0, -12));
  __ sw(a0, FieldMemOperand(a2, HeapNumber::kMantissaOffset));
  __ sw(a1, FieldMemOperand(a2, HeapNumber::kExponentOffset));
  __ mov(a0, a3);
  __ sw(a2, MemOperand(a3));
  __ Addu(a3, a3, kIntSize);
  __ RecordWrite(t2,
                 a0,
                 a2,
                 kRAHasBeenSaved,
                 kDontSaveFPRegs,
                 EMIT_REMEMBERED_SET,
                 OMIT_SMI_CHECK);
  __ Branch(&entry);

  // Replace the-hole NaN with the-hole pointer.
  __ bind(&convert_hole);
  __ sw(t3, MemOperand(a3));
  __ Addu(a3, a3, kIntSize);

  __ bind(&entry);
  __ Branch(&loop, lt, a3, Operand(t1));

  __ MultiPop(a2.bit() | a3.bit() | a0.bit() | a1.bit());
  // Replace receiver's backing store with newly created and filled FixedArray.
  __ sw(t2, FieldMemOperand(a2, JSObject::kElementsOffset));
  __ RecordWriteField(a2,
                      JSObject::kElementsOffset,
                      t2,
                      t5,
                      kRAHasBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ pop(ra);

  __ bind(&only_change_map);
  // Update receiver's map.
  __ sw(a3, FieldMemOperand(a2, HeapObject::kMapOffset));
  __ RecordWriteField(a2,
                      HeapObject::kMapOffset,
                      a3,
                      t5,
                      kRAHasNotBeenSaved,
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
  __ lw(result, FieldMemOperand(string, HeapObject::kMapOffset));
  __ lbu(result, FieldMemOperand(result, Map::kInstanceTypeOffset));

  // We need special handling for indirect strings.
  Label check_sequential;
  __ And(at, result, Operand(kIsIndirectStringMask));
  __ Branch(&check_sequential, eq, at, Operand(zero_reg));

  // Dispatch on the indirect string shape: slice or cons.
  Label cons_string;
  __ And(at, result, Operand(kSlicedNotConsMask));
  __ Branch(&cons_string, eq, at, Operand(zero_reg));

  // Handle slices.
  Label indirect_string_loaded;
  __ lw(result, FieldMemOperand(string, SlicedString::kOffsetOffset));
  __ lw(string, FieldMemOperand(string, SlicedString::kParentOffset));
  __ sra(at, result, kSmiTagSize);
  __ Addu(index, index, at);
  __ jmp(&indirect_string_loaded);

  // Handle cons strings.
  // Check whether the right hand side is the empty string (i.e. if
  // this is really a flat string in a cons string). If that is not
  // the case we would rather go to the runtime system now to flatten
  // the string.
  __ bind(&cons_string);
  __ lw(result, FieldMemOperand(string, ConsString::kSecondOffset));
  __ LoadRoot(at, Heap::kEmptyStringRootIndex);
  __ Branch(call_runtime, ne, result, Operand(at));
  // Get the first of the two strings and load its instance type.
  __ lw(string, FieldMemOperand(string, ConsString::kFirstOffset));

  __ bind(&indirect_string_loaded);
  __ lw(result, FieldMemOperand(string, HeapObject::kMapOffset));
  __ lbu(result, FieldMemOperand(result, Map::kInstanceTypeOffset));

  // Distinguish sequential and external strings. Only these two string
  // representations can reach here (slices and flat cons strings have been
  // reduced to the underlying sequential or external string).
  Label external_string, check_encoding;
  __ bind(&check_sequential);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ And(at, result, Operand(kStringRepresentationMask));
  __ Branch(&external_string, ne, at, Operand(zero_reg));

  // Prepare sequential strings
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqAsciiString::kHeaderSize);
  __ Addu(string,
          string,
          SeqTwoByteString::kHeaderSize - kHeapObjectTag);
  __ jmp(&check_encoding);

  // Handle external strings.
  __ bind(&external_string);
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ And(at, result, Operand(kIsIndirectStringMask));
    __ Assert(eq, "external string expected, but not found",
        at, Operand(zero_reg));
  }
  // Rule out short external strings.
  STATIC_CHECK(kShortExternalStringTag != 0);
  __ And(at, result, Operand(kShortExternalStringMask));
  __ Branch(call_runtime, ne, at, Operand(zero_reg));
  __ lw(string, FieldMemOperand(string, ExternalString::kResourceDataOffset));

  Label ascii, done;
  __ bind(&check_encoding);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ And(at, result, Operand(kStringEncodingMask));
  __ Branch(&ascii, ne, at, Operand(zero_reg));
  // Two-byte string.
  __ sll(at, index, 1);
  __ Addu(at, string, at);
  __ lhu(result, MemOperand(at));
  __ jmp(&done);
  __ bind(&ascii);
  // Ascii string.
  __ Addu(at, string, index);
  __ lbu(result, MemOperand(at));
  __ bind(&done);
}

#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS
