
// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/codegen.h"
#include "src/deoptimizer.h"
#include "src/full-codegen.h"
#include "src/safepoint-table.h"

namespace v8 {
namespace internal {


int Deoptimizer::patch_size() {
  const int kCallInstructionSizeInWords = 4;
  return kCallInstructionSizeInWords * Assembler::kInstrSize;
}


void Deoptimizer::PatchCodeForDeoptimization(Isolate* isolate, Code* code) {
  Address code_start_address = code->instruction_start();
  // Invalidate the relocation information, as it will become invalid by the
  // code patching below, and is not needed any more.
  code->InvalidateRelocation();

  if (FLAG_zap_code_space) {
    // Fail hard and early if we enter this code object again.
    byte* pointer = code->FindCodeAgeSequence();
    if (pointer != NULL) {
      pointer += kNoCodeAgeSequenceLength;
    } else {
      pointer = code->instruction_start();
    }
    CodePatcher patcher(pointer, 1);
    patcher.masm()->break_(0xCC);

    DeoptimizationInputData* data =
        DeoptimizationInputData::cast(code->deoptimization_data());
    int osr_offset = data->OsrPcOffset()->value();
    if (osr_offset > 0) {
      CodePatcher osr_patcher(code->instruction_start() + osr_offset, 1);
      osr_patcher.masm()->break_(0xCC);
    }
  }

  DeoptimizationInputData* deopt_data =
      DeoptimizationInputData::cast(code->deoptimization_data());
#ifdef DEBUG
  Address prev_call_address = NULL;
#endif
  // For each LLazyBailout instruction insert a call to the corresponding
  // deoptimization entry.
  for (int i = 0; i < deopt_data->DeoptCount(); i++) {
    if (deopt_data->Pc(i)->value() == -1) continue;
    Address call_address = code_start_address + deopt_data->Pc(i)->value();
    Address deopt_entry = GetDeoptimizationEntry(isolate, i, LAZY);
    int call_size_in_bytes = MacroAssembler::CallSize(deopt_entry,
                                                      RelocInfo::NONE32);
    int call_size_in_words = call_size_in_bytes / Assembler::kInstrSize;
    DCHECK(call_size_in_bytes % Assembler::kInstrSize == 0);
    DCHECK(call_size_in_bytes <= patch_size());
    CodePatcher patcher(call_address, call_size_in_words);
    patcher.masm()->Call(deopt_entry, RelocInfo::NONE32);
    DCHECK(prev_call_address == NULL ||
           call_address >= prev_call_address + patch_size());
    DCHECK(call_address + patch_size() <= code->instruction_end());

#ifdef DEBUG
    prev_call_address = call_address;
#endif
  }
}


void Deoptimizer::FillInputFrame(Address tos, JavaScriptFrame* frame) {
  // Set the register values. The values are not important as there are no
  // callee saved registers in JavaScript frames, so all registers are
  // spilled. Registers fp and sp are set to the correct values though.

  for (int i = 0; i < Register::kNumRegisters; i++) {
    input_->SetRegister(i, i * 4);
  }
  input_->SetRegister(sp.code(), reinterpret_cast<intptr_t>(frame->sp()));
  input_->SetRegister(fp.code(), reinterpret_cast<intptr_t>(frame->fp()));
  for (int i = 0; i < DoubleRegister::NumAllocatableRegisters(); i++) {
    input_->SetDoubleRegister(i, 0.0);
  }

  // Fill the frame content from the actual data on the frame.
  for (unsigned i = 0; i < input_->GetFrameSize(); i += kPointerSize) {
    input_->SetFrameSlot(i, Memory::uint32_at(tos + i));
  }
}


void Deoptimizer::SetPlatformCompiledStubRegisters(
    FrameDescription* output_frame, CodeStubDescriptor* descriptor) {
  ApiFunction function(descriptor->deoptimization_handler());
  ExternalReference xref(&function, ExternalReference::BUILTIN_CALL, isolate_);
  intptr_t handler = reinterpret_cast<intptr_t>(xref.address());
  int params = descriptor->GetHandlerParameterCount();
  output_frame->SetRegister(a0.code(), params);
  output_frame->SetRegister(a1.code(), handler);
}


void Deoptimizer::CopyDoubleRegisters(FrameDescription* output_frame) {
  for (int i = 0; i < DoubleRegister::kMaxNumRegisters; ++i) {
    double double_value = input_->GetDoubleRegister(i);
    output_frame->SetDoubleRegister(i, double_value);
  }
}


bool Deoptimizer::HasAlignmentPadding(JSFunction* function) {
  // There is no dynamic alignment padding on MIPS in the input frame.
  return false;
}


#define __ masm()->


// This code tries to be close to ia32 code so that any changes can be
// easily ported.
void Deoptimizer::EntryGenerator::Generate() {
  GeneratePrologue();

  // Unlike on ARM we don't save all the registers, just the useful ones.
  // For the rest, there are gaps on the stack, so the offsets remain the same.
  const int kNumberOfRegisters = Register::kNumRegisters;

  RegList restored_regs = kJSCallerSaved | kCalleeSaved;
  RegList saved_regs = restored_regs | sp.bit() | ra.bit();

  const int kDoubleRegsSize =
      kDoubleSize * FPURegister::kMaxNumAllocatableRegisters;

  // Save all FPU registers before messing with them.
  __ Subu(sp, sp, Operand(kDoubleRegsSize));
  for (int i = 0; i < FPURegister::kMaxNumAllocatableRegisters; ++i) {
    FPURegister fpu_reg = FPURegister::FromAllocationIndex(i);
    int offset = i * kDoubleSize;
    __ sdc1(fpu_reg, MemOperand(sp, offset));
  }

  // Push saved_regs (needed to populate FrameDescription::registers_).
  // Leave gaps for other registers.
  __ Subu(sp, sp, kNumberOfRegisters * kPointerSize);
  for (int16_t i = kNumberOfRegisters - 1; i >= 0; i--) {
    if ((saved_regs & (1 << i)) != 0) {
      __ sw(ToRegister(i), MemOperand(sp, kPointerSize * i));
    }
  }

  const int kSavedRegistersAreaSize =
      (kNumberOfRegisters * kPointerSize) + kDoubleRegsSize;

  // Get the bailout id from the stack.
  __ lw(a2, MemOperand(sp, kSavedRegistersAreaSize));

  // Get the address of the location in the code object (a3) (return
  // address for lazy deoptimization) and compute the fp-to-sp delta in
  // register t0.
  __ mov(a3, ra);
  // Correct one word for bailout id.
  __ Addu(t0, sp, Operand(kSavedRegistersAreaSize + (1 * kPointerSize)));

  __ Subu(t0, fp, t0);

  // Allocate a new deoptimizer object.
  // Pass four arguments in a0 to a3 and fifth & sixth arguments on stack.
  __ PrepareCallCFunction(6, t1);
  __ lw(a0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ li(a1, Operand(type()));  // bailout type,
  // a2: bailout id already loaded.
  // a3: code address or 0 already loaded.
  __ sw(t0, CFunctionArgumentOperand(5));  // Fp-to-sp delta.
  __ li(t1, Operand(ExternalReference::isolate_address(isolate())));
  __ sw(t1, CFunctionArgumentOperand(6));  // Isolate.
  // Call Deoptimizer::New().
  {
    AllowExternalCallThatCantCauseGC scope(masm());
    __ CallCFunction(ExternalReference::new_deoptimizer_function(isolate()), 6);
  }

  // Preserve "deoptimizer" object in register v0 and get the input
  // frame descriptor pointer to a1 (deoptimizer->input_);
  // Move deopt-obj to a0 for call to Deoptimizer::ComputeOutputFrames() below.
  __ mov(a0, v0);
  __ lw(a1, MemOperand(v0, Deoptimizer::input_offset()));

  // Copy core registers into FrameDescription::registers_[kNumRegisters].
  DCHECK(Register::kNumRegisters == kNumberOfRegisters);
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    if ((saved_regs & (1 << i)) != 0) {
      __ lw(a2, MemOperand(sp, i * kPointerSize));
      __ sw(a2, MemOperand(a1, offset));
    } else if (FLAG_debug_code) {
      __ li(a2, kDebugZapValue);
      __ sw(a2, MemOperand(a1, offset));
    }
  }

  int double_regs_offset = FrameDescription::double_registers_offset();
  // Copy FPU registers to
  // double_registers_[DoubleRegister::kNumAllocatableRegisters]
  for (int i = 0; i < FPURegister::NumAllocatableRegisters(); ++i) {
    int dst_offset = i * kDoubleSize + double_regs_offset;
    int src_offset = i * kDoubleSize + kNumberOfRegisters * kPointerSize;
    __ ldc1(f0, MemOperand(sp, src_offset));
    __ sdc1(f0, MemOperand(a1, dst_offset));
  }

  // Remove the bailout id and the saved registers from the stack.
  __ Addu(sp, sp, Operand(kSavedRegistersAreaSize + (1 * kPointerSize)));

  // Compute a pointer to the unwinding limit in register a2; that is
  // the first stack slot not part of the input frame.
  __ lw(a2, MemOperand(a1, FrameDescription::frame_size_offset()));
  __ Addu(a2, a2, sp);

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ Addu(a3, a1, Operand(FrameDescription::frame_content_offset()));
  Label pop_loop;
  Label pop_loop_header;
  __ BranchShort(&pop_loop_header);
  __ bind(&pop_loop);
  __ pop(t0);
  __ sw(t0, MemOperand(a3, 0));
  __ addiu(a3, a3, sizeof(uint32_t));
  __ bind(&pop_loop_header);
  __ BranchShort(&pop_loop, ne, a2, Operand(sp));

  // Compute the output frame in the deoptimizer.
  __ push(a0);  // Preserve deoptimizer object across call.
  // a0: deoptimizer object; a1: scratch.
  __ PrepareCallCFunction(1, a1);
  // Call Deoptimizer::ComputeOutputFrames().
  {
    AllowExternalCallThatCantCauseGC scope(masm());
    __ CallCFunction(
        ExternalReference::compute_output_frames_function(isolate()), 1);
  }
  __ pop(a0);  // Restore deoptimizer object (class Deoptimizer).

  // Replace the current (input) frame with the output frames.
  Label outer_push_loop, inner_push_loop,
      outer_loop_header, inner_loop_header;
  // Outer loop state: t0 = current "FrameDescription** output_",
  // a1 = one past the last FrameDescription**.
  __ lw(a1, MemOperand(a0, Deoptimizer::output_count_offset()));
  __ lw(t0, MemOperand(a0, Deoptimizer::output_offset()));  // t0 is output_.
  __ sll(a1, a1, kPointerSizeLog2);  // Count to offset.
  __ addu(a1, t0, a1);  // a1 = one past the last FrameDescription**.
  __ jmp(&outer_loop_header);
  __ bind(&outer_push_loop);
  // Inner loop state: a2 = current FrameDescription*, a3 = loop index.
  __ lw(a2, MemOperand(t0, 0));  // output_[ix]
  __ lw(a3, MemOperand(a2, FrameDescription::frame_size_offset()));
  __ jmp(&inner_loop_header);
  __ bind(&inner_push_loop);
  __ Subu(a3, a3, Operand(sizeof(uint32_t)));
  __ Addu(t2, a2, Operand(a3));
  __ lw(t3, MemOperand(t2, FrameDescription::frame_content_offset()));
  __ push(t3);
  __ bind(&inner_loop_header);
  __ BranchShort(&inner_push_loop, ne, a3, Operand(zero_reg));

  __ Addu(t0, t0, Operand(kPointerSize));
  __ bind(&outer_loop_header);
  __ BranchShort(&outer_push_loop, lt, t0, Operand(a1));

  __ lw(a1, MemOperand(a0, Deoptimizer::input_offset()));
  for (int i = 0; i < FPURegister::kMaxNumAllocatableRegisters; ++i) {
    const FPURegister fpu_reg = FPURegister::FromAllocationIndex(i);
    int src_offset = i * kDoubleSize + double_regs_offset;
    __ ldc1(fpu_reg, MemOperand(a1, src_offset));
  }

  // Push state, pc, and continuation from the last output frame.
  __ lw(t2, MemOperand(a2, FrameDescription::state_offset()));
  __ push(t2);

  __ lw(t2, MemOperand(a2, FrameDescription::pc_offset()));
  __ push(t2);
  __ lw(t2, MemOperand(a2, FrameDescription::continuation_offset()));
  __ push(t2);


  // Technically restoring 'at' should work unless zero_reg is also restored
  // but it's safer to check for this.
  DCHECK(!(at.bit() & restored_regs));
  // Restore the registers from the last output frame.
  __ mov(at, a2);
  for (int i = kNumberOfRegisters - 1; i >= 0; i--) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    if ((restored_regs & (1 << i)) != 0) {
      __ lw(ToRegister(i), MemOperand(at, offset));
    }
  }

  __ InitializeRootRegister();

  __ pop(at);  // Get continuation, leave pc on stack.
  __ pop(ra);
  __ Jump(at);
  __ stop("Unreachable.");
}


// Maximum size of a table entry generated below.
const int Deoptimizer::table_entry_size_ = 2 * Assembler::kInstrSize;

void Deoptimizer::TableEntryGenerator::GeneratePrologue() {
  Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm());

  // Create a sequence of deoptimization entries.
  // Note that registers are still live when jumping to an entry.
  Label table_start, done, done_special, trampoline_jump;
  __ bind(&table_start);
  int kMaxEntriesBranchReach = (1 << (kImm16Bits - 2))/
     (table_entry_size_ /  Assembler::kInstrSize);

  if (count() <= kMaxEntriesBranchReach) {
    // Common case.
    for (int i = 0; i < count(); i++) {
      Label start;
      __ bind(&start);
      DCHECK(is_int16(i));
      __ Branch(USE_DELAY_SLOT, &done);  // Expose delay slot.
      __ li(at, i);  // In the delay slot.

      DCHECK_EQ(table_entry_size_, masm()->SizeOfCodeGeneratedSince(&start));
    }

    DCHECK_EQ(masm()->SizeOfCodeGeneratedSince(&table_start),
        count() * table_entry_size_);
    __ bind(&done);
    __ Push(at);
  } else {
    // Uncommon case, the branch cannot reach.
    // Create mini trampoline and adjust id constants to get proper value at
    // the end of table.
    for (int i = kMaxEntriesBranchReach; i > 1; i--) {
      Label start;
      __ bind(&start);
      DCHECK(is_int16(i));
      __ Branch(USE_DELAY_SLOT, &trampoline_jump);  // Expose delay slot.
      __ li(at, - i);  // In the delay slot.
      DCHECK_EQ(table_entry_size_, masm()->SizeOfCodeGeneratedSince(&start));
    }
    // Entry with id == kMaxEntriesBranchReach - 1.
    __ bind(&trampoline_jump);
    __ Branch(USE_DELAY_SLOT, &done_special);
    __ li(at, -1);

    for (int i = kMaxEntriesBranchReach ; i < count(); i++) {
      Label start;
      __ bind(&start);
      DCHECK(is_int16(i));
      __ Branch(USE_DELAY_SLOT, &done);  // Expose delay slot.
      __ li(at, i);  // In the delay slot.
    }

    DCHECK_EQ(masm()->SizeOfCodeGeneratedSince(&table_start),
        count() * table_entry_size_);
    __ bind(&done_special);
    __ addiu(at, at, kMaxEntriesBranchReach);
    __ bind(&done);
    __ Push(at);
  }
}


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


} }  // namespace v8::internal
