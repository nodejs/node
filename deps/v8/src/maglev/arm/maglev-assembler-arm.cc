// Copyright 2023 the V8 project authors. All rights reserved.
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
  DCHECK(allow_allocate());
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
  ldr(object, ExternalReferenceAsOperand(top, scratch));
  add(new_top, object, Operand(size_in_bytes), LeaveCC);
  ldr(scratch, ExternalReferenceAsOperand(limit, scratch));
  cmp(new_top, scratch);
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
        __ b(*done);
      },
      register_snapshot, object,
      in_new_space ? Builtin::kAllocateRegularInYoungGeneration
                   : Builtin::kAllocateRegularInOldGeneration,
      size_in_bytes, done);
  // Store new top and tag object.
  Move(ExternalReferenceAsOperand(top, scratch), new_top);
  add(object, object, Operand(kHeapObjectTag - size_in_bytes), LeaveCC);
  bind(*done);
}

void MaglevAssembler::StoreTaggedFieldWithWriteBarrier(
    Register object, int offset, Register value,
    RegisterSnapshot register_snapshot, ValueIsCompressed value_is_compressed,
    ValueCanBeSmi value_can_be_smi) {
  MAGLEV_NOT_IMPLEMENTED();
}

void MaglevAssembler::TestTypeOf(
    Register object, interpreter::TestTypeOfFlags::LiteralFlag literal,
    Label* is_true, Label::Distance true_distance, bool fallthrough_when_true,
    Label* is_false, Label::Distance false_distance,
    bool fallthrough_when_false) {
  MAGLEV_NOT_IMPLEMENTED();
}

void MaglevAssembler::Prologue(Graph* graph) {
  ScratchRegisterScope temps(this);
  temps.Include({r4, r8});
  if (!graph->is_osr()) {
    BailoutIfDeoptimized();
  }

  CHECK_IMPLIES(graph->is_osr(), !graph->has_recursive_calls());
  if (graph->has_recursive_calls()) {
    bind(code_gen_state()->entry_label());
  }

  // Tiering support.
  // TODO(jgruber): Extract to a builtin.
  if (v8_flags.turbofan && !graph->is_osr()) {
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

  if (graph->is_osr()) {
    MAGLEV_NOT_IMPLEMENTED();
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
    ScratchRegisterScope temps(this);
    Register scratch = temps.Acquire();
    Move(scratch, 0);

    // Magic value. Experimentally, an unroll size of 8 doesn't seem any
    // worse than fully unrolled pushes.
    const int kLoopUnrollSize = 8;
    int tagged_slots = graph->tagged_stack_slots();
    if (tagged_slots < kLoopUnrollSize) {
      // If the frame is small enough, just unroll the frame fill
      // completely.
      for (int i = 0; i < tagged_slots; ++i) {
        Push(scratch);
      }
    } else {
      // Extract the first few slots to round to the unroll size.
      int first_slots = tagged_slots % kLoopUnrollSize;
      for (int i = 0; i < first_slots; ++i) {
        Push(scratch);
      }
      Register unroll_counter = temps.Acquire();
      Move(unroll_counter, tagged_slots / kLoopUnrollSize);
      // We enter the loop unconditionally, so make sure we need to loop at
      // least once.
      DCHECK_GT(tagged_slots / kLoopUnrollSize, 0);
      Label loop;
      bind(&loop);
      for (int i = 0; i < kLoopUnrollSize; ++i) {
        Push(scratch);
      }
      sub(unroll_counter, unroll_counter, Operand(1), SetCC);
      b(kGreaterThan, &loop);
    }
  }
  if (graph->untagged_stack_slots() > 0) {
    // Extend rsp by the size of the remaining untagged part of the frame,
    // no need to initialise these.
    sub(sp, sp, Operand(graph->untagged_stack_slots() * kSystemPointerSize));
  }
}

void MaglevAssembler::MaybeEmitDeoptBuiltinsCall(size_t eager_deopt_count,
                                                 Label* eager_deopt_entry,
                                                 size_t lazy_deopt_count,
                                                 Label* lazy_deopt_entry) {
  CheckConstPool(true, false);
}

void MaglevAssembler::StringFromCharCode(RegisterSnapshot register_snapshot,
                                         Label* char_code_fits_one_byte,
                                         Register result, Register char_code,
                                         Register scratch) {
  MAGLEV_NOT_IMPLEMENTED();
}

void MaglevAssembler::LoadSingleCharacterString(Register result,
                                                Register char_code,
                                                Register scratch) {
  MAGLEV_NOT_IMPLEMENTED();
}

void MaglevAssembler::StringCharCodeOrCodePointAt(
    BuiltinStringPrototypeCharCodeOrCodePointAt::Mode mode,
    RegisterSnapshot& register_snapshot, Register result, Register string,
    Register index, Register instance_type, Label* result_fits_one_byte) {
  MAGLEV_NOT_IMPLEMENTED();
}

void MaglevAssembler::TruncateDoubleToInt32(Register dst, DoubleRegister src) {
  ZoneLabelRef done(this);
  Label* slow_path = MakeDeferredCode(
      [](MaglevAssembler* masm, DoubleRegister src, Register dst,
         ZoneLabelRef done) {
        __ push(lr);
        __ AllocateStackSpace(kDoubleSize);
        __ vstr(src, MemOperand(sp, 0));
        __ CallBuiltin(Builtin::kDoubleToI);
        __ ldr(dst, MemOperand(sp, 0));
        __ add(sp, sp, Operand(kDoubleSize));
        __ pop(lr);
        __ Jump(*done);
      },
      src, dst, done);
  TryInlineTruncateDoubleToI(dst, src, *done);
  Jump(slow_path);
  bind(*done);
}

void MaglevAssembler::TryTruncateDoubleToInt32(Register dst, DoubleRegister src,
                                               Label* fail) {
  UseScratchRegisterScope temps(this);
  LowDwVfpRegister low_double = temps.AcquireLowD();
  SwVfpRegister temp_vfps = low_double.low();
  DoubleRegister converted_back = low_double;
  Label done;

  // Convert the input float64 value to int32.
  vcvt_s32_f64(temp_vfps, src);
  vmov(dst, temp_vfps);

  // Convert that int32 value back to float64.
  vcvt_f64_s32(converted_back, temp_vfps);

  // Check that the result of the float64->int32->float64 is equal to the input
  // (i.e. that the conversion didn't truncate.
  VFPCompareAndSetFlags(src, converted_back);
  JumpIf(kNotEqual, fail);

  // Check if {input} is -0.
  tst(dst, dst);
  JumpIf(kNotEqual, &done);

  // In case of 0, we need to check the high bits for the IEEE -0 pattern.
  {
    Register high_word32_of_input = temps.Acquire();
    VmovHigh(high_word32_of_input, src);
    cmp(high_word32_of_input, Operand(0));
    JumpIf(kLessThan, fail);
  }

  bind(&done);
}

void MaglevAssembler::TryChangeFloat64ToIndex(Register result,
                                              DoubleRegister value,
                                              Label* success, Label* fail) {
  UseScratchRegisterScope temps(this);
  LowDwVfpRegister low_double = temps.AcquireLowD();
  SwVfpRegister temp_vfps = low_double.low();
  DoubleRegister converted_back = low_double;
  // Convert the input float64 value to int32.
  vcvt_s32_f64(temp_vfps, value);
  vmov(result, temp_vfps);
  // Convert that int32 value back to float64.
  vcvt_f64_s32(converted_back, temp_vfps);
  // Check that the result of the float64->int32->float64 is equal to
  // the input (i.e. that the conversion didn't truncate).
  VFPCompareAndSetFlags(value, converted_back);
  JumpIf(kEqual, success);
  Jump(fail);
}

void MaglevAssembler::StringLength(Register result, Register string) {
  MAGLEV_NOT_IMPLEMENTED();
}

void MaglevAssembler::StoreFixedArrayElementWithWriteBarrier(
    Register array, Register index, Register value,
    RegisterSnapshot register_snapshot) {
  MAGLEV_NOT_IMPLEMENTED();
}

void MaglevAssembler::StoreFixedArrayElementNoWriteBarrier(Register array,
                                                           Register index,
                                                           Register value) {
  MAGLEV_NOT_IMPLEMENTED();
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8
