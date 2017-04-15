// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/x87/codegen-x87.h"

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


UnaryMathFunctionWithIsolate CreateSqrtFunction(Isolate* isolate) {
  size_t actual_size;
  // Allocate buffer in executable space.
  byte* buffer =
      static_cast<byte*>(base::OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == nullptr) return nullptr;

  MacroAssembler masm(isolate, buffer, static_cast<int>(actual_size),
                      CodeObjectRequired::kNo);
  // Load double input into registers.
  __ fld_d(MemOperand(esp, 4));
  __ X87SetFPUCW(0x027F);
  __ fsqrt();
  __ X87SetFPUCW(0x037F);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(&desc);
  DCHECK(!RelocInfo::RequiresRelocation(desc));

  Assembler::FlushICache(isolate, buffer, actual_size);
  base::OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<UnaryMathFunctionWithIsolate>(buffer);
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


MemMoveFunction CreateMemMoveFunction(Isolate* isolate) {
  size_t actual_size;
  // Allocate buffer in executable space.
  byte* buffer =
      static_cast<byte*>(base::OS::Allocate(1 * KB, &actual_size, true));
  if (buffer == nullptr) return nullptr;
  MacroAssembler masm(isolate, buffer, static_cast<int>(actual_size),
                      CodeObjectRequired::kNo);
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
  Assembler::FlushICache(isolate, buffer, actual_size);
  base::OS::ProtectCode(buffer, actual_size);
  // TODO(jkummerow): It would be nice to register this code creation event
  // with the PROFILE / GDBJIT system.
  return FUNCTION_CAST<MemMoveFunction>(buffer);
}


#undef __

// -------------------------------------------------------------------------
// Code generators

#define __ ACCESS_MASM(masm)

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
  Label one_byte_external, done;
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ test(result, Immediate(kIsIndirectStringMask));
    __ Assert(zero, kExternalStringExpectedButNotFound);
  }
  // Rule out short external strings.
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ test_b(result, Immediate(kShortExternalStringMask));
  __ j(not_zero, call_runtime);
  // Check encoding.
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ test_b(result, Immediate(kStringEncodingMask));
  __ mov(result, FieldOperand(string, ExternalString::kResourceDataOffset));
  __ j(not_equal, &one_byte_external, Label::kNear);
  // Two-byte string.
  __ movzx_w(result, Operand(result, index, times_2, 0));
  __ jmp(&done, Label::kNear);
  __ bind(&one_byte_external);
  // One-byte string.
  __ movzx_b(result, Operand(result, index, times_1, 0));
  __ jmp(&done, Label::kNear);

  // Dispatch on the encoding: one-byte or two-byte.
  Label one_byte;
  __ bind(&seq_string);
  STATIC_ASSERT((kStringEncodingMask & kOneByteStringTag) != 0);
  STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);
  __ test(result, Immediate(kStringEncodingMask));
  __ j(not_zero, &one_byte, Label::kNear);

  // Two-byte string.
  // Load the two-byte character code into the result register.
  __ movzx_w(result, FieldOperand(string,
                                  index,
                                  times_2,
                                  SeqTwoByteString::kHeaderSize));
  __ jmp(&done, Label::kNear);

  // One-byte string.
  // Load the byte into the result register.
  __ bind(&one_byte);
  __ movzx_b(result, FieldOperand(string,
                                  index,
                                  times_1,
                                  SeqOneByteString::kHeaderSize));
  __ bind(&done);
}


#undef __


CodeAgingHelper::CodeAgingHelper(Isolate* isolate) {
  USE(isolate);
  DCHECK(young_sequence_.length() == kNoCodeAgeSequenceLength);
  CodePatcher patcher(isolate, young_sequence_.start(),
                      young_sequence_.length());
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

Code::Age Code::GetCodeAge(Isolate* isolate, byte* sequence) {
  if (IsYoungSequence(isolate, sequence)) return kNoAgeCodeAge;

  sequence++;  // Skip the kCallOpcode byte
  Address target_address = sequence + *reinterpret_cast<int*>(sequence) +
                           Assembler::kCallTargetAddressOffset;
  Code* stub = GetCodeFromTargetAddress(target_address);
  return GetAgeOfCodeAgeStub(stub);
}

void Code::PatchPlatformCodeAge(Isolate* isolate, byte* sequence,
                                Code::Age age) {
  uint32_t young_length = isolate->code_aging_helper()->young_sequence_length();
  if (age == kNoAgeCodeAge) {
    isolate->code_aging_helper()->CopyYoungSequenceTo(sequence);
    Assembler::FlushICache(isolate, sequence, young_length);
  } else {
    Code* stub = GetCodeAgeStub(isolate, age);
    CodePatcher patcher(isolate, sequence, young_length);
    patcher.masm()->call(stub->instruction_start(), RelocInfo::NONE32);
  }
}


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X87
