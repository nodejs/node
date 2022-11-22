// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/interface-descriptors-inl.h"
#include "src/maglev/maglev-assembler-inl.h"
#include "src/objects/heap-number.h"

namespace v8 {
namespace internal {
namespace maglev {

#define __ masm->

void MaglevAssembler::Allocate(RegisterSnapshot& register_snapshot,
                               Register object, int size_in_bytes,
                               AllocationType alloc_type,
                               AllocationAlignment alignment) {
  // TODO(victorgomes): Call the runtime for large object allocation.
  // TODO(victorgomes): Support double alignment.
  DCHECK_EQ(alignment, kTaggedAligned);
  size_in_bytes = ALIGN_TO_ALLOCATION_ALIGNMENT(size_in_bytes);
  if (v8_flags.single_generation) {
    alloc_type = AllocationType::kOld;
  }
  bool in_new_space = alloc_type == AllocationType::kYoung;
  ExternalReference top =
      in_new_space
          ? ExternalReference::new_space_allocation_top_address(isolate_)
          : ExternalReference::old_space_allocation_top_address(isolate_);
  ExternalReference limit =
      in_new_space
          ? ExternalReference::new_space_allocation_limit_address(isolate_)
          : ExternalReference::old_space_allocation_limit_address(isolate_);

  ZoneLabelRef done(this);
  Register new_top = kScratchRegister;
  // Check if there is enough space.
  Move(object, ExternalReferenceAsOperand(top));
  leaq(new_top, Operand(object, size_in_bytes));
  cmpq(new_top, ExternalReferenceAsOperand(limit));
  // Otherwise call runtime.
  JumpToDeferredIf(
      greater_equal,
      [](MaglevAssembler* masm, RegisterSnapshot register_snapshot,
         Register object, Builtin builtin, int size_in_bytes,
         ZoneLabelRef done) {
        // Remove {object} from snapshot, since it is the returned allocated
        // HeapObject.
        register_snapshot.live_registers.clear(object);
        register_snapshot.live_tagged_registers.clear(object);
        {
          SaveRegisterStateForCall save_register_state(masm, register_snapshot);
          using D = AllocateDescriptor;
          __ Move(D::GetRegisterParameter(D::kRequestedSize), size_in_bytes);
          __ CallBuiltin(builtin);
          save_register_state.DefineSafepoint();
          __ Move(object, kReturnRegister0);
        }
        __ jmp(*done);
      },
      register_snapshot, object,
      in_new_space ? Builtin::kAllocateRegularInYoungGeneration
                   : Builtin::kAllocateRegularInOldGeneration,
      size_in_bytes, done);
  // Store new top and tag object.
  movq(ExternalReferenceAsOperand(top), new_top);
  addq(object, Immediate(kHeapObjectTag));
  bind(*done);
}

void MaglevAssembler::AllocateTwoByteString(RegisterSnapshot register_snapshot,
                                            Register result, int length) {
  Allocate(register_snapshot, result, SeqTwoByteString::SizeFor(length));
  LoadRoot(kScratchRegister, RootIndex::kStringMap);
  StoreTaggedField(FieldOperand(result, HeapObject::kMapOffset),
                   kScratchRegister);
  StoreTaggedField(FieldOperand(result, Name::kRawHashFieldOffset),
                   Immediate(Name::kEmptyHashField));
  StoreTaggedField(FieldOperand(result, String::kLengthOffset),
                   Immediate(length));
}

void MaglevAssembler::LoadSingleCharacterString(Register result,
                                                int char_code) {
  DCHECK_GE(char_code, 0);
  DCHECK_LT(char_code, String::kMaxOneByteCharCode);
  Register table = result;
  LoadRoot(table, RootIndex::kSingleCharacterStringTable);
  DecompressAnyTagged(result, FieldOperand(table, FixedArray::kHeaderSize +
                                                      char_code * kTaggedSize));
}

void MaglevAssembler::LoadSingleCharacterString(Register result,
                                                Register char_code,
                                                Register scratch) {
  if (v8_flags.debug_code) {
    cmpl(char_code, Immediate(String::kMaxOneByteCharCode));
    Assert(below_equal, AbortReason::kUnexpectedValue);
  }
  DCHECK_NE(char_code, scratch);
  Register table = scratch;
  LoadRoot(table, RootIndex::kSingleCharacterStringTable);
  DecompressAnyTagged(result, FieldOperand(table, char_code, times_tagged_size,
                                           FixedArray::kHeaderSize));
}

void MaglevAssembler::StringFromCharCode(RegisterSnapshot register_snapshot,
                                         Label* char_code_fits_one_byte,
                                         Register result, Register char_code,
                                         Register scratch) {
  DCHECK_NE(char_code, scratch);
  ZoneLabelRef done(this);
  cmpl(char_code, Immediate(String::kMaxOneByteCharCode));
  JumpToDeferredIf(
      above,
      [](MaglevAssembler* masm, RegisterSnapshot register_snapshot,
         ZoneLabelRef done, Register result, Register char_code,
         Register scratch) {
        // Be sure to save {char_code}. If it aliases with {result}, use
        // the scratch register.
        if (char_code == result) {
          // This is guaranteed to be true since we've already checked
          // char_code != scratch.
          DCHECK_NE(scratch, result);
          __ Move(scratch, char_code);
          char_code = scratch;
        }
        DCHECK(!register_snapshot.live_tagged_registers.has(char_code));
        register_snapshot.live_registers.set(char_code);
        __ AllocateTwoByteString(register_snapshot, result, 1);
        __ andl(char_code, Immediate(0xFFFF));
        __ movw(FieldOperand(result, SeqTwoByteString::kHeaderSize), char_code);
        __ jmp(*done);
      },
      register_snapshot, done, result, char_code, scratch);
  if (char_code_fits_one_byte != nullptr) {
    bind(char_code_fits_one_byte);
  }
  LoadSingleCharacterString(result, char_code, scratch);
  bind(*done);
}

void MaglevAssembler::StringCharCodeAt(RegisterSnapshot& register_snapshot,
                                       Register result, Register string,
                                       Register index, Register scratch,
                                       Label* result_fits_one_byte) {
  ZoneLabelRef done(this);
  Label seq_string;
  Label cons_string;
  Label sliced_string;

  DeferredCodeInfo* deferred_runtime_call = PushDeferredCode(
      [](MaglevAssembler* masm, RegisterSnapshot register_snapshot,
         ZoneLabelRef done, Register result, Register string, Register index) {
        DCHECK(!register_snapshot.live_registers.has(result));
        DCHECK(!register_snapshot.live_registers.has(string));
        DCHECK(!register_snapshot.live_registers.has(index));
        {
          SaveRegisterStateForCall save_register_state(masm, register_snapshot);
          __ Push(string);
          __ SmiTag(index);
          __ Push(index);
          __ Move(kContextRegister, masm->native_context().object());
          // This call does not throw nor can deopt.
          __ CallRuntime(Runtime::kStringCharCodeAt);
          __ SmiUntag(kReturnRegister0);
          __ Move(result, kReturnRegister0);
        }
        __ jmp(*done);
      },
      register_snapshot, done, result, string, index);

  Register instance_type = scratch;

  // We might need to try more than one time for ConsString, SlicedString and
  // ThinString.
  Label loop;
  bind(&loop);

  if (v8_flags.debug_code) {
    // Check if {string} is a string.
    AssertNotSmi(string);
    LoadMap(scratch, string);
    CmpInstanceTypeRange(scratch, scratch, FIRST_STRING_TYPE, LAST_STRING_TYPE);
    Check(below_equal, AbortReason::kUnexpectedValue);

    movl(scratch, FieldOperand(string, String::kLengthOffset));
    cmpl(index, scratch);
    Check(below, AbortReason::kUnexpectedValue);
  }

  // Get instance type.
  LoadMap(instance_type, string);
  mov_tagged(instance_type,
             FieldOperand(instance_type, Map::kInstanceTypeOffset));

  {
    // TODO(victorgomes): Add fast path for external strings.
    Register representation = kScratchRegister;
    movl(representation, instance_type);
    andl(representation, Immediate(kStringRepresentationMask));
    cmpl(representation, Immediate(kSeqStringTag));
    j(equal, &seq_string, Label::kNear);
    cmpl(representation, Immediate(kConsStringTag));
    j(equal, &cons_string, Label::kNear);
    cmpl(representation, Immediate(kSlicedStringTag));
    j(equal, &sliced_string, Label::kNear);
    cmpl(representation, Immediate(kThinStringTag));
    j(not_equal, &deferred_runtime_call->deferred_code_label);
    // Fallthrough to thin string.
  }

  // Is a thin string.
  {
    DecompressAnyTagged(string,
                        FieldOperand(string, ThinString::kActualOffset));
    jmp(&loop, Label::kNear);
  }

  bind(&sliced_string);
  {
    Register offset = scratch;
    movl(offset, FieldOperand(string, SlicedString::kOffsetOffset));
    SmiUntag(offset);
    DecompressAnyTagged(string,
                        FieldOperand(string, SlicedString::kParentOffset));
    addl(index, offset);
    jmp(&loop, Label::kNear);
  }

  bind(&cons_string);
  {
    CompareRoot(FieldOperand(string, ConsString::kSecondOffset),
                RootIndex::kempty_string);
    j(not_equal, &deferred_runtime_call->deferred_code_label);
    DecompressAnyTagged(string, FieldOperand(string, ConsString::kFirstOffset));
    jmp(&loop, Label::kNear);  // Try again with first string.
  }

  bind(&seq_string);
  {
    Label two_byte_string;
    andl(instance_type, Immediate(kStringEncodingMask));
    cmpl(instance_type, Immediate(kTwoByteStringTag));
    j(equal, &two_byte_string, Label::kNear);
    movzxbl(result, FieldOperand(string, index, times_1,
                                 SeqOneByteString::kHeaderSize));
    jmp(result_fits_one_byte);
    bind(&two_byte_string);
    movzxwl(result, FieldOperand(string, index, times_2,
                                 SeqTwoByteString::kHeaderSize));
    // Fallthrough.
  }

  bind(*done);

  if (v8_flags.debug_code) {
    // We make sure that the user of this macro is not relying in string and
    // index to not be clobbered.
    if (result != string) {
      movl(string, Immediate(0xdeadbeef));
    }
    if (result != index) {
      movl(index, Immediate(0xdeadbeef));
    }
  }
}

void MaglevAssembler::ToBoolean(Register value, ZoneLabelRef is_true,
                                ZoneLabelRef is_false,
                                bool fallthrough_when_true) {
  Register map = kScratchRegister;

  // Check if {{value}} is Smi.
  CheckSmi(value);
  JumpToDeferredIf(
      zero,
      [](MaglevAssembler* masm, Register value, ZoneLabelRef is_true,
         ZoneLabelRef is_false) {
        // Check if {value} is not zero.
        __ SmiCompare(value, Smi::FromInt(0));
        __ j(equal, *is_false);
        __ jmp(*is_true);
      },
      value, is_true, is_false);

  // Check if {{value}} is false.
  CompareRoot(value, RootIndex::kFalseValue);
  j(equal, *is_false);

  // Check if {{value}} is empty string.
  CompareRoot(value, RootIndex::kempty_string);
  j(equal, *is_false);

  // Check if {{value}} is undetectable.
  LoadMap(map, value);
  testl(FieldOperand(map, Map::kBitFieldOffset),
        Immediate(Map::Bits1::IsUndetectableBit::kMask));
  j(not_zero, *is_false);

  // Check if {{value}} is a HeapNumber.
  CompareRoot(map, RootIndex::kHeapNumberMap);
  JumpToDeferredIf(
      equal,
      [](MaglevAssembler* masm, Register value, ZoneLabelRef is_true,
         ZoneLabelRef is_false) {
        // Sets scratch register to 0.0.
        __ Xorpd(kScratchDoubleReg, kScratchDoubleReg);
        // Sets ZF if equal to 0.0, -0.0 or NaN.
        __ Ucomisd(kScratchDoubleReg,
                   FieldOperand(value, HeapNumber::kValueOffset));
        __ j(zero, *is_false);
        __ jmp(*is_true);
      },
      value, is_true, is_false);

  // Check if {{value}} is a BigInt.
  CompareRoot(map, RootIndex::kBigIntMap);
  JumpToDeferredIf(
      equal,
      [](MaglevAssembler* masm, Register value, ZoneLabelRef is_true,
         ZoneLabelRef is_false) {
        __ testl(FieldOperand(value, BigInt::kBitfieldOffset),
                 Immediate(BigInt::LengthBits::kMask));
        __ j(zero, *is_false);
        __ jmp(*is_true);
      },
      value, is_true, is_false);

  // Otherwise true.
  if (!fallthrough_when_true) {
    jmp(*is_true);
  }
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
