// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/arm64/codegen-arm64.h"

#if V8_TARGET_ARCH_ARM64

#include "src/arm64/assembler-arm64-inl.h"
#include "src/arm64/macro-assembler-arm64-inl.h"
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
// Code generators

void StringCharLoadGenerator::Generate(MacroAssembler* masm,
                                       Register string,
                                       Register index,
                                       Register result,
                                       Label* call_runtime) {
  DCHECK(string.Is64Bits() && index.Is32Bits() && result.Is64Bits());
  Label indirect_string_loaded;
  __ Bind(&indirect_string_loaded);

  // Fetch the instance type of the receiver into result register.
  __ Ldr(result, FieldMemOperand(string, HeapObject::kMapOffset));
  __ Ldrb(result, FieldMemOperand(result, Map::kInstanceTypeOffset));

  // We need special handling for indirect strings.
  Label check_sequential;
  __ TestAndBranchIfAllClear(result, kIsIndirectStringMask, &check_sequential);

  // Dispatch on the indirect string shape: slice or cons.
  Label cons_string, thin_string;
  __ And(result, result, kStringRepresentationMask);
  __ Cmp(result, kConsStringTag);
  __ B(eq, &cons_string);
  __ Cmp(result, kThinStringTag);
  __ B(eq, &thin_string);

  // Handle slices.
  __ Ldr(result.W(),
         UntagSmiFieldMemOperand(string, SlicedString::kOffsetOffset));
  __ Ldr(string, FieldMemOperand(string, SlicedString::kParentOffset));
  __ Add(index, index, result.W());
  __ B(&indirect_string_loaded);

  // Handle thin strings.
  __ Bind(&thin_string);
  __ Ldr(string, FieldMemOperand(string, ThinString::kActualOffset));
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
  __ B(&indirect_string_loaded);

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
