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

#if V8_TARGET_ARCH_X64

#include "codegen.h"
#include "deoptimizer.h"
#include "full-codegen.h"
#include "safepoint-table.h"

namespace v8 {
namespace internal {


const int Deoptimizer::table_entry_size_ = 10;


int Deoptimizer::patch_size() {
  return Assembler::kCallSequenceLength;
}


void Deoptimizer::PatchCodeForDeoptimization(Isolate* isolate, Code* code) {
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
    patcher.masm()->int3();

    DeoptimizationInputData* data =
        DeoptimizationInputData::cast(code->deoptimization_data());
    int osr_offset = data->OsrPcOffset()->value();
    if (osr_offset > 0) {
      CodePatcher osr_patcher(code->instruction_start() + osr_offset, 1);
      osr_patcher.masm()->int3();
    }
  }

  // For each LLazyBailout instruction insert a absolute call to the
  // corresponding deoptimization entry, or a short call to an absolute
  // jump if space is short. The absolute jumps are put in a table just
  // before the safepoint table (space was allocated there when the Code
  // object was created, if necessary).

  Address instruction_start = code->instruction_start();
#ifdef DEBUG
  Address prev_call_address = NULL;
#endif
  DeoptimizationInputData* deopt_data =
      DeoptimizationInputData::cast(code->deoptimization_data());
  SharedFunctionInfo* shared =
      SharedFunctionInfo::cast(deopt_data->SharedFunctionInfo());
  shared->EvictFromOptimizedCodeMap(code, "deoptimized code");
  deopt_data->SetSharedFunctionInfo(Smi::FromInt(0));
  // For each LLazyBailout instruction insert a call to the corresponding
  // deoptimization entry.
  for (int i = 0; i < deopt_data->DeoptCount(); i++) {
    if (deopt_data->Pc(i)->value() == -1) continue;
    // Position where Call will be patched in.
    Address call_address = instruction_start + deopt_data->Pc(i)->value();
    // There is room enough to write a long call instruction because we pad
    // LLazyBailout instructions with nops if necessary.
    CodePatcher patcher(call_address, Assembler::kCallSequenceLength);
    patcher.masm()->Call(GetDeoptimizationEntry(isolate, i, LAZY),
                         Assembler::RelocInfoNone());
    ASSERT(prev_call_address == NULL ||
           call_address >= prev_call_address + patch_size());
    ASSERT(call_address + patch_size() <= code->instruction_end());
#ifdef DEBUG
    prev_call_address = call_address;
#endif
  }
}


void Deoptimizer::FillInputFrame(Address tos, JavaScriptFrame* frame) {
  // Set the register values. The values are not important as there are no
  // callee saved registers in JavaScript frames, so all registers are
  // spilled. Registers rbp and rsp are set to the correct values though.
  for (int i = 0; i < Register::kNumRegisters; i++) {
    input_->SetRegister(i, i * 4);
  }
  input_->SetRegister(rsp.code(), reinterpret_cast<intptr_t>(frame->sp()));
  input_->SetRegister(rbp.code(), reinterpret_cast<intptr_t>(frame->fp()));
  for (int i = 0; i < DoubleRegister::NumAllocatableRegisters(); i++) {
    input_->SetDoubleRegister(i, 0.0);
  }

  // Fill the frame content from the actual data on the frame.
  for (unsigned i = 0; i < input_->GetFrameSize(); i += kPointerSize) {
    input_->SetFrameSlot(i, Memory::uintptr_at(tos + i));
  }
}


void Deoptimizer::SetPlatformCompiledStubRegisters(
    FrameDescription* output_frame, CodeStubInterfaceDescriptor* descriptor) {
  intptr_t handler =
      reinterpret_cast<intptr_t>(descriptor->deoptimization_handler_);
  int params = descriptor->GetHandlerParameterCount();
  output_frame->SetRegister(rax.code(), params);
  output_frame->SetRegister(rbx.code(), handler);
}


void Deoptimizer::CopyDoubleRegisters(FrameDescription* output_frame) {
  for (int i = 0; i < XMMRegister::NumAllocatableRegisters(); ++i) {
    double double_value = input_->GetDoubleRegister(i);
    output_frame->SetDoubleRegister(i, double_value);
  }
}


bool Deoptimizer::HasAlignmentPadding(JSFunction* function) {
  // There is no dynamic alignment padding on x64 in the input frame.
  return false;
}


Code* Deoptimizer::NotifyStubFailureBuiltin() {
  return isolate_->builtins()->builtin(Builtins::kNotifyStubFailureSaveDoubles);
}


#define __ masm()->

void Deoptimizer::EntryGenerator::Generate() {
  GeneratePrologue();

  // Save all general purpose registers before messing with them.
  const int kNumberOfRegisters = Register::kNumRegisters;

  const int kDoubleRegsSize = kDoubleSize *
      XMMRegister::NumAllocatableRegisters();
  __ subp(rsp, Immediate(kDoubleRegsSize));

  for (int i = 0; i < XMMRegister::NumAllocatableRegisters(); ++i) {
    XMMRegister xmm_reg = XMMRegister::FromAllocationIndex(i);
    int offset = i * kDoubleSize;
    __ movsd(Operand(rsp, offset), xmm_reg);
  }

  // We push all registers onto the stack, even though we do not need
  // to restore all later.
  for (int i = 0; i < kNumberOfRegisters; i++) {
    Register r = Register::from_code(i);
    __ pushq(r);
  }

  const int kSavedRegistersAreaSize = kNumberOfRegisters * kRegisterSize +
                                      kDoubleRegsSize;

  // We use this to keep the value of the fifth argument temporarily.
  // Unfortunately we can't store it directly in r8 (used for passing
  // this on linux), since it is another parameter passing register on windows.
  Register arg5 = r11;

  // Get the bailout id from the stack.
  __ movp(arg_reg_3, Operand(rsp, kSavedRegistersAreaSize));

  // Get the address of the location in the code object
  // and compute the fp-to-sp delta in register arg5.
  __ movp(arg_reg_4, Operand(rsp, kSavedRegistersAreaSize + 1 * kRegisterSize));
  __ leap(arg5, Operand(rsp, kSavedRegistersAreaSize + 1 * kRegisterSize +
                            kPCOnStackSize));

  __ subp(arg5, rbp);
  __ negp(arg5);

  // Allocate a new deoptimizer object.
  __ PrepareCallCFunction(6);
  __ movp(rax, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ movp(arg_reg_1, rax);
  __ Set(arg_reg_2, type());
  // Args 3 and 4 are already in the right registers.

  // On windows put the arguments on the stack (PrepareCallCFunction
  // has created space for this). On linux pass the arguments in r8 and r9.
#ifdef _WIN64
  __ movq(Operand(rsp, 4 * kRegisterSize), arg5);
  __ LoadAddress(arg5, ExternalReference::isolate_address(isolate()));
  __ movq(Operand(rsp, 5 * kRegisterSize), arg5);
#else
  __ movp(r8, arg5);
  __ LoadAddress(r9, ExternalReference::isolate_address(isolate()));
#endif

  { AllowExternalCallThatCantCauseGC scope(masm());
    __ CallCFunction(ExternalReference::new_deoptimizer_function(isolate()), 6);
  }
  // Preserve deoptimizer object in register rax and get the input
  // frame descriptor pointer.
  __ movp(rbx, Operand(rax, Deoptimizer::input_offset()));

  // Fill in the input registers.
  for (int i = kNumberOfRegisters -1; i >= 0; i--) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    __ Pop(Operand(rbx, offset));
  }

  // Fill in the double input registers.
  int double_regs_offset = FrameDescription::double_registers_offset();
  for (int i = 0; i < XMMRegister::NumAllocatableRegisters(); i++) {
    int dst_offset = i * kDoubleSize + double_regs_offset;
    __ popq(Operand(rbx, dst_offset));
  }

  // Remove the bailout id and return address from the stack.
  __ addp(rsp, Immediate(1 * kRegisterSize + kPCOnStackSize));

  // Compute a pointer to the unwinding limit in register rcx; that is
  // the first stack slot not part of the input frame.
  __ movp(rcx, Operand(rbx, FrameDescription::frame_size_offset()));
  __ addp(rcx, rsp);

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ leap(rdx, Operand(rbx, FrameDescription::frame_content_offset()));
  Label pop_loop_header;
  __ jmp(&pop_loop_header);
  Label pop_loop;
  __ bind(&pop_loop);
  __ Pop(Operand(rdx, 0));
  __ addp(rdx, Immediate(sizeof(intptr_t)));
  __ bind(&pop_loop_header);
  __ cmpp(rcx, rsp);
  __ j(not_equal, &pop_loop);

  // Compute the output frame in the deoptimizer.
  __ pushq(rax);
  __ PrepareCallCFunction(2);
  __ movp(arg_reg_1, rax);
  __ LoadAddress(arg_reg_2, ExternalReference::isolate_address(isolate()));
  {
    AllowExternalCallThatCantCauseGC scope(masm());
    __ CallCFunction(
        ExternalReference::compute_output_frames_function(isolate()), 2);
  }
  __ popq(rax);

  // Replace the current frame with the output frames.
  Label outer_push_loop, inner_push_loop,
      outer_loop_header, inner_loop_header;
  // Outer loop state: rax = current FrameDescription**, rdx = one past the
  // last FrameDescription**.
  __ movl(rdx, Operand(rax, Deoptimizer::output_count_offset()));
  __ movp(rax, Operand(rax, Deoptimizer::output_offset()));
  __ leap(rdx, Operand(rax, rdx, times_pointer_size, 0));
  __ jmp(&outer_loop_header);
  __ bind(&outer_push_loop);
  // Inner loop state: rbx = current FrameDescription*, rcx = loop index.
  __ movp(rbx, Operand(rax, 0));
  __ movp(rcx, Operand(rbx, FrameDescription::frame_size_offset()));
  __ jmp(&inner_loop_header);
  __ bind(&inner_push_loop);
  __ subp(rcx, Immediate(sizeof(intptr_t)));
  __ Push(Operand(rbx, rcx, times_1, FrameDescription::frame_content_offset()));
  __ bind(&inner_loop_header);
  __ testp(rcx, rcx);
  __ j(not_zero, &inner_push_loop);
  __ addp(rax, Immediate(kPointerSize));
  __ bind(&outer_loop_header);
  __ cmpp(rax, rdx);
  __ j(below, &outer_push_loop);

  for (int i = 0; i < XMMRegister::NumAllocatableRegisters(); ++i) {
    XMMRegister xmm_reg = XMMRegister::FromAllocationIndex(i);
    int src_offset = i * kDoubleSize + double_regs_offset;
    __ movsd(xmm_reg, Operand(rbx, src_offset));
  }

  // Push state, pc, and continuation from the last output frame.
  __ Push(Operand(rbx, FrameDescription::state_offset()));
  __ Push(Operand(rbx, FrameDescription::pc_offset()));
  __ Push(Operand(rbx, FrameDescription::continuation_offset()));

  // Push the registers from the last output frame.
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    __ Push(Operand(rbx, offset));
  }

  // Restore the registers from the stack.
  for (int i = kNumberOfRegisters - 1; i >= 0 ; i--) {
    Register r = Register::from_code(i);
    // Do not restore rsp, simply pop the value into the next register
    // and overwrite this afterwards.
    if (r.is(rsp)) {
      ASSERT(i > 0);
      r = Register::from_code(i - 1);
    }
    __ popq(r);
  }

  // Set up the roots register.
  __ InitializeRootRegister();
  __ InitializeSmiConstantRegister();

  // Return to the continuation point.
  __ ret(0);
}


void Deoptimizer::TableEntryGenerator::GeneratePrologue() {
  // Create a sequence of deoptimization entries.
  Label done;
  for (int i = 0; i < count(); i++) {
    int start = masm()->pc_offset();
    USE(start);
    __ pushq_imm32(i);
    __ jmp(&done);
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


void FrameDescription::SetCallerConstantPool(unsigned offset, intptr_t value) {
  // No out-of-line constant pool support.
  UNREACHABLE();
}


#undef __


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
