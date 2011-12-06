// Copyright 2011 the V8 project authors. All rights reserved.
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

void ElementsTransitionGenerator::GenerateSmiOnlyToObject(
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


void ElementsTransitionGenerator::GenerateSmiOnlyToDouble(
    MacroAssembler* masm, Label* fail) {
  // ----------- S t a t e -------------
  //  -- a0    : value
  //  -- a1    : key
  //  -- a2    : receiver
  //  -- ra    : return address
  //  -- a3    : target map, scratch for subsequent call
  //  -- t0    : scratch (elements)
  // -----------------------------------
  Label loop, entry, convert_hole, gc_required;
  bool fpu_supported = CpuFeatures::IsSupported(FPU);
  __ push(ra);

  Register scratch = t6;

  __ lw(t0, FieldMemOperand(a2, JSObject::kElementsOffset));
  __ lw(t1, FieldMemOperand(t0, FixedArray::kLengthOffset));
  // t0: source FixedArray
  // t1: number of elements (smi-tagged)

  // Allocate new FixedDoubleArray.
  __ sll(scratch, t1, 2);
  __ Addu(scratch, scratch, FixedDoubleArray::kHeaderSize);
  __ AllocateInNewSpace(scratch, t2, t3, t5, &gc_required, NO_ALLOCATION_FLAGS);
  // t2: destination FixedDoubleArray, not tagged as heap object
  __ LoadRoot(t5, Heap::kFixedDoubleArrayMapRootIndex);
  __ sw(t5, MemOperand(t2, HeapObject::kMapOffset));
  // Set destination FixedDoubleArray's length.
  __ sw(t1, MemOperand(t2, FixedDoubleArray::kLengthOffset));
  // Update receiver's map.

  __ sw(a3, FieldMemOperand(a2, HeapObject::kMapOffset));
  __ RecordWriteField(a2,
                      HeapObject::kMapOffset,
                      a3,
                      t5,
                      kRAHasBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
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

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  __ pop(ra);
  __ Branch(fail);

  // Convert and copy elements.
  __ bind(&loop);
  __ lw(t5, MemOperand(a3));
  __ Addu(a3, a3, kIntSize);
  // t5: current element
  __ JumpIfNotSmi(t5, &convert_hole);

  // Normal smi, convert to double and store.
  __ SmiUntag(t5);
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
  Label entry, loop, convert_hole, gc_required;
  __ MultiPush(a0.bit() | a1.bit() | a2.bit() | a3.bit() | ra.bit());

  __ lw(t0, FieldMemOperand(a2, JSObject::kElementsOffset));
  __ lw(t1, FieldMemOperand(t0, FixedArray::kLengthOffset));
  // t0: source FixedArray
  // t1: number of elements (smi-tagged)

  // Allocate new FixedArray.
  __ sll(a0, t1, 1);
  __ Addu(a0, a0, FixedDoubleArray::kHeaderSize);
  __ AllocateInNewSpace(a0, t2, t3, t5, &gc_required, NO_ALLOCATION_FLAGS);
  // t2: destination FixedArray, not tagged as heap object
  __ LoadRoot(t5, Heap::kFixedArrayMapRootIndex);
  __ sw(t5, MemOperand(t2, HeapObject::kMapOffset));
  // Set destination FixedDoubleArray's length.
  __ sw(t1, MemOperand(t2, FixedDoubleArray::kLengthOffset));

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
  // Update receiver's map.
  __ sw(a3, FieldMemOperand(a2, HeapObject::kMapOffset));
  __ RecordWriteField(a2,
                      HeapObject::kMapOffset,
                      a3,
                      t5,
                      kRAHasBeenSaved,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
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
}

#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS
