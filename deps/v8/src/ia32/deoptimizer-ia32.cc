// Copyright 2011 the V8 project authors. All rights reserved.
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

#if defined(V8_TARGET_ARCH_IA32)

#include "codegen.h"
#include "deoptimizer.h"
#include "full-codegen.h"
#include "safepoint-table.h"

namespace v8 {
namespace internal {


int Deoptimizer::table_entry_size_ = 10;

void Deoptimizer::DeoptimizeFunction(JSFunction* function) {
  AssertNoAllocation no_allocation;

  if (!function->IsOptimized()) return;

  // Get the optimized code.
  Code* code = function->code();

  // Invalidate the relocation information, as it will become invalid by the
  // code patching below, and is not needed any more.
  code->InvalidateRelocation();

  // For each return after a safepoint insert a absolute call to the
  // corresponding deoptimization entry.
  unsigned last_pc_offset = 0;
  SafepointTable table(function->code());
  for (unsigned i = 0; i < table.length(); i++) {
    unsigned pc_offset = table.GetPcOffset(i);
    SafepointEntry safepoint_entry = table.GetEntry(i);
    int deoptimization_index = safepoint_entry.deoptimization_index();
    int gap_code_size = safepoint_entry.gap_code_size();
#ifdef DEBUG
    // Destroy the code which is not supposed to run again.
    unsigned instructions = pc_offset - last_pc_offset;
    CodePatcher destroyer(code->instruction_start() + last_pc_offset,
                          instructions);
    for (unsigned i = 0; i < instructions; i++) {
      destroyer.masm()->int3();
    }
#endif
    last_pc_offset = pc_offset;
    if (deoptimization_index != Safepoint::kNoDeoptimizationIndex) {
      CodePatcher patcher(
          code->instruction_start() + pc_offset + gap_code_size,
          Assembler::kCallInstructionLength);
      patcher.masm()->call(GetDeoptimizationEntry(deoptimization_index, LAZY),
                           RelocInfo::NONE);
      last_pc_offset += gap_code_size + Assembler::kCallInstructionLength;
    }
  }
#ifdef DEBUG
  // Destroy the code which is not supposed to run again.
  unsigned instructions = code->safepoint_table_start() - last_pc_offset;
  CodePatcher destroyer(code->instruction_start() + last_pc_offset,
                        instructions);
  for (unsigned i = 0; i < instructions; i++) {
    destroyer.masm()->int3();
  }
#endif

  // Add the deoptimizing code to the list.
  DeoptimizingCodeListNode* node = new DeoptimizingCodeListNode(code);
  node->set_next(deoptimizing_code_list_);
  deoptimizing_code_list_ = node;

  // Set the code for the function to non-optimized version.
  function->ReplaceCode(function->shared()->code());

  if (FLAG_trace_deopt) {
    PrintF("[forced deoptimization: ");
    function->PrintName();
    PrintF(" / %x]\n", reinterpret_cast<uint32_t>(function));
  }
}


void Deoptimizer::PatchStackCheckCode(RelocInfo* rinfo,
                                      Code* replacement_code) {
  // The stack check code matches the pattern:
  //
  //     cmp esp, <limit>
  //     jae ok
  //     call <stack guard>
  //     test eax, <loop nesting depth>
  // ok: ...
  //
  // We will patch away the branch so the code is:
  //
  //     cmp esp, <limit>  ;; Not changed
  //     nop
  //     nop
  //     call <on-stack replacment>
  //     test eax, <loop nesting depth>
  // ok:
  Address call_target_address = rinfo->pc();
  ASSERT(*(call_target_address - 3) == 0x73 &&  // jae
         *(call_target_address - 2) == 0x07 &&  // offset
         *(call_target_address - 1) == 0xe8);   // call
  *(call_target_address - 3) = 0x90;  // nop
  *(call_target_address - 2) = 0x90;  // nop
  rinfo->set_target_address(replacement_code->entry());
}


void Deoptimizer::RevertStackCheckCode(RelocInfo* rinfo, Code* check_code) {
  // Replace the nops from patching (Deoptimizer::PatchStackCheckCode) to
  // restore the conditional branch.
  Address call_target_address = rinfo->pc();
  ASSERT(*(call_target_address - 3) == 0x90 &&  // nop
         *(call_target_address - 2) == 0x90 &&  // nop
         *(call_target_address - 1) == 0xe8);   // call
  *(call_target_address - 3) = 0x73;  // jae
  *(call_target_address - 2) = 0x07;  // offset
  rinfo->set_target_address(check_code->entry());
}


static int LookupBailoutId(DeoptimizationInputData* data, unsigned ast_id) {
  ByteArray* translations = data->TranslationByteArray();
  int length = data->DeoptCount();
  for (int i = 0; i < length; i++) {
    if (static_cast<unsigned>(data->AstId(i)->value()) == ast_id) {
      TranslationIterator it(translations,  data->TranslationIndex(i)->value());
      int value = it.Next();
      ASSERT(Translation::BEGIN == static_cast<Translation::Opcode>(value));
      // Read the number of frames.
      value = it.Next();
      if (value == 1) return i;
    }
  }
  UNREACHABLE();
  return -1;
}


void Deoptimizer::DoComputeOsrOutputFrame() {
  DeoptimizationInputData* data = DeoptimizationInputData::cast(
      optimized_code_->deoptimization_data());
  unsigned ast_id = data->OsrAstId()->value();
  // TODO(kasperl): This should not be the bailout_id_. It should be
  // the ast id. Confusing.
  ASSERT(bailout_id_ == ast_id);

  int bailout_id = LookupBailoutId(data, ast_id);
  unsigned translation_index = data->TranslationIndex(bailout_id)->value();
  ByteArray* translations = data->TranslationByteArray();

  TranslationIterator iterator(translations, translation_index);
  Translation::Opcode opcode =
      static_cast<Translation::Opcode>(iterator.Next());
  ASSERT(Translation::BEGIN == opcode);
  USE(opcode);
  int count = iterator.Next();
  ASSERT(count == 1);
  USE(count);

  opcode = static_cast<Translation::Opcode>(iterator.Next());
  USE(opcode);
  ASSERT(Translation::FRAME == opcode);
  unsigned node_id = iterator.Next();
  USE(node_id);
  ASSERT(node_id == ast_id);
  JSFunction* function = JSFunction::cast(ComputeLiteral(iterator.Next()));
  USE(function);
  ASSERT(function == function_);
  unsigned height = iterator.Next();
  unsigned height_in_bytes = height * kPointerSize;
  USE(height_in_bytes);

  unsigned fixed_size = ComputeFixedSize(function_);
  unsigned input_frame_size = input_->GetFrameSize();
  ASSERT(fixed_size + height_in_bytes == input_frame_size);

  unsigned stack_slot_size = optimized_code_->stack_slots() * kPointerSize;
  unsigned outgoing_height = data->ArgumentsStackHeight(bailout_id)->value();
  unsigned outgoing_size = outgoing_height * kPointerSize;
  unsigned output_frame_size = fixed_size + stack_slot_size + outgoing_size;
  ASSERT(outgoing_size == 0);  // OSR does not happen in the middle of a call.

  if (FLAG_trace_osr) {
    PrintF("[on-stack replacement: begin 0x%08" V8PRIxPTR " ",
           reinterpret_cast<intptr_t>(function_));
    function_->PrintName();
    PrintF(" => node=%u, frame=%d->%d]\n",
           ast_id,
           input_frame_size,
           output_frame_size);
  }

  // There's only one output frame in the OSR case.
  output_count_ = 1;
  output_ = new FrameDescription*[1];
  output_[0] = new(output_frame_size) FrameDescription(
      output_frame_size, function_);

  // Clear the incoming parameters in the optimized frame to avoid
  // confusing the garbage collector.
  unsigned output_offset = output_frame_size - kPointerSize;
  int parameter_count = function_->shared()->formal_parameter_count() + 1;
  for (int i = 0; i < parameter_count; ++i) {
    output_[0]->SetFrameSlot(output_offset, 0);
    output_offset -= kPointerSize;
  }

  // Translate the incoming parameters. This may overwrite some of the
  // incoming argument slots we've just cleared.
  int input_offset = input_frame_size - kPointerSize;
  bool ok = true;
  int limit = input_offset - (parameter_count * kPointerSize);
  while (ok && input_offset > limit) {
    ok = DoOsrTranslateCommand(&iterator, &input_offset);
  }

  // There are no translation commands for the caller's pc and fp, the
  // context, and the function.  Set them up explicitly.
  for (int i = 0; ok && i < 4; i++) {
    uint32_t input_value = input_->GetFrameSlot(input_offset);
    if (FLAG_trace_osr) {
      PrintF("    [esp + %d] <- 0x%08x ; [esp + %d] (fixed part)\n",
             output_offset,
             input_value,
             input_offset);
    }
    output_[0]->SetFrameSlot(output_offset, input_->GetFrameSlot(input_offset));
    input_offset -= kPointerSize;
    output_offset -= kPointerSize;
  }

  // Translate the rest of the frame.
  while (ok && input_offset >= 0) {
    ok = DoOsrTranslateCommand(&iterator, &input_offset);
  }

  // If translation of any command failed, continue using the input frame.
  if (!ok) {
    delete output_[0];
    output_[0] = input_;
    output_[0]->SetPc(reinterpret_cast<uint32_t>(from_));
  } else {
    // Setup the frame pointer and the context pointer.
    output_[0]->SetRegister(ebp.code(), input_->GetRegister(ebp.code()));
    output_[0]->SetRegister(esi.code(), input_->GetRegister(esi.code()));

    unsigned pc_offset = data->OsrPcOffset()->value();
    uint32_t pc = reinterpret_cast<uint32_t>(
        optimized_code_->entry() + pc_offset);
    output_[0]->SetPc(pc);
  }
  Code* continuation = Builtins::builtin(Builtins::NotifyOSR);
  output_[0]->SetContinuation(
      reinterpret_cast<uint32_t>(continuation->entry()));

  if (FLAG_trace_osr) {
    PrintF("[on-stack replacement translation %s: 0x%08" V8PRIxPTR " ",
           ok ? "finished" : "aborted",
           reinterpret_cast<intptr_t>(function));
    function->PrintName();
    PrintF(" => pc=0x%0x]\n", output_[0]->GetPc());
  }
}


void Deoptimizer::DoComputeFrame(TranslationIterator* iterator,
                                 int frame_index) {
  // Read the ast node id, function, and frame height for this output frame.
  Translation::Opcode opcode =
      static_cast<Translation::Opcode>(iterator->Next());
  USE(opcode);
  ASSERT(Translation::FRAME == opcode);
  int node_id = iterator->Next();
  JSFunction* function = JSFunction::cast(ComputeLiteral(iterator->Next()));
  unsigned height = iterator->Next();
  unsigned height_in_bytes = height * kPointerSize;
  if (FLAG_trace_deopt) {
    PrintF("  translating ");
    function->PrintName();
    PrintF(" => node=%d, height=%d\n", node_id, height_in_bytes);
  }

  // The 'fixed' part of the frame consists of the incoming parameters and
  // the part described by JavaScriptFrameConstants.
  unsigned fixed_frame_size = ComputeFixedSize(function);
  unsigned input_frame_size = input_->GetFrameSize();
  unsigned output_frame_size = height_in_bytes + fixed_frame_size;

  // Allocate and store the output frame description.
  FrameDescription* output_frame =
      new(output_frame_size) FrameDescription(output_frame_size, function);

  bool is_bottommost = (0 == frame_index);
  bool is_topmost = (output_count_ - 1 == frame_index);
  ASSERT(frame_index >= 0 && frame_index < output_count_);
  ASSERT(output_[frame_index] == NULL);
  output_[frame_index] = output_frame;

  // The top address for the bottommost output frame can be computed from
  // the input frame pointer and the output frame's height.  For all
  // subsequent output frames, it can be computed from the previous one's
  // top address and the current frame's size.
  uint32_t top_address;
  if (is_bottommost) {
    // 2 = context and function in the frame.
    top_address =
        input_->GetRegister(ebp.code()) - (2 * kPointerSize) - height_in_bytes;
  } else {
    top_address = output_[frame_index - 1]->GetTop() - output_frame_size;
  }
  output_frame->SetTop(top_address);

  // Compute the incoming parameter translation.
  int parameter_count = function->shared()->formal_parameter_count() + 1;
  unsigned output_offset = output_frame_size;
  unsigned input_offset = input_frame_size;
  for (int i = 0; i < parameter_count; ++i) {
    output_offset -= kPointerSize;
    DoTranslateCommand(iterator, frame_index, output_offset);
  }
  input_offset -= (parameter_count * kPointerSize);

  // There are no translation commands for the caller's pc and fp, the
  // context, and the function.  Synthesize their values and set them up
  // explicitly.
  //
  // The caller's pc for the bottommost output frame is the same as in the
  // input frame.  For all subsequent output frames, it can be read from the
  // previous one.  This frame's pc can be computed from the non-optimized
  // function code and AST id of the bailout.
  output_offset -= kPointerSize;
  input_offset -= kPointerSize;
  intptr_t value;
  if (is_bottommost) {
    value = input_->GetFrameSlot(input_offset);
  } else {
    value = output_[frame_index - 1]->GetPc();
  }
  output_frame->SetFrameSlot(output_offset, value);
  if (FLAG_trace_deopt) {
    PrintF("    0x%08x: [top + %d] <- 0x%08x ; caller's pc\n",
           top_address + output_offset, output_offset, value);
  }

  // The caller's frame pointer for the bottommost output frame is the same
  // as in the input frame.  For all subsequent output frames, it can be
  // read from the previous one.  Also compute and set this frame's frame
  // pointer.
  output_offset -= kPointerSize;
  input_offset -= kPointerSize;
  if (is_bottommost) {
    value = input_->GetFrameSlot(input_offset);
  } else {
    value = output_[frame_index - 1]->GetFp();
  }
  output_frame->SetFrameSlot(output_offset, value);
  intptr_t fp_value = top_address + output_offset;
  ASSERT(!is_bottommost || input_->GetRegister(ebp.code()) == fp_value);
  output_frame->SetFp(fp_value);
  if (is_topmost) output_frame->SetRegister(ebp.code(), fp_value);
  if (FLAG_trace_deopt) {
    PrintF("    0x%08x: [top + %d] <- 0x%08x ; caller's fp\n",
           fp_value, output_offset, value);
  }

  // The context can be gotten from the function so long as we don't
  // optimize functions that need local contexts.
  output_offset -= kPointerSize;
  input_offset -= kPointerSize;
  value = reinterpret_cast<uint32_t>(function->context());
  // The context for the bottommost output frame should also agree with the
  // input frame.
  ASSERT(!is_bottommost || input_->GetFrameSlot(input_offset) == value);
  output_frame->SetFrameSlot(output_offset, value);
  if (is_topmost) output_frame->SetRegister(esi.code(), value);
  if (FLAG_trace_deopt) {
    PrintF("    0x%08x: [top + %d] <- 0x%08x ; context\n",
           top_address + output_offset, output_offset, value);
  }

  // The function was mentioned explicitly in the BEGIN_FRAME.
  output_offset -= kPointerSize;
  input_offset -= kPointerSize;
  value = reinterpret_cast<uint32_t>(function);
  // The function for the bottommost output frame should also agree with the
  // input frame.
  ASSERT(!is_bottommost || input_->GetFrameSlot(input_offset) == value);
  output_frame->SetFrameSlot(output_offset, value);
  if (FLAG_trace_deopt) {
    PrintF("    0x%08x: [top + %d] <- 0x%08x ; function\n",
           top_address + output_offset, output_offset, value);
  }

  // Translate the rest of the frame.
  for (unsigned i = 0; i < height; ++i) {
    output_offset -= kPointerSize;
    DoTranslateCommand(iterator, frame_index, output_offset);
  }
  ASSERT(0 == output_offset);

  // Compute this frame's PC, state, and continuation.
  Code* non_optimized_code = function->shared()->code();
  FixedArray* raw_data = non_optimized_code->deoptimization_data();
  DeoptimizationOutputData* data = DeoptimizationOutputData::cast(raw_data);
  Address start = non_optimized_code->instruction_start();
  unsigned pc_and_state = GetOutputInfo(data, node_id, function->shared());
  unsigned pc_offset = FullCodeGenerator::PcField::decode(pc_and_state);
  uint32_t pc_value = reinterpret_cast<uint32_t>(start + pc_offset);
  output_frame->SetPc(pc_value);

  FullCodeGenerator::State state =
      FullCodeGenerator::StateField::decode(pc_and_state);
  output_frame->SetState(Smi::FromInt(state));

  // Set the continuation for the topmost frame.
  if (is_topmost) {
    Code* continuation = (bailout_type_ == EAGER)
        ? Builtins::builtin(Builtins::NotifyDeoptimized)
        : Builtins::builtin(Builtins::NotifyLazyDeoptimized);
    output_frame->SetContinuation(
        reinterpret_cast<uint32_t>(continuation->entry()));
  }

  if (output_count_ - 1 == frame_index) iterator->Done();
}


#define __ masm()->

void Deoptimizer::EntryGenerator::Generate() {
  GeneratePrologue();
  CpuFeatures::Scope scope(SSE2);

  // Save all general purpose registers before messing with them.
  const int kNumberOfRegisters = Register::kNumRegisters;

  const int kDoubleRegsSize = kDoubleSize *
                              XMMRegister::kNumAllocatableRegisters;
  __ sub(Operand(esp), Immediate(kDoubleRegsSize));
  for (int i = 0; i < XMMRegister::kNumAllocatableRegisters; ++i) {
    XMMRegister xmm_reg = XMMRegister::FromAllocationIndex(i);
    int offset = i * kDoubleSize;
    __ movdbl(Operand(esp, offset), xmm_reg);
  }

  __ pushad();

  const int kSavedRegistersAreaSize = kNumberOfRegisters * kPointerSize +
                                      kDoubleRegsSize;

  // Get the bailout id from the stack.
  __ mov(ebx, Operand(esp, kSavedRegistersAreaSize));

  // Get the address of the location in the code object if possible
  // and compute the fp-to-sp delta in register edx.
  if (type() == EAGER) {
    __ Set(ecx, Immediate(0));
    __ lea(edx, Operand(esp, kSavedRegistersAreaSize + 1 * kPointerSize));
  } else {
    __ mov(ecx, Operand(esp, kSavedRegistersAreaSize + 1 * kPointerSize));
    __ lea(edx, Operand(esp, kSavedRegistersAreaSize + 2 * kPointerSize));
  }
  __ sub(edx, Operand(ebp));
  __ neg(edx);

  // Allocate a new deoptimizer object.
  __ PrepareCallCFunction(5, eax);
  __ mov(eax, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ mov(Operand(esp, 0 * kPointerSize), eax);  // Function.
  __ mov(Operand(esp, 1 * kPointerSize), Immediate(type()));  // Bailout type.
  __ mov(Operand(esp, 2 * kPointerSize), ebx);  // Bailout id.
  __ mov(Operand(esp, 3 * kPointerSize), ecx);  // Code address or 0.
  __ mov(Operand(esp, 4 * kPointerSize), edx);  // Fp-to-sp delta.
  __ CallCFunction(ExternalReference::new_deoptimizer_function(), 5);

  // Preserve deoptimizer object in register eax and get the input
  // frame descriptor pointer.
  __ mov(ebx, Operand(eax, Deoptimizer::input_offset()));

  // Fill in the input registers.
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset = (i * kIntSize) + FrameDescription::registers_offset();
    __ mov(ecx, Operand(esp, (kNumberOfRegisters - 1 - i) * kPointerSize));
    __ mov(Operand(ebx, offset), ecx);
  }

  // Fill in the double input registers.
  int double_regs_offset = FrameDescription::double_registers_offset();
  for (int i = 0; i < XMMRegister::kNumAllocatableRegisters; ++i) {
    int dst_offset = i * kDoubleSize + double_regs_offset;
    int src_offset = i * kDoubleSize + kNumberOfRegisters * kPointerSize;
    __ movdbl(xmm0, Operand(esp, src_offset));
    __ movdbl(Operand(ebx, dst_offset), xmm0);
  }

  // Remove the bailout id and the general purpose registers from the stack.
  if (type() == EAGER) {
    __ add(Operand(esp), Immediate(kSavedRegistersAreaSize + kPointerSize));
  } else {
    __ add(Operand(esp), Immediate(kSavedRegistersAreaSize + 2 * kPointerSize));
  }

  // Compute a pointer to the unwinding limit in register ecx; that is
  // the first stack slot not part of the input frame.
  __ mov(ecx, Operand(ebx, FrameDescription::frame_size_offset()));
  __ add(ecx, Operand(esp));

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ lea(edx, Operand(ebx, FrameDescription::frame_content_offset()));
  Label pop_loop;
  __ bind(&pop_loop);
  __ pop(Operand(edx, 0));
  __ add(Operand(edx), Immediate(sizeof(uint32_t)));
  __ cmp(ecx, Operand(esp));
  __ j(not_equal, &pop_loop);

  // Compute the output frame in the deoptimizer.
  __ push(eax);
  __ PrepareCallCFunction(1, ebx);
  __ mov(Operand(esp, 0 * kPointerSize), eax);
  __ CallCFunction(ExternalReference::compute_output_frames_function(), 1);
  __ pop(eax);

  // Replace the current frame with the output frames.
  Label outer_push_loop, inner_push_loop;
  // Outer loop state: eax = current FrameDescription**, edx = one past the
  // last FrameDescription**.
  __ mov(edx, Operand(eax, Deoptimizer::output_count_offset()));
  __ mov(eax, Operand(eax, Deoptimizer::output_offset()));
  __ lea(edx, Operand(eax, edx, times_4, 0));
  __ bind(&outer_push_loop);
  // Inner loop state: ebx = current FrameDescription*, ecx = loop index.
  __ mov(ebx, Operand(eax, 0));
  __ mov(ecx, Operand(ebx, FrameDescription::frame_size_offset()));
  __ bind(&inner_push_loop);
  __ sub(Operand(ecx), Immediate(sizeof(uint32_t)));
  __ push(Operand(ebx, ecx, times_1, FrameDescription::frame_content_offset()));
  __ test(ecx, Operand(ecx));
  __ j(not_zero, &inner_push_loop);
  __ add(Operand(eax), Immediate(kPointerSize));
  __ cmp(eax, Operand(edx));
  __ j(below, &outer_push_loop);

  // In case of OSR, we have to restore the XMM registers.
  if (type() == OSR) {
    for (int i = 0; i < XMMRegister::kNumAllocatableRegisters; ++i) {
      XMMRegister xmm_reg = XMMRegister::FromAllocationIndex(i);
      int src_offset = i * kDoubleSize + double_regs_offset;
      __ movdbl(xmm_reg, Operand(ebx, src_offset));
    }
  }

  // Push state, pc, and continuation from the last output frame.
  if (type() != OSR) {
    __ push(Operand(ebx, FrameDescription::state_offset()));
  }
  __ push(Operand(ebx, FrameDescription::pc_offset()));
  __ push(Operand(ebx, FrameDescription::continuation_offset()));


  // Push the registers from the last output frame.
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset = (i * kIntSize) + FrameDescription::registers_offset();
    __ push(Operand(ebx, offset));
  }

  // Restore the registers from the stack.
  __ popad();

  // Return to the continuation point.
  __ ret(0);
}


void Deoptimizer::TableEntryGenerator::GeneratePrologue() {
  // Create a sequence of deoptimization entries.
  Label done;
  for (int i = 0; i < count(); i++) {
    int start = masm()->pc_offset();
    USE(start);
    __ push_imm32(i);
    __ jmp(&done);
    ASSERT(masm()->pc_offset() - start == table_entry_size_);
  }
  __ bind(&done);
}

#undef __


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32
