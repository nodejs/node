// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/s390/codegen-s390.h"

#if V8_TARGET_ARCH_S390

#include <memory>

#include "src/codegen.h"
#include "src/macro-assembler.h"
#include "src/s390/simulator-s390.h"

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

  __ MovFromFloatParameter(d0);
  __ sqdbr(d0, d0);
  __ MovToFloatResult(d0);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  DCHECK(ABI_USES_FUNCTION_DESCRIPTORS ||
         !RelocInfo::RequiresRelocation(isolate, desc));

  Assembler::FlushICache(isolate, buffer, actual_size);
  base::OS::ProtectCode(buffer, actual_size);
  return FUNCTION_CAST<UnaryMathFunctionWithIsolate>(buffer);
#endif
}

#undef __

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
  __ LoadlB(result, FieldMemOperand(result, Map::kInstanceTypeOffset));

  // We need special handling for indirect strings.
  Label check_sequential;
  __ mov(r0, Operand(kIsIndirectStringMask));
  __ AndP(r0, result);
  __ beq(&check_sequential, Label::kNear /*, cr0*/);

  // Dispatch on the indirect string shape: slice or cons.
  Label cons_string, thin_string;
  __ LoadRR(ip, result);
  __ nilf(ip, Operand(kStringRepresentationMask));
  __ CmpP(ip, Operand(kConsStringTag));
  __ beq(&cons_string);
  __ CmpP(ip, Operand(kThinStringTag));
  __ beq(&thin_string);

  // Handle slices.
  __ LoadP(result, FieldMemOperand(string, SlicedString::kOffsetOffset));
  __ LoadP(string, FieldMemOperand(string, SlicedString::kParentOffset));
  __ SmiUntag(ip, result);
  __ AddP(index, ip);
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
  __ mov(r0, Operand(kStringRepresentationMask));
  __ AndP(r0, result);
  __ bne(&external_string, Label::kNear);

  // Prepare sequential strings
  STATIC_ASSERT(SeqTwoByteString::kHeaderSize == SeqOneByteString::kHeaderSize);
  __ AddP(string, Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  __ b(&check_encoding, Label::kNear);

  // Handle external strings.
  __ bind(&external_string);
  if (FLAG_debug_code) {
    // Assert that we do not have a cons or slice (indirect strings) here.
    // Sequential strings have already been ruled out.
    __ mov(r0, Operand(kIsIndirectStringMask));
    __ AndP(r0, result);
    __ Assert(eq, kExternalStringExpectedButNotFound, cr0);
  }
  // Rule out short external strings.
  STATIC_ASSERT(kShortExternalStringTag != 0);
  __ mov(r0, Operand(kShortExternalStringMask));
  __ AndP(r0, result);
  __ bne(call_runtime /*, cr0*/);
  __ LoadP(string,
           FieldMemOperand(string, ExternalString::kResourceDataOffset));

  Label one_byte, done;
  __ bind(&check_encoding);
  STATIC_ASSERT(kTwoByteStringTag == 0);
  __ mov(r0, Operand(kStringEncodingMask));
  __ AndP(r0, result);
  __ bne(&one_byte, Label::kNear);
  // Two-byte string.
  __ ShiftLeftP(result, index, Operand(1));
  __ LoadLogicalHalfWordP(result, MemOperand(string, result));
  __ b(&done, Label::kNear);
  __ bind(&one_byte);
  // One-byte string.
  __ LoadlB(result, MemOperand(string, index));
  __ bind(&done);
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390
