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
  DCHECK(ABI_USES_FUNCTION_DESCRIPTORS ||
         !RelocInfo::RequiresRelocation(isolate, desc));

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

// assume ip can be used as a scratch register below
void StringCharLoadGenerator::Generate(MacroAssembler* masm, Register string,
                                       Register index, Register result,
                                       Label* call_runtime) {
  Label indirect_string_loaded;
  __ bind(&indirect_string_loaded);

  // Fetch the instance type of the receiver into result register.
  __ LoadP(result, FieldMemOperand(string, HeapObject::kMapOffset));
  __ lbz(result, FieldMemOperand(result, Map::kInstanceTypeOffset));

  // We need special handling for indirect strings.
  Label check_sequential;
  __ andi(r0, result, Operand(kIsIndirectStringMask));
  __ beq(&check_sequential, cr0);

  // Dispatch on the indirect string shape: slice or cons or thin.
  Label cons_string, thin_string;
  __ andi(ip, result, Operand(kStringRepresentationMask));
  __ cmpi(ip, Operand(kConsStringTag));
  __ beq(&cons_string);
  __ cmpi(ip, Operand(kThinStringTag));
  __ beq(&thin_string);

  // Handle slices.
  __ LoadP(result, FieldMemOperand(string, SlicedString::kOffsetOffset));
  __ LoadP(string, FieldMemOperand(string, SlicedString::kParentOffset));
  __ SmiUntag(ip, result);
  __ add(index, index, ip);
  __ b(&indirect_string_loaded);

  // Handle thin strings.
  __ bind(&thin_string);
  __ LoadP(string, FieldMemOperand(string, ThinString::kActualOffset));
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
  __ b(&indirect_string_loaded);

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

Code::Age Code::GetCodeAge(Isolate* isolate, byte* sequence) {
  if (IsYoungSequence(isolate, sequence)) return kNoAgeCodeAge;

  Code* code = NULL;
  Address target_address =
      Assembler::target_address_at(sequence + kCodeAgingTargetDelta, code);
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
    // FIXED_SEQUENCE
    Code* stub = GetCodeAgeStub(isolate, age);
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
