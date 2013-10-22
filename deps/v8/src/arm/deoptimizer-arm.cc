// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "codegen.h"
#include "deoptimizer.h"
#include "full-codegen.h"
#include "safepoint-table.h"

namespace v8 {
namespace internal {

const int Deoptimizer::table_entry_size_ = 12;


int Deoptimizer::patch_size() {
  const int kCallInstructionSizeInWords = 3;
  return kCallInstructionSizeInWords * Assembler::kInstrSize;
}


void Deoptimizer::PatchCodeForDeoptimization(Isolate* isolate, Code* code) {
  Address code_start_address = code->instruction_start();
  // Invalidate the relocation information, as it will become invalid by the
  // code patching below, and is not needed any more.
  code->InvalidateRelocation();

  // For each LLazyBailout instruction insert a call to the corresponding
  // deoptimization entry.
  DeoptimizationInputData* deopt_data =
      DeoptimizationInputData::cast(code->deoptimization_data());
#ifdef DEBUG
  Address prev_call_address = NULL;
#endif
  for (int i = 0; i < deopt_data->DeoptCount(); i++) {
    if (deopt_data->Pc(i)->value() == -1) continue;
    Address call_address = code_start_address + deopt_data->Pc(i)->value();
    Address deopt_entry = GetDeoptimizationEntry(isolate, i, LAZY);
    // We need calls to have a predictable size in the unoptimized code, but
    // this is optimized code, so we don't have to have a predictable size.
    int call_size_in_bytes =
        MacroAssembler::CallSizeNotPredictableCodeSize(deopt_entry,
                                                       RelocInfo::NONE32);
    int call_size_in_words = call_size_in_bytes / Assembler::kInstrSize;
    ASSERT(call_size_in_bytes % Assembler::kInstrSize == 0);
    ASSERT(call_size_in_bytes <= patch_size());
    CodePatcher patcher(call_address, call_size_in_words);
    patcher.masm()->Call(deopt_entry, RelocInfo::NONE32);
    ASSERT(prev_call_address == NULL ||
           call_address >= prev_call_address + patch_size());
    ASSERT(call_address + patch_size() <= code->instruction_end());
#ifdef DEBUG
    prev_call_address = call_address;
#endif
  }
}


static const int32_t kBranchBeforeInterrupt =  0x5a000004;

// The back edge bookkeeping code matches the pattern:
//
//  <decrement profiling counter>
//  2a 00 00 01       bpl ok
//  e5 9f c? ??       ldr ip, [pc, <interrupt stub address>]
//  e1 2f ff 3c       blx ip
//  ok-label
//
// We patch the code to the following form:
//
//  <decrement profiling counter>
//  e1 a0 00 00       mov r0, r0 (NOP)
//  e5 9f c? ??       ldr ip, [pc, <on-stack replacement address>]
//  e1 2f ff 3c       blx ip
//  ok-label

void Deoptimizer::PatchInterruptCodeAt(Code* unoptimized_code,
                                       Address pc_after,
                                       Code* replacement_code) {
  static const int kInstrSize = Assembler::kInstrSize;
  // Turn the jump into nops.
  CodePatcher patcher(pc_after - 3 * kInstrSize, 1);
  patcher.masm()->nop();
  // Replace the call address.
  uint32_t interrupt_address_offset = Memory::uint16_at(pc_after -
      2 * kInstrSize) & 0xfff;
  Address interrupt_address_pointer = pc_after + interrupt_address_offset;
  Memory::uint32_at(interrupt_address_pointer) =
      reinterpret_cast<uint32_t>(replacement_code->entry());

  unoptimized_code->GetHeap()->incremental_marking()->RecordCodeTargetPatch(
      unoptimized_code, pc_after - 2 * kInstrSize, replacement_code);
}


void Deoptimizer::RevertInterruptCodeAt(Code* unoptimized_code,
                                        Address pc_after,
                                        Code* interrupt_code) {
  static const int kInstrSize = Assembler::kInstrSize;
  // Restore the original jump.
  CodePatcher patcher(pc_after - 3 * kInstrSize, 1);
  patcher.masm()->b(4 * kInstrSize, pl);  // ok-label is 4 instructions later.
  ASSERT_EQ(kBranchBeforeInterrupt,
            Memory::int32_at(pc_after - 3 * kInstrSize));
  // Restore the original call address.
  uint32_t interrupt_address_offset = Memory::uint16_at(pc_after -
      2 * kInstrSize) & 0xfff;
  Address interrupt_address_pointer = pc_after + interrupt_address_offset;
  Memory::uint32_at(interrupt_address_pointer) =
      reinterpret_cast<uint32_t>(interrupt_code->entry());

  interrupt_code->GetHeap()->incremental_marking()->RecordCodeTargetPatch(
      unoptimized_code, pc_after - 2 * kInstrSize, interrupt_code);
}


#ifdef DEBUG
Deoptimizer::InterruptPatchState Deoptimizer::GetInterruptPatchState(
    Isolate* isolate,
    Code* unoptimized_code,
    Address pc_after) {
  static const int kInstrSize = Assembler::kInstrSize;
  ASSERT(Memory::int32_at(pc_after - kInstrSize) == kBlxIp);

  uint32_t interrupt_address_offset =
      Memory::uint16_at(pc_after - 2 * kInstrSize) & 0xfff;
  Address interrupt_address_pointer = pc_after + interrupt_address_offset;

  if (Assembler::IsNop(Assembler::instr_at(pc_after - 3 * kInstrSize))) {
    ASSERT(Assembler::IsLdrPcImmediateOffset(
        Assembler::instr_at(pc_after - 2 * kInstrSize)));
    Code* osr_builtin =
        isolate->builtins()->builtin(Builtins::kOnStackReplacement);
    ASSERT(reinterpret_cast<uint32_t>(osr_builtin->entry()) ==
           Memory::uint32_at(interrupt_address_pointer));
    return PATCHED_FOR_OSR;
  } else {
    // Get the interrupt stub code object to match against from cache.
    Code* interrupt_builtin =
        isolate->builtins()->builtin(Builtins::kInterruptCheck);
    ASSERT(Assembler::IsLdrPcImmediateOffset(
        Assembler::instr_at(pc_after - 2 * kInstrSize)));
    ASSERT_EQ(kBranchBeforeInterrupt,
              Memory::int32_at(pc_after - 3 * kInstrSize));
    ASSERT(reinterpret_cast<uint32_t>(interrupt_builtin->entry()) ==
           Memory::uint32_at(interrupt_address_pointer));
    return NOT_PATCHED;
  }
}
#endif  // DEBUG


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
    FrameDescription* output_frame, CodeStubInterfaceDescriptor* descriptor) {
  ApiFunction function(descriptor->deoptimization_handler_);
  ExternalReference xref(&function, ExternalReference::BUILTIN_CALL, isolate_);
  intptr_t handler = reinterpret_cast<intptr_t>(xref.address());
  int params = descriptor->register_param_count_;
  if (descriptor->stack_parameter_count_ != NULL) {
    params++;
  }
  output_frame->SetRegister(r0.code(), params);
  output_frame->SetRegister(r1.code(), handler);
}


void Deoptimizer::CopyDoubleRegisters(FrameDescription* output_frame) {
  for (int i = 0; i < DwVfpRegister::kMaxNumRegisters; ++i) {
    double double_value = input_->GetDoubleRegister(i);
    output_frame->SetDoubleRegister(i, double_value);
  }
}


bool Deoptimizer::HasAlignmentPadding(JSFunction* function) {
  // There is no dynamic alignment padding on ARM in the input frame.
  return false;
}


#define __ masm()->

// This code tries to be close to ia32 code so that any changes can be
// easily ported.
void Deoptimizer::EntryGenerator::Generate() {
  GeneratePrologue();

  // Save all general purpose registers before messing with them.
  const int kNumberOfRegisters = Register::kNumRegisters;

  // Everything but pc, lr and ip which will be saved but not restored.
  RegList restored_regs = kJSCallerSaved | kCalleeSaved | ip.bit();

  const int kDoubleRegsSize =
      kDoubleSize * DwVfpRegister::kMaxNumAllocatableRegisters;

  // Save all allocatable VFP registers before messing with them.
  ASSERT(kDoubleRegZero.code() == 14);
  ASSERT(kScratchDoubleReg.code() == 15);

  // Check CPU flags for number of registers, setting the Z condition flag.
  __ CheckFor32DRegs(ip);

  // Push registers d0-d13, and possibly d16-d31, on the stack.
  // If d16-d31 are not pushed, decrease the stack pointer instead.
  __ vstm(db_w, sp, d16, d31, ne);
  __ sub(sp, sp, Operand(16 * kDoubleSize), LeaveCC, eq);
  __ vstm(db_w, sp, d0, d13);

  // Push all 16 registers (needed to populate FrameDescription::registers_).
  // TODO(1588) Note that using pc with stm is deprecated, so we should perhaps
  // handle this a bit differently.
  __ stm(db_w, sp, restored_regs  | sp.bit() | lr.bit() | pc.bit());

  const int kSavedRegistersAreaSize =
      (kNumberOfRegisters * kPointerSize) + kDoubleRegsSize;

  // Get the bailout id from the stack.
  __ ldr(r2, MemOperand(sp, kSavedRegistersAreaSize));

  // Get the address of the location in the code object (r3) (return
  // address for lazy deoptimization) and compute the fp-to-sp delta in
  // register r4.
  __ mov(r3, lr);
  // Correct one word for bailout id.
  __ add(r4, sp, Operand(kSavedRegistersAreaSize + (1 * kPointerSize)));
  __ sub(r4, fp, r4);

  // Allocate a new deoptimizer object.
  // Pass four arguments in r0 to r3 and fifth argument on stack.
  __ PrepareCallCFunction(6, r5);
  __ ldr(r0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ mov(r1, Operand(type()));  // bailout type,
  // r2: bailout id already loaded.
  // r3: code address or 0 already loaded.
  __ str(r4, MemOperand(sp, 0 * kPointerSize));  // Fp-to-sp delta.
  __ mov(r5, Operand(ExternalReference::isolate_address(isolate())));
  __ str(r5, MemOperand(sp, 1 * kPointerSize));  // Isolate.
  // Call Deoptimizer::New().
  {
    AllowExternalCallThatCantCauseGC scope(masm());
    __ CallCFunction(ExternalReference::new_deoptimizer_function(isolate()), 6);
  }

  // Preserve "deoptimizer" object in register r0 and get the input
  // frame descriptor pointer to r1 (deoptimizer->input_);
  __ ldr(r1, MemOperand(r0, Deoptimizer::input_offset()));

  // Copy core registers into FrameDescription::registers_[kNumRegisters].
  ASSERT(Register::kNumRegisters == kNumberOfRegisters);
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    __ ldr(r2, MemOperand(sp, i * kPointerSize));
    __ str(r2, MemOperand(r1, offset));
  }

  // Copy VFP registers to
  // double_registers_[DoubleRegister::kMaxNumAllocatableRegisters]
  int double_regs_offset = FrameDescription::double_registers_offset();
  for (int i = 0; i < DwVfpRegister::kMaxNumAllocatableRegisters; ++i) {
    int dst_offset = i * kDoubleSize + double_regs_offset;
    int src_offset = i * kDoubleSize + kNumberOfRegisters * kPointerSize;
    __ vldr(d0, sp, src_offset);
    __ vstr(d0, r1, dst_offset);
  }

  // Remove the bailout id and the saved registers from the stack.
  __ add(sp, sp, Operand(kSavedRegistersAreaSize + (1 * kPointerSize)));

  // Compute a pointer to the unwinding limit in register r2; that is
  // the first stack slot not part of the input frame.
  __ ldr(r2, MemOperand(r1, FrameDescription::frame_size_offset()));
  __ add(r2, r2, sp);

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ add(r3,  r1, Operand(FrameDescription::frame_content_offset()));
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
  __ PrepareCallCFunction(1, r1);
  // Call Deoptimizer::ComputeOutputFrames().
  {
    AllowExternalCallThatCantCauseGC scope(masm());
    __ CallCFunction(
        ExternalReference::compute_output_frames_function(isolate()), 1);
  }
  __ pop(r0);  // Restore deoptimizer object (class Deoptimizer).

  // Replace the current (input) frame with the output frames.
  Label outer_push_loop, inner_push_loop,
      outer_loop_header, inner_loop_header;
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
  __ ldr(r7, MemOperand(r6, FrameDescription::frame_content_offset()));
  __ push(r7);
  __ bind(&inner_loop_header);
  __ cmp(r3, Operand::Zero());
  __ b(ne, &inner_push_loop);  // test for gt?
  __ add(r4, r4, Operand(kPointerSize));
  __ bind(&outer_loop_header);
  __ cmp(r4, r1);
  __ b(lt, &outer_push_loop);

  // Check CPU flags for number of registers, setting the Z condition flag.
  __ CheckFor32DRegs(ip);

  __ ldr(r1, MemOperand(r0, Deoptimizer::input_offset()));
  int src_offset = FrameDescription::double_registers_offset();
  for (int i = 0; i < DwVfpRegister::kMaxNumRegisters; ++i) {
    if (i == kDoubleRegZero.code()) continue;
    if (i == kScratchDoubleReg.code()) continue;

    const DwVfpRegister reg = DwVfpRegister::from_code(i);
    __ vldr(reg, r1, src_offset, i < 16 ? al : ne);
    src_offset += kDoubleSize;
  }

  // Push state, pc, and continuation from the last output frame.
  __ ldr(r6, MemOperand(r2, FrameDescription::state_offset()));
  __ push(r6);
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
  __ pop(ip);  // remove sp
  __ pop(ip);  // remove lr

  __ InitializeRootRegister();

  __ pop(ip);  // remove pc
  __ pop(r7);  // get continuation, leave pc on stack
  __ pop(lr);
  __ Jump(r7);
  __ stop("Unreachable.");
}


void Deoptimizer::TableEntryGenerator::GeneratePrologue() {
  // Create a sequence of deoptimization entries.
  // Note that registers are still live when jumping to an entry.
  Label done;
  for (int i = 0; i < count(); i++) {
    int start = masm()->pc_offset();
    USE(start);
    __ mov(ip, Operand(i));
    __ push(ip);
    __ b(&done);
    ASSERT(masm()->pc_offset() - start == table_entry_size_);
  }
  __ bind(&done);
}


void FrameDescription::SetCallerPc(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}


void FrameDescription::SetCallerFp(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}


#undef __

} }  // namespace v8::internal
