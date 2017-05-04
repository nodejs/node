// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/codegen.h"
#include "src/deoptimizer.h"
#include "src/full-codegen/full-codegen.h"
#include "src/objects-inl.h"
#include "src/register-configuration.h"
#include "src/safepoint-table.h"

namespace v8 {
namespace internal {


const int Deoptimizer::table_entry_size_ = 10;


int Deoptimizer::patch_size() {
  return Assembler::kCallSequenceLength;
}


void Deoptimizer::EnsureRelocSpaceForLazyDeoptimization(Handle<Code> code) {
  // Empty because there is no need for relocation information for the code
  // patching in Deoptimizer::PatchCodeForDeoptimization below.
}


void Deoptimizer::PatchCodeForDeoptimization(Isolate* isolate, Code* code) {
  Address instruction_start = code->instruction_start();
  // Invalidate the relocation information, as it will become invalid by the
  // code patching below, and is not needed any more.
  code->InvalidateRelocation();

  // Fail hard and early if we enter this code object again.
  byte* pointer = code->FindCodeAgeSequence();
  if (pointer != NULL) {
    pointer += kNoCodeAgeSequenceLength;
  } else {
    pointer = code->instruction_start();
  }
  CodePatcher patcher(isolate, pointer, 1);
  patcher.masm()->int3();

  DeoptimizationInputData* data =
      DeoptimizationInputData::cast(code->deoptimization_data());
  int osr_offset = data->OsrPcOffset()->value();
  if (osr_offset > 0) {
    CodePatcher osr_patcher(isolate, instruction_start + osr_offset, 1);
    osr_patcher.masm()->int3();
  }

  // For each LLazyBailout instruction insert a absolute call to the
  // corresponding deoptimization entry, or a short call to an absolute
  // jump if space is short. The absolute jumps are put in a table just
  // before the safepoint table (space was allocated there when the Code
  // object was created, if necessary).

#ifdef DEBUG
  Address prev_call_address = NULL;
#endif
  DeoptimizationInputData* deopt_data =
      DeoptimizationInputData::cast(code->deoptimization_data());
  deopt_data->SetSharedFunctionInfo(Smi::kZero);
  // For each LLazyBailout instruction insert a call to the corresponding
  // deoptimization entry.
  for (int i = 0; i < deopt_data->DeoptCount(); i++) {
    if (deopt_data->Pc(i)->value() == -1) continue;
    // Position where Call will be patched in.
    Address call_address = instruction_start + deopt_data->Pc(i)->value();
    // There is room enough to write a long call instruction because we pad
    // LLazyBailout instructions with nops if necessary.
    CodePatcher patcher(isolate, call_address, Assembler::kCallSequenceLength);
    patcher.masm()->Call(GetDeoptimizationEntry(isolate, i, LAZY),
                         Assembler::RelocInfoNone());
    DCHECK(prev_call_address == NULL ||
           call_address >= prev_call_address + patch_size());
    DCHECK(call_address + patch_size() <= code->instruction_end());
#ifdef DEBUG
    prev_call_address = call_address;
#endif
  }
}


void Deoptimizer::SetPlatformCompiledStubRegisters(
    FrameDescription* output_frame, CodeStubDescriptor* descriptor) {
  intptr_t handler =
      reinterpret_cast<intptr_t>(descriptor->deoptimization_handler());
  int params = descriptor->GetHandlerParameterCount();
  output_frame->SetRegister(rax.code(), params);
  output_frame->SetRegister(rbx.code(), handler);
}


void Deoptimizer::CopyDoubleRegisters(FrameDescription* output_frame) {
  for (int i = 0; i < XMMRegister::kMaxNumRegisters; ++i) {
    Float64 double_value = input_->GetDoubleRegister(i);
    output_frame->SetDoubleRegister(i, double_value);
  }
}

#define __ masm()->

void Deoptimizer::TableEntryGenerator::Generate() {
  GeneratePrologue();

  // Save all general purpose registers before messing with them.
  const int kNumberOfRegisters = Register::kNumRegisters;

  const int kDoubleRegsSize = kDoubleSize * XMMRegister::kMaxNumRegisters;
  __ subp(rsp, Immediate(kDoubleRegsSize));

  const RegisterConfiguration* config = RegisterConfiguration::Crankshaft();
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    XMMRegister xmm_reg = XMMRegister::from_code(code);
    int offset = code * kDoubleSize;
    __ Movsd(Operand(rsp, offset), xmm_reg);
  }

  const int kFloatRegsSize = kFloatSize * XMMRegister::kMaxNumRegisters;
  __ subp(rsp, Immediate(kFloatRegsSize));

  for (int i = 0; i < config->num_allocatable_float_registers(); ++i) {
    int code = config->GetAllocatableFloatCode(i);
    XMMRegister xmm_reg = XMMRegister::from_code(code);
    int offset = code * kFloatSize;
    __ Movss(Operand(rsp, offset), xmm_reg);
  }

  // We push all registers onto the stack, even though we do not need
  // to restore all later.
  for (int i = 0; i < kNumberOfRegisters; i++) {
    Register r = Register::from_code(i);
    __ pushq(r);
  }

  const int kSavedRegistersAreaSize =
      kNumberOfRegisters * kRegisterSize + kDoubleRegsSize + kFloatRegsSize;

  __ Store(ExternalReference(Isolate::kCEntryFPAddress, isolate()), rbp);

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
  __ movp(rax, Immediate(0));
  Label context_check;
  __ movp(rdi, Operand(rbp, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ JumpIfSmi(rdi, &context_check);
  __ movp(rax, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ bind(&context_check);
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
    __ PopQuad(Operand(rbx, offset));
  }

  // Fill in the float input registers.
  int float_regs_offset = FrameDescription::float_registers_offset();
  for (int i = 0; i < XMMRegister::kMaxNumRegisters; i++) {
    int src_offset = i * kFloatSize;
    int dst_offset = i * kFloatSize + float_regs_offset;
    __ movl(rcx, Operand(rsp, src_offset));
    __ movl(Operand(rbx, dst_offset), rcx);
  }
  __ addp(rsp, Immediate(kFloatRegsSize));

  // Fill in the double input registers.
  int double_regs_offset = FrameDescription::double_registers_offset();
  for (int i = 0; i < XMMRegister::kMaxNumRegisters; i++) {
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

  __ movp(rsp, Operand(rax, Deoptimizer::caller_frame_top_offset()));

  // Replace the current (input) frame with the output frames.
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

  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    XMMRegister xmm_reg = XMMRegister::from_code(code);
    int src_offset = code * kDoubleSize + double_regs_offset;
    __ Movsd(xmm_reg, Operand(rbx, src_offset));
  }

  // Push state, pc, and continuation from the last output frame.
  __ Push(Operand(rbx, FrameDescription::state_offset()));
  __ PushQuad(Operand(rbx, FrameDescription::pc_offset()));
  __ PushQuad(Operand(rbx, FrameDescription::continuation_offset()));

  // Push the registers from the last output frame.
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    __ PushQuad(Operand(rbx, offset));
  }

  // Restore the registers from the stack.
  for (int i = kNumberOfRegisters - 1; i >= 0 ; i--) {
    Register r = Register::from_code(i);
    // Do not restore rsp, simply pop the value into the next register
    // and overwrite this afterwards.
    if (r.is(rsp)) {
      DCHECK(i > 0);
      r = Register::from_code(i - 1);
    }
    __ popq(r);
  }

  // Set up the roots register.
  __ InitializeRootRegister();

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
    DCHECK(masm()->pc_offset() - start == table_entry_size_);
  }
  __ bind(&done);
}


void FrameDescription::SetCallerPc(unsigned offset, intptr_t value) {
  if (kPCOnStackSize == 2 * kPointerSize) {
    // Zero out the high-32 bit of PC for x32 port.
    SetFrameSlot(offset + kPointerSize, 0);
  }
  SetFrameSlot(offset, value);
}


void FrameDescription::SetCallerFp(unsigned offset, intptr_t value) {
  if (kFPOnStackSize == 2 * kPointerSize) {
    // Zero out the high-32 bit of FP for x32 port.
    SetFrameSlot(offset + kPointerSize, 0);
  }
  SetFrameSlot(offset, value);
}


void FrameDescription::SetCallerConstantPool(unsigned offset, intptr_t value) {
  // No embedded constant pool support.
  UNREACHABLE();
}


#undef __


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64
