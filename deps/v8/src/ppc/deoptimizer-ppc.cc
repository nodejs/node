// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assembler-inl.h"
#include "src/deoptimizer.h"
#include "src/register-configuration.h"
#include "src/safepoint-table.h"

namespace v8 {
namespace internal {

const int Deoptimizer::table_entry_size_ = 8;

#define __ masm()->

// This code tries to be close to ia32 code so that any changes can be
// easily ported.
void Deoptimizer::TableEntryGenerator::Generate() {
  GeneratePrologue();

  // Unlike on ARM we don't save all the registers, just the useful ones.
  // For the rest, there are gaps on the stack, so the offsets remain the same.
  const int kNumberOfRegisters = Register::kNumRegisters;

  RegList restored_regs = kJSCallerSaved | kCalleeSaved;
  RegList saved_regs = restored_regs | sp.bit();

  const int kDoubleRegsSize = kDoubleSize * DoubleRegister::kNumRegisters;
  const int kFloatRegsSize = kFloatSize * FloatRegister::kNumRegisters;

  // Save all double registers before messing with them.
  __ subi(sp, sp, Operand(kDoubleRegsSize));
  const RegisterConfiguration* config = RegisterConfiguration::Default();
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    const DoubleRegister dreg = DoubleRegister::from_code(code);
    int offset = code * kDoubleSize;
    __ stfd(dreg, MemOperand(sp, offset));
  }
  // Save all float registers before messing with them.
  __ subi(sp, sp, Operand(kFloatRegsSize));
  for (int i = 0; i < config->num_allocatable_float_registers(); ++i) {
    int code = config->GetAllocatableFloatCode(i);
    const FloatRegister freg = FloatRegister::from_code(code);
    int offset = code * kFloatSize;
    __ stfs(freg, MemOperand(sp, offset));
  }

  // Push saved_regs (needed to populate FrameDescription::registers_).
  // Leave gaps for other registers.
  __ subi(sp, sp, Operand(kNumberOfRegisters * kPointerSize));
  for (int16_t i = kNumberOfRegisters - 1; i >= 0; i--) {
    if ((saved_regs & (1 << i)) != 0) {
      __ StoreP(ToRegister(i), MemOperand(sp, kPointerSize * i));
    }
  }

  __ mov(ip, Operand(ExternalReference(IsolateAddressId::kCEntryFPAddress,
                                       isolate())));
  __ StoreP(fp, MemOperand(ip));

  const int kSavedRegistersAreaSize =
      (kNumberOfRegisters * kPointerSize) + kDoubleRegsSize + kFloatRegsSize;

  // Get the bailout id from the stack.
  __ LoadP(r5, MemOperand(sp, kSavedRegistersAreaSize));

  // Get the address of the location in the code object (r6) (return
  // address for lazy deoptimization) and compute the fp-to-sp delta in
  // register r7.
  __ mflr(r6);
  // Correct one word for bailout id.
  __ addi(r7, sp, Operand(kSavedRegistersAreaSize + (1 * kPointerSize)));
  __ sub(r7, fp, r7);

  // Allocate a new deoptimizer object.
  // Pass six arguments in r3 to r8.
  __ PrepareCallCFunction(6, r8);
  __ li(r3, Operand::Zero());
  Label context_check;
  __ LoadP(r4, MemOperand(fp, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ JumpIfSmi(r4, &context_check);
  __ LoadP(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ bind(&context_check);
  __ li(r4, Operand(type()));  // bailout type,
  // r5: bailout id already loaded.
  // r6: code address or 0 already loaded.
  // r7: Fp-to-sp delta.
  __ mov(r8, Operand(ExternalReference::isolate_address(isolate())));
  // Call Deoptimizer::New().
  {
    AllowExternalCallThatCantCauseGC scope(masm());
    __ CallCFunction(ExternalReference::new_deoptimizer_function(isolate()), 6);
  }

  // Preserve "deoptimizer" object in register r3 and get the input
  // frame descriptor pointer to r4 (deoptimizer->input_);
  __ LoadP(r4, MemOperand(r3, Deoptimizer::input_offset()));

  // Copy core registers into FrameDescription::registers_[kNumRegisters].
  DCHECK_EQ(Register::kNumRegisters, kNumberOfRegisters);
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    __ LoadP(r5, MemOperand(sp, i * kPointerSize));
    __ StoreP(r5, MemOperand(r4, offset));
  }

  int double_regs_offset = FrameDescription::double_registers_offset();
  // Copy double registers to
  // double_registers_[DoubleRegister::kNumRegisters]
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    int dst_offset = code * kDoubleSize + double_regs_offset;
    int src_offset =
        code * kDoubleSize + kNumberOfRegisters * kPointerSize + kFloatRegsSize;
    __ lfd(d0, MemOperand(sp, src_offset));
    __ stfd(d0, MemOperand(r4, dst_offset));
  }
  int float_regs_offset = FrameDescription::float_registers_offset();
  // Copy float registers to
  // float_registers_[FloatRegister::kNumRegisters]
  for (int i = 0; i < config->num_allocatable_float_registers(); ++i) {
    int code = config->GetAllocatableFloatCode(i);
    int dst_offset = code * kFloatSize + float_regs_offset;
    int src_offset = code * kFloatSize + kNumberOfRegisters * kPointerSize;
    __ lfs(d0, MemOperand(sp, src_offset));
    __ stfs(d0, MemOperand(r4, dst_offset));
  }
  // Remove the bailout id and the saved registers from the stack.
  __ addi(sp, sp, Operand(kSavedRegistersAreaSize + (1 * kPointerSize)));

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
  __ addi(r6, r6, Operand(kPointerSize));
  __ bind(&pop_loop_header);
  __ cmp(r5, sp);
  __ bne(&pop_loop);

  // Compute the output frame in the deoptimizer.
  __ push(r3);  // Preserve deoptimizer object across call.
  // r3: deoptimizer object; r4: scratch.
  __ PrepareCallCFunction(1, r4);
  // Call Deoptimizer::ComputeOutputFrames().
  {
    AllowExternalCallThatCantCauseGC scope(masm());
    __ CallCFunction(
        ExternalReference::compute_output_frames_function(isolate()), 1);
  }
  __ pop(r3);  // Restore deoptimizer object (class Deoptimizer).

  __ LoadP(sp, MemOperand(r3, Deoptimizer::caller_frame_top_offset()));

  // Replace the current (input) frame with the output frames.
  Label outer_push_loop, inner_push_loop, outer_loop_header, inner_loop_header;
  // Outer loop state: r7 = current "FrameDescription** output_",
  // r4 = one past the last FrameDescription**.
  __ lwz(r4, MemOperand(r3, Deoptimizer::output_count_offset()));
  __ LoadP(r7, MemOperand(r3, Deoptimizer::output_offset()));  // r7 is output_.
  __ ShiftLeftImm(r4, r4, Operand(kPointerSizeLog2));
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

  __ addi(r7, r7, Operand(kPointerSize));
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
  DCHECK(!(ip.bit() & restored_regs));
  __ mr(ip, r5);
  for (int i = kNumberOfRegisters - 1; i >= 0; i--) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    if ((restored_regs & (1 << i)) != 0) {
      __ LoadP(ToRegister(i), MemOperand(ip, offset));
    }
  }

  __ InitializeRootRegister();

  __ pop(ip);  // get continuation, leave pc on stack
  __ pop(r0);
  __ mtlr(r0);
  __ Jump(ip);
  __ stop("Unreachable.");
}


void Deoptimizer::TableEntryGenerator::GeneratePrologue() {
  Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm());

  // Create a sequence of deoptimization entries.
  // Note that registers are still live when jumping to an entry.
  Label done;
  for (int i = 0; i < count(); i++) {
    int start = masm()->pc_offset();
    USE(start);
    __ li(ip, Operand(i));
    __ b(&done);
    DCHECK(masm()->pc_offset() - start == table_entry_size_);
  }
  __ bind(&done);
  __ push(ip);
}

bool Deoptimizer::PadTopOfStackRegister() { return false; }

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


#undef __
}  // namespace internal
}  // namespace v8
