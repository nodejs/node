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

#if defined(V8_TARGET_ARCH_X64)

#include "codegen.h"
#include "deoptimizer.h"
#include "full-codegen.h"
#include "safepoint-table.h"

namespace v8 {
namespace internal {


const int Deoptimizer::table_entry_size_ = 10;


int Deoptimizer::patch_size() {
  return Assembler::kCallInstructionLength;
}


void Deoptimizer::DeoptimizeFunctionWithPreparedFunctionList(
    JSFunction* function) {
  Isolate* isolate = function->GetIsolate();
  HandleScope scope(isolate);
  AssertNoAllocation no_allocation;

  ASSERT(function->IsOptimized());
  ASSERT(function->FunctionsInFunctionListShareSameCode());

  // Get the optimized code.
  Code* code = function->code();

  // The optimized code is going to be patched, so we cannot use it any more.
  function->shared()->EvictFromOptimizedCodeMap(code, "deoptimized function");

  // Invalidate the relocation information, as it will become invalid by the
  // code patching below, and is not needed any more.
  code->InvalidateRelocation();

  // For each LLazyBailout instruction insert a absolute call to the
  // corresponding deoptimization entry, or a short call to an absolute
  // jump if space is short. The absolute jumps are put in a table just
  // before the safepoint table (space was allocated there when the Code
  // object was created, if necessary).

  Address instruction_start = function->code()->instruction_start();
#ifdef DEBUG
  Address prev_call_address = NULL;
#endif
  DeoptimizationInputData* deopt_data =
      DeoptimizationInputData::cast(code->deoptimization_data());
  for (int i = 0; i < deopt_data->DeoptCount(); i++) {
    if (deopt_data->Pc(i)->value() == -1) continue;
    // Position where Call will be patched in.
    Address call_address = instruction_start + deopt_data->Pc(i)->value();
    // There is room enough to write a long call instruction because we pad
    // LLazyBailout instructions with nops if necessary.
    CodePatcher patcher(call_address, Assembler::kCallInstructionLength);
    patcher.masm()->Call(GetDeoptimizationEntry(isolate, i, LAZY),
                         RelocInfo::NONE64);
    ASSERT(prev_call_address == NULL ||
           call_address >= prev_call_address + patch_size());
    ASSERT(call_address + patch_size() <= code->instruction_end());
#ifdef DEBUG
    prev_call_address = call_address;
#endif
  }

  // Add the deoptimizing code to the list.
  DeoptimizingCodeListNode* node = new DeoptimizingCodeListNode(code);
  DeoptimizerData* data = isolate->deoptimizer_data();
  node->set_next(data->deoptimizing_code_list_);
  data->deoptimizing_code_list_ = node;

  // We might be in the middle of incremental marking with compaction.
  // Tell collector to treat this code object in a special way and
  // ignore all slots that might have been recorded on it.
  isolate->heap()->mark_compact_collector()->InvalidateCode(code);

  ReplaceCodeForRelatedFunctions(function, code);

  if (FLAG_trace_deopt) {
    PrintF("[forced deoptimization: ");
    function->PrintName();
    PrintF(" / %" V8PRIxPTR "]\n", reinterpret_cast<intptr_t>(function));
  }
}


static const byte kJnsInstruction = 0x79;
static const byte kJnsOffset = 0x1d;
static const byte kCallInstruction = 0xe8;
static const byte kNopByteOne = 0x66;
static const byte kNopByteTwo = 0x90;

// The back edge bookkeeping code matches the pattern:
//
//     add <profiling_counter>, <-delta>
//     jns ok
//     call <stack guard>
//   ok:
//
// We will patch away the branch so the code is:
//
//     add <profiling_counter>, <-delta>  ;; Not changed
//     nop
//     nop
//     call <on-stack replacment>
//   ok:

void Deoptimizer::PatchInterruptCodeAt(Code* unoptimized_code,
                                       Address pc_after,
                                       Code* interrupt_code,
                                       Code* replacement_code) {
  ASSERT(!InterruptCodeIsPatched(unoptimized_code,
                                 pc_after,
                                 interrupt_code,
                                 replacement_code));
  // Turn the jump into nops.
  Address call_target_address = pc_after - kIntSize;
  *(call_target_address - 3) = kNopByteOne;
  *(call_target_address - 2) = kNopByteTwo;
  // Replace the call address.
  Assembler::set_target_address_at(call_target_address,
                                   replacement_code->entry());

  unoptimized_code->GetHeap()->incremental_marking()->RecordCodeTargetPatch(
      unoptimized_code, call_target_address, replacement_code);
}


void Deoptimizer::RevertInterruptCodeAt(Code* unoptimized_code,
                                        Address pc_after,
                                        Code* interrupt_code,
                                        Code* replacement_code) {
  ASSERT(InterruptCodeIsPatched(unoptimized_code,
                                pc_after,
                                interrupt_code,
                                replacement_code));
  // Restore the original jump.
  Address call_target_address = pc_after - kIntSize;
  *(call_target_address - 3) = kJnsInstruction;
  *(call_target_address - 2) = kJnsOffset;
  // Restore the original call address.
  Assembler::set_target_address_at(call_target_address,
                                   interrupt_code->entry());

  interrupt_code->GetHeap()->incremental_marking()->RecordCodeTargetPatch(
      unoptimized_code, call_target_address, interrupt_code);
}


#ifdef DEBUG
bool Deoptimizer::InterruptCodeIsPatched(Code* unoptimized_code,
                                         Address pc_after,
                                         Code* interrupt_code,
                                         Code* replacement_code) {
  Address call_target_address = pc_after - kIntSize;
  ASSERT_EQ(kCallInstruction, *(call_target_address - 1));
  if (*(call_target_address - 3) == kNopByteOne) {
    ASSERT(replacement_code->entry() ==
           Assembler::target_address_at(call_target_address));
    ASSERT_EQ(kNopByteTwo,      *(call_target_address - 2));
    return true;
  } else {
    ASSERT_EQ(interrupt_code->entry(),
              Assembler::target_address_at(call_target_address));
    ASSERT_EQ(kJnsInstruction,  *(call_target_address - 3));
    ASSERT_EQ(kJnsOffset,       *(call_target_address - 2));
    return false;
  }
}
#endif  // DEBUG


static int LookupBailoutId(DeoptimizationInputData* data, BailoutId ast_id) {
  ByteArray* translations = data->TranslationByteArray();
  int length = data->DeoptCount();
  for (int i = 0; i < length; i++) {
    if (data->AstId(i) == ast_id) {
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
      compiled_code_->deoptimization_data());
  unsigned ast_id = data->OsrAstId()->value();
  // TODO(kasperl): This should not be the bailout_id_. It should be
  // the ast id. Confusing.
  ASSERT(bailout_id_ == ast_id);

  int bailout_id = LookupBailoutId(data, BailoutId(ast_id));
  unsigned translation_index = data->TranslationIndex(bailout_id)->value();
  ByteArray* translations = data->TranslationByteArray();

  TranslationIterator iterator(translations, translation_index);
  Translation::Opcode opcode =
      static_cast<Translation::Opcode>(iterator.Next());
  ASSERT(Translation::BEGIN == opcode);
  USE(opcode);
  int count = iterator.Next();
  iterator.Skip(1);  // Drop JS frame count.
  ASSERT(count == 1);
  USE(count);

  opcode = static_cast<Translation::Opcode>(iterator.Next());
  USE(opcode);
  ASSERT(Translation::JS_FRAME == opcode);
  unsigned node_id = iterator.Next();
  USE(node_id);
  ASSERT(node_id == ast_id);
  int closure_id = iterator.Next();
  USE(closure_id);
  ASSERT_EQ(Translation::kSelfLiteralId, closure_id);
  unsigned height = iterator.Next();
  unsigned height_in_bytes = height * kPointerSize;
  USE(height_in_bytes);

  unsigned fixed_size = ComputeFixedSize(function_);
  unsigned input_frame_size = input_->GetFrameSize();
  ASSERT(fixed_size + height_in_bytes == input_frame_size);

  unsigned stack_slot_size = compiled_code_->stack_slots() * kPointerSize;
  unsigned outgoing_height = data->ArgumentsStackHeight(bailout_id)->value();
  unsigned outgoing_size = outgoing_height * kPointerSize;
  unsigned output_frame_size = fixed_size + stack_slot_size + outgoing_size;
  ASSERT(outgoing_size == 0);  // OSR does not happen in the middle of a call.

  if (FLAG_trace_osr) {
    PrintF("[on-stack replacement: begin 0x%08" V8PRIxPTR " ",
           reinterpret_cast<intptr_t>(function_));
    PrintFunctionName();
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
  output_[0]->SetFrameType(StackFrame::JAVA_SCRIPT);

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
  for (int i = StandardFrameConstants::kCallerPCOffset;
       ok && i >=  StandardFrameConstants::kMarkerOffset;
       i -= kPointerSize) {
    intptr_t input_value = input_->GetFrameSlot(input_offset);
    if (FLAG_trace_osr) {
      const char* name = "UNKNOWN";
      switch (i) {
        case StandardFrameConstants::kCallerPCOffset:
          name = "caller's pc";
          break;
        case StandardFrameConstants::kCallerFPOffset:
          name = "fp";
          break;
        case StandardFrameConstants::kContextOffset:
          name = "context";
          break;
        case StandardFrameConstants::kMarkerOffset:
          name = "function";
          break;
      }
      PrintF("    [rsp + %d] <- 0x%08" V8PRIxPTR " ; [rsp + %d] "
             "(fixed part - %s)\n",
             output_offset,
             input_value,
             input_offset,
             name);
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
    // Set up the frame pointer and the context pointer.
    output_[0]->SetRegister(rbp.code(), input_->GetRegister(rbp.code()));
    output_[0]->SetRegister(rsi.code(), input_->GetRegister(rsi.code()));

    unsigned pc_offset = data->OsrPcOffset()->value();
    intptr_t pc = reinterpret_cast<intptr_t>(
        compiled_code_->entry() + pc_offset);
    output_[0]->SetPc(pc);
  }
  Code* continuation =
      function_->GetIsolate()->builtins()->builtin(Builtins::kNotifyOSR);
  output_[0]->SetContinuation(
      reinterpret_cast<intptr_t>(continuation->entry()));

  if (FLAG_trace_osr) {
    PrintF("[on-stack replacement translation %s: 0x%08" V8PRIxPTR " ",
           ok ? "finished" : "aborted",
           reinterpret_cast<intptr_t>(function_));
    PrintFunctionName();
    PrintF(" => pc=0x%0" V8PRIxPTR "]\n", output_[0]->GetPc());
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
    input_->SetFrameSlot(i, Memory::uint64_at(tos + i));
  }
}


void Deoptimizer::SetPlatformCompiledStubRegisters(
    FrameDescription* output_frame, CodeStubInterfaceDescriptor* descriptor) {
  intptr_t handler =
      reinterpret_cast<intptr_t>(descriptor->deoptimization_handler_);
  int params = descriptor->register_param_count_;
  if (descriptor->stack_parameter_count_ != NULL) {
    params++;
  }
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


#define __ masm()->

void Deoptimizer::EntryGenerator::Generate() {
  GeneratePrologue();

  // Save all general purpose registers before messing with them.
  const int kNumberOfRegisters = Register::kNumRegisters;

  const int kDoubleRegsSize = kDoubleSize *
      XMMRegister::NumAllocatableRegisters();
  __ subq(rsp, Immediate(kDoubleRegsSize));

  for (int i = 0; i < XMMRegister::NumAllocatableRegisters(); ++i) {
    XMMRegister xmm_reg = XMMRegister::FromAllocationIndex(i);
    int offset = i * kDoubleSize;
    __ movsd(Operand(rsp, offset), xmm_reg);
  }

  // We push all registers onto the stack, even though we do not need
  // to restore all later.
  for (int i = 0; i < kNumberOfRegisters; i++) {
    Register r = Register::from_code(i);
    __ push(r);
  }

  const int kSavedRegistersAreaSize = kNumberOfRegisters * kPointerSize +
                                      kDoubleRegsSize;

  // We use this to keep the value of the fifth argument temporarily.
  // Unfortunately we can't store it directly in r8 (used for passing
  // this on linux), since it is another parameter passing register on windows.
  Register arg5 = r11;

  // Get the bailout id from the stack.
  __ movq(arg_reg_3, Operand(rsp, kSavedRegistersAreaSize));

  // Get the address of the location in the code object if possible
  // and compute the fp-to-sp delta in register arg5.
  if (type() == EAGER || type() == SOFT) {
    __ Set(arg_reg_4, 0);
    __ lea(arg5, Operand(rsp, kSavedRegistersAreaSize + 1 * kPointerSize));
  } else {
    __ movq(arg_reg_4,
            Operand(rsp, kSavedRegistersAreaSize + 1 * kPointerSize));
    __ lea(arg5, Operand(rsp, kSavedRegistersAreaSize + 2 * kPointerSize));
  }

  __ subq(arg5, rbp);
  __ neg(arg5);

  // Allocate a new deoptimizer object.
  __ PrepareCallCFunction(6);
  __ movq(rax, Operand(rbp, JavaScriptFrameConstants::kFunctionOffset));
  __ movq(arg_reg_1, rax);
  __ Set(arg_reg_2, type());
  // Args 3 and 4 are already in the right registers.

  // On windows put the arguments on the stack (PrepareCallCFunction
  // has created space for this). On linux pass the arguments in r8 and r9.
#ifdef _WIN64
  __ movq(Operand(rsp, 4 * kPointerSize), arg5);
  __ LoadAddress(arg5, ExternalReference::isolate_address(isolate()));
  __ movq(Operand(rsp, 5 * kPointerSize), arg5);
#else
  __ movq(r8, arg5);
  __ LoadAddress(r9, ExternalReference::isolate_address(isolate()));
#endif

  { AllowExternalCallThatCantCauseGC scope(masm());
    __ CallCFunction(ExternalReference::new_deoptimizer_function(isolate()), 6);
  }
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
  for (int i = 0; i < XMMRegister::NumAllocatableRegisters(); i++) {
    int dst_offset = i * kDoubleSize + double_regs_offset;
    __ pop(Operand(rbx, dst_offset));
  }

  // Remove the bailout id from the stack.
  if (type() == EAGER || type() == SOFT) {
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
  Label pop_loop_header;
  __ jmp(&pop_loop_header);
  Label pop_loop;
  __ bind(&pop_loop);
  __ pop(Operand(rdx, 0));
  __ addq(rdx, Immediate(sizeof(intptr_t)));
  __ bind(&pop_loop_header);
  __ cmpq(rcx, rsp);
  __ j(not_equal, &pop_loop);

  // Compute the output frame in the deoptimizer.
  __ push(rax);
  __ PrepareCallCFunction(2);
  __ movq(arg_reg_1, rax);
  __ LoadAddress(arg_reg_2, ExternalReference::isolate_address(isolate()));
  {
    AllowExternalCallThatCantCauseGC scope(masm());
    __ CallCFunction(
        ExternalReference::compute_output_frames_function(isolate()), 2);
  }
  __ pop(rax);

  // Replace the current frame with the output frames.
  Label outer_push_loop, inner_push_loop,
      outer_loop_header, inner_loop_header;
  // Outer loop state: rax = current FrameDescription**, rdx = one past the
  // last FrameDescription**.
  __ movl(rdx, Operand(rax, Deoptimizer::output_count_offset()));
  __ movq(rax, Operand(rax, Deoptimizer::output_offset()));
  __ lea(rdx, Operand(rax, rdx, times_8, 0));
  __ jmp(&outer_loop_header);
  __ bind(&outer_push_loop);
  // Inner loop state: rbx = current FrameDescription*, rcx = loop index.
  __ movq(rbx, Operand(rax, 0));
  __ movq(rcx, Operand(rbx, FrameDescription::frame_size_offset()));
  __ jmp(&inner_loop_header);
  __ bind(&inner_push_loop);
  __ subq(rcx, Immediate(sizeof(intptr_t)));
  __ push(Operand(rbx, rcx, times_1, FrameDescription::frame_content_offset()));
  __ bind(&inner_loop_header);
  __ testq(rcx, rcx);
  __ j(not_zero, &inner_push_loop);
  __ addq(rax, Immediate(kPointerSize));
  __ bind(&outer_loop_header);
  __ cmpq(rax, rdx);
  __ j(below, &outer_push_loop);

  for (int i = 0; i < XMMRegister::NumAllocatableRegisters(); ++i) {
    XMMRegister xmm_reg = XMMRegister::FromAllocationIndex(i);
    int src_offset = i * kDoubleSize + double_regs_offset;
    __ movsd(xmm_reg, Operand(rbx, src_offset));
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
    Register r = Register::from_code(i);
    // Do not restore rsp, simply pop the value into the next register
    // and overwrite this afterwards.
    if (r.is(rsp)) {
      ASSERT(i > 0);
      r = Register::from_code(i - 1);
    }
    __ pop(r);
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
    __ push_imm32(i);
    __ jmp(&done);
    ASSERT(masm()->pc_offset() - start == table_entry_size_);
  }
  __ bind(&done);
}

#undef __


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_X64
