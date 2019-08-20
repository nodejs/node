// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/assembler-inl.h"
#include "src/codegen/macro-assembler.h"
#include "src/codegen/register-configuration.h"
#include "src/codegen/safepoint-table.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

#define __ masm->

// This code tries to be close to ia32 code so that any changes can be
// easily ported.
void Deoptimizer::GenerateDeoptimizationEntries(MacroAssembler* masm,
                                                Isolate* isolate,
                                                DeoptimizeKind deopt_kind) {
  NoRootArrayScope no_root_array(masm);

  // Save all general purpose registers before messing with them.
  const int kNumberOfRegisters = Register::kNumRegisters;

  // Everything but pc, lr and ip which will be saved but not restored.
  RegList restored_regs = kJSCallerSaved | kCalleeSaved | ip.bit();

  const int kDoubleRegsSize = kDoubleSize * DwVfpRegister::kNumRegisters;
  const int kFloatRegsSize = kFloatSize * SwVfpRegister::kNumRegisters;

  // Save all allocatable VFP registers before messing with them.
  {
    // We use a run-time check for VFP32DREGS.
    CpuFeatureScope scope(masm, VFP32DREGS,
                          CpuFeatureScope::kDontCheckSupported);
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();

    // Check CPU flags for number of registers, setting the Z condition flag.
    __ CheckFor32DRegs(scratch);

    // Push registers d0-d15, and possibly d16-d31, on the stack.
    // If d16-d31 are not pushed, decrease the stack pointer instead.
    __ vstm(db_w, sp, d16, d31, ne);
    // Okay to not call AllocateStackSpace here because the size is a known
    // small number and we need to use condition codes.
    __ sub(sp, sp, Operand(16 * kDoubleSize), LeaveCC, eq);
    __ vstm(db_w, sp, d0, d15);

    // Push registers s0-s31 on the stack.
    __ vstm(db_w, sp, s0, s31);
  }

  // Push all 16 registers (needed to populate FrameDescription::registers_).
  // TODO(1588) Note that using pc with stm is deprecated, so we should perhaps
  // handle this a bit differently.
  __ stm(db_w, sp, restored_regs | sp.bit() | lr.bit() | pc.bit());

  {
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ mov(scratch, Operand(ExternalReference::Create(
                        IsolateAddressId::kCEntryFPAddress, isolate)));
    __ str(fp, MemOperand(scratch));
  }

  const int kSavedRegistersAreaSize =
      (kNumberOfRegisters * kPointerSize) + kDoubleRegsSize + kFloatRegsSize;

  // Get the bailout id is passed as r10 by the caller.
  __ mov(r2, r10);

  // Get the address of the location in the code object (r3) (return
  // address for lazy deoptimization) and compute the fp-to-sp delta in
  // register r4.
  __ mov(r3, lr);
  __ add(r4, sp, Operand(kSavedRegistersAreaSize));
  __ sub(r4, fp, r4);

  // Allocate a new deoptimizer object.
  // Pass four arguments in r0 to r3 and fifth argument on stack.
  __ PrepareCallCFunction(6);
  __ mov(r0, Operand(0));
  Label context_check;
  __ ldr(r1, MemOperand(fp, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ JumpIfSmi(r1, &context_check);
  __ ldr(r0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ bind(&context_check);
  __ mov(r1, Operand(static_cast<int>(deopt_kind)));
  // r2: bailout id already loaded.
  // r3: code address or 0 already loaded.
  __ str(r4, MemOperand(sp, 0 * kPointerSize));  // Fp-to-sp delta.
  __ mov(r5, Operand(ExternalReference::isolate_address(isolate)));
  __ str(r5, MemOperand(sp, 1 * kPointerSize));  // Isolate.
  // Call Deoptimizer::New().
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ CallCFunction(ExternalReference::new_deoptimizer_function(), 6);
  }

  // Preserve "deoptimizer" object in register r0 and get the input
  // frame descriptor pointer to r1 (deoptimizer->input_);
  __ ldr(r1, MemOperand(r0, Deoptimizer::input_offset()));

  // Copy core registers into FrameDescription::registers_[kNumRegisters].
  DCHECK_EQ(Register::kNumRegisters, kNumberOfRegisters);
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    __ ldr(r2, MemOperand(sp, i * kPointerSize));
    __ str(r2, MemOperand(r1, offset));
  }

  // Copy VFP registers to
  // double_registers_[DoubleRegister::kNumAllocatableRegisters]
  int double_regs_offset = FrameDescription::double_registers_offset();
  const RegisterConfiguration* config = RegisterConfiguration::Default();
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    int dst_offset = code * kDoubleSize + double_regs_offset;
    int src_offset =
        code * kDoubleSize + kNumberOfRegisters * kPointerSize + kFloatRegsSize;
    __ vldr(d0, sp, src_offset);
    __ vstr(d0, r1, dst_offset);
  }

  // Copy VFP registers to
  // float_registers_[FloatRegister::kNumAllocatableRegisters]
  int float_regs_offset = FrameDescription::float_registers_offset();
  for (int i = 0; i < config->num_allocatable_float_registers(); ++i) {
    int code = config->GetAllocatableFloatCode(i);
    int dst_offset = code * kFloatSize + float_regs_offset;
    int src_offset = code * kFloatSize + kNumberOfRegisters * kPointerSize;
    __ ldr(r2, MemOperand(sp, src_offset));
    __ str(r2, MemOperand(r1, dst_offset));
  }

  // Remove the saved registers from the stack.
  __ add(sp, sp, Operand(kSavedRegistersAreaSize));

  // Compute a pointer to the unwinding limit in register r2; that is
  // the first stack slot not part of the input frame.
  __ ldr(r2, MemOperand(r1, FrameDescription::frame_size_offset()));
  __ add(r2, r2, sp);

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ add(r3, r1, Operand(FrameDescription::frame_content_offset()));
  Label pop_loop;
  Label pop_loop_header;
  __ b(&pop_loop_header);
  __ bind(&pop_loop);
  __ pop(r4);
  __ str(r4, MemOperand(r3, 0));
  __ add(r3, r3, Operand(sizeof(uint32_t)));
  __ bind(&pop_loop_header);
  __ cmp(r2, sp);
  __ b(ne, &pop_loop);

  // Compute the output frame in the deoptimizer.
  __ push(r0);  // Preserve deoptimizer object across call.
  // r0: deoptimizer object; r1: scratch.
  __ PrepareCallCFunction(1);
  // Call Deoptimizer::ComputeOutputFrames().
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ CallCFunction(ExternalReference::compute_output_frames_function(), 1);
  }
  __ pop(r0);  // Restore deoptimizer object (class Deoptimizer).

  __ ldr(sp, MemOperand(r0, Deoptimizer::caller_frame_top_offset()));

  // Replace the current (input) frame with the output frames.
  Label outer_push_loop, inner_push_loop, outer_loop_header, inner_loop_header;
  // Outer loop state: r4 = current "FrameDescription** output_",
  // r1 = one past the last FrameDescription**.
  __ ldr(r1, MemOperand(r0, Deoptimizer::output_count_offset()));
  __ ldr(r4, MemOperand(r0, Deoptimizer::output_offset()));  // r4 is output_.
  __ add(r1, r4, Operand(r1, LSL, 2));
  __ jmp(&outer_loop_header);
  __ bind(&outer_push_loop);
  // Inner loop state: r2 = current FrameDescription*, r3 = loop index.
  __ ldr(r2, MemOperand(r4, 0));  // output_[ix]
  __ ldr(r3, MemOperand(r2, FrameDescription::frame_size_offset()));
  __ jmp(&inner_loop_header);
  __ bind(&inner_push_loop);
  __ sub(r3, r3, Operand(sizeof(uint32_t)));
  __ add(r6, r2, Operand(r3));
  __ ldr(r6, MemOperand(r6, FrameDescription::frame_content_offset()));
  __ push(r6);
  __ bind(&inner_loop_header);
  __ cmp(r3, Operand::Zero());
  __ b(ne, &inner_push_loop);  // test for gt?
  __ add(r4, r4, Operand(kPointerSize));
  __ bind(&outer_loop_header);
  __ cmp(r4, r1);
  __ b(lt, &outer_push_loop);

  __ ldr(r1, MemOperand(r0, Deoptimizer::input_offset()));
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    DwVfpRegister reg = DwVfpRegister::from_code(code);
    int src_offset = code * kDoubleSize + double_regs_offset;
    __ vldr(reg, r1, src_offset);
  }

  // Push pc and continuation from the last output frame.
  __ ldr(r6, MemOperand(r2, FrameDescription::pc_offset()));
  __ push(r6);
  __ ldr(r6, MemOperand(r2, FrameDescription::continuation_offset()));
  __ push(r6);

  // Push the registers from the last output frame.
  for (int i = kNumberOfRegisters - 1; i >= 0; i--) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    __ ldr(r6, MemOperand(r2, offset));
    __ push(r6);
  }

  // Restore the registers from the stack.
  __ ldm(ia_w, sp, restored_regs);  // all but pc registers.

  // Remove sp, lr and pc.
  __ Drop(3);
  {
    UseScratchRegisterScope temps(masm);
    Register scratch = temps.Acquire();
    __ pop(scratch);  // get continuation, leave pc on stack
    __ pop(lr);
    __ Jump(scratch);
  }
  __ stop();
}

bool Deoptimizer::PadTopOfStackRegister() { return false; }

void FrameDescription::SetCallerPc(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerFp(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerConstantPool(unsigned offset, intptr_t value) {
  // No embedded constant pool support.
  UNREACHABLE();
}

#undef __

}  // namespace internal
}  // namespace v8
