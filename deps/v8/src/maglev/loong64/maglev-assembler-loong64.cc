// Copyright 2024 the V8 project authors. All rights reserved.
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

void SubSizeAndTagObject(MaglevAssembler* masm, Register object,
                         Register size_in_bytes) {
  __ Sub_d(object, object, Operand(size_in_bytes));
  __ Add_d(object, object, Operand(kHeapObjectTag));
}

void SubSizeAndTagObject(MaglevAssembler* masm, Register object,
                         int size_in_bytes) {
  __ Add_d(object, object, Operand(kHeapObjectTag - size_in_bytes));
}

template <typename T>
void AllocateRaw(MaglevAssembler* masm, Isolate* isolate,
                 RegisterSnapshot register_snapshot, Register object,
                 T size_in_bytes, AllocationType alloc_type,
                 AllocationAlignment alignment) {
  // TODO(victorgomes): Call the runtime for large object allocation.
  // TODO(victorgomes): Support double alignment.
  DCHECK(masm->allow_allocate());
  DCHECK_EQ(alignment, kTaggedAligned);
  if (v8_flags.single_generation) {
    alloc_type = AllocationType::kOld;
  }
  IsolateFieldId top = SpaceAllocationTopAddress(alloc_type);
  IsolateFieldId limit = SpaceAllocationLimitAddress(alloc_type);

  ZoneLabelRef done(masm);
  MaglevAssembler::TemporaryRegisterScope temps(masm);
  Register scratch = temps.AcquireScratch();
  // We are a bit short on registers, so we use the same register for {object}
  // and {new_top}. Once we have defined {new_top}, we don't use {object} until
  // {new_top} is used for the last time. And there (at the end of this
  // function), we recover the original {object} from {new_top} by subtracting
  // {size_in_bytes}.
  Register new_top = object;
  // Check if there is enough space.
  __ Ld_d(object, __ ExternalReferenceAsOperand(top));
  __ Add_d(new_top, object, Operand(size_in_bytes));
  __ Ld_d(scratch, __ ExternalReferenceAsOperand(limit));

  if (v8_flags.code_comments) {
    __ RecordComment("-- Jump to deferred code");
  }
  // Call runtime if new_top >= limit.
  // TODO(loong64): Recheck JumpToDeferredIf in AllocateRaw
  __ MacroAssembler::Branch(
      __ MakeDeferredCode(
          [](MaglevAssembler* masm, RegisterSnapshot register_snapshot,
             Register object, AllocationType alloc_type, T size_in_bytes,
             ZoneLabelRef done) {
            AllocateSlow(masm, register_snapshot, object,
                         AllocateBuiltin(alloc_type), size_in_bytes, done);
          },
          register_snapshot, object, alloc_type, size_in_bytes, done),
      ge, new_top, Operand(scratch));

  // Store new top and tag object.
  __ Move(__ ExternalReferenceAsOperand(top), new_top);
#if V8_VERIFY_WRITE_BARRIERS
  if (v8_flags.verify_write_barriers) {
    ExternalReference last_young_allocation =
        ExternalReference::last_young_allocation_address(isolate);

    if (alloc_type == AllocationType::kYoung) {
      __ Sub_d(object, object, size_in_bytes);
      __ St_d(object,
              __ ExternalReferenceAsOperand(last_young_allocation, scratch));
      __ Add_d(object, object, size_in_bytes);
    } else {
      __ St_d(zero_reg,
              __ ExternalReferenceAsOperand(last_young_allocation, scratch));
    }
  }
#endif  // V8_VERIFY_WRITE_BARRIERS
  SubSizeAndTagObject(masm, object, size_in_bytes);
  __ bind(*done);
}

void MaglevAssembler::Allocate(RegisterSnapshot register_snapshot,
                               Register object, int size_in_bytes,
                               AllocationType alloc_type,
                               AllocationAlignment alignment) {
  AllocateRaw(this, isolate_, register_snapshot, object, size_in_bytes,
              alloc_type, alignment);
}

void MaglevAssembler::Allocate(RegisterSnapshot register_snapshot,
                               Register object, Register size_in_bytes,
                               AllocationType alloc_type,
                               AllocationAlignment alignment) {
  AllocateRaw(this, isolate_, register_snapshot, object, size_in_bytes,
              alloc_type, alignment);
}

void MaglevAssembler::OSRPrologue(Graph* graph) {
  DCHECK(graph->is_osr());
  CHECK(!graph->has_recursive_calls());

  uint32_t source_frame_size =
      graph->min_maglev_stackslots_for_unoptimized_frame_size();

  if (V8_ENABLE_SANDBOX_BOOL || v8_flags.debug_code) {
    TemporaryRegisterScope temps(this);
    Register scratch = temps.AcquireScratch();
    int32_t expected_osr_stack_size =
        source_frame_size * kSystemPointerSize +
        StandardFrameConstants::kFixedFrameSizeFromFp;
    Add_d(scratch, sp, Operand(expected_osr_stack_size));
    MacroAssembler::SbxCheck(eq, AbortReason::kOsrUnexpectedStackSize, scratch,
                             Operand(fp));
  }

  uint32_t target_frame_size =
      graph->tagged_stack_slots() + graph->untagged_stack_slots();
  CHECK_LE(source_frame_size, target_frame_size);
  if (source_frame_size < target_frame_size) {
    ASM_CODE_COMMENT_STRING(this, "Growing frame for OSR");
    uint32_t additional_tagged =
        source_frame_size < graph->tagged_stack_slots()
            ? graph->tagged_stack_slots() - source_frame_size
            : 0;
    if (additional_tagged != 0)
      Sub_d(sp, sp, Operand(additional_tagged * kSystemPointerSize));
    for (size_t i = 0; i < additional_tagged; ++i) {
      // TODO(loong64): Recheck push simd in MaglevAssembler::OSRPrologue?
      St_d(zero_reg, MemOperand(sp, i * 8));
    }
    uint32_t size_so_far = source_frame_size + additional_tagged;
    CHECK_LE(size_so_far, target_frame_size);
    if (size_so_far < target_frame_size) {
      Sub_d(sp, sp,
            Operand((target_frame_size - size_so_far) * kSystemPointerSize));
    }
  }
}

void MaglevAssembler::Prologue(Graph* graph) {
  TemporaryRegisterScope temps(this);
  //  We add two extra registers to the scope. Ideally we could add all the
  //  allocatable general registers, except Context, JSFunction, NewTarget and
  //  ArgCount. Unfortunately, OptimizeCodeOrTailCallOptimizedCodeSlot and
  //  LoadFeedbackVectorFlagsAndJumpIfNeedsProcessing pick random registers and
  //  we could alias those.
  // TODO(victorgomes): Fix these builtins to either use the scope or pass the
  // used registers manually.
  temps.Include({s4, s4});  // use register not overlapping with flags,
                            // feedback and so on
  DCHECK(!graph->is_osr());

  if (v8_flags.debug_code) {
    AssertNotDeoptimized();
  }

  if (graph->has_recursive_calls()) {
    bind(code_gen_state()->entry_label());
  }

  EnterFrame(StackFrame::MAGLEV);
  // Save arguments in frame.
  // TODO(leszeks): Consider eliding this frame if we don't make any calls
  // that could clobber these registers.
  // Push the context and the JSFunction.
  Push(kContextRegister, kJSFunctionRegister);
  // Push the actual argument count and a _possible_ stack slot.
  Push(kJavaScriptCallArgCountRegister);

  // Initialize stack slots.
  if (graph->tagged_stack_slots() > 0) {
    ASM_CODE_COMMENT_STRING(this, "Initializing stack slots");

    // Magic value. Experimentally, an unroll size of 8 doesn't seem any
    // worse than fully unrolled pushes.
    const int kLoopUnrollSize = 8;
    int tagged_slots = graph->tagged_stack_slots();

    // TODO(loong64): Recheck push 128-bit
    if (tagged_slots < 2 * kLoopUnrollSize) {
      // If the frame is small enough, just unroll the frame fill
      // completely.
      if (tagged_slots != 0)
        Add_d(sp, sp, Operand(-(tagged_slots * kSystemPointerSize)));
      for (int i = 0; i < tagged_slots; ++i) {
        St_d(zero_reg, MemOperand(sp, i * 8));
      }
    } else {
      // Extract the first few slots to round to the unroll size.
      int first_slots = tagged_slots % kLoopUnrollSize;
      if (first_slots != 0)
        Add_d(sp, sp, Operand(-(first_slots * kSystemPointerSize)));
      for (int i = 0; i < first_slots; ++i) {
        St_d(zero_reg, MemOperand(sp, i * 8));
      }
      TemporaryRegisterScope temps(this);
      Register count = temps.AcquireScratch();
      Move(count, tagged_slots / kLoopUnrollSize);
      // We enter the loop unconditionally, so make sure we need to loop at
      // least once.
      DCHECK_GT(tagged_slots / kLoopUnrollSize, 0);
      Label loop;
      bind(&loop);
      Add_d(sp, sp, Operand(-(kLoopUnrollSize * kSystemPointerSize)));
      for (int i = 0; i < kLoopUnrollSize; ++i) {
        St_d(zero_reg, MemOperand(sp, i * 8));
      }
      Sub_d(count, count, Operand(1));
      MacroAssembler::Branch(&loop, gt, count, Operand(zero_reg), Label::kNear);
    }
  }
  if (graph->untagged_stack_slots() > 0) {
    // Extend sp by the size of the remaining untagged part of the frame,
    // no need to initialise these.
    Sub_d(sp, sp, Operand(graph->untagged_stack_slots() * kSystemPointerSize));
  }
}

// TODO(loong64): seems only implementabed on arm64
void MaglevAssembler::MaybeEmitDeoptBuiltinsCall(size_t eager_deopt_count,
                                                 Label* eager_deopt_entry,
                                                 size_t lazy_deopt_count,
                                                 Label* lazy_deopt_entry) {}

void MaglevAssembler::LoadSingleCharacterString(Register result,
                                                Register char_code,
                                                Register scratch) {
  DCHECK_NE(char_code, scratch);
  if (v8_flags.debug_code) {
    MacroAssembler::Assert(less_equal, AbortReason::kUnexpectedValue, char_code,
                           Operand(String::kMaxOneByteCharCode));
  }
  Register table = scratch;
  Add_d(table, kRootRegister,
        Operand(RootRegisterOffsetForRootIndex(
            RootIndex::kFirstSingleCharacterString)));
  Alsl_d(table, char_code, table, kSystemPointerSizeLog2);
  Ld_d(result, MemOperand(table, 0));
}

void MaglevAssembler::StringFromCharCode(RegisterSnapshot register_snapshot,
                                         Label* char_code_fits_one_byte,
                                         Register result, Register char_code,
                                         Register scratch,
                                         CharCodeMaskMode mask_mode) {
  // AssertZeroExtended(char_code);
  MacroAssembler::lu32i_d(char_code, 0);
  DCHECK_NE(char_code, scratch);
  ZoneLabelRef done(this);
  if (mask_mode == CharCodeMaskMode::kMustApplyMask) {
    And(char_code, char_code, Operand(0xFFFF));
  }
  // Allocate two-bytes string if {char_code} doesn't fit one byte.
  MacroAssembler::Branch(  // FIXME: reimplement with JumpToDeferredIf
      MakeDeferredCode(
          [](MaglevAssembler* masm, RegisterSnapshot register_snapshot,
             ZoneLabelRef done, Register result, Register char_code,
             Register scratch) {
            MaglevAssembler::TemporaryRegisterScope temps(masm);
            // Ensure that {result} never aliases {scratch}, otherwise use
            // a temporary register to restore {result} at the end.
            const bool need_restore_result = (scratch == result);
            Register string =
                need_restore_result ? temps.AcquireScratch() : result;
            // Ensure that {char_code} never aliases {result}, otherwise use
            // the given {scratch} register.
            if (char_code == result) {
              __ Move(scratch, char_code);
              char_code = scratch;
            }
            DCHECK(char_code != string);
            DCHECK(scratch != string);
            DCHECK(!register_snapshot.live_tagged_registers.has(char_code));
            register_snapshot.live_registers.set(char_code);
            __ AllocateTwoByteString(register_snapshot, string, 1);
            __ And(scratch, char_code, Operand(0xFFFF));
            __ St_h(scratch, FieldMemOperand(string, OFFSET_OF_DATA_START(
                                                         SeqTwoByteString)));
            if (need_restore_result) {
              __ Move(result, string);
            }
            __ jmp(*done);
          },
          register_snapshot, done, result, char_code, scratch),
      Ugreater_equal, char_code, Operand(String::kMaxOneByteCharCode));

  if (char_code_fits_one_byte != nullptr) {
    bind(char_code_fits_one_byte);
  }
  LoadSingleCharacterString(result, char_code, scratch);
  bind(*done);
}

void MaglevAssembler::StringCharCodeOrCodePointAt(
    BuiltinStringPrototypeCharCodeOrCodePointAt::Mode mode,
    RegisterSnapshot& register_snapshot, Register result, Register string,
    Register index, Register scratch1, Register scratch2,
    Label* result_fits_one_byte) {
  ASM_CODE_COMMENT(this);
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
    // Check if {string} is a string.
    AssertObjectTypeInRange(string, FIRST_STRING_TYPE, LAST_STRING_TYPE,
                            AbortReason::kUnexpectedValue);

    Ld_w(scratch1, FieldMemOperand(string, offsetof(String, length_)));
    // TODO(loong64): Check index sign/zero-extension?
    CompareInt32AndAssert(index, scratch1, kUnsignedLessThan,
                          AbortReason::kUnexpectedValue);
  }

#if V8_STATIC_ROOTS_BOOL
  Register map = scratch1;
  LoadMapForCompare(map, string);
#else
  Register instance_type = scratch1;
  // Get instance type.
  LoadInstanceType(instance_type, string);
#endif

  {
#if V8_STATIC_ROOTS_BOOL
    using StringTypeRange = InstanceTypeChecker::kUniqueMapRangeOfStringType;
    // Check the string map ranges in dense increasing order, to avoid needing
    // to subtract away the lower bound.
    // Map is sign-extended.
    static_assert(StringTypeRange::kSeqString.first == 0);
    MacroAssembler::Branch(
        &seq_string, kUnsignedLessThanEqual, map,
        Operand(static_cast<int32_t>(StringTypeRange::kSeqString.second)));

    static_assert(StringTypeRange::kSeqString.second + Map::kSize ==
                  StringTypeRange::kExternalString.first);
    MacroAssembler::Branch(
        deferred_runtime_call, kUnsignedLessThanEqual, map,
        Operand(static_cast<int32_t>(StringTypeRange::kExternalString.second)));
    // TODO(victorgomes): Add fast path for external strings.

    static_assert(StringTypeRange::kExternalString.second + Map::kSize ==
                  StringTypeRange::kConsString.first);
    MacroAssembler::Branch(
        &cons_string, kUnsignedLessThanEqual, map,
        Operand(static_cast<int32_t>(StringTypeRange::kConsString.second)));

    static_assert(StringTypeRange::kConsString.second + Map::kSize ==
                  StringTypeRange::kSlicedString.first);
    MacroAssembler::Branch(
        &sliced_string, kUnsignedLessThanEqual, map,
        Operand(static_cast<int32_t>(StringTypeRange::kSlicedString.second)));

    static_assert(StringTypeRange::kSlicedString.second + Map::kSize ==
                  StringTypeRange::kThinString.first);
    // No need to check for thin strings, they're the last string map.
    static_assert(StringTypeRange::kThinString.second ==
                  InstanceTypeChecker::kStringMapUpperBound);
    // Fallthrough to thin string.
#else
    TemporaryRegisterScope temps(this);
    Register representation = temps.AcquireScratch();

    // TODO(victorgomes): Add fast path for external strings.
    And(representation, instance_type, Operand(kStringRepresentationMask));
    MacroAssembler::Branch(&seq_string, kEqual, representation,
                           Operand(kSeqStringTag), Label::kNear);
    MacroAssembler::Branch(&cons_string, kEqual, representation,
                           Operand(kConsStringTag), Label::kNear);
    MacroAssembler::Branch(&sliced_string, kEqual, representation,
                           Operand(kSlicedStringTag), Label::kNear);
    MacroAssembler::Branch(deferred_runtime_call, kNotEqual, representation,
                           Operand(kThinStringTag));
    // Fallthrough to thin string.
#endif
  }

  // Is a thin string.
  {
    LoadTaggedField(string, string, offsetof(ThinString, actual_));
    MacroAssembler::Branch(&loop, Label::kNear);
  }

  bind(&sliced_string);
  {
    TemporaryRegisterScope temps(this);
    Register offset = temps.AcquireScratch();

    LoadAndUntagTaggedSignedField(offset, string,
                                  offsetof(SlicedString, offset_));
    LoadTaggedField(string, string, offsetof(SlicedString, parent_));
    Add_d(index, index, Operand(offset));
    MacroAssembler::Branch(&loop, Label::kNear);
  }

  bind(&cons_string);
  {
    // Reuse {instance_type} register here, since CompareRoot requires a scratch
    // register as well.
    Register second_string = scratch1;
    LoadTaggedFieldWithoutDecompressing(second_string, string,
                                        offsetof(ConsString, second_));
    CompareRoot(second_string,
                RootIndex::kempty_string);  // Sets 1 to flag if not equal
    JumpIf(ne, deferred_runtime_call);      // Check the flag to not be equal 0
    LoadTaggedField(string, string, offsetof(ConsString, first_));
    MacroAssembler::Branch(&loop,
                           Label::kNear);  // Try again with first string.
  }

  bind(&seq_string);
  {
    Label two_byte_string;
#if V8_STATIC_ROOTS_BOOL
    if (InstanceTypeChecker::kTwoByteStringMapBit == 0) {
      TestInt32AndJumpIfAllClear(map,
                                 InstanceTypeChecker::kStringMapEncodingMask,
                                 &two_byte_string, Label::kNear);
    } else {
      TestInt32AndJumpIfAnySet(map, InstanceTypeChecker::kStringMapEncodingMask,
                               &two_byte_string, Label::kNear);
    }
#else
    And(instance_type, instance_type, Operand(kStringEncodingMask));
    MacroAssembler::Branch(&two_byte_string, equal, instance_type,
                           Operand(kTwoByteStringTag), Label::kNear);
#endif
    // The result of one-byte string will be the same for both modes
    // (CharCodeAt/CodePointAt), since it cannot be the first half of a
    // surrogate pair.
    SeqOneByteStringCharCodeAt(result, string, index);
    MacroAssembler::Branch(result_fits_one_byte);

    bind(&two_byte_string);
    // {instance_type} is unused from this point, so we can use as scratch.
    Register scratch = scratch1;
    slli_d(scratch, index, 1);
    Add_d(scratch, scratch,
          Operand(OFFSET_OF_DATA_START(SeqTwoByteString) - kHeapObjectTag));

    if (mode == BuiltinStringPrototypeCharCodeOrCodePointAt::kCharCodeAt) {
      Ld_hu(result, MemOperand(string, scratch));
    } else {
      DCHECK_EQ(mode,
                BuiltinStringPrototypeCharCodeOrCodePointAt::kCodePointAt);
      Register string_backup = string;
      if (result == string) {
        string_backup = scratch2;
        mov(string_backup, string);
      }
      Ld_hu(result, MemOperand(string, scratch));

      Register first_code_point = scratch;
      And(first_code_point, result, Operand(0xfc00));
      MacroAssembler::Branch(*done, kNotEqual, first_code_point,
                             Operand(0xd800), Label::kNear);

      Register length = scratch;
      Ld_w(length, FieldMemOperand(string_backup, offsetof(String, length_)));
      Add_w(index, index, Operand(1));
      MacroAssembler::Branch(*done, kGreaterThanEqual, index, Operand(length),
                             Label::kNear);

      Register second_code_point = scratch;
      slli_d(index, index, 1);
      Add_d(index, index,
            Operand(OFFSET_OF_DATA_START(SeqTwoByteString) - kHeapObjectTag));
      Ld_hu(second_code_point, MemOperand(string_backup, index));

      // {index} is not needed at this point.
      Register scratch2 = index;
      And(scratch2, second_code_point, Operand(0xfc00));
      MacroAssembler::Branch(*done, kNotEqual, scratch2, Operand(0xdc00),
                             Label::kNear);

      int surrogate_offset = 0x10000 - (0xd800 << 10) - 0xdc00;
      Add_d(second_code_point, second_code_point, Operand(surrogate_offset));
      slli_d(result, result, 10);
      Add_d(result, result, Operand(second_code_point));
    }

    // Fallthrough.
  }

  bind(*done);

  if (v8_flags.debug_code) {
    // We make sure that the user of this macro is not relying in string and
    // index to not be clobbered.
    if (result != string) {
      li(string, Operand(0xdeadbeef));
    }
    if (result != index) {
      li(index, Operand(0xdeadbeef));
    }
  }
}

void MaglevAssembler::SeqOneByteStringCharCodeAt(Register result,
                                                 Register string,
                                                 Register index) {
  ASM_CODE_COMMENT(this);
  TemporaryRegisterScope scope(this);
  Register scratch = scope.AcquireScratch();
  if (v8_flags.debug_code) {
    // Check if {string} is a string.
    AssertNotSmi(string);
    LoadMap(scratch, string);
    {
      TemporaryRegisterScope scope(this);
      Register scratch2 = scope.AcquireScratch();
      GetInstanceTypeRange(scratch, scratch, FIRST_STRING_TYPE, scratch2);
      Check(kUnsignedLessThanEqual, AbortReason::kUnexpectedValue, scratch2,
            Operand(LAST_STRING_TYPE - FIRST_STRING_TYPE));
    }

    // Check if {string} is a sequential one-byte string.
    AndInt32(scratch, kStringRepresentationAndEncodingMask);
    CompareInt32AndAssert(scratch, kSeqOneByteStringTag, kEqual,
                          AbortReason::kUnexpectedValue);

    LoadInt32(scratch, FieldMemOperand(string, offsetof(String, length_)));
    CompareInt32AndAssert(index, scratch, kUnsignedLessThan,
                          AbortReason::kUnexpectedValue);
  }

  // TODO(loong64): is index an uint32 value?
  Add_d(scratch, index,
        Operand(OFFSET_OF_DATA_START(SeqOneByteString) - kHeapObjectTag));
  Ld_bu(result, MemOperand(string, scratch));
}

void MaglevAssembler::CountLeadingZerosInt32(Register dst, Register src) {
  clz_w(dst, src);
}

void MaglevAssembler::TruncateDoubleToInt32(Register dst, DoubleRegister src) {
  ZoneLabelRef done(this);
  Label* slow_path = MakeDeferredCode(
      [](MaglevAssembler* masm, DoubleRegister src, Register dst,
         ZoneLabelRef done) {
        __ AllocateStackSpace(kDoubleSize);
        __ Fst_d(src, MemOperand(sp, 0));
        __ CallBuiltin(Builtin::kDoubleToI);
        __ Ld_w(dst, MemOperand(sp, 0));
        __ Add_d(sp, sp, Operand(kDoubleSize));
        __ Jump(*done);
      },
      src, dst, done);
  TryInlineTruncateDoubleToI(dst, src, *done);
  Jump(slow_path);
  bind(*done);
}

void MaglevAssembler::TryTruncateDoubleToInt32(Register dst, DoubleRegister src,
                                               Label* fail) {
  TemporaryRegisterScope temps(this);
  DoubleRegister converted_back = temps.AcquireScratchDouble();
  DoubleRegister scratch = temps.AcquireScratchDouble();

  // Convert the input float64 value to int32.
  ftintrz_w_d(scratch, src);
  // Convert that int32 value back to float64.
  ffint_d_w(converted_back, scratch);
  // Check that the result of the float64->int32->float64 is equal to the input
  // (i.e. that the conversion didn't truncate).
  CompareF64(converted_back, src, CEQ);
  BranchFalseF(fail);

  movfr2gr_s(dst, scratch);
  // Check if {input} is -0.
  Label check_done;
  BranchShort(&check_done, ne, dst, Operand(zero_reg));

  // In case of 0, we need to check for the IEEE 0 pattern (which is all zeros).
  Register input_bits = temps.AcquireScratch();
  movfr2gr_d(input_bits, src);
  MacroAssembler::Branch(fail, ne, input_bits, Operand(zero_reg));

  bind(&check_done);
}

void MaglevAssembler::TryTruncateDoubleToUint32(Register dst,
                                                DoubleRegister src,
                                                Label* fail) {
  TemporaryRegisterScope temps(this);
  DoubleRegister converted_back = temps.AcquireScratchDouble();

  // Convert the input float64 value to uint32.
  ftintrz_l_d(converted_back, src);
  movfr2gr_d(dst, converted_back);
  Bstrpick_d(dst, dst, 31, 0);
  // Convert that uint32 value back to float64.
  movgr2fr_d(converted_back, dst);
  ffint_d_l(converted_back, converted_back);
  // Check that the result of the float64->uint32->float64 is equal to the input
  // (i.e. that the conversion didn't truncate).
  CompareF64(converted_back, src, CEQ);
  BranchFalseF(fail);

  // Check if {input} is -0.
  Label check_done;
  BranchShort(&check_done, ne, dst, Operand(zero_reg));

  // In case of 0, we need to check for the IEEE 0 pattern (which is all zeros).
  Register input_bits = temps.AcquireScratch();
  movfr2gr_d(input_bits, src);
  MacroAssembler::Branch(fail, ne, input_bits, Operand(zero_reg));

  bind(&check_done);
  slli_w(dst, dst, 0);
}

void MaglevAssembler::TryChangeFloat64ToIndex(Register result,
                                              DoubleRegister value,
                                              Label* success, Label* fail) {
  TemporaryRegisterScope temps(this);
  DoubleRegister converted_back = temps.AcquireScratchDouble();

  // Convert the input float64 value to int32.
  ftintrz_w_d(converted_back, value);
  movfr2gr_s(result, converted_back);
  // Convert that int32 value back to float64.
  ffint_d_w(converted_back, converted_back);
  // Check that the result of the float64->int32->float64 is equal to
  // the input (i.e. that the conversion didn't truncate).
  CompareF64(value, converted_back, CEQ);
  BranchFalseF(fail);
  Jump(success);
}

void MaglevAssembler::Move(ExternalReference dst, int32_t imm) {
  TemporaryRegisterScope temps(this);
  Register scratch_imm = temps.AcquireScratch();
  Register scratch_dst = temps.AcquireScratch();
  li(scratch_imm, imm);
  St_d(scratch_imm, ExternalReferenceAsOperand(dst, scratch_dst));
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
