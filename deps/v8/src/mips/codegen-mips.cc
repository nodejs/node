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
#include "simulator-mips.h"

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
byte* fast_exp_mips_machine_code = NULL;
double fast_exp_simulator(double x) {
  return Simulator::current(Isolate::Current())->CallFP(
      fast_exp_mips_machine_code, x, 0);
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
    DoubleRegister input = f12;
    DoubleRegister result = f0;
    DoubleRegister double_scratch1 = f4;
    DoubleRegister double_scratch2 = f6;
    Register temp1 = t0;
    Register temp2 = t1;
    Register temp3 = t2;

    if (!IsMipsSoftFloatABI) {
      // Input value is in f12 anyway, nothing to do.
    } else {
      __ Move(input, a0, a1);
    }
    __ Push(temp3, temp2, temp1);
    MathExpGenerator::EmitMathExp(
        &masm, input, result, double_scratch1, double_scratch2,
        temp1, temp2, temp3);
    __ Pop(temp3, temp2, temp1);
    if (!IsMipsSoftFloatABI) {
      // Result is already in f0, nothing to do.
    } else {
      __ Move(v0, v1, result);
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
  fast_exp_mips_machine_code = buffer;
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
  //  -- a0    : value
  //  -- a1    : key
  //  -- a2    : receiver
  //  -- ra    : return address
  //  -- a3    : target map, scratch for subsequent call
  //  -- t0    : scratch (elements)
  // -----------------------------------
  if (mode == TRACK_ALLOCATION_SITE) {
    ASSERT(allocation_site_info_found != NULL);
    masm->TestJSArrayForAllocationSiteInfo(a2, t0, eq,
                                           allocation_site_info_found);
  }

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
    MacroAssembler* masm, AllocationSiteMode mode, Label* fail) {
  // ----------- S t a t e -------------
  //  -- a0    : value
  //  -- a1    : key
  //  -- a2    : receiver
  //  -- ra    : return address
  //  -- a3    : target map, scratch for subsequent call
  //  -- t0    : scratch (elements)
  // -----------------------------------
  Label loop, entry, convert_hole, gc_required, only_change_map, done;

  Register scratch = t6;

  if (mode == TRACK_ALLOCATION_SITE) {
    masm->TestJSArrayForAllocationSiteInfo(a2, t0, eq, fail);
  }

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
  __ Allocate(scratch, t2, t3, t5, &gc_required, NO_ALLOCATION_FLAGS);
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

  __ Branch(&entry);

  __ bind(&only_change_map);
  __ sw(a3, FieldMemOperand(a2, HeapObject::kMapOffset));
  __ RecordWriteField(a2,
                      HeapObject::kMapOffset,
                      a3,
                      t5,
                      kRAHasNotBeenSaved,
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
  __ mtc1(t5, f0);
  __ cvt_d_w(f0, f0);
  __ sdc1(f0, MemOperand(t3));
  __ Addu(t3, t3, kDoubleSize);

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

  __ pop(ra);
  __ bind(&done);
}


void ElementsTransitionGenerator::GenerateDoubleToObject(
    MacroAssembler* masm, AllocationSiteMode mode, Label* fail) {
  // ----------- S t a t e -------------
  //  -- a0    : value
  //  -- a1    : key
  //  -- a2    : receiver
  //  -- ra    : return address
  //  -- a3    : target map, scratch for subsequent call
  //  -- t0    : scratch (elements)
  // -----------------------------------
  Label entry, loop, convert_hole, gc_required, only_change_map;

  if (mode == TRACK_ALLOCATION_SITE) {
    masm->TestJSArrayForAllocationSiteInfo(a2, t0, eq, fail);
  }

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
  __ Allocate(a0, t2, t3, t5, &gc_required, NO_ALLOCATION_FLAGS);
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
  __ LoadRoot(at, Heap::kempty_stringRootIndex);
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
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
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


void SeqStringSetCharGenerator::Generate(MacroAssembler* masm,
                                         String::Encoding encoding,
                                         Register string,
                                         Register index,
                                         Register value) {
  if (FLAG_debug_code) {
    __ And(at, index, Operand(kSmiTagMask));
    __ Check(eq, "Non-smi index", at, Operand(zero_reg));
    __ And(at, value, Operand(kSmiTagMask));
    __ Check(eq, "Non-smi value", at, Operand(zero_reg));

    __ lw(at, FieldMemOperand(string, String::kLengthOffset));
    __ Check(lt, "Index is too large", index, Operand(at));

    __ Check(ge, "Index is negative", index, Operand(zero_reg));

    __ lw(at, FieldMemOperand(string, HeapObject::kMapOffset));
    __ lbu(at, FieldMemOperand(at, Map::kInstanceTypeOffset));

    __ And(at, at, Operand(kStringRepresentationMask | kStringEncodingMask));
    static const uint32_t one_byte_seq_type = kSeqStringTag | kOneByteStringTag;
    static const uint32_t two_byte_seq_type = kSeqStringTag | kTwoByteStringTag;
    __ Subu(at, at, Operand(encoding == String::ONE_BYTE_ENCODING
        ? one_byte_seq_type : two_byte_seq_type));
    __ Check(eq, "Unexpected string type", at, Operand(zero_reg));
  }

  __ Addu(at,
          string,
          Operand(SeqString::kHeaderSize - kHeapObjectTag));
  __ SmiUntag(value);
  STATIC_ASSERT(kSmiTagSize == 1 && kSmiTag == 0);
  if (encoding == String::ONE_BYTE_ENCODING) {
    __ SmiUntag(index);
    __ Addu(at, at, index);
    __ sb(value, MemOperand(at));
  } else {
    // No need to untag a smi for two-byte addressing.
    __ Addu(at, at, index);
    __ sh(value, MemOperand(at));
  }
}


static MemOperand ExpConstant(int index, Register base) {
  return MemOperand(base, index * kDoubleSize);
}


void MathExpGenerator::EmitMathExp(MacroAssembler* masm,
                                   DoubleRegister input,
                                   DoubleRegister result,
                                   DoubleRegister double_scratch1,
                                   DoubleRegister double_scratch2,
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

  __ li(temp3, Operand(ExternalReference::math_exp_constants(0)));

  __ ldc1(double_scratch1, ExpConstant(0, temp3));
  __ Move(result, kDoubleRegZero);
  __ BranchF(&done, NULL, ge, double_scratch1, input);
  __ ldc1(double_scratch2, ExpConstant(1, temp3));
  __ ldc1(result, ExpConstant(2, temp3));
  __ BranchF(&done, NULL, ge, input, double_scratch2);
  __ ldc1(double_scratch1, ExpConstant(3, temp3));
  __ ldc1(result, ExpConstant(4, temp3));
  __ mul_d(double_scratch1, double_scratch1, input);
  __ add_d(double_scratch1, double_scratch1, result);
  __ Move(temp2, temp1, double_scratch1);
  __ sub_d(double_scratch1, double_scratch1, result);
  __ ldc1(result, ExpConstant(6, temp3));
  __ ldc1(double_scratch2, ExpConstant(5, temp3));
  __ mul_d(double_scratch1, double_scratch1, double_scratch2);
  __ sub_d(double_scratch1, double_scratch1, input);
  __ sub_d(result, result, double_scratch1);
  __ mul_d(input, double_scratch1, double_scratch1);
  __ mul_d(result, result, input);
  __ srl(temp1, temp2, 11);
  __ ldc1(double_scratch2, ExpConstant(7, temp3));
  __ mul_d(result, result, double_scratch2);
  __ sub_d(result, result, double_scratch1);
  __ ldc1(double_scratch2, ExpConstant(8, temp3));
  __ add_d(result, result, double_scratch2);
  __ li(at, 0x7ff);
  __ And(temp2, temp2, at);
  __ Addu(temp1, temp1, Operand(0x3ff));
  __ sll(temp1, temp1, 20);

  // Must not call ExpConstant() after overwriting temp3!
  __ li(temp3, Operand(ExternalReference::math_exp_log_table()));
  __ sll(at, temp2, 3);
  __ addu(at, at, temp3);
  __ lw(at, MemOperand(at));
  __ Addu(temp3, temp3, Operand(kPointerSize));
  __ sll(temp2, temp2, 3);
  __ addu(temp2, temp2, temp3);
  __ lw(temp2, MemOperand(temp2));
  __ Or(temp1, temp1, temp2);
  __ Move(input, at, temp1);
  __ mul_d(result, result, input);
  __ bind(&done);
}


// nop(CODE_AGE_MARKER_NOP)
static const uint32_t kCodeAgePatchFirstInstruction = 0x00010180;

static byte* GetNoCodeAgeSequence(uint32_t* length) {
  // The sequence of instructions that is patched out for aging code is the
  // following boilerplate stack-building prologue that is found in FUNCTIONS
  static bool initialized = false;
  static uint32_t sequence[kNoCodeAgeSequenceLength];
  byte* byte_sequence = reinterpret_cast<byte*>(sequence);
  *length = kNoCodeAgeSequenceLength * Assembler::kInstrSize;
  if (!initialized) {
    CodePatcher patcher(byte_sequence, kNoCodeAgeSequenceLength);
    patcher.masm()->Push(ra, fp, cp, a1);
    patcher.masm()->LoadRoot(at, Heap::kUndefinedValueRootIndex);
    patcher.masm()->Addu(fp, sp, Operand(2 * kPointerSize));
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
    // Mark this code sequence for FindPlatformCodeAgeSequence()
    patcher.masm()->nop(Assembler::CODE_AGE_MARKER_NOP);
    // Save the function's original return address
    // (it will be clobbered by Call(t9))
    patcher.masm()->mov(at, ra);
    // Load the stub address to t9 and call it
    patcher.masm()->li(t9,
        Operand(reinterpret_cast<uint32_t>(stub->instruction_start())));
    patcher.masm()->Call(t9);
    // Record the stub address in the empty space for GetCodeAgeAndParity()
    patcher.masm()->dd(reinterpret_cast<uint32_t>(stub->instruction_start()));
  }
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS
