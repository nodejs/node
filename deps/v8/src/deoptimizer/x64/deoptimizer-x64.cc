// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/codegen/macro-assembler.h"
#include "src/codegen/register-configuration.h"
#include "src/codegen/safepoint-table.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

#define __ masm->

void Deoptimizer::GenerateDeoptimizationEntries(MacroAssembler* masm,
                                                Isolate* isolate,
                                                DeoptimizeKind deopt_kind) {
  NoRootArrayScope no_root_array(masm);

  // Save all general purpose registers before messing with them.
  const int kNumberOfRegisters = Register::kNumRegisters;

  const int kDoubleRegsSize = kDoubleSize * XMMRegister::kNumRegisters;
  __ AllocateStackSpace(kDoubleRegsSize);

  const RegisterConfiguration* config = RegisterConfiguration::Default();
  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    XMMRegister xmm_reg = XMMRegister::from_code(code);
    int offset = code * kDoubleSize;
    __ Movsd(Operand(rsp, offset), xmm_reg);
  }

  const int kFloatRegsSize = kFloatSize * XMMRegister::kNumRegisters;
  __ AllocateStackSpace(kFloatRegsSize);

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

  const int kSavedRegistersAreaSize = kNumberOfRegisters * kSystemPointerSize +
                                      kDoubleRegsSize + kFloatRegsSize;

  __ Store(
      ExternalReference::Create(IsolateAddressId::kCEntryFPAddress, isolate),
      rbp);

  // We use this to keep the value of the fifth argument temporarily.
  // Unfortunately we can't store it directly in r8 (used for passing
  // this on linux), since it is another parameter passing register on windows.
  Register arg5 = r11;

  // The bailout id is passed using r13 on the stack.
  __ movq(arg_reg_3, r13);

  // Get the address of the location in the code object
  // and compute the fp-to-sp delta in register arg5.
  __ movq(arg_reg_4, Operand(rsp, kSavedRegistersAreaSize));
  __ leaq(arg5, Operand(rsp, kSavedRegistersAreaSize + kPCOnStackSize));

  __ subq(arg5, rbp);
  __ negq(arg5);

  // Allocate a new deoptimizer object.
  __ PrepareCallCFunction(6);
  __ movq(rax, Immediate(0));
  Label context_check;
  __ movq(rdi, Operand(rbp, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ JumpIfSmi(rdi, &context_check);
  __ movq(rax, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ bind(&context_check);
  __ movq(arg_reg_1, rax);
  __ Set(arg_reg_2, static_cast<int>(deopt_kind));
  // Args 3 and 4 are already in the right registers.

  // On windows put the arguments on the stack (PrepareCallCFunction
  // has created space for this). On linux pass the arguments in r8 and r9.
#ifdef _WIN64
  __ movq(Operand(rsp, 4 * kSystemPointerSize), arg5);
  __ LoadAddress(arg5, ExternalReference::isolate_address(isolate));
  __ movq(Operand(rsp, 5 * kSystemPointerSize), arg5);
#else
  __ movq(r8, arg5);
  __ LoadAddress(r9, ExternalReference::isolate_address(isolate));
#endif

  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ CallCFunction(ExternalReference::new_deoptimizer_function(), 6);
  }
  // Preserve deoptimizer object in register rax and get the input
  // frame descriptor pointer.
  __ movq(rbx, Operand(rax, Deoptimizer::input_offset()));

  // Fill in the input registers.
  for (int i = kNumberOfRegisters - 1; i >= 0; i--) {
    int offset =
        (i * kSystemPointerSize) + FrameDescription::registers_offset();
    __ PopQuad(Operand(rbx, offset));
  }

  // Fill in the float input registers.
  int float_regs_offset = FrameDescription::float_registers_offset();
  for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
    int src_offset = i * kFloatSize;
    int dst_offset = i * kFloatSize + float_regs_offset;
    __ movl(rcx, Operand(rsp, src_offset));
    __ movl(Operand(rbx, dst_offset), rcx);
  }
  __ addq(rsp, Immediate(kFloatRegsSize));

  // Fill in the double input registers.
  int double_regs_offset = FrameDescription::double_registers_offset();
  for (int i = 0; i < XMMRegister::kNumRegisters; i++) {
    int dst_offset = i * kDoubleSize + double_regs_offset;
    __ popq(Operand(rbx, dst_offset));
  }

  // Remove the return address from the stack.
  __ addq(rsp, Immediate(kPCOnStackSize));

  // Compute a pointer to the unwinding limit in register rcx; that is
  // the first stack slot not part of the input frame.
  __ movq(rcx, Operand(rbx, FrameDescription::frame_size_offset()));
  __ addq(rcx, rsp);

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ leaq(rdx, Operand(rbx, FrameDescription::frame_content_offset()));
  Label pop_loop_header;
  __ jmp(&pop_loop_header);
  Label pop_loop;
  __ bind(&pop_loop);
  __ Pop(Operand(rdx, 0));
  __ addq(rdx, Immediate(sizeof(intptr_t)));
  __ bind(&pop_loop_header);
  __ cmpq(rcx, rsp);
  __ j(not_equal, &pop_loop);

  // Compute the output frame in the deoptimizer.
  __ pushq(rax);
  __ PrepareCallCFunction(2);
  __ movq(arg_reg_1, rax);
  __ LoadAddress(arg_reg_2, ExternalReference::isolate_address(isolate));
  {
    AllowExternalCallThatCantCauseGC scope(masm);
    __ CallCFunction(ExternalReference::compute_output_frames_function(), 2);
  }
  __ popq(rax);

  __ movq(rsp, Operand(rax, Deoptimizer::caller_frame_top_offset()));

  // Replace the current (input) frame with the output frames.
  Label outer_push_loop, inner_push_loop, outer_loop_header, inner_loop_header;
  // Outer loop state: rax = current FrameDescription**, rdx = one past the
  // last FrameDescription**.
  __ movl(rdx, Operand(rax, Deoptimizer::output_count_offset()));
  __ movq(rax, Operand(rax, Deoptimizer::output_offset()));
  __ leaq(rdx, Operand(rax, rdx, times_system_pointer_size, 0));
  __ jmp(&outer_loop_header);
  __ bind(&outer_push_loop);
  // Inner loop state: rbx = current FrameDescription*, rcx = loop index.
  __ movq(rbx, Operand(rax, 0));
  __ movq(rcx, Operand(rbx, FrameDescription::frame_size_offset()));
  __ jmp(&inner_loop_header);
  __ bind(&inner_push_loop);
  __ subq(rcx, Immediate(sizeof(intptr_t)));
  __ Push(Operand(rbx, rcx, times_1, FrameDescription::frame_content_offset()));
  __ bind(&inner_loop_header);
  __ testq(rcx, rcx);
  __ j(not_zero, &inner_push_loop);
  __ addq(rax, Immediate(kSystemPointerSize));
  __ bind(&outer_loop_header);
  __ cmpq(rax, rdx);
  __ j(below, &outer_push_loop);

  for (int i = 0; i < config->num_allocatable_double_registers(); ++i) {
    int code = config->GetAllocatableDoubleCode(i);
    XMMRegister xmm_reg = XMMRegister::from_code(code);
    int src_offset = code * kDoubleSize + double_regs_offset;
    __ Movsd(xmm_reg, Operand(rbx, src_offset));
  }

  // Push pc and continuation from the last output frame.
  __ PushQuad(Operand(rbx, FrameDescription::pc_offset()));
  __ PushQuad(Operand(rbx, FrameDescription::continuation_offset()));

  // Push the registers from the last output frame.
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset =
        (i * kSystemPointerSize) + FrameDescription::registers_offset();
    __ PushQuad(Operand(rbx, offset));
  }

  // Restore the registers from the stack.
  for (int i = kNumberOfRegisters - 1; i >= 0; i--) {
    Register r = Register::from_code(i);
    // Do not restore rsp, simply pop the value into the next register
    // and overwrite this afterwards.
    if (r == rsp) {
      DCHECK_GT(i, 0);
      r = Register::from_code(i - 1);
    }
    __ popq(r);
  }

  // Return to the continuation point.
  __ ret(0);
}

bool Deoptimizer::PadTopOfStackRegister() { return false; }

void FrameDescription::SetCallerPc(unsigned offset, intptr_t value) {
  if (kPCOnStackSize == 2 * kSystemPointerSize) {
    // Zero out the high-32 bit of PC for x32 port.
    SetFrameSlot(offset + kSystemPointerSize, 0);
  }
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerFp(unsigned offset, intptr_t value) {
  if (kFPOnStackSize == 2 * kSystemPointerSize) {
    // Zero out the high-32 bit of FP for x32 port.
    SetFrameSlot(offset + kSystemPointerSize, 0);
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
