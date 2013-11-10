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

#if V8_TARGET_ARCH_X64

#include "codegen.h"
#include "macro-assembler.h"

namespace v8 {
namespace internal {

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


#define __ masm.


UnaryMathFunction CreateTranscendentalFunction(TranscendentalCache::Type type) {
  size_t actual_size;
  // Allocate buffer in executable space.
  byte* buffer = static_cast<byte*>(OS::Allocate(1 * KB,
                                                 &actual_size,
                                                 true));
  if (buffer == NULL) {
    // Fallback to library function if function cannot be created.
    switch (type) {
      case TranscendentalCache::SIN: return &sin;
      case TranscendentalCache::COS: return &cos;
      case TranscendentalCache::TAN: return &tan;
      case TranscendentalCache::LOG: return &log;
      default: UNIMPLEMENTED();
    }
  }

  MacroAssembler masm(NULL, buffer, static_cast<int>(actual_size));
  // xmm0: raw double input.
  // Move double input into registers.
  __ push(rbx);
  __ push(rdi);
  __ movq(rbx, xmm0);
  __ push(rbx);
  __ fld_d(Operand(rsp, 0));
  TranscendentalCacheStub::GenerateOperation(&masm, type);
  // The return value is expected to be in xmm0.
  __ fstp_d(Operand(rsp, 0));
  __ pop(rbx);
  __ movq(xmm0, rbx);
  __ pop(rdi);
  __ pop(rbx);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(&desc);
  ASSERT(!RelocInfo::RequiresRelocation(desc));

  CPU::FlushICache(buffer, actual_size);
  OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<UnaryMathFunction>(buffer);
}


UnaryMathFunction CreateExpFunction() {
  if (!FLAG_fast_math) return &exp;
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == NULL) return &exp;
  ExternalReference::InitializeMathExpData();

  MacroAssembler masm(NULL, buffer, static_cast<int>(actual_size));
  // xmm0: raw double input.
  XMMRegister input = xmm0;
  XMMRegister result = xmm1;
  __ push(rax);
  __ push(rbx);

  MathExpGenerator::EmitMathExp(&masm, input, result, xmm2, rax, rbx);

  __ pop(rbx);
  __ pop(rax);
  __ movsd(xmm0, result);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(&desc);
  ASSERT(!RelocInfo::RequiresRelocation(desc));

  CPU::FlushICache(buffer, actual_size);
  OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<UnaryMathFunction>(buffer);
}


UnaryMathFunction CreateSqrtFunction() {
  size_t actual_size;
  // Allocate buffer in executable space.
  byte* buffer = static_cast<byte*>(OS::Allocate(1 * KB,
                                                 &actual_size,
                                                 true));
  if (buffer == NULL) return &sqrt;

  MacroAssembler masm(NULL, buffer, static_cast<int>(actual_size));
  // xmm0: raw double input.
  // Move double input into registers.
  __ sqrtsd(xmm0, xmm0);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(&desc);
  ASSERT(!RelocInfo::RequiresRelocation(desc));

  CPU::FlushICache(buffer, actual_size);
  OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<UnaryMathFunction>(buffer);
}


#ifdef _WIN64
typedef double (*ModuloFunction)(double, double);
// Define custom fmod implementation.
ModuloFunction CreateModuloFunction() {
  size_t actual_size;
  byte* buffer = static_cast<byte*>(OS::Allocate(Assembler::kMinimalBufferSize,
                                                 &actual_size,
                                                 true));
  CHECK(buffer);
  Assembler masm(NULL, buffer, static_cast<int>(actual_size));
  // Generated code is put into a fixed, unmovable, buffer, and not into
  // the V8 heap. We can't, and don't, refer to any relocatable addresses
  // (e.g. the JavaScript nan-object).

  // Windows 64 ABI passes double arguments in xmm0, xmm1 and
  // returns result in xmm0.
  // Argument backing space is allocated on the stack above
  // the return address.

  // Compute x mod y.
  // Load y and x (use argument backing store as temporary storage).
  __ movsd(Operand(rsp, kPointerSize * 2), xmm1);
  __ movsd(Operand(rsp, kPointerSize), xmm0);
  __ fld_d(Operand(rsp, kPointerSize * 2));
  __ fld_d(Operand(rsp, kPointerSize));

  // Clear exception flags before operation.
  {
    Label no_exceptions;
    __ fwait();
    __ fnstsw_ax();
    // Clear if Illegal Operand or Zero Division exceptions are set.
    __ testb(rax, Immediate(5));
    __ j(zero, &no_exceptions);
    __ fnclex();
    __ bind(&no_exceptions);
  }

  // Compute st(0) % st(1)
  {
    Label partial_remainder_loop;
    __ bind(&partial_remainder_loop);
    __ fprem();
    __ fwait();
    __ fnstsw_ax();
    __ testl(rax, Immediate(0x400 /* C2 */));
    // If C2 is set, computation only has partial result. Loop to
    // continue computation.
    __ j(not_zero, &partial_remainder_loop);
  }

  Label valid_result;
  Label return_result;
  // If Invalid Operand or Zero Division exceptions are set,
  // return NaN.
  __ testb(rax, Immediate(5));
  __ j(zero, &valid_result);
  __ fstp(0);  // Drop result in st(0).
  int64_t kNaNValue = V8_INT64_C(0x7ff8000000000000);
  __ movq(rcx, kNaNValue, RelocInfo::NONE64);
  __ movq(Operand(rsp, kPointerSize), rcx);
  __ movsd(xmm0, Operand(rsp, kPointerSize));
  __ jmp(&return_result);

  // If result is valid, return that.
  __ bind(&valid_result);
  __ fstp_d(Operand(rsp, kPointerSize));
  __ movsd(xmm0, Operand(rsp, kPointerSize));

  // Clean up FPU stack and exceptions and return xmm0
  __ bind(&return_result);
  __ fstp(0);  // Unload y.

  Label clear_exceptions;
  __ testb(rax, Immediate(0x3f /* Any Exception*/));
  __ j(not_zero, &clear_exceptions);
  __ ret(0);
  __ bind(&clear_exceptions);
  __ fnclex();
  __ ret(0);

  CodeDesc desc;
  masm.GetCode(&desc);
  OS::ProtectCode(buffer, actual_size);
  // Call the function from C++ through this pointer.
  return FUNCTION_CAST<ModuloFunction>(buffer);
}

#endif

#undef __

// -------------------------------------------------------------------------
// Code generators

#define __ ACCESS_MASM(masm)

void ElementsTransitionGenerator::GenerateMapChangeElementsTransition(
    MacroAssembler* masm, AllocationSiteMode mode,
    Label* allocation_memento_found) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rbx    : target map
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  if (mode == TRACK_ALLOCATION_SITE) {
    ASSERT(allocation_memento_found != NULL);
    __ JumpIfJSArrayHasAllocationMemento(rdx, rdi, allocation_memento_found);
  }

  // Set transitioned map.
  __ movq(FieldOperand(rdx, HeapObject::kMapOffset), rbx);
  __ RecordWriteField(rdx,
                      HeapObject::kMapOffset,
                      rbx,
                      rdi,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
}


void ElementsTransitionGenerator::GenerateSmiToDouble(
    MacroAssembler* masm, AllocationSiteMode mode, Label* fail) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rbx    : target map
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  // The fail label is not actually used since we do not allocate.
  Label allocated, new_backing_store, only_change_map, done;

  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(rdx, rdi, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ movq(r8, FieldOperand(rdx, JSObject::kElementsOffset));
  __ CompareRoot(r8, Heap::kEmptyFixedArrayRootIndex);
  __ j(equal, &only_change_map);

  // Check backing store for COW-ness.  For COW arrays we have to
  // allocate a new backing store.
  __ SmiToInteger32(r9, FieldOperand(r8, FixedDoubleArray::kLengthOffset));
  __ CompareRoot(FieldOperand(r8, HeapObject::kMapOffset),
                 Heap::kFixedCOWArrayMapRootIndex);
  __ j(equal, &new_backing_store);
  // Check if the backing store is in new-space. If not, we need to allocate
  // a new one since the old one is in pointer-space.
  // If in new space, we can reuse the old backing store because it is
  // the same size.
  __ JumpIfNotInNewSpace(r8, rdi, &new_backing_store);

  __ movq(r14, r8);  // Destination array equals source array.

  // r8 : source FixedArray
  // r9 : elements array length
  // r14: destination FixedDoubleArray
  // Set backing store's map
  __ LoadRoot(rdi, Heap::kFixedDoubleArrayMapRootIndex);
  __ movq(FieldOperand(r14, HeapObject::kMapOffset), rdi);

  __ bind(&allocated);
  // Set transitioned map.
  __ movq(FieldOperand(rdx, HeapObject::kMapOffset), rbx);
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
  __ movq(r15, BitCast<int64_t, uint64_t>(kHoleNanInt64), RelocInfo::NONE64);
  // r15: the-hole NaN
  __ jmp(&entry);

  // Allocate new backing store.
  __ bind(&new_backing_store);
  __ lea(rdi, Operand(r9, times_8, FixedArray::kHeaderSize));
  __ Allocate(rdi, r14, r11, r15, fail, TAG_OBJECT);
  // Set backing store's map
  __ LoadRoot(rdi, Heap::kFixedDoubleArrayMapRootIndex);
  __ movq(FieldOperand(r14, HeapObject::kMapOffset), rdi);
  // Set receiver's backing store.
  __ movq(FieldOperand(rdx, JSObject::kElementsOffset), r14);
  __ movq(r11, r14);
  __ RecordWriteField(rdx,
                      JSObject::kElementsOffset,
                      r11,
                      r15,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  // Set backing store's length.
  __ Integer32ToSmi(r11, r9);
  __ movq(FieldOperand(r14, FixedDoubleArray::kLengthOffset), r11);
  __ jmp(&allocated);

  __ bind(&only_change_map);
  // Set transitioned map.
  __ movq(FieldOperand(rdx, HeapObject::kMapOffset), rbx);
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
  __ movq(rbx,
          FieldOperand(r8, r9, times_pointer_size, FixedArray::kHeaderSize));
  // r9 : current element's index
  // rbx: current element (smi-tagged)
  __ JumpIfNotSmi(rbx, &convert_hole);
  __ SmiToInteger32(rbx, rbx);
  __ Cvtlsi2sd(xmm0, rbx);
  __ movsd(FieldOperand(r14, r9, times_8, FixedDoubleArray::kHeaderSize),
           xmm0);
  __ jmp(&entry);
  __ bind(&convert_hole);

  if (FLAG_debug_code) {
    __ CompareRoot(rbx, Heap::kTheHoleValueRootIndex);
    __ Assert(equal, kObjectFoundInSmiOnlyArray);
  }

  __ movq(FieldOperand(r14, r9, times_8, FixedDoubleArray::kHeaderSize), r15);
  __ bind(&entry);
  __ decq(r9);
  __ j(not_sign, &loop);

  __ bind(&done);
}


void ElementsTransitionGenerator::GenerateDoubleToObject(
    MacroAssembler* masm, AllocationSiteMode mode, Label* fail) {
  // ----------- S t a t e -------------
  //  -- rax    : value
  //  -- rbx    : target map
  //  -- rcx    : key
  //  -- rdx    : receiver
  //  -- rsp[0] : return address
  // -----------------------------------
  Label loop, entry, convert_hole, gc_required, only_change_map;

  if (mode == TRACK_ALLOCATION_SITE) {
    __ JumpIfJSArrayHasAllocationMemento(rdx, rdi, fail);
  }

  // Check for empty arrays, which only require a map transition and no changes
  // to the backing store.
  __ movq(r8, FieldOperand(rdx, JSObject::kElementsOffset));
  __ CompareRoot(r8, Heap::kEmptyFixedArrayRootIndex);
  __ j(equal, &only_change_map);

  __ push(rax);

  __ movq(r8, FieldOperand(rdx, JSObject::kElementsOffset));
  __ SmiToInteger32(r9, FieldOperand(r8, FixedDoubleArray::kLengthOffset));
  // r8 : source FixedDoubleArray
  // r9 : number of elements
  __ lea(rdi, Operand(r9, times_pointer_size, FixedArray::kHeaderSize));
  __ Allocate(rdi, r11, r14, r15, &gc_required, TAG_OBJECT);
  // r11: destination FixedArray
  __ LoadRoot(rdi, Heap::kFixedArrayMapRootIndex);
  __ movq(FieldOperand(r11, HeapObject::kMapOffset), rdi);
  __ Integer32ToSmi(r14, r9);
  __ movq(FieldOperand(r11, FixedArray::kLengthOffset), r14);

  // Prepare for conversion loop.
  __ movq(rsi, BitCast<int64_t, uint64_t>(kHoleNanInt64), RelocInfo::NONE64);
  __ LoadRoot(rdi, Heap::kTheHoleValueRootIndex);
  // rsi: the-hole NaN
  // rdi: pointer to the-hole
  __ jmp(&entry);

  // Call into runtime if GC is required.
  __ bind(&gc_required);
  __ pop(rax);
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));
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
  __ MoveDouble(FieldOperand(rax, HeapNumber::kValueOffset), r14);
  __ movq(FieldOperand(r11,
                       r9,
                       times_pointer_size,
                       FixedArray::kHeaderSize),
          rax);
  __ movq(r15, r9);
  __ RecordWriteArray(r11,
                      rax,
                      r15,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ jmp(&entry, Label::kNear);

  // Replace the-hole NaN with the-hole pointer.
  __ bind(&convert_hole);
  __ movq(FieldOperand(r11,
                       r9,
                       times_pointer_size,
                       FixedArray::kHeaderSize),
          rdi);

  __ bind(&entry);
  __ decq(r9);
  __ j(not_sign, &loop);

  // Replace receiver's backing store with newly created and filled FixedArray.
  __ movq(FieldOperand(rdx, JSObject::kElementsOffset), r11);
  __ RecordWriteField(rdx,
                      JSObject::kElementsOffset,
                      r11,
                      r15,
                      kDontSaveFPRegs,
                      EMIT_REMEMBERED_SET,
                      OMIT_SMI_CHECK);
  __ pop(rax);
  __ movq(rsi, Operand(rbp, StandardFrameConstants::kContextOffset));

  __ bind(&only_change_map);
  // Set transitioned map.
  __ movq(FieldOperand(rdx, HeapObject::kMapOffset), rbx);
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
  __ movq(result, FieldOperand(string, HeapObject::kMapOffset));
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
  __ addq(index, result);
  __ movq(string, FieldOperand(string, SlicedString::kParentOffset));
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
  __ movq(string, FieldOperand(string, ConsString::kFirstOffset));

  __ bind(&indirect_string_loaded);
  __ movq(result, FieldOperand(string, HeapObject::kMapOffset));
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
  Label ascii_external, done;
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ testb(result, Immediate(kIsIndirectStringMask));
    __ Assert(zero, kExternalStringExpectedButNotFound);
  }
  // Rule out short external strings.
  STATIC_CHECK(kShortExternalStringTag != 0);
  __ testb(result, Immediate(kShortExternalStringTag));
  __ j(not_zero, call_runtime);
  // Check encoding.
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ testb(result, Immediate(kStringEncodingMask));
  __ movq(result, FieldOperand(string, ExternalString::kResourceDataOffset));
  __ j(not_equal, &ascii_external, Label::kNear);
  // Two-byte string.
  __ movzxwl(result, Operand(result, index, times_2, 0));
  __ jmp(&done, Label::kNear);
  __ bind(&ascii_external);
  // Ascii string.
  __ movzxbl(result, Operand(result, index, times_1, 0));
  __ jmp(&done, Label::kNear);

  // Dispatch on the encoding: ASCII or two-byte.
  Label ascii;
  __ bind(&seq_string);
  STATIC_ASSERT((kStringEncodingMask & kOneByteStringTag) != 0);
  STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);
  __ testb(result, Immediate(kStringEncodingMask));
  __ j(not_zero, &ascii, Label::kNear);

  // Two-byte string.
  // Load the two-byte character code into the result register.
  STATIC_ASSERT(kSmiTag == 0 && kSmiTagSize == 1);
  __ movzxwl(result, FieldOperand(string,
                                  index,
                                  times_2,
                                  SeqTwoByteString::kHeaderSize));
  __ jmp(&done, Label::kNear);

  // ASCII string.
  // Load the byte into the result register.
  __ bind(&ascii);
  __ movzxbl(result, FieldOperand(string,
                                  index,
                                  times_1,
                                  SeqOneByteString::kHeaderSize));
  __ bind(&done);
}


void MathExpGenerator::EmitMathExp(MacroAssembler* masm,
                                   XMMRegister input,
                                   XMMRegister result,
                                   XMMRegister double_scratch,
                                   Register temp1,
                                   Register temp2) {
  ASSERT(!input.is(result));
  ASSERT(!input.is(double_scratch));
  ASSERT(!result.is(double_scratch));
  ASSERT(!temp1.is(temp2));
  ASSERT(ExternalReference::math_exp_constants(0).address() != NULL);

  Label done;

  __ movq(kScratchRegister, ExternalReference::math_exp_constants(0));
  __ movsd(double_scratch, Operand(kScratchRegister, 0 * kDoubleSize));
  __ xorpd(result, result);
  __ ucomisd(double_scratch, input);
  __ j(above_equal, &done);
  __ ucomisd(input, Operand(kScratchRegister, 1 * kDoubleSize));
  __ movsd(result, Operand(kScratchRegister, 2 * kDoubleSize));
  __ j(above_equal, &done);
  __ movsd(double_scratch, Operand(kScratchRegister, 3 * kDoubleSize));
  __ movsd(result, Operand(kScratchRegister, 4 * kDoubleSize));
  __ mulsd(double_scratch, input);
  __ addsd(double_scratch, result);
  __ movq(temp2, double_scratch);
  __ subsd(double_scratch, result);
  __ movsd(result, Operand(kScratchRegister, 6 * kDoubleSize));
  __ lea(temp1, Operand(temp2, 0x1ff800));
  __ and_(temp2, Immediate(0x7ff));
  __ shr(temp1, Immediate(11));
  __ mulsd(double_scratch, Operand(kScratchRegister, 5 * kDoubleSize));
  __ movq(kScratchRegister, ExternalReference::math_exp_log_table());
  __ shl(temp1, Immediate(52));
  __ or_(temp1, Operand(kScratchRegister, temp2, times_8, 0));
  __ movq(kScratchRegister, ExternalReference::math_exp_constants(0));
  __ subsd(double_scratch, input);
  __ movsd(input, double_scratch);
  __ subsd(result, double_scratch);
  __ mulsd(input, double_scratch);
  __ mulsd(result, input);
  __ movq(input, temp1);
  __ mulsd(result, Operand(kScratchRegister, 7 * kDoubleSize));
  __ subsd(result, double_scratch);
  __ addsd(result, Operand(kScratchRegister, 8 * kDoubleSize));
  __ mulsd(result, input);

  __ bind(&done);
}

#undef __


static byte* GetNoCodeAgeSequence(uint32_t* length) {
  static bool initialized = false;
  static byte sequence[kNoCodeAgeSequenceLength];
  *length = kNoCodeAgeSequenceLength;
  if (!initialized) {
    // The sequence of instructions that is patched out for aging code is the
    // following boilerplate stack-building prologue that is found both in
    // FUNCTION and OPTIMIZED_FUNCTION code:
    CodePatcher patcher(sequence, kNoCodeAgeSequenceLength);
    patcher.masm()->push(rbp);
    patcher.masm()->movq(rbp, rsp);
    patcher.masm()->push(rsi);
    patcher.masm()->push(rdi);
    initialized = true;
  }
  return sequence;
}


bool Code::IsYoungSequence(byte* sequence) {
  uint32_t young_length;
  byte* young_sequence = GetNoCodeAgeSequence(&young_length);
  bool result = (!memcmp(sequence, young_sequence, young_length));
  ASSERT(result || *sequence == kCallOpcode);
  return result;
}


void Code::GetCodeAgeAndParity(byte* sequence, Age* age,
                               MarkingParity* parity) {
  if (IsYoungSequence(sequence)) {
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
  uint32_t young_length;
  byte* young_sequence = GetNoCodeAgeSequence(&young_length);
  if (age == kNoAgeCodeAge) {
    CopyBytes(sequence, young_sequence, young_length);
    CPU::FlushICache(sequence, young_length);
  } else {
    Code* stub = GetCodeAgeStub(isolate, age, parity);
    CodePatcher patcher(sequence, young_length);
    patcher.masm()->call(stub->instruction_start());
    patcher.masm()->Nop(
        kNoCodeAgeSequenceLength - Assembler::kShortCallInstructionLength);
  }
}


Operand StackArgumentsAccessor::GetArgumentOperand(int index) {
  ASSERT(index >= 0);
  int receiver = (receiver_mode_ == ARGUMENTS_CONTAIN_RECEIVER) ? 1 : 0;
  int displacement_to_last_argument = base_reg_.is(rsp) ?
      kPCOnStackSize : kFPOnStackSize + kPCOnStackSize;
  displacement_to_last_argument += extra_displacement_to_last_argument_;
  if (argument_count_reg_.is(no_reg)) {
    // argument[0] is at base_reg_ + displacement_to_last_argument +
    // (argument_count_immediate_ + receiver - 1) * kPointerSize.
    ASSERT(argument_count_immediate_ + receiver > 0);
    return Operand(base_reg_, displacement_to_last_argument +
        (argument_count_immediate_ + receiver - 1 - index) * kPointerSize);
  } else {
    // argument[0] is at base_reg_ + displacement_to_last_argument +
    // argument_count_reg_ * times_pointer_size + (receiver - 1) * kPointerSize.
    return Operand(base_reg_, argument_count_reg_, times_pointer_size,
        displacement_to_last_argument + (receiver - 1 - index) * kPointerSize);
  }
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
