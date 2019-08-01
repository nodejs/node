// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/macro-assembler.h"
#include "src/codegen/register-configuration.h"
#include "src/codegen/safepoint-table.h"
#include "src/deoptimizer/deoptimizer.h"

namespace v8 {
namespace internal {

#define __ masm->

// This code tries to be close to ia32 code so that any changes can be
// easily ported.
void Deoptimizer::GenerateDeoptimizationEntries(MacroAssembler* masm,
                                                Isolate* isolate,
                                                DeoptimizeKind deopt_kind) {
  NoRootArrayScope no_root_array(masm);

  // Save all the registers onto the stack
  const int kNumberOfRegisters = Register::kNumRegisters;

  RegList restored_regs = kJSCallerSaved | kCalleeSaved;

  const int kDoubleRegsSize = kDoubleSize * DoubleRegister::kNumRegisters;
  const int kFloatRegsSize = kFloatSize * FloatRegister::kNumRegisters;

  // Save all double registers before messing with them.
  __ lay(sp, MemOperand(sp, -kDoubleRegsSize));
  const RegisterConfiguration* config = RegisterConfiguration::Default();
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    const DoubleRegister dreg = DoubleRegister::from_code(code);
    int offset = code * kDoubleSize;
    __ StoreDouble(dreg, MemOperand(sp, offset));
  }
  // Save all float registers before messing with them.
  __ lay(sp, MemOperand(sp, -kFloatRegsSize));
  for (int i = 0; i < config->num_allocatable_float_registers(); ++i) {
    int code = config->GetAllocatableFloatCode(i);
    const FloatRegister dreg = FloatRegister::from_code(code);
    int offset = code * kFloatSize;
    __ StoreFloat32(dreg, MemOperand(sp, offset));
  }

  // Push all GPRs onto the stack
  __ lay(sp, MemOperand(sp, -kNumberOfRegisters * kPointerSize));
  __ StoreMultipleP(r0, sp, MemOperand(sp));  // Save all 16 registers

  __ mov(r1, Operand(ExternalReference::Create(
                 IsolateAddressId::kCEntryFPAddress, isolate)));
  __ StoreP(fp, MemOperand(r1));

  const int kSavedRegistersAreaSize =
      (kNumberOfRegisters * kPointerSize) + kDoubleRegsSize + kFloatRegsSize;

  // The bailout id is passed using r10
  __ LoadRR(r4, r10);

  // Cleanse the Return address for 31-bit
  __ CleanseP(r14);

  // Get the address of the location in the code object (r5)(return
  // address for lazy deoptimization) and compute the fp-to-sp delta in
  // register r6.
  __ LoadRR(r5, r14);

  __ la(r6, MemOperand(sp, kSavedRegistersAreaSize));
  __ SubP(r6, fp, r6);

  // Allocate a new deoptimizer object.
  // Pass six arguments in r2 to r7.
  __ PrepareCallCFunction(6, r7);
  __ LoadImmP(r2, Operand::Zero());
  Label context_check;
  __ LoadP(r3, MemOperand(fp, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ JumpIfSmi(r3, &context_check);
  __ LoadP(r2, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ bind(&context_check);
  __ LoadImmP(r3, Operand(static_cast<int>(deopt_kind)));
  // r4: bailout id already loaded.
  // r5: code address or 0 already loaded.
  // r6: Fp-to-sp delta.
  // Parm6: isolate is passed on the stack.
  __ mov(r7, Operand(ExternalReference::isolate_address(isolate)));
  __ StoreP(r7, MemOperand(sp, kStackFrameExtraParamSlot * kPointerSize));

  // Call Deoptimizer::New().
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ CallCFunction(ExternalReference::new_deoptimizer_function(), 6);
  }

  // Preserve "deoptimizer" object in register r2 and get the input
  // frame descriptor pointer to r3 (deoptimizer->input_);
  __ LoadP(r3, MemOperand(r2, Deoptimizer::input_offset()));

  // Copy core registers into FrameDescription::registers_[kNumRegisters].
  // DCHECK_EQ(Register::kNumRegisters, kNumberOfRegisters);
  // __ mvc(MemOperand(r3, FrameDescription::registers_offset()),
  //        MemOperand(sp), kNumberOfRegisters * kPointerSize);
  // Copy core registers into FrameDescription::registers_[kNumRegisters].
  // TODO(john.yan): optimize the following code by using mvc instruction
  DCHECK_EQ(Register::kNumRegisters, kNumberOfRegisters);
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    __ LoadP(r4, MemOperand(sp, i * kPointerSize));
    __ StoreP(r4, MemOperand(r3, offset));
  }

  int double_regs_offset = FrameDescription::double_registers_offset();
  // Copy double registers to
  // double_registers_[DoubleRegister::kNumRegisters]
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    int dst_offset = code * kDoubleSize + double_regs_offset;
    int src_offset =
        code * kDoubleSize + kNumberOfRegisters * kPointerSize + kFloatRegsSize;
    // TODO(joransiu): MVC opportunity
    __ LoadDouble(d0, MemOperand(sp, src_offset));
    __ StoreDouble(d0, MemOperand(r3, dst_offset));
  }

  int float_regs_offset = FrameDescription::float_registers_offset();
  // Copy float registers to
  // float_registers_[FloatRegister::kNumRegisters]
  for (int i = 0; i < config->num_allocatable_float_registers(); ++i) {
    int code = config->GetAllocatableFloatCode(i);
    int dst_offset = code * kFloatSize + float_regs_offset;
    int src_offset = code * kFloatSize + kNumberOfRegisters * kPointerSize;
    // TODO(joransiu): MVC opportunity
    __ LoadFloat32(d0, MemOperand(sp, src_offset));
    __ StoreFloat32(d0, MemOperand(r3, dst_offset));
  }

  // Remove the saved registers from the stack.
  __ la(sp, MemOperand(sp, kSavedRegistersAreaSize));

  // Compute a pointer to the unwinding limit in register r4; that is
  // the first stack slot not part of the input frame.
  __ LoadP(r4, MemOperand(r3, FrameDescription::frame_size_offset()));
  __ AddP(r4, sp);

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ la(r5, MemOperand(r3, FrameDescription::frame_content_offset()));
  Label pop_loop;
  Label pop_loop_header;
  __ b(&pop_loop_header, Label::kNear);
  __ bind(&pop_loop);
  __ pop(r6);
  __ StoreP(r6, MemOperand(r5, 0));
  __ la(r5, MemOperand(r5, kPointerSize));
  __ bind(&pop_loop_header);
  __ CmpP(r4, sp);
  __ bne(&pop_loop);

  // Compute the output frame in the deoptimizer.
  __ push(r2);  // Preserve deoptimizer object across call.
  // r2: deoptimizer object; r3: scratch.
  __ PrepareCallCFunction(1, r3);
  // Call Deoptimizer::ComputeOutputFrames().
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ CallCFunction(ExternalReference::compute_output_frames_function(), 1);
  }
  __ pop(r2);  // Restore deoptimizer object (class Deoptimizer).

  __ LoadP(sp, MemOperand(r2, Deoptimizer::caller_frame_top_offset()));

  // Replace the current (input) frame with the output frames.
  Label outer_push_loop, inner_push_loop, outer_loop_header, inner_loop_header;
  // Outer loop state: r6 = current "FrameDescription** output_",
  // r3 = one past the last FrameDescription**.
  __ LoadlW(r3, MemOperand(r2, Deoptimizer::output_count_offset()));
  __ LoadP(r6, MemOperand(r2, Deoptimizer::output_offset()));  // r6 is output_.
  __ ShiftLeftP(r3, r3, Operand(kPointerSizeLog2));
  __ AddP(r3, r6, r3);
  __ b(&outer_loop_header, Label::kNear);

  __ bind(&outer_push_loop);
  // Inner loop state: r4 = current FrameDescription*, r5 = loop index.
  __ LoadP(r4, MemOperand(r6, 0));  // output_[ix]
  __ LoadP(r5, MemOperand(r4, FrameDescription::frame_size_offset()));
  __ b(&inner_loop_header, Label::kNear);

  __ bind(&inner_push_loop);
  __ SubP(r5, Operand(sizeof(intptr_t)));
  __ AddP(r8, r4, r5);
  __ LoadP(r8, MemOperand(r8, FrameDescription::frame_content_offset()));
  __ push(r8);

  __ bind(&inner_loop_header);
  __ CmpP(r5, Operand::Zero());
  __ bne(&inner_push_loop);  // test for gt?

  __ AddP(r6, r6, Operand(kPointerSize));
  __ bind(&outer_loop_header);
  __ CmpP(r6, r3);
  __ blt(&outer_push_loop);

  __ LoadP(r3, MemOperand(r2, Deoptimizer::input_offset()));
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    const DoubleRegister dreg = DoubleRegister::from_code(code);
    int src_offset = code * kDoubleSize + double_regs_offset;
    __ ld(dreg, MemOperand(r3, src_offset));
  }

  // Push pc and continuation from the last output frame.
  __ LoadP(r8, MemOperand(r4, FrameDescription::pc_offset()));
  __ push(r8);
  __ LoadP(r8, MemOperand(r4, FrameDescription::continuation_offset()));
  __ push(r8);

  // Restore the registers from the last output frame.
  __ LoadRR(r1, r4);
  for (int i = kNumberOfRegisters - 1; i > 0; i--) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    if ((restored_regs & (1 << i)) != 0) {
      __ LoadP(ToRegister(i), MemOperand(r1, offset));
    }
  }

  __ pop(ip);  // get continuation, leave pc on stack
  __ pop(r14);
  __ Jump(ip);
  __ stop("Unreachable.");
}

bool Deoptimizer::PadTopOfStackRegister() { return false; }

void FrameDescription::SetCallerPc(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerFp(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerConstantPool(unsigned offset, intptr_t value) {
  // No out-of-line constant pool support.
  UNREACHABLE();
}

#undef __

}  // namespace internal
}  // namespace v8
