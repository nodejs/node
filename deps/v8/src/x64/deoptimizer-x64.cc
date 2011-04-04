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

#if defined(V8_TARGET_ARCH_X64)

#include "codegen.h"
#include "deoptimizer.h"
#include "full-codegen.h"
#include "safepoint-table.h"

namespace v8 {
namespace internal {


int Deoptimizer::table_entry_size_ = 10;


int Deoptimizer::patch_size() {
  return MacroAssembler::kCallInstructionLength;
}


#ifdef DEBUG
// Overwrites code with int3 instructions.
static void ZapCodeRange(Address from, Address to) {
  CHECK(from <= to);
  int length = static_cast<int>(to - from);
  CodePatcher destroyer(from, length);
  while (length-- > 0) {
    destroyer.masm()->int3();
  }
}
#endif


// Iterate through the entries of a SafepointTable that corresponds to
// deoptimization points.
class SafepointTableDeoptimiztionEntryIterator {
 public:
  explicit SafepointTableDeoptimiztionEntryIterator(Code* code)
      : code_(code), table_(code), index_(-1), limit_(table_.length()) {
    FindNextIndex();
  }

  SafepointEntry Next(Address* pc) {
    if (index_ >= limit_) {
      *pc = NULL;
      return SafepointEntry();  // Invalid entry.
    }
    *pc = code_->instruction_start() + table_.GetPcOffset(index_);
    SafepointEntry entry = table_.GetEntry(index_);
    FindNextIndex();
    return entry;
  }

 private:
  void FindNextIndex() {
    ASSERT(index_ < limit_);
    while (++index_ < limit_) {
      if (table_.GetEntry(index_).deoptimization_index() !=
          Safepoint::kNoDeoptimizationIndex) {
        return;
      }
    }
  }

  Code* code_;
  SafepointTable table_;
  // Index of next deoptimization entry. If negative after calling
  // FindNextIndex, there are no more, and Next will return an invalid
  // SafepointEntry.
  int index_;
  // Table length.
  int limit_;
};


void Deoptimizer::EnsureRelocSpaceForLazyDeoptimization(Handle<Code> code) {
  // TODO(1276): Implement.
}


void Deoptimizer::DeoptimizeFunction(JSFunction* function) {
  AssertNoAllocation no_allocation;

  if (!function->IsOptimized()) return;

  // Get the optimized code.
  Code* code = function->code();

  // Invalidate the relocation information, as it will become invalid by the
  // code patching below, and is not needed any more.
  code->InvalidateRelocation();

  // For each return after a safepoint insert a absolute call to the
  // corresponding deoptimization entry, or a short call to an absolute
  // jump if space is short. The absolute jumps are put in a table just
  // before the safepoint table (space was allocated there when the Code
  // object was created, if necessary).

  Address instruction_start = function->code()->instruction_start();
  Address jump_table_address =
      instruction_start + function->code()->safepoint_table_offset();
  Address previous_pc = instruction_start;

  SafepointTableDeoptimiztionEntryIterator deoptimizations(function->code());
  Address entry_pc = NULL;

  SafepointEntry current_entry = deoptimizations.Next(&entry_pc);
  while (current_entry.is_valid()) {
    int gap_code_size = current_entry.gap_code_size();
    unsigned deoptimization_index = current_entry.deoptimization_index();

#ifdef DEBUG
    // Destroy the code which is not supposed to run again.
    ZapCodeRange(previous_pc, entry_pc);
#endif
    // Position where Call will be patched in.
    Address call_address = entry_pc + gap_code_size;
    // End of call instruction, if using a direct call to a 64-bit address.
    Address call_end_address =
        call_address + MacroAssembler::kCallInstructionLength;

    // Find next deoptimization entry, if any.
    Address next_pc = NULL;
    SafepointEntry next_entry = deoptimizations.Next(&next_pc);

    if (!next_entry.is_valid() || next_pc >= call_end_address) {
      // Room enough to write a long call instruction.
      CodePatcher patcher(call_address, Assembler::kCallInstructionLength);
      patcher.masm()->Call(GetDeoptimizationEntry(deoptimization_index, LAZY),
                           RelocInfo::NONE);
      previous_pc = call_end_address;
    } else {
      // Not room enough for a long Call instruction. Write a short call
      // instruction to a long jump placed elsewhere in the code.
      Address short_call_end_address =
          call_address + MacroAssembler::kShortCallInstructionLength;
      ASSERT(next_pc >= short_call_end_address);

      // Write jump in jump-table.
      jump_table_address -= MacroAssembler::kJumpInstructionLength;
      CodePatcher jump_patcher(jump_table_address,
                               MacroAssembler::kJumpInstructionLength);
      jump_patcher.masm()->Jump(
          GetDeoptimizationEntry(deoptimization_index, LAZY),
          RelocInfo::NONE);

      // Write call to jump at call_offset.
      CodePatcher call_patcher(call_address,
                               MacroAssembler::kShortCallInstructionLength);
      call_patcher.masm()->call(jump_table_address);
      previous_pc = short_call_end_address;
    }

    // Continue with next deoptimization entry.
    current_entry = next_entry;
    entry_pc = next_pc;
  }

#ifdef DEBUG
  // Destroy the code which is not supposed to run again.
  ZapCodeRange(previous_pc, jump_table_address);
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
    PrintF(" / %" V8PRIxPTR "]\n", reinterpret_cast<intptr_t>(function));
  }
}


void Deoptimizer::PatchStackCheckCodeAt(Address pc_after,
                                        Code* check_code,
                                        Code* replacement_code) {
  Address call_target_address = pc_after - kIntSize;
  ASSERT(check_code->entry() ==
         Assembler::target_address_at(call_target_address));
  // The stack check code matches the pattern:
  //
  //     cmp rsp, <limit>
  //     jae ok
  //     call <stack guard>
  //     test rax, <loop nesting depth>
  // ok: ...
  //
  // We will patch away the branch so the code is:
  //
  //     cmp rsp, <limit>  ;; Not changed
  //     nop
  //     nop
  //     call <on-stack replacment>
  //     test rax, <loop nesting depth>
  // ok:
  //
  ASSERT(*(call_target_address - 3) == 0x73 &&  // jae
         *(call_target_address - 2) == 0x07 &&  // offset
         *(call_target_address - 1) == 0xe8);   // call
  *(call_target_address - 3) = 0x90;  // nop
  *(call_target_address - 2) = 0x90;  // nop
  Assembler::set_target_address_at(call_target_address,
                                   replacement_code->entry());
}


void Deoptimizer::RevertStackCheckCodeAt(Address pc_after,
                                         Code* check_code,
                                         Code* replacement_code) {
  Address call_target_address = pc_after - kIntSize;
  ASSERT(replacement_code->entry() ==
         Assembler::target_address_at(call_target_address));
  // Replace the nops from patching (Deoptimizer::PatchStackCheckCode) to
  // restore the conditional branch.
  ASSERT(*(call_target_address - 3) == 0x90 &&  // nop
         *(call_target_address - 2) == 0x90 &&  // nop
         *(call_target_address - 1) == 0xe8);   // call
  *(call_target_address - 3) = 0x73;  // jae
  *(call_target_address - 2) = 0x07;  // offset
  Assembler::set_target_address_at(call_target_address,
                                   check_code->entry());
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
  unsigned input_frame_size = static_cast<unsigned>(input_->GetFrameSize());
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
    intptr_t input_value = input_->GetFrameSlot(input_offset);
    if (FLAG_trace_osr) {
      PrintF("    [esp + %d] <- 0x%08" V8PRIxPTR " ; [esp + %d] (fixed part)\n",
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
    output_[0]->SetPc(reinterpret_cast<intptr_t>(from_));
  } else {
    // Setup the frame pointer and the context pointer.
    output_[0]->SetRegister(rbp.code(), input_->GetRegister(rbp.code()));
    output_[0]->SetRegister(rsi.code(), input_->GetRegister(rsi.code()));

    unsigned pc_offset = data->OsrPcOffset()->value();
    intptr_t pc = reinterpret_cast<intptr_t>(
        optimized_code_->entry() + pc_offset);
    output_[0]->SetPc(pc);
  }
  Code* continuation = Builtins::builtin(Builtins::NotifyOSR);
  output_[0]->SetContinuation(
      reinterpret_cast<intptr_t>(continuation->entry()));

  if (FLAG_trace_osr) {
    PrintF("[on-stack replacement translation %s: 0x%08" V8PRIxPTR " ",
           ok ? "finished" : "aborted",
           reinterpret_cast<intptr_t>(function));
    function->PrintName();
    PrintF(" => pc=0x%0" V8PRIxPTR "]\n", output_[0]->GetPc());
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
  unsigned input_frame_size = static_cast<unsigned>(input_->GetFrameSize());
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
  intptr_t top_address;
  if (is_bottommost) {
    // 2 = context and function in the frame.
    top_address =
        input_->GetRegister(rbp.code()) - (2 * kPointerSize) - height_in_bytes;
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
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR  " ; caller's pc\n",
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
  ASSERT(!is_bottommost || input_->GetRegister(rbp.code()) == fp_value);
  output_frame->SetFp(fp_value);
  if (is_topmost) output_frame->SetRegister(rbp.code(), fp_value);
  if (FLAG_trace_deopt) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR " ; caller's fp\n",
           fp_value, output_offset, value);
  }

  // For the bottommost output frame the context can be gotten from the input
  // frame. For all subsequent output frames it can be gotten from the function
  // so long as we don't inline functions that need local contexts.
  output_offset -= kPointerSize;
  input_offset -= kPointerSize;
  if (is_bottommost) {
    value = input_->GetFrameSlot(input_offset);
  } else {
    value = reinterpret_cast<intptr_t>(function->context());
  }
  output_frame->SetFrameSlot(output_offset, value);
  if (is_topmost) output_frame->SetRegister(rsi.code(), value);
  if (FLAG_trace_deopt) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR "; context\n",
           top_address + output_offset, output_offset, value);
  }

  // The function was mentioned explicitly in the BEGIN_FRAME.
  output_offset -= kPointerSize;
  input_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(function);
  // The function for the bottommost output frame should also agree with the
  // input frame.
  ASSERT(!is_bottommost || input_->GetFrameSlot(input_offset) == value);
  output_frame->SetFrameSlot(output_offset, value);
  if (FLAG_trace_deopt) {
    PrintF("    0x%08" V8PRIxPTR ": [top + %d] <- 0x%08"
           V8PRIxPTR "; function\n",
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
  intptr_t pc_value = reinterpret_cast<intptr_t>(start + pc_offset);
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
        reinterpret_cast<intptr_t>(continuation->entry()));
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
  __ subq(rsp, Immediate(kDoubleRegsSize));

  for (int i = 0; i < XMMRegister::kNumAllocatableRegisters; ++i) {
    XMMRegister xmm_reg = XMMRegister::FromAllocationIndex(i);
    int offset = i * kDoubleSize;
    __ movsd(Operand(rsp, offset), xmm_reg);
  }

  // We push all registers onto the stack, even though we do not need
  // to restore all later.
  for (int i = 0; i < kNumberOfRegisters; i++) {
    Register r = Register::toRegister(i);
    __ push(r);
  }

  const int kSavedRegistersAreaSize = kNumberOfRegisters * kPointerSize +
                                      kDoubleRegsSize;

  // When calling new_deoptimizer_function we need to pass the last argument
  // on the stack on windows and in r8 on linux. The remaining arguments are
  // all passed in registers (different ones on linux and windows though).

#ifdef _WIN64
  Register arg4 = r9;
  Register arg3 = r8;
  Register arg2 = rdx;
  Register arg1 = rcx;
#else
  Register arg4 = rcx;
  Register arg3 = rdx;
  Register arg2 = rsi;
  Register arg1 = rdi;
#endif

  // We use this to keep the value of the fifth argument temporarily.
  // Unfortunately we can't store it directly in r8 (used for passing
  // this on linux), since it is another parameter passing register on windows.
  Register arg5 = r11;

  // Get the bailout id from the stack.
  __ movq(arg3, Operand(rsp, kSavedRegistersAreaSize));

  // Get the address of the location in the code object if possible
  // and compute the fp-to-sp delta in register arg5.
  if (type() == EAGER) {
    __ Set(arg4, 0);
    __ lea(arg5, Operand(rsp, kSavedRegistersAreaSize + 1 * kPointerSize));
  } else {
    __ movq(arg4, Operand(rsp, kSavedRegistersAreaSize + 1 * kPointerSize));
    __ lea(arg5, Operand(rsp, kSavedRegistersAreaSize + 2 * kPointerSize));
  }

  __ subq(arg5, rbp);
  __ neg(arg5);

  // Allocate a new deoptimizer object.
  __ PrepareCallCFunction(5);
  __ movq(rax, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ movq(arg1, rax);
  __ movq(arg2, Immediate(type()));
  // Args 3 and 4 are already in the right registers.

  // On windows put the argument on the stack (PrepareCallCFunction have
  // created space for this). On linux pass the argument in r8.
#ifdef _WIN64
  __ movq(Operand(rsp, 0 * kPointerSize), arg5);
#else
  __ movq(r8, arg5);
#endif

  __ CallCFunction(ExternalReference::new_deoptimizer_function(), 5);
  // Preserve deoptimizer object in register rax and get the input
  // frame descriptor pointer.
  __ movq(rbx, Operand(rax, Deoptimizer::input_offset()));

  // Fill in the input registers.
  for (int i = kNumberOfRegisters -1; i >= 0; i--) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    __ pop(Operand(rbx, offset));
  }

  // Fill in the double input registers.
  int double_regs_offset = FrameDescription::double_registers_offset();
  for (int i = 0; i < XMMRegister::kNumAllocatableRegisters; i++) {
    int dst_offset = i * kDoubleSize + double_regs_offset;
    __ pop(Operand(rbx, dst_offset));
  }

  // Remove the bailout id from the stack.
  if (type() == EAGER) {
    __ addq(rsp, Immediate(kPointerSize));
  } else {
    __ addq(rsp, Immediate(2 * kPointerSize));
  }

  // Compute a pointer to the unwinding limit in register rcx; that is
  // the first stack slot not part of the input frame.
  __ movq(rcx, Operand(rbx, FrameDescription::frame_size_offset()));
  __ addq(rcx, rsp);

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ lea(rdx, Operand(rbx, FrameDescription::frame_content_offset()));
  Label pop_loop;
  __ bind(&pop_loop);
  __ pop(Operand(rdx, 0));
  __ addq(rdx, Immediate(sizeof(intptr_t)));
  __ cmpq(rcx, rsp);
  __ j(not_equal, &pop_loop);

  // Compute the output frame in the deoptimizer.
  __ push(rax);
  __ PrepareCallCFunction(1);
  __ movq(arg1, rax);
  __ CallCFunction(ExternalReference::compute_output_frames_function(), 1);
  __ pop(rax);

  // Replace the current frame with the output frames.
  Label outer_push_loop, inner_push_loop;
  // Outer loop state: rax = current FrameDescription**, rdx = one past the
  // last FrameDescription**.
  __ movl(rdx, Operand(rax, Deoptimizer::output_count_offset()));
  __ movq(rax, Operand(rax, Deoptimizer::output_offset()));
  __ lea(rdx, Operand(rax, rdx, times_8, 0));
  __ bind(&outer_push_loop);
  // Inner loop state: rbx = current FrameDescription*, rcx = loop index.
  __ movq(rbx, Operand(rax, 0));
  __ movq(rcx, Operand(rbx, FrameDescription::frame_size_offset()));
  __ bind(&inner_push_loop);
  __ subq(rcx, Immediate(sizeof(intptr_t)));
  __ push(Operand(rbx, rcx, times_1, FrameDescription::frame_content_offset()));
  __ testq(rcx, rcx);
  __ j(not_zero, &inner_push_loop);
  __ addq(rax, Immediate(kPointerSize));
  __ cmpq(rax, rdx);
  __ j(below, &outer_push_loop);

  // In case of OSR, we have to restore the XMM registers.
  if (type() == OSR) {
    for (int i = 0; i < XMMRegister::kNumAllocatableRegisters; ++i) {
      XMMRegister xmm_reg = XMMRegister::FromAllocationIndex(i);
      int src_offset = i * kDoubleSize + double_regs_offset;
      __ movsd(xmm_reg, Operand(rbx, src_offset));
    }
  }

  // Push state, pc, and continuation from the last output frame.
  if (type() != OSR) {
    __ push(Operand(rbx, FrameDescription::state_offset()));
  }
  __ push(Operand(rbx, FrameDescription::pc_offset()));
  __ push(Operand(rbx, FrameDescription::continuation_offset()));

  // Push the registers from the last output frame.
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset = (i * kPointerSize) + FrameDescription::registers_offset();
    __ push(Operand(rbx, offset));
  }

  // Restore the registers from the stack.
  for (int i = kNumberOfRegisters - 1; i >= 0 ; i--) {
    Register r = Register::toRegister(i);
    // Do not restore rsp, simply pop the value into the next register
    // and overwrite this afterwards.
    if (r.is(rsp)) {
      ASSERT(i > 0);
      r = Register::toRegister(i - 1);
    }
    __ pop(r);
  }

  // Set up the roots register.
  ExternalReference roots_address = ExternalReference::roots_address();
  __ movq(r13, roots_address);

  __ movq(kSmiConstantRegister,
          reinterpret_cast<uint64_t>(Smi::FromInt(kSmiConstantRegisterValue)),
          RelocInfo::NONE);

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

#endif  // V8_TARGET_ARCH_X64
