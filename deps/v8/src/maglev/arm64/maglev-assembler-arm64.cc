// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/interface-descriptors-inl.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/maglev/maglev-assembler-inl.h"
#include "src/maglev/maglev-graph.h"

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
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  // We are a bit short on registers, so we use the same register for {object}
  // and {new_top}. Once we have defined {new_top}, we don't use {object} until
  // {new_top} is used for the last time. And there (at the end of this
  // function), we recover the original {object} from {new_top} by subtracting
  // {size_in_bytes}.
  Register new_top = object;
  // Check if there is enough space.
  Ldr(object, ExternalReferenceAsOperand(top, scratch));
  Add(new_top, object, size_in_bytes);
  Ldr(scratch, ExternalReferenceAsOperand(limit, scratch));
  Cmp(new_top, scratch);
  // Otherwise call runtime.
  JumpToDeferredIf(
      ge,
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
        __ B(*done);
      },
      register_snapshot, object,
      in_new_space ? Builtin::kAllocateRegularInYoungGeneration
                   : Builtin::kAllocateRegularInOldGeneration,
      size_in_bytes, done);
  // Store new top and tag object.
  Move(ExternalReferenceAsOperand(top, scratch), new_top);
  Add(object, object, kHeapObjectTag - size_in_bytes);
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
  // `Allocate` needs 2 scratch registers, so it's important to `Acquire` after
  // `Allocate` is done and not before.
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  LoadTaggedRoot(scratch, RootIndex::kHeapNumberMap);
  StoreTaggedField(scratch, FieldMemOperand(result, HeapObject::kMapOffset));
  Str(value, FieldMemOperand(result, HeapNumber::kValueOffset));
}

void MaglevAssembler::StoreTaggedFieldWithWriteBarrier(
    Register object, int offset, Register value,
    RegisterSnapshot register_snapshot, ValueIsCompressed value_is_compressed,
    ValueCanBeSmi value_can_be_smi) {
  AssertNotSmi(object);
  StoreTaggedField(FieldMemOperand(object, offset), value);

  ZoneLabelRef done(this);
  Label* deferred_write_barrier = MakeDeferredCode(
      [](MaglevAssembler* masm, ZoneLabelRef done, Register object, int offset,
         Register value, RegisterSnapshot register_snapshot,
         ValueIsCompressed value_is_compressed) {
        ASM_CODE_COMMENT_STRING(masm, "Write barrier slow path");
        if (value_is_compressed == kValueIsCompressed) {
          __ DecompressTagged(value, value);
        }
        __ CheckPageFlag(value, MemoryChunk::kPointersToHereAreInterestingMask,
                         eq, *done);

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
        __ Add(slot_reg, object, offset - kHeapObjectTag);

        SaveFPRegsMode const save_fp_mode =
            !register_snapshot.live_double_registers.is_empty()
                ? SaveFPRegsMode::kSave
                : SaveFPRegsMode::kIgnore;

        __ CallRecordWriteStub(object, slot_reg, save_fp_mode);

        __ PopAll(saved);
        __ B(*done);
      },
      done, object, offset, value, register_snapshot, value_is_compressed);

  if (value_can_be_smi == kValueCanBeSmi) {
    JumpIfSmi(value, *done);
  } else {
    AssertNotSmi(value);
  }
  CheckPageFlag(object, MemoryChunk::kPointersFromHereAreInterestingMask, ne,
                deferred_write_barrier);
  bind(*done);
}

void MaglevAssembler::ToBoolean(Register value, ZoneLabelRef is_true,
                                ZoneLabelRef is_false,
                                bool fallthrough_when_true) {
  ScratchRegisterScope temps(this);
  Register map = temps.Acquire();

  // Check if {{value}} is Smi.
  Condition is_smi = CheckSmi(value);
  JumpToDeferredIf(
      is_smi,
      [](MaglevAssembler* masm, Register value, ZoneLabelRef is_true,
         ZoneLabelRef is_false) {
        // Check if {value} is not zero.
        __ CmpTagged(value, Smi::FromInt(0));
        __ JumpIf(eq, *is_false);
        __ Jump(*is_true);
      },
      value, is_true, is_false);

  // Check if {{value}} is false.
  CompareRoot(value, RootIndex::kFalseValue);
  JumpIf(eq, *is_false);

  // Check if {{value}} is empty string.
  CompareRoot(value, RootIndex::kempty_string);
  JumpIf(eq, *is_false);

  // Check if {{value}} is undetectable.
  LoadMap(map, value);
  {
    ScratchRegisterScope scope(this);
    Register tmp = scope.Acquire().W();
    Move(tmp, FieldMemOperand(map, Map::kBitFieldOffset));
    Tst(tmp, Immediate(Map::Bits1::IsUndetectableBit::kMask));
    JumpIf(ne, *is_false);
  }

  // Check if {{value}} is a HeapNumber.
  CompareRoot(map, RootIndex::kHeapNumberMap);
  JumpToDeferredIf(
      eq,
      [](MaglevAssembler* masm, Register value, ZoneLabelRef is_true,
         ZoneLabelRef is_false) {
        ScratchRegisterScope scope(masm);
        DoubleRegister value_double = scope.AcquireDouble();
        __ Ldr(value_double, FieldMemOperand(value, HeapNumber::kValueOffset));
        __ Fcmp(value_double, 0.0);
        __ JumpIf(eq, *is_false);
        __ JumpIf(vs, *is_false);  // NaN check
        __ Jump(*is_true);
      },
      value, is_true, is_false);

  // Check if {{value}} is a BigInt.
  CompareRoot(map, RootIndex::kBigIntMap);
  JumpToDeferredIf(
      eq,
      [](MaglevAssembler* masm, Register value, ZoneLabelRef is_true,
         ZoneLabelRef is_false) {
        ScratchRegisterScope scope(masm);
        Register tmp = scope.Acquire().W();
        __ Ldr(tmp, FieldMemOperand(value, BigInt::kBitfieldOffset));
        __ Tst(tmp, Immediate(BigInt::LengthBits::kMask));
        __ JumpIf(eq, *is_false);
        __ Jump(*is_true);
      },
      value, is_true, is_false);

  // Otherwise true.
  if (!fallthrough_when_true) {
    Jump(*is_true);
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
    case LiteralFlag::kNumber: {
      MaglevAssembler::ScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      JumpIfSmi(object, is_true);
      Ldr(scratch.W(), FieldMemOperand(object, HeapObject::kMapOffset));
      CompareRoot(scratch.W(), RootIndex::kHeapNumberMap);
      Branch(eq, is_true, true_distance, fallthrough_when_true, is_false,
             false_distance, fallthrough_when_false);
      return;
    }
    case LiteralFlag::kString: {
      MaglevAssembler::ScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      JumpIfSmi(object, is_false);
      LoadMap(scratch, object);
      CompareInstanceTypeRange(scratch, scratch, FIRST_STRING_TYPE,
                               LAST_STRING_TYPE);
      Branch(le, is_true, true_distance, fallthrough_when_true, is_false,
             false_distance, fallthrough_when_false);
      return;
    }
    case LiteralFlag::kSymbol: {
      MaglevAssembler::ScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      JumpIfSmi(object, is_false);
      LoadMap(scratch, object);
      CompareInstanceType(scratch, scratch, SYMBOL_TYPE);
      Branch(eq, is_true, true_distance, fallthrough_when_true, is_false,
             false_distance, fallthrough_when_false);
      return;
    }
    case LiteralFlag::kBoolean:
      CompareRoot(object, RootIndex::kTrueValue);
      B(eq, is_true);
      CompareRoot(object, RootIndex::kFalseValue);
      Branch(eq, is_true, true_distance, fallthrough_when_true, is_false,
             false_distance, fallthrough_when_false);
      return;
    case LiteralFlag::kBigInt: {
      MaglevAssembler::ScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      JumpIfSmi(object, is_false);
      LoadMap(scratch, object);
      CompareInstanceType(scratch, scratch, BIGINT_TYPE);
      Branch(eq, is_true, true_distance, fallthrough_when_true, is_false,
             false_distance, fallthrough_when_false);
      return;
    }
    case LiteralFlag::kUndefined: {
      MaglevAssembler::ScratchRegisterScope temps(this);
      // Make sure `object` isn't a valid temp here, since we re-use it.
      temps.SetAvailable(temps.Available() - object);
      Register map = temps.Acquire();
      JumpIfSmi(object, is_false);
      // Check it has the undetectable bit set and it is not null.
      LoadMap(map, object);
      Ldr(map.W(), FieldMemOperand(map, Map::kBitFieldOffset));
      TestAndBranchIfAllClear(map.W(), Map::Bits1::IsUndetectableBit::kMask,
                              is_false);
      CompareRoot(object, RootIndex::kNullValue);
      Branch(ne, is_true, true_distance, fallthrough_when_true, is_false,
             false_distance, fallthrough_when_false);
      return;
    }
    case LiteralFlag::kFunction: {
      MaglevAssembler::ScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      JumpIfSmi(object, is_false);
      // Check if callable bit is set and not undetectable.
      LoadMap(scratch, object);
      Ldr(scratch.W(), FieldMemOperand(scratch, Map::kBitFieldOffset));
      And(scratch.W(), scratch.W(),
          Map::Bits1::IsUndetectableBit::kMask |
              Map::Bits1::IsCallableBit::kMask);
      Cmp(scratch.W(), Map::Bits1::IsCallableBit::kMask);
      Branch(eq, is_true, true_distance, fallthrough_when_true, is_false,
             false_distance, fallthrough_when_false);
      return;
    }
    case LiteralFlag::kObject: {
      MaglevAssembler::ScratchRegisterScope temps(this);
      Register scratch = temps.Acquire();
      JumpIfSmi(object, is_false);
      // If the object is null then return true.
      CompareRoot(object, RootIndex::kNullValue);
      B(eq, is_true);
      // Check if the object is a receiver type,
      LoadMap(scratch, object);
      {
        MaglevAssembler::ScratchRegisterScope temps(this);
        CompareInstanceType(scratch, temps.Acquire(), FIRST_JS_RECEIVER_TYPE);
      }
      B(lt, is_false);
      // ... and is not undefined (undetectable) nor callable.
      Ldr(scratch.W(), FieldMemOperand(scratch, Map::kBitFieldOffset));
      Tst(scratch.W(), Immediate(Map::Bits1::IsUndetectableBit::kMask |
                                 Map::Bits1::IsCallableBit::kMask));
      Branch(eq, is_true, true_distance, fallthrough_when_true, is_false,
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

void MaglevAssembler::Prologue(Graph* graph) {
  ScratchRegisterScope temps(this);
  //  We add two extra registers to the scope. Ideally we could add all the
  //  allocatable general registers, except Context, JSFunction, NewTarget and
  //  ArgCount. Unfortunately, OptimizeCodeOrTailCallOptimizedCodeSlot and
  //  LoadFeedbackVectorFlagsAndJumpIfNeedsProcessing pick random registers and
  //  we could alias those.
  // TODO(victorgomes): Fix these builtins to either use the scope or pass the
  // used registers manually.
  temps.Include({x14, x15});

  CallTarget();

  BailoutIfDeoptimized();

  if (graph->has_recursive_calls()) {
    BindCallTarget(code_gen_state()->entry_label());
  }

  // Tiering support.
  // TODO(jgruber): Extract to a builtin.
  {
    ScratchRegisterScope temps(this);
    Register flags = temps.Acquire();
    Register feedback_vector = temps.Acquire();

    Label* deferred_flags_need_processing = MakeDeferredCode(
        [](MaglevAssembler* masm, Register flags, Register feedback_vector) {
          ASM_CODE_COMMENT_STRING(masm, "Optimized marker check");
          // TODO(leszeks): This could definitely be a builtin that we
          // tail-call.
          __ OptimizeCodeOrTailCallOptimizedCodeSlot(flags, feedback_vector);
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
  // Push the context and the JSFunction.
  Push(kContextRegister, kJSFunctionRegister);
  // Push the actual argument count and a _possible_ stack slot.
  Push(kJavaScriptCallArgCountRegister, xzr);
  int remaining_stack_slots = code_gen_state()->stack_slots() - 1;
  DCHECK_GE(remaining_stack_slots, 0);

  // Initialize stack slots.
  if (graph->tagged_stack_slots() > 0) {
    ASM_CODE_COMMENT_STRING(this, "Initializing stack slots");

    // If tagged_stack_slots is divisible by 2, we overshoot and allocate one
    // extra stack slot, otherwise we allocate exactly the right amount, since
    // one stack has already been allocated.
    int tagged_two_slots_count = graph->tagged_stack_slots() / 2;
    remaining_stack_slots -= 2 * tagged_two_slots_count;

    // Magic value. Experimentally, an unroll size of 8 doesn't seem any
    // worse than fully unrolled pushes.
    const int kLoopUnrollSize = 8;
    if (tagged_two_slots_count < kLoopUnrollSize) {
      for (int i = 0; i < tagged_two_slots_count; i++) {
        Push(xzr, xzr);
      }
    } else {
      ScratchRegisterScope temps(this);
      Register count = temps.Acquire();
      // Extract the first few slots to round to the unroll size.
      int first_slots = tagged_two_slots_count % kLoopUnrollSize;
      for (int i = 0; i < first_slots; ++i) {
        Push(xzr, xzr);
      }
      Move(count, tagged_two_slots_count / kLoopUnrollSize);
      // We enter the loop unconditionally, so make sure we need to loop at
      // least once.
      DCHECK_GT(tagged_two_slots_count / kLoopUnrollSize, 0);
      Label loop;
      bind(&loop);
      for (int i = 0; i < kLoopUnrollSize; ++i) {
        Push(xzr, xzr);
      }
      Subs(count, count, Immediate(1));
      B(&loop, gt);
    }
  }
  if (remaining_stack_slots > 0) {
    // Round up.
    remaining_stack_slots += (remaining_stack_slots % 2);
    // Extend sp by the size of the remaining untagged part of the frame,
    // no need to initialise these.
    Sub(sp, sp, Immediate(remaining_stack_slots * kSystemPointerSize));
  }
}

void MaglevAssembler::MaybeEmitDeoptBuiltinsCall(size_t eager_deopt_count,
                                                 Label* eager_deopt_entry,
                                                 size_t lazy_deopt_count,
                                                 Label* lazy_deopt_entry) {
  ForceConstantPoolEmissionWithoutJump();

  DCHECK_GE(Deoptimizer::kLazyDeoptExitSize, Deoptimizer::kEagerDeoptExitSize);
  size_t deopt_count = eager_deopt_count + lazy_deopt_count;
  CheckVeneerPool(
      false, false,
      static_cast<int>(deopt_count) * Deoptimizer::kLazyDeoptExitSize);

  ScratchRegisterScope scope(this);
  Register scratch = scope.Acquire();
  if (eager_deopt_count > 0) {
    Bind(eager_deopt_entry);
    LoadEntryFromBuiltin(Builtin::kDeoptimizationEntry_Eager, scratch);
    MacroAssembler::Jump(scratch);
  }
  if (lazy_deopt_count > 0) {
    Bind(lazy_deopt_entry);
    LoadEntryFromBuiltin(Builtin::kDeoptimizationEntry_Lazy, scratch);
    MacroAssembler::Jump(scratch);
  }
}

void MaglevAssembler::AllocateTwoByteString(RegisterSnapshot register_snapshot,
                                            Register result, int length) {
  int size = SeqTwoByteString::SizeFor(length);
  Allocate(register_snapshot, result, size);
  ScratchRegisterScope scope(this);
  Register scratch = scope.Acquire();
  StoreTaggedField(xzr, FieldMemOperand(result, size - kObjectAlignment));
  LoadTaggedRoot(scratch, RootIndex::kStringMap);
  StoreTaggedField(scratch, FieldMemOperand(result, HeapObject::kMapOffset));
  Move(scratch, Name::kEmptyHashField);
  StoreTaggedField(scratch, FieldMemOperand(result, Name::kRawHashFieldOffset));
  Move(scratch, length);
  StoreTaggedField(scratch, FieldMemOperand(result, String::kLengthOffset));
}

void MaglevAssembler::LoadSingleCharacterString(Register result,
                                                Register char_code,
                                                Register scratch) {
  DCHECK_NE(char_code, scratch);
  if (v8_flags.debug_code) {
    Cmp(char_code, Immediate(String::kMaxOneByteCharCode));
    Assert(ls, AbortReason::kUnexpectedValue);
  }
  Register table = scratch;
  LoadRoot(table, RootIndex::kSingleCharacterStringTable);
  Add(table, table, Operand(char_code, LSL, kTaggedSizeLog2));
  DecompressTagged(result, FieldMemOperand(table, FixedArray::kHeaderSize));
}

void MaglevAssembler::StringFromCharCode(RegisterSnapshot register_snapshot,
                                         Label* char_code_fits_one_byte,
                                         Register result, Register char_code,
                                         Register scratch) {
  AssertZeroExtended(char_code);
  DCHECK_NE(char_code, scratch);
  ZoneLabelRef done(this);
  Cmp(char_code, Immediate(String::kMaxOneByteCharCode));
  JumpToDeferredIf(
      hi,
      [](MaglevAssembler* masm, RegisterSnapshot register_snapshot,
         ZoneLabelRef done, Register result, Register char_code,
         Register scratch) {
        ScratchRegisterScope temps(masm);
        // Ensure that {result} never aliases {scratch}, otherwise the store
        // will fail.
        Register string = result;
        bool reallocate_result = scratch.Aliases(result);
        if (reallocate_result) {
          string = temps.Acquire();
        }
        // Be sure to save {char_code}. If it aliases with {result}, use
        // the scratch register.
        if (char_code.Aliases(result)) {
          __ Move(scratch, char_code);
          char_code = scratch;
        }
        DCHECK(!char_code.Aliases(string));
        DCHECK(!scratch.Aliases(string));
        DCHECK(!register_snapshot.live_tagged_registers.has(char_code));
        register_snapshot.live_registers.set(char_code);
        __ AllocateTwoByteString(register_snapshot, string, 1);
        __ And(scratch, char_code, Immediate(0xFFFF));
        __ Strh(scratch.W(),
                FieldMemOperand(string, SeqTwoByteString::kHeaderSize));
        if (reallocate_result) {
          __ Move(result, string);
        }
        __ B(*done);
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
    Register index, Register instance_type, Label* result_fits_one_byte) {
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
          __ SmiTag(index);
          __ Push(string, index);
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

  // We might need to try more than one time for ConsString, SlicedString and
  // ThinString.
  Label loop;
  bind(&loop);

  if (v8_flags.debug_code) {
    Register scratch = instance_type;

    // Check if {string} is a string.
    AssertNotSmi(string);
    LoadMap(scratch, string);
    CompareInstanceTypeRange(scratch, scratch, FIRST_STRING_TYPE,
                             LAST_STRING_TYPE);
    Check(ls, AbortReason::kUnexpectedValue);

    Ldr(scratch.W(), FieldMemOperand(string, String::kLengthOffset));
    Cmp(index.W(), scratch.W());
    Check(lo, AbortReason::kUnexpectedValue);
  }

  // Get instance type.
  LoadMap(instance_type, string);
  Ldr(instance_type.W(),
      FieldMemOperand(instance_type, Map::kInstanceTypeOffset));

  {
    ScratchRegisterScope temps(this);
    Register representation = temps.Acquire().W();

    // TODO(victorgomes): Add fast path for external strings.
    And(representation, instance_type.W(),
        Immediate(kStringRepresentationMask));
    Cmp(representation, Immediate(kSeqStringTag));
    B(&seq_string, eq);
    Cmp(representation, Immediate(kConsStringTag));
    B(&cons_string, eq);
    Cmp(representation, Immediate(kSlicedStringTag));
    B(&sliced_string, eq);
    Cmp(representation, Immediate(kThinStringTag));
    B(deferred_runtime_call, ne);
    // Fallthrough to thin string.
  }

  // Is a thin string.
  {
    DecompressTagged(string,
                     FieldMemOperand(string, ThinString::kActualOffset));
    B(&loop);
  }

  bind(&sliced_string);
  {
    ScratchRegisterScope temps(this);
    Register offset = temps.Acquire();

    Ldr(offset.W(), FieldMemOperand(string, SlicedString::kOffsetOffset));
    SmiUntag(offset);
    DecompressTagged(string,
                     FieldMemOperand(string, SlicedString::kParentOffset));
    Add(index, index, offset);
    B(&loop);
  }

  bind(&cons_string);
  {
    // Reuse {instance_type} register here, since CompareRoot requires a scratch
    // register as well.
    Register second_string = instance_type;
    Ldr(second_string.W(), FieldMemOperand(string, ConsString::kSecondOffset));
    CompareRoot(second_string, RootIndex::kempty_string);
    B(deferred_runtime_call, ne);
    DecompressTagged(string, FieldMemOperand(string, ConsString::kFirstOffset));
    B(&loop);  // Try again with first string.
  }

  bind(&seq_string);
  {
    Label two_byte_string;
    TestAndBranchIfAllClear(instance_type, kOneByteStringTag, &two_byte_string);
    // The result of one-byte string will be the same for both modes
    // (CharCodeAt/CodePointAt), since it cannot be the first half of a
    // surrogate pair.
    Add(index, index, SeqOneByteString::kHeaderSize - kHeapObjectTag);
    Ldrb(result, MemOperand(string, index));
    B(result_fits_one_byte);

    bind(&two_byte_string);
    // {instance_type} is unused from this point, so we can use as scratch.
    Register scratch = instance_type;
    Lsl(scratch, index, 1);
    Add(scratch, scratch, SeqTwoByteString::kHeaderSize - kHeapObjectTag);
    Ldrh(result, MemOperand(string, scratch));

    if (mode == BuiltinStringPrototypeCharCodeOrCodePointAt::kCodePointAt) {
      Register first_code_point = scratch;
      And(first_code_point.W(), result.W(), Immediate(0xfc00));
      CompareAndBranch(first_code_point, Immediate(0xd800), kNotEqual, *done);

      Register length = scratch;
      Ldr(length.W(), FieldMemOperand(string, String::kLengthOffset));
      Add(index.W(), index.W(), Immediate(1));
      CompareAndBranch(index, length, kGreaterThanEqual, *done);

      Register second_code_point = scratch;
      Lsl(index, index, 1);
      Add(index, index, SeqTwoByteString::kHeaderSize - kHeapObjectTag);
      Ldrh(second_code_point, MemOperand(string, index));

      // {index} is not needed at this point.
      Register scratch2 = index;
      And(scratch2.W(), second_code_point.W(), Immediate(0xfc00));
      CompareAndBranch(scratch2, Immediate(0xdc00), kNotEqual, *done);

      int surrogate_offset = 0x10000 - (0xd800 << 10) - 0xdc00;
      Add(second_code_point, second_code_point, Immediate(surrogate_offset));
      Lsl(result, result, 10);
      Add(result, result, second_code_point);
    }

    // Fallthrough.
  }

  bind(*done);

  if (v8_flags.debug_code) {
    // We make sure that the user of this macro is not relying in string and
    // index to not be clobbered.
    if (result != string) {
      Mov(string, Immediate(0xdeadbeef));
    }
    if (result != index) {
      Mov(index, Immediate(0xdeadbeef));
    }
  }
}

void MaglevAssembler::TruncateDoubleToInt32(Register dst, DoubleRegister src) {
  if (CpuFeatures::IsSupported(JSCVT)) {
    Fjcvtzs(dst.W(), src);
    return;
  }

  ZoneLabelRef done(this);
  // Try to convert with an FPU convert instruction. It's trivial to compute
  // the modulo operation on an integer register so we convert to a 64-bit
  // integer.
  //
  // Fcvtzs will saturate to INT64_MIN (0x800...00) or INT64_MAX (0x7FF...FF)
  // when the double is out of range. NaNs and infinities will be converted to 0
  // (as ECMA-262 requires).
  Fcvtzs(dst.X(), src);

  // The values INT64_MIN (0x800...00) or INT64_MAX (0x7FF...FF) are not
  // representable using a double, so if the result is one of those then we know
  // that saturation occurred, and we need to manually handle the conversion.
  //
  // It is easy to detect INT64_MIN and INT64_MAX because adding or subtracting
  // 1 will cause signed overflow.
  Cmp(dst.X(), 1);
  Ccmp(dst.X(), -1, VFlag, vc);

  JumpToDeferredIf(
      vs,
      [](MaglevAssembler* masm, DoubleRegister src, Register dst,
         ZoneLabelRef done) {
        __ MacroAssembler::Push(xzr, src);
        __ CallBuiltin(Builtin::kDoubleToI);
        __ Ldr(dst.W(), MemOperand(sp, 0));
        DCHECK_EQ(xzr.SizeInBytes(), src.SizeInBytes());
        __ Drop(2);
        __ B(*done);
      },
      src, dst, done);

  Bind(*done);
  // Zero extend the converted value to complete the truncation.
  Mov(dst, Operand(dst.W(), UXTW));
}

void MaglevAssembler::TryTruncateDoubleToInt32(Register dst, DoubleRegister src,
                                               Label* fail) {
  ScratchRegisterScope temps(this);
  DoubleRegister converted_back = temps.AcquireDouble();

  // Convert the input float64 value to int32.
  Fcvtzs(dst.W(), src);
  // Convert that int32 value back to float64.
  Scvtf(converted_back, dst.W());
  // Check that the result of the float64->int32->float64 is equal to the input
  // (i.e. that the conversion didn't truncate.
  Fcmp(src, converted_back);
  JumpIf(ne, fail);

  // Check if {input} is -0.
  Label check_done;
  Cbnz(dst, &check_done);

  // In case of 0, we need to check for the IEEE 0 pattern (which is all zeros).
  Register input_bits = temps.Acquire();
  Fmov(input_bits, src);
  Cbnz(input_bits, fail);

  Bind(&check_done);
}

void MaglevAssembler::StringLength(Register result, Register string) {
  if (v8_flags.debug_code) {
    // Check if {string} is a string.
    MaglevAssembler::ScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    AssertNotSmi(string);
    LoadMap(scratch, string);
    CompareInstanceTypeRange(scratch, scratch, FIRST_STRING_TYPE,
                             LAST_STRING_TYPE);
    Check(ls, AbortReason::kUnexpectedValue);
  }
  Ldr(result.W(), FieldMemOperand(string, String::kLengthOffset));
}

void MaglevAssembler::StoreFixedArrayElementWithWriteBarrier(
    Register array, Register index, Register value,
    RegisterSnapshot register_snapshot) {
  if (v8_flags.debug_code) {
    AssertNotSmi(array);
    IsObjectType(array, FIXED_ARRAY_TYPE);
    Assert(eq, AbortReason::kUnexpectedValue);
    Cmp(index, Immediate(0));
    Assert(hs, AbortReason::kUnexpectedNegativeValue);
  }
  {
    ScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Add(scratch, array, Operand(index, LSL, kTaggedSizeLog2));
    Str(value.W(), FieldMemOperand(scratch, FixedArray::kHeaderSize));
  }

  ZoneLabelRef done(this);
  Label* deferred_write_barrier = MakeDeferredCode(
      [](MaglevAssembler* masm, ZoneLabelRef done, Register object,
         Register index, Register value, RegisterSnapshot register_snapshot) {
        ASM_CODE_COMMENT_STRING(masm, "Write barrier slow path");
        __ CheckPageFlag(value, MemoryChunk::kPointersToHereAreInterestingMask,
                         eq, *done);

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
        __ Add(slot_reg, object, FixedArray::kHeaderSize - kHeapObjectTag);
        __ Add(slot_reg, slot_reg, Operand(index, LSL, kTaggedSizeLog2));

        SaveFPRegsMode const save_fp_mode =
            !register_snapshot.live_double_registers.is_empty()
                ? SaveFPRegsMode::kSave
                : SaveFPRegsMode::kIgnore;

        __ CallRecordWriteStub(object, slot_reg, save_fp_mode);

        __ PopAll(saved);
        __ B(*done);
      },
      done, array, index, value, register_snapshot);

  JumpIfSmi(value, *done);
  CheckPageFlag(array, MemoryChunk::kPointersFromHereAreInterestingMask, ne,
                deferred_write_barrier);
  bind(*done);
}

void MaglevAssembler::StoreFixedArrayElementNoWriteBarrier(Register array,
                                                           Register index,
                                                           Register value) {
  ScratchRegisterScope temps(this);
  Register scratch = temps.Acquire();
  if (v8_flags.debug_code) {
    AssertNotSmi(array);
    IsObjectType(array, FIXED_ARRAY_TYPE);
    Assert(eq, AbortReason::kUnexpectedValue);
    Cmp(index, Immediate(0));
    Assert(hs, AbortReason::kUnexpectedNegativeValue);
  }
  Add(scratch, array, Operand(index, LSL, kTaggedSizeLog2));
  Str(value.W(), FieldMemOperand(scratch, FixedArray::kHeaderSize));
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
