// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/assembler-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/codegen/register-configuration.h"
#include "src/codegen/safepoint-table.h"
#include "src/deoptimizer/deoptimizer.h"

namespace v8 {
namespace internal {

const bool Deoptimizer::kSupportsFixedDeoptExitSizes = false;
const int Deoptimizer::kNonLazyDeoptExitSize = 0;
const int Deoptimizer::kLazyDeoptExitSize = 0;

#define __ masm->

// This code tries to be close to ia32 code so that any changes can be
// easily ported.
void Deoptimizer::GenerateDeoptimizationEntries(MacroAssembler* masm,
                                                Isolate* isolate,
                                                DeoptimizeKind deopt_kind) {
  NoRootArrayScope no_root_array(masm);

  // Unlike on ARM we don't save all the registers, just the useful ones.
  // For the rest, there are gaps on the stack, so the offsets remain the same.
  const int kNumberOfRegisters = Register::kNumRegisters;

  RegList restored_regs = kJSCallerSaved | kCalleeSaved;
  RegList saved_regs = restored_regs | sp.bit();

  const int kDoubleRegsSize = kDoubleSize * DoubleRegister::kNumRegisters;

  // Save all double registers before messing with them.
  __ subi(sp, sp, Operand(kDoubleRegsSize));
  const RegisterConfiguration* config = RegisterConfiguration::Default();
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    const DoubleRegister dreg = DoubleRegister::from_code(code);
    int offset = code * kDoubleSize;
    __ stfd(dreg, MemOperand(sp, offset));
  }

  // Push saved_regs (needed to populate FrameDescription::registers_).
  // Leave gaps for other registers.
  __ subi(sp, sp, Operand(kNumberOfRegisters * kSystemPointerSize));
  for (int16_t i = kNumberOfRegisters - 1; i >= 0; i--) {
    if ((saved_regs & (1 << i)) != 0) {
      __ StoreP(ToRegister(i), MemOperand(sp, kSystemPointerSize * i));
    }
  }
  {
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ mov(scratch, Operand(ExternalReference::Create(
                        IsolateAddressId::kCEntryFPAddress, isolate)));
    __ StoreP(fp, MemOperand(scratch));
  }
  const int kSavedRegistersAreaSize =
      (kNumberOfRegisters * kSystemPointerSize) + kDoubleRegsSize;

  // Get the bailout id is passed as r29 by the caller.
  __ mr(r5, r29);

  // Get the address of the location in the code object (r6) (return
  // address for lazy deoptimization) and compute the fp-to-sp delta in
  // register r7.
  __ mflr(r6);
  __ addi(r7, sp, Operand(kSavedRegistersAreaSize));
  __ sub(r7, fp, r7);

  // Allocate a new deoptimizer object.
  // Pass six arguments in r3 to r8.
  __ PrepareCallCFunction(6, r8);
  __ li(r3, Operand::Zero());
  Label context_check;
  __ LoadP(r4, MemOperand(fp, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ JumpIfSmi(r4, &context_check);
  __ LoadP(r3, MemOperand(fp, StandardFrameConstants::kFunctionOffset));
  __ bind(&context_check);
  __ li(r4, Operand(static_cast<int>(deopt_kind)));
  // r5: bailout id already loaded.
  // r6: code address or 0 already loaded.
  // r7: Fp-to-sp delta.
  __ mov(r8, Operand(ExternalReference::isolate_address(isolate)));
  // Call Deoptimizer::New().
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ CallCFunction(ExternalReference::new_deoptimizer_function(), 6);
  }

  // Preserve "deoptimizer" object in register r3 and get the input
  // frame descriptor pointer to r4 (deoptimizer->input_);
  __ LoadP(r4, MemOperand(r3, Deoptimizer::input_offset()));

  // Copy core registers into FrameDescription::registers_[kNumRegisters].
  DCHECK_EQ(Register::kNumRegisters, kNumberOfRegisters);
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset =
        (i * kSystemPointerSize) + FrameDescription::registers_offset();
    __ LoadP(r5, MemOperand(sp, i * kSystemPointerSize));
    __ StoreP(r5, MemOperand(r4, offset));
  }

  int double_regs_offset = FrameDescription::double_registers_offset();
  // Copy double registers to
  // double_registers_[DoubleRegister::kNumRegisters]
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    int dst_offset = code * kDoubleSize + double_regs_offset;
    int src_offset =
        code * kDoubleSize + kNumberOfRegisters * kSystemPointerSize;
    __ lfd(d0, MemOperand(sp, src_offset));
    __ stfd(d0, MemOperand(r4, dst_offset));
  }

  // Mark the stack as not iterable for the CPU profiler which won't be able to
  // walk the stack without the return address.
  {
    UseScratchRegisterScope temps(masm);
    Register is_iterable = temps.Acquire();
    Register zero = r7;
    __ Move(is_iterable, ExternalReference::stack_is_iterable_address(isolate));
    __ li(zero, Operand(0));
    __ stb(zero, MemOperand(is_iterable));
  }

  // Remove the saved registers from the stack.
  __ addi(sp, sp, Operand(kSavedRegistersAreaSize));

  // Compute a pointer to the unwinding limit in register r5; that is
  // the first stack slot not part of the input frame.
  __ LoadP(r5, MemOperand(r4, FrameDescription::frame_size_offset()));
  __ add(r5, r5, sp);

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ addi(r6, r4, Operand(FrameDescription::frame_content_offset()));
  Label pop_loop;
  Label pop_loop_header;
  __ b(&pop_loop_header);
  __ bind(&pop_loop);
  __ pop(r7);
  __ StoreP(r7, MemOperand(r6, 0));
  __ addi(r6, r6, Operand(kSystemPointerSize));
  __ bind(&pop_loop_header);
  __ cmp(r5, sp);
  __ bne(&pop_loop);

  // Compute the output frame in the deoptimizer.
  __ push(r3);  // Preserve deoptimizer object across call.
  // r3: deoptimizer object; r4: scratch.
  __ PrepareCallCFunction(1, r4);
  // Call Deoptimizer::ComputeOutputFrames().
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ CallCFunction(ExternalReference::compute_output_frames_function(), 1);
  }
  __ pop(r3);  // Restore deoptimizer object (class Deoptimizer).

  __ LoadP(sp, MemOperand(r3, Deoptimizer::caller_frame_top_offset()));

  // Replace the current (input) frame with the output frames.
  Label outer_push_loop, inner_push_loop, outer_loop_header, inner_loop_header;
  // Outer loop state: r7 = current "FrameDescription** output_",
  // r4 = one past the last FrameDescription**.
  __ lwz(r4, MemOperand(r3, Deoptimizer::output_count_offset()));
  __ LoadP(r7, MemOperand(r3, Deoptimizer::output_offset()));  // r7 is output_.
  __ ShiftLeftImm(r4, r4, Operand(kSystemPointerSizeLog2));
  __ add(r4, r7, r4);
  __ b(&outer_loop_header);

  __ bind(&outer_push_loop);
  // Inner loop state: r5 = current FrameDescription*, r6 = loop index.
  __ LoadP(r5, MemOperand(r7, 0));  // output_[ix]
  __ LoadP(r6, MemOperand(r5, FrameDescription::frame_size_offset()));
  __ b(&inner_loop_header);

  __ bind(&inner_push_loop);
  __ addi(r6, r6, Operand(-sizeof(intptr_t)));
  __ add(r9, r5, r6);
  __ LoadP(r9, MemOperand(r9, FrameDescription::frame_content_offset()));
  __ push(r9);

  __ bind(&inner_loop_header);
  __ cmpi(r6, Operand::Zero());
  __ bne(&inner_push_loop);  // test for gt?

  __ addi(r7, r7, Operand(kSystemPointerSize));
  __ bind(&outer_loop_header);
  __ cmp(r7, r4);
  __ blt(&outer_push_loop);

  __ LoadP(r4, MemOperand(r3, Deoptimizer::input_offset()));
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    const DoubleRegister dreg = DoubleRegister::from_code(code);
    int src_offset = code * kDoubleSize + double_regs_offset;
    __ lfd(dreg, MemOperand(r4, src_offset));
  }

  // Push pc, and continuation from the last output frame.
  __ LoadP(r9, MemOperand(r5, FrameDescription::pc_offset()));
  __ push(r9);
  __ LoadP(r9, MemOperand(r5, FrameDescription::continuation_offset()));
  __ push(r9);

  // Restore the registers from the last output frame.
  {
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    DCHECK(!(scratch.bit() & restored_regs));
    __ mr(scratch, r5);
    for (int i = kNumberOfRegisters - 1; i >= 0; i--) {
      int offset =
          (i * kSystemPointerSize) + FrameDescription::registers_offset();
      if ((restored_regs & (1 << i)) != 0) {
        __ LoadP(ToRegister(i), MemOperand(scratch, offset));
      }
    }
  }

  {
    UseScratchRegisterScope temps(masm);
    Register is_iterable = temps.Acquire();
    Register one = r7;
    __ Move(is_iterable, ExternalReference::stack_is_iterable_address(isolate));
    __ li(one, Operand(1));
    __ stb(one, MemOperand(is_iterable));
  }

  {
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ pop(scratch);  // get continuation, leave pc on stack
    __ pop(r0);
    __ mtlr(r0);
    __ Jump(scratch);
  }

  __ stop();
}

Float32 RegisterValues::GetFloatRegister(unsigned n) const {
  float float_val = static_cast<float>(double_registers_[n].get_scalar());
  return Float32::FromBits(bit_cast<uint32_t>(float_val));
}

void FrameDescription::SetCallerPc(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerFp(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerConstantPool(unsigned offset, intptr_t value) {
  DCHECK(FLAG_enable_embedded_constant_pool);
  SetFrameSlot(offset, value);
}

void FrameDescription::SetPc(intptr_t pc) { pc_ = pc; }

#undef __
}  // namespace internal
}  // namespace v8
