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

#if defined(V8_TARGET_ARCH_ARM)

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
  return Simulator::current(Isolate::Current())->CallFP(
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
    Label* allocation_site_info_found) {
  // ----------- S t a t e -------------
  //  -- r0    : value
  //  -- r1    : key
  //  -- r2    : receiver
  //  -- lr    : return address
  //  -- r3    : target map, scratch for subsequent call
  //  -- r4    : scratch (elements)
  // -----------------------------------
  if (mode == TRACK_ALLOCATION_SITE) {
    ASSERT(allocation_site_info_found != NULL);
    __ TestJSArrayForAllocationSiteInfo(r2, r4);
    __ b(eq, allocation_site_info_found);
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
    __ TestJSArrayForAllocationSiteInfo(r2, r4);
    __ b(eq, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ ldr(r4, FieldMemOperand(r2, JSObject::kElementsOffset));
  __ CompareRoot(r4, Heap::kEmptyFixedArrayRootIndex);
  __ b(eq, &only_change_map);

  __ push(lr);
  __ ldr(r5, FieldMemOperand(r4, FixedArray::kLengthOffset));
  // r4: source FixedArray
  // r5: number of elements (smi-tagged)

  // Allocate new FixedDoubleArray.
  // Use lr as a temporary register.
  __ mov(lr, Operand(r5, LSL, 2));
  __ add(lr, lr, Operand(FixedDoubleArray::kHeaderSize));
  __ Allocate(lr, r6, r7, r9, &gc_required, DOUBLE_ALIGNMENT);
  // r6: destination FixedDoubleArray, not tagged as heap object.

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
  __ add(r7, r6, Operand(FixedDoubleArray::kHeaderSize));
  __ add(r6, r7, Operand(r5, LSL, 2));
  __ mov(r4, Operand(kHoleNanLower32));
  __ mov(r5, Operand(kHoleNanUpper32));
  // r3: begin of source FixedArray element fields, not tagged
  // r4: kHoleNanLower32
  // r5: kHoleNanUpper32
  // r6: end of destination FixedDoubleArray, not tagged
  // r7: begin of FixedDoubleArray element fields, not tagged

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
  __ ldr(r9, MemOperand(r3, 4, PostIndex));
  // r9: current element
  __ UntagAndJumpIfNotSmi(r9, r9, &convert_hole);

  // Normal smi, convert to double and store.
  __ vmov(s0, r9);
  __ vcvt_f64_s32(d0, s0);
  __ vstr(d0, r7, 0);
  __ add(r7, r7, Operand(8));
  __ b(&entry);

  // Hole found, store the-hole NaN.
  __ bind(&convert_hole);
  if (FLAG_debug_code) {
    // Restore a "smi-untagged" heap object.
    __ SmiTag(r9);
    __ orr(r9, r9, Operand(1));
    __ CompareRoot(r9, Heap::kTheHoleValueRootIndex);
    __ Assert(eq, "object found in smi-only array");
  }
  __ Strd(r4, r5, MemOperand(r7, 8, PostIndex));

  __ bind(&entry);
  __ cmp(r7, r6);
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
    __ TestJSArrayForAllocationSiteInfo(r2, r4);
    __ b(eq, fail);
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
  __ Allocate(r0, r6, r7, r9, &gc_required, NO_ALLOCATION_FLAGS);
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
  __ LoadRoot(r7, Heap::kTheHoleValueRootIndex);
  __ LoadRoot(r9, Heap::kHeapNumberMapRootIndex);
  // Using offsetted addresses in r4 to fully take advantage of post-indexing.
  // r3: begin of destination FixedArray element fields, not tagged
  // r4: begin of source FixedDoubleArray element fields, not tagged, +4
  // r5: end of destination FixedArray, not tagged
  // r6: destination FixedArray
  // r7: the-hole pointer
  // r9: heap number map
  __ b(&entry);

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  __ Pop(r3, r2, r1, r0);
  __ pop(lr);
  __ b(fail);

  __ bind(&loop);
  __ ldr(r1, MemOperand(r4, 8, PostIndex));
  // lr: current element's upper 32 bit
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
  __ str(r7, MemOperand(r3, 4, PostIndex));

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
  __ add(index, index, Operand(result, ASR, kSmiTagSize));
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
    __ Assert(eq, "external string expected, but not found");
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


void SeqStringSetCharGenerator::Generate(MacroAssembler* masm,
                                         String::Encoding encoding,
                                         Register string,
                                         Register index,
                                         Register value) {
  if (FLAG_debug_code) {
    __ tst(index, Operand(kSmiTagMask));
    __ Check(eq, "Non-smi index");
    __ tst(value, Operand(kSmiTagMask));
    __ Check(eq, "Non-smi value");

    __ ldr(ip, FieldMemOperand(string, String::kLengthOffset));
    __ cmp(index, ip);
    __ Check(lt, "Index is too large");

    __ cmp(index, Operand(Smi::FromInt(0)));
    __ Check(ge, "Index is negative");

    __ ldr(ip, FieldMemOperand(string, HeapObject::kMapOffset));
    __ ldrb(ip, FieldMemOperand(ip, Map::kInstanceTypeOffset));

    __ and_(ip, ip, Operand(kStringRepresentationMask | kStringEncodingMask));
    static const uint32_t one_byte_seq_type = kSeqStringTag | kOneByteStringTag;
    static const uint32_t two_byte_seq_type = kSeqStringTag | kTwoByteStringTag;
    __ cmp(ip, Operand(encoding == String::ONE_BYTE_ENCODING
                           ? one_byte_seq_type : two_byte_seq_type));
    __ Check(eq, "Unexpected string type");
  }

  __ add(ip,
         string,
         Operand(SeqString::kHeaderSize - kHeapObjectTag));
  __ SmiUntag(value, value);
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  if (encoding == String::ONE_BYTE_ENCODING) {
    // Smis are tagged by left shift by 1, thus LSR by 1 to smi-untag inline.
    __ strb(value, MemOperand(ip, index, LSR, 1));
  } else {
    // No need to untag a smi for two-byte addressing.
    __ strh(value, MemOperand(ip, index));
  }
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

  Label done;

  __ mov(temp3, Operand(ExternalReference::math_exp_constants(0)));

  __ vldr(double_scratch1, ExpConstant(0, temp3));
  __ vmov(result, kDoubleRegZero);
  __ VFPCompareAndSetFlags(double_scratch1, input);
  __ b(ge, &done);
  __ vldr(double_scratch2, ExpConstant(1, temp3));
  __ VFPCompareAndSetFlags(input, double_scratch2);
  __ vldr(result, ExpConstant(2, temp3));
  __ b(ge, &done);
  __ vldr(double_scratch1, ExpConstant(3, temp3));
  __ vldr(result, ExpConstant(4, temp3));
  __ vmul(double_scratch1, double_scratch1, input);
  __ vadd(double_scratch1, double_scratch1, result);
  __ vmov(temp2, temp1, double_scratch1);
  __ vsub(double_scratch1, double_scratch1, result);
  __ vldr(result, ExpConstant(6, temp3));
  __ vldr(double_scratch2, ExpConstant(5, temp3));
  __ vmul(double_scratch1, double_scratch1, double_scratch2);
  __ vsub(double_scratch1, double_scratch1, input);
  __ vsub(result, result, double_scratch1);
  __ vmul(input, double_scratch1, double_scratch1);
  __ vmul(result, result, input);
  __ mov(temp1, Operand(temp2, LSR, 11));
  __ vldr(double_scratch2, ExpConstant(7, temp3));
  __ vmul(result, result, double_scratch2);
  __ vsub(result, result, double_scratch1);
  __ vldr(double_scratch2, ExpConstant(8, temp3));
  __ vadd(result, result, double_scratch2);
  __ movw(ip, 0x7ff);
  __ and_(temp2, temp2, Operand(ip));
  __ add(temp1, temp1, Operand(0x3ff));
  __ mov(temp1, Operand(temp1, LSL, 20));

  // Must not call ExpConstant() after overwriting temp3!
  __ mov(temp3, Operand(ExternalReference::math_exp_log_table()));
  __ ldr(ip, MemOperand(temp3, temp2, LSL, 3));
  __ add(temp3, temp3, Operand(kPointerSize));
  __ ldr(temp2, MemOperand(temp3, temp2, LSL, 3));
  __ orr(temp1, temp1, temp2);
  __ vmov(input, ip, temp1);
  __ vmul(result, result, input);
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
    patcher.masm()->LoadRoot(ip, Heap::kUndefinedValueRootIndex);
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
    *age = kNoAge;
    *parity = NO_MARKING_PARITY;
  } else {
    Address target_address = Memory::Address_at(
        sequence + Assembler::kInstrSize * (kNoCodeAgeSequenceLength - 1));
    Code* stub = GetCodeFromTargetAddress(target_address);
    GetCodeAgeAndParity(stub, age, parity);
  }
}


void Code::PatchPlatformCodeAge(byte* sequence,
                                Code::Age age,
                                MarkingParity parity) {
  uint32_t young_length;
  byte* young_sequence = GetNoCodeAgeSequence(&young_length);
  if (age == kNoAge) {
    CopyBytes(sequence, young_sequence, young_length);
    CPU::FlushICache(sequence, young_length);
  } else {
    Code* stub = GetCodeAgeStub(age, parity);
    CodePatcher patcher(sequence, young_length / Assembler::kInstrSize);
    patcher.masm()->add(r0, pc, Operand(-8));
    patcher.masm()->ldr(pc, MemOperand(pc, -4));
    patcher.masm()->dd(reinterpret_cast<uint32_t>(stub->instruction_start()));
  }
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM
