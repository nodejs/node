// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/logging.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/common/globals.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/maglev/maglev-assembler-inl.h"
#include "src/maglev/maglev-assembler.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir.h"
#include "src/objects/heap-number.h"

namespace v8 {
namespace internal {
namespace maglev {

#define __ masm->

void MaglevAssembler::Allocate(RegisterSnapshot register_snapshot,
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

void MaglevAssembler::AllocateHeapNumber(RegisterSnapshot register_snapshot,
                                         Register result,
                                         DoubleRegister value) {
  // In the case we need to call the runtime, we should spill the value
  // register. Even if it is not live in the next node, otherwise the
  // allocation call might trash it.
  register_snapshot.live_double_registers.set(value);
  Allocate(register_snapshot, result, HeapNumber::kSize);
  LoadRoot(kScratchRegister, RootIndex::kHeapNumberMap);
  StoreTaggedField(FieldOperand(result, HeapObject::kMapOffset),
                   kScratchRegister);
  Movsd(FieldOperand(result, HeapNumber::kValueOffset), value);
}

void MaglevAssembler::AllocateTwoByteString(RegisterSnapshot register_snapshot,
                                            Register result, int length) {
  int size = SeqTwoByteString::SizeFor(length);
  Allocate(register_snapshot, result, size);
  LoadRoot(kScratchRegister, RootIndex::kStringMap);
  StoreTaggedField(FieldOperand(result, size - kObjectAlignment), Immediate(0));
  StoreTaggedField(FieldOperand(result, HeapObject::kMapOffset),
                   kScratchRegister);
  StoreTaggedField(FieldOperand(result, Name::kRawHashFieldOffset),
                   Immediate(Name::kEmptyHashField));
  StoreTaggedField(FieldOperand(result, String::kLengthOffset),
                   Immediate(length));
}

void MaglevAssembler::LoadSingleCharacterString(Register result,
                                                Register char_code,
                                                Register scratch) {
  AssertZeroExtended(char_code);
  if (v8_flags.debug_code) {
    cmpq(char_code, Immediate(String::kMaxOneByteCharCode));
    Assert(below_equal, AbortReason::kUnexpectedValue);
  }
  DCHECK_NE(char_code, scratch);
  Register table = scratch;
  LoadRoot(table, RootIndex::kSingleCharacterStringTable);
  DecompressTagged(result, FieldOperand(table, char_code, times_tagged_size,
                                        FixedArray::kHeaderSize));
}

void MaglevAssembler::StoreTaggedFieldWithWriteBarrier(
    Register object, int offset, Register value,
    RegisterSnapshot register_snapshot, ValueIsCompressed value_is_compressed,
    ValueCanBeSmi value_can_be_smi) {
  DCHECK_NE(object, kScratchRegister);
  DCHECK_NE(value, kScratchRegister);
  AssertNotSmi(object);
  StoreTaggedField(FieldOperand(object, offset), value);

  ZoneLabelRef done(this);
  Label* deferred_write_barrier = MakeDeferredCode(
      [](MaglevAssembler* masm, ZoneLabelRef done, Register object, int offset,
         Register value, RegisterSnapshot register_snapshot,
         ValueIsCompressed value_is_compressed) {
        ASM_CODE_COMMENT_STRING(masm, "Write barrier slow path");
        if (value_is_compressed == kValueIsCompressed) {
          __ DecompressTagged(value, value);
        }

        // Use the value as the scratch register if possible, since
        // CheckPageFlag emits slightly better code when value == scratch.
        Register scratch = kScratchRegister;
        if (value != object && !register_snapshot.live_registers.has(value)) {
          scratch = value;
        }
        __ CheckPageFlag(value, scratch,
                         MemoryChunk::kPointersToHereAreInterestingMask, zero,
                         *done);

        Register stub_object_reg = WriteBarrierDescriptor::ObjectRegister();
        Register slot_reg = WriteBarrierDescriptor::SlotAddressRegister();

        RegList saved;
        if (object != stub_object_reg &&
            register_snapshot.live_registers.has(stub_object_reg)) {
          saved.set(stub_object_reg);
        }
        if (register_snapshot.live_registers.has(slot_reg)) {
          saved.set(slot_reg);
        }

        __ PushAll(saved);

        if (object != stub_object_reg) {
          __ Move(stub_object_reg, object);
          object = stub_object_reg;
        }
        __ leaq(slot_reg, FieldOperand(object, offset));

        SaveFPRegsMode const save_fp_mode =
            !register_snapshot.live_double_registers.is_empty()
                ? SaveFPRegsMode::kSave
                : SaveFPRegsMode::kIgnore;

        __ CallRecordWriteStub(object, slot_reg, save_fp_mode);

        __ PopAll(saved);
        __ jmp(*done);
      },
      done, object, offset, value, register_snapshot, value_is_compressed);

  if (value_can_be_smi == kValueCanBeSmi) {
    JumpIfSmi(value, *done);
  } else {
    AssertNotSmi(value);
  }
  CheckPageFlag(object, kScratchRegister,
                MemoryChunk::kPointersFromHereAreInterestingMask, not_zero,
                deferred_write_barrier);
  bind(*done);
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

void MaglevAssembler::StringCharCodeOrCodePointAt(
    BuiltinStringPrototypeCharCodeOrCodePointAt::Mode mode,
    RegisterSnapshot& register_snapshot, Register result, Register string,
    Register index, Register scratch, Label* result_fits_one_byte) {
  ZoneLabelRef done(this);
  Label seq_string;
  Label cons_string;
  Label sliced_string;

  Label* deferred_runtime_call = MakeDeferredCode(
      [](MaglevAssembler* masm,
         BuiltinStringPrototypeCharCodeOrCodePointAt::Mode mode,
         RegisterSnapshot register_snapshot, ZoneLabelRef done, Register result,
         Register string, Register index) {
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
          if (mode ==
              BuiltinStringPrototypeCharCodeOrCodePointAt::kCodePointAt) {
            __ CallRuntime(Runtime::kStringCodePointAt);
          } else {
            DCHECK_EQ(mode,
                      BuiltinStringPrototypeCharCodeOrCodePointAt::kCharCodeAt);
            __ CallRuntime(Runtime::kStringCharCodeAt);
          }
          save_register_state.DefineSafepoint();
          __ SmiUntag(kReturnRegister0);
          __ Move(result, kReturnRegister0);
        }
        __ jmp(*done);
      },
      mode, register_snapshot, done, result, string, index);

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
    j(not_equal, deferred_runtime_call);
    // Fallthrough to thin string.
  }

  // Is a thin string.
  {
    DecompressTagged(string, FieldOperand(string, ThinString::kActualOffset));
    jmp(&loop, Label::kNear);
  }

  bind(&sliced_string);
  {
    Register offset = scratch;
    movl(offset, FieldOperand(string, SlicedString::kOffsetOffset));
    SmiUntag(offset);
    DecompressTagged(string, FieldOperand(string, SlicedString::kParentOffset));
    addl(index, offset);
    jmp(&loop, Label::kNear);
  }

  bind(&cons_string);
  {
    CompareRoot(FieldOperand(string, ConsString::kSecondOffset),
                RootIndex::kempty_string);
    j(not_equal, deferred_runtime_call);
    DecompressTagged(string, FieldOperand(string, ConsString::kFirstOffset));
    jmp(&loop, Label::kNear);  // Try again with first string.
  }

  bind(&seq_string);
  {
    Label two_byte_string;
    andl(instance_type, Immediate(kStringEncodingMask));
    cmpl(instance_type, Immediate(kTwoByteStringTag));
    j(equal, &two_byte_string, Label::kNear);
    // The result of one-byte string will be the same for both modes
    // (CharCodeAt/CodePointAt), since it cannot be the first half of a
    // surrogate pair.
    movzxbl(result, FieldOperand(string, index, times_1,
                                 SeqOneByteString::kHeaderSize));
    jmp(result_fits_one_byte);
    bind(&two_byte_string);
    movzxwl(result, FieldOperand(string, index, times_2,
                                 SeqTwoByteString::kHeaderSize));

    if (mode == BuiltinStringPrototypeCharCodeOrCodePointAt::kCodePointAt) {
      Register first_code_point = scratch;
      movl(first_code_point, result);
      andl(first_code_point, Immediate(0xfc00));
      cmpl(first_code_point, Immediate(0xd800));
      j(not_equal, *done);

      Register length = scratch;
      StringLength(length, string);
      incl(index);
      cmpl(index, length);
      j(greater_equal, *done);

      Register second_code_point = scratch;
      movzxwl(second_code_point, FieldOperand(string, index, times_2,
                                              SeqTwoByteString::kHeaderSize));

      // {index} is not needed at this point.
      Register scratch2 = index;
      movl(scratch2, second_code_point);
      andl(scratch2, Immediate(0xfc00));
      cmpl(scratch2, Immediate(0xdc00));
      j(not_equal, *done);

      int surrogate_offset = 0x10000 - (0xd800 << 10) - 0xdc00;
      addl(second_code_point, Immediate(surrogate_offset));
      shll(result, Immediate(10));
      addl(result, second_code_point);
    }

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

void MaglevAssembler::TestTypeOf(
    Register object, interpreter::TestTypeOfFlags::LiteralFlag literal,
    Label* is_true, Label::Distance true_distance, bool fallthrough_when_true,
    Label* is_false, Label::Distance false_distance,
    bool fallthrough_when_false) {
  // If both true and false are fallthroughs, we don't have to do anything.
  if (fallthrough_when_true && fallthrough_when_false) return;

  // IMPORTANT: Note that `object` could be a register that aliases registers in
  // the ScratchRegisterScope. Make sure that all reads of `object` are before
  // any writes to scratch registers
  using LiteralFlag = interpreter::TestTypeOfFlags::LiteralFlag;
  switch (literal) {
    case LiteralFlag::kNumber:
      JumpIfSmi(object, is_true, true_distance);
      CompareRoot(FieldOperand(object, HeapObject::kMapOffset),
                  RootIndex::kHeapNumberMap);
      Branch(equal, is_true, true_distance, fallthrough_when_true, is_false,
             false_distance, fallthrough_when_false);
      return;
    case LiteralFlag::kString: {
      MaglevAssembler::ScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      JumpIfSmi(object, is_false, false_distance);
      LoadMap(scratch, object);
      cmpw(FieldOperand(scratch, Map::kInstanceTypeOffset),
           Immediate(LAST_STRING_TYPE));
      Branch(less_equal, is_true, true_distance, fallthrough_when_true,
             is_false, false_distance, fallthrough_when_false);
      return;
    }
    case LiteralFlag::kSymbol: {
      MaglevAssembler::ScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      JumpIfSmi(object, is_false, false_distance);
      LoadMap(scratch, object);
      cmpw(FieldOperand(scratch, Map::kInstanceTypeOffset),
           Immediate(SYMBOL_TYPE));
      Branch(equal, is_true, true_distance, fallthrough_when_true, is_false,
             false_distance, fallthrough_when_false);
      return;
    }
    case LiteralFlag::kBoolean:
      CompareRoot(object, RootIndex::kTrueValue);
      JumpIf(equal, is_true, true_distance);
      CompareRoot(object, RootIndex::kFalseValue);
      Branch(equal, is_true, true_distance, fallthrough_when_true, is_false,
             false_distance, fallthrough_when_false);
      return;
    case LiteralFlag::kBigInt: {
      MaglevAssembler::ScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      JumpIfSmi(object, is_false, false_distance);
      LoadMap(scratch, object);
      cmpw(FieldOperand(scratch, Map::kInstanceTypeOffset),
           Immediate(BIGINT_TYPE));
      Branch(equal, is_true, true_distance, fallthrough_when_true, is_false,
             false_distance, fallthrough_when_false);
      return;
    }
    case LiteralFlag::kUndefined: {
      JumpIfSmi(object, is_false, false_distance);
      // Check it has the undetectable bit set and it is not null.
      LoadMap(kScratchRegister, object);
      testl(FieldOperand(kScratchRegister, Map::kBitFieldOffset),
            Immediate(Map::Bits1::IsUndetectableBit::kMask));
      JumpIf(zero, is_false, false_distance);
      CompareRoot(object, RootIndex::kNullValue);
      Branch(not_equal, is_true, true_distance, fallthrough_when_true, is_false,
             false_distance, fallthrough_when_false);
      return;
    }
    case LiteralFlag::kFunction: {
      MaglevAssembler::ScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      JumpIfSmi(object, is_false, false_distance);
      // Check if callable bit is set and not undetectable.
      LoadMap(scratch, object);
      movl(scratch, FieldOperand(scratch, Map::kBitFieldOffset));
      andl(scratch, Immediate(Map::Bits1::IsUndetectableBit::kMask |
                              Map::Bits1::IsCallableBit::kMask));
      cmpl(scratch, Immediate(Map::Bits1::IsCallableBit::kMask));
      Branch(equal, is_true, true_distance, fallthrough_when_true, is_false,
             false_distance, fallthrough_when_false);
      return;
    }
    case LiteralFlag::kObject: {
      MaglevAssembler::ScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      JumpIfSmi(object, is_false, false_distance);
      // If the object is null then return true.
      CompareRoot(object, RootIndex::kNullValue);
      JumpIf(equal, is_true, true_distance);
      // Check if the object is a receiver type,
      LoadMap(scratch, object);
      cmpw(FieldOperand(scratch, Map::kInstanceTypeOffset),
           Immediate(FIRST_JS_RECEIVER_TYPE));
      JumpIf(less, is_false, false_distance);
      // ... and is not undefined (undetectable) nor callable.
      testl(FieldOperand(scratch, Map::kBitFieldOffset),
            Immediate(Map::Bits1::IsUndetectableBit::kMask |
                      Map::Bits1::IsCallableBit::kMask));
      Branch(equal, is_true, true_distance, fallthrough_when_true, is_false,
             false_distance, fallthrough_when_false);
      return;
    }
    case LiteralFlag::kOther:
      if (!fallthrough_when_false) {
        Jump(is_false, false_distance);
      }
      return;
  }
  UNREACHABLE();
}

void MaglevAssembler::TruncateDoubleToInt32(Register dst, DoubleRegister src) {
  ZoneLabelRef done(this);

  Cvttsd2siq(dst, src);
  // Check whether the Cvt overflowed.
  cmpq(dst, Immediate(1));
  JumpToDeferredIf(
      overflow,
      [](MaglevAssembler* masm, DoubleRegister src, Register dst,
         ZoneLabelRef done) {
        // Push the double register onto the stack as an input argument.
        __ AllocateStackSpace(kDoubleSize);
        __ Movsd(MemOperand(rsp, 0), src);
        __ CallBuiltin(Builtin::kDoubleToI);
        // DoubleToI sets the result on the stack, pop the result off the stack.
        // Avoid using `pop` to not mix implicit and explicit rsp updates.
        __ movl(dst, MemOperand(rsp, 0));
        __ addq(rsp, Immediate(kDoubleSize));
        __ jmp(*done);
      },
      src, dst, done);
  bind(*done);
  // Zero extend the converted value to complete the truncation.
  movl(dst, dst);
}

void MaglevAssembler::TryTruncateDoubleToInt32(Register dst, DoubleRegister src,
                                               Label* fail) {
  DoubleRegister converted_back = kScratchDoubleReg;

  // Convert the input float64 value to int32.
  Cvttsd2si(dst, src);
  // Convert that int32 value back to float64.
  Cvtlsi2sd(converted_back, dst);
  // Check that the result of the float64->int32->float64 is equal to the input
  // (i.e. that the conversion didn't truncate.
  Ucomisd(src, converted_back);
  JumpIf(parity_even, fail);
  JumpIf(not_equal, fail);

  // Check if {input} is -0.
  Label check_done;
  cmpl(dst, Immediate(0));
  j(not_equal, &check_done);

  // In case of 0, we need to check the high bits for the IEEE -0 pattern.
  Register high_word32_of_input = kScratchRegister;
  Pextrd(high_word32_of_input, src, 1);
  cmpl(high_word32_of_input, Immediate(0));
  JumpIf(less, fail);

  bind(&check_done);
}

void MaglevAssembler::Prologue(Graph* graph) {
  BailoutIfDeoptimized(rbx);

  if (graph->has_recursive_calls()) {
    bind(code_gen_state()->entry_label());
  }

  // Tiering support.
  // TODO(jgruber): Extract to a builtin (the tiering prologue is ~230 bytes
  // per Maglev code object on x64).
  {
    // Scratch registers. Don't clobber regs related to the calling
    // convention (e.g. kJavaScriptCallArgCountRegister). Keep up-to-date
    // with deferred flags code.
    Register flags = rcx;
    Register feedback_vector = r9;

    Label* deferred_flags_need_processing = MakeDeferredCode(
        [](MaglevAssembler* masm, Register flags, Register feedback_vector) {
          ASM_CODE_COMMENT_STRING(masm, "Optimized marker check");
          // TODO(leszeks): This could definitely be a builtin that we
          // tail-call.
          __ OptimizeCodeOrTailCallOptimizedCodeSlot(
              flags, feedback_vector, kJSFunctionRegister, JumpMode::kJump);
          __ Trap();
        },
        flags, feedback_vector);

    Move(feedback_vector,
         compilation_info()->toplevel_compilation_unit()->feedback().object());
    LoadFeedbackVectorFlagsAndJumpIfNeedsProcessing(
        flags, feedback_vector, CodeKind::MAGLEV,
        deferred_flags_need_processing);
  }

  EnterFrame(StackFrame::MAGLEV);

  // Save arguments in frame.
  // TODO(leszeks): Consider eliding this frame if we don't make any calls
  // that could clobber these registers.
  Push(kContextRegister);
  Push(kJSFunctionRegister);              // Callee's JS function.
  Push(kJavaScriptCallArgCountRegister);  // Actual argument count.

  // Initialize stack slots.
  if (graph->tagged_stack_slots() > 0) {
    ASM_CODE_COMMENT_STRING(this, "Initializing stack slots");
    // TODO(leszeks): Consider filling with xmm + movdqa instead.
    Move(rax, 0);

    // Magic value. Experimentally, an unroll size of 8 doesn't seem any
    // worse than fully unrolled pushes.
    const int kLoopUnrollSize = 8;
    int tagged_slots = graph->tagged_stack_slots();
    if (tagged_slots < 2 * kLoopUnrollSize) {
      // If the frame is small enough, just unroll the frame fill
      // completely.
      for (int i = 0; i < tagged_slots; ++i) {
        pushq(rax);
      }
    } else {
      // Extract the first few slots to round to the unroll size.
      int first_slots = tagged_slots % kLoopUnrollSize;
      for (int i = 0; i < first_slots; ++i) {
        pushq(rax);
      }
      Move(rbx, tagged_slots / kLoopUnrollSize);
      // We enter the loop unconditionally, so make sure we need to loop at
      // least once.
      DCHECK_GT(tagged_slots / kLoopUnrollSize, 0);
      Label loop;
      bind(&loop);
      for (int i = 0; i < kLoopUnrollSize; ++i) {
        pushq(rax);
      }
      decl(rbx);
      j(greater, &loop);
    }
  }
  if (graph->untagged_stack_slots() > 0) {
    // Extend rsp by the size of the remaining untagged part of the frame,
    // no need to initialise these.
    subq(rsp, Immediate(graph->untagged_stack_slots() * kSystemPointerSize));
  }
}

void MaglevAssembler::MaybeEmitDeoptBuiltinsCall(size_t eager_deopt_count,
                                                 Label* eager_deopt_entry,
                                                 size_t lazy_deopt_count,
                                                 Label* lazy_deopt_entry) {}

void MaglevAssembler::StringLength(Register result, Register string) {
  if (v8_flags.debug_code) {
    // Check if {string} is a string.
    AssertNotSmi(string);
    LoadMap(kScratchRegister, string);
    CmpInstanceTypeRange(kScratchRegister, kScratchRegister, FIRST_STRING_TYPE,
                         LAST_STRING_TYPE);
    Check(below_equal, AbortReason::kUnexpectedValue);
  }
  movl(result, FieldOperand(string, String::kLengthOffset));
}

void MaglevAssembler::StoreFixedArrayElementWithWriteBarrier(
    Register array, Register index, Register value,
    RegisterSnapshot register_snapshot) {
  if (v8_flags.debug_code) {
    AssertNotSmi(array);
    CmpObjectType(array, FIXED_ARRAY_TYPE, kScratchRegister);
    Assert(equal, AbortReason::kUnexpectedValue);
    cmpq(index, Immediate(0));
    Assert(above_equal, AbortReason::kUnexpectedNegativeValue);
  }
  mov_tagged(
      FieldOperand(array, index, times_tagged_size, FixedArray::kHeaderSize),
      value);
  ZoneLabelRef done(this);
  Label* deferred_write_barrier = MakeDeferredCode(
      [](MaglevAssembler* masm, ZoneLabelRef done, Register object,
         Register index, Register value, RegisterSnapshot register_snapshot) {
        ASM_CODE_COMMENT_STRING(masm, "Write barrier slow path");
        // Use the value as the scratch register if possible, since
        // CheckPageFlag emits slightly better code when value == scratch.
        Register scratch = kScratchRegister;
        if (value != object && !register_snapshot.live_registers.has(value)) {
          scratch = value;
        }
        __ CheckPageFlag(value, scratch,
                         MemoryChunk::kPointersToHereAreInterestingMask, zero,
                         *done);

        Register stub_object_reg = WriteBarrierDescriptor::ObjectRegister();
        Register slot_reg = WriteBarrierDescriptor::SlotAddressRegister();

        RegList saved;
        if (object != stub_object_reg &&
            register_snapshot.live_registers.has(stub_object_reg)) {
          saved.set(stub_object_reg);
        }
        if (register_snapshot.live_registers.has(slot_reg)) {
          saved.set(slot_reg);
        }

        __ PushAll(saved);

        if (object != stub_object_reg) {
          __ Move(stub_object_reg, object);
          object = stub_object_reg;
        }
        __ leaq(slot_reg, FieldOperand(object, index, times_tagged_size,
                                       FixedArray::kHeaderSize));

        SaveFPRegsMode const save_fp_mode =
            !register_snapshot.live_double_registers.is_empty()
                ? SaveFPRegsMode::kSave
                : SaveFPRegsMode::kIgnore;

        __ CallRecordWriteStub(object, slot_reg, save_fp_mode);

        __ PopAll(saved);
        __ jmp(*done);
      },
      done, array, index, value, register_snapshot);

  JumpIfSmi(value, *done);
  CheckPageFlag(array, kScratchRegister,
                MemoryChunk::kPointersFromHereAreInterestingMask, not_zero,
                deferred_write_barrier);
  bind(*done);
}

void MaglevAssembler::StoreFixedArrayElementNoWriteBarrier(Register array,
                                                           Register index,
                                                           Register value) {
  if (v8_flags.debug_code) {
    AssertNotSmi(array);
    CmpObjectType(array, FIXED_ARRAY_TYPE, kScratchRegister);
    Assert(equal, AbortReason::kUnexpectedValue);
    cmpq(index, Immediate(0));
    Assert(above_equal, AbortReason::kUnexpectedNegativeValue);
  }
  mov_tagged(
      FieldOperand(array, index, times_tagged_size, FixedArray::kHeaderSize),
      value);
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
