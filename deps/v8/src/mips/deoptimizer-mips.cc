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

#include "codegen.h"
#include "deoptimizer.h"
#include "full-codegen.h"
#include "safepoint-table.h"

namespace v8 {
namespace internal {


int Deoptimizer::patch_size() {
  const int kCallInstructionSizeInWords = 4;
  return kCallInstructionSizeInWords * Assembler::kInstrSize;
}


void Deoptimizer::DeoptimizeFunctionWithPreparedFunctionList(
    JSFunction* function) {
  Isolate* isolate = function->GetIsolate();
  HandleScope scope(isolate);
  AssertNoAllocation no_allocation;

  ASSERT(function->IsOptimized());
  ASSERT(function->FunctionsInFunctionListShareSameCode());

  // The optimized code is going to be patched, so we cannot use it
  // any more.  Play safe and reset the whole cache.
  function->shared()->ClearOptimizedCodeMap();

  // Get the optimized code.
  Code* code = function->code();
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
    int call_size_in_bytes = MacroAssembler::CallSize(deopt_entry,
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
    PrintF(" / %x]\n", reinterpret_cast<uint32_t>(function));
#ifdef DEBUG
    if (FLAG_print_code) {
      code->PrintLn();
    }
#endif
  }
}


// This structure comes from FullCodeGenerator::EmitBackEdgeBookkeeping.
// The back edge bookkeeping code matches the pattern:
//
// sltu at, sp, t0 / slt at, a3, zero_reg (in case of count based interrupts)
// beq at, zero_reg, ok
// lui t9, <interrupt stub address> upper
// ori t9, <interrupt stub address> lower
// jalr t9
// nop
// ok-label ----- pc_after points here
//
// We patch the code to the following form:
//
// addiu at, zero_reg, 1
// beq at, zero_reg, ok  ;; Not changed
// lui t9, <on-stack replacement address> upper
// ori t9, <on-stack replacement address> lower
// jalr t9  ;; Not changed
// nop  ;; Not changed
// ok-label ----- pc_after points here

void Deoptimizer::PatchInterruptCodeAt(Code* unoptimized_code,
                                        Address pc_after,
                                        Code* interrupt_code,
                                        Code* replacement_code) {
  ASSERT(!InterruptCodeIsPatched(unoptimized_code,
                                 pc_after,
                                 interrupt_code,
                                 replacement_code));
  static const int kInstrSize = Assembler::kInstrSize;
  // Replace the sltu instruction with load-imm 1 to at, so beq is not taken.
  CodePatcher patcher(pc_after - 6 * kInstrSize, 1);
  patcher.masm()->addiu(at, zero_reg, 1);
  // Replace the stack check address in the load-immediate (lui/ori pair)
  // with the entry address of the replacement code.
  Assembler::set_target_address_at(pc_after - 4 * kInstrSize,
                                   replacement_code->entry());

  unoptimized_code->GetHeap()->incremental_marking()->RecordCodeTargetPatch(
      unoptimized_code, pc_after - 4 * kInstrSize, replacement_code);
}


void Deoptimizer::RevertInterruptCodeAt(Code* unoptimized_code,
                                        Address pc_after,
                                        Code* interrupt_code,
                                        Code* replacement_code) {
  ASSERT(InterruptCodeIsPatched(unoptimized_code,
                                 pc_after,
                                 interrupt_code,
                                 replacement_code));
  static const int kInstrSize = Assembler::kInstrSize;
  // Restore the sltu instruction so beq can be taken again.
  CodePatcher patcher(pc_after - 6 * kInstrSize, 1);
  patcher.masm()->slt(at, a3, zero_reg);
  // Restore the original call address.
  Assembler::set_target_address_at(pc_after - 4 * kInstrSize,
                                   interrupt_code->entry());

  interrupt_code->GetHeap()->incremental_marking()->RecordCodeTargetPatch(
      unoptimized_code, pc_after - 4 * kInstrSize, interrupt_code);
}


#ifdef DEBUG
bool Deoptimizer::InterruptCodeIsPatched(Code* unoptimized_code,
                                         Address pc_after,
                                         Code* interrupt_code,
                                         Code* replacement_code) {
  static const int kInstrSize = Assembler::kInstrSize;
  ASSERT(Assembler::IsBeq(Assembler::instr_at(pc_after - 5 * kInstrSize)));
  if (Assembler::IsAddImmediate(
      Assembler::instr_at(pc_after - 6 * kInstrSize))) {
    ASSERT(reinterpret_cast<uint32_t>(
        Assembler::target_address_at(pc_after - 4 * kInstrSize)) ==
        reinterpret_cast<uint32_t>(replacement_code->entry()));
    return true;
  } else {
    ASSERT(reinterpret_cast<uint32_t>(
        Assembler::target_address_at(pc_after - 4 * kInstrSize)) ==
        reinterpret_cast<uint32_t>(interrupt_code->entry()));
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
  for (int i =  StandardFrameConstants::kCallerPCOffset;
       ok && i >=  StandardFrameConstants::kMarkerOffset;
       i -= kPointerSize) {
    uint32_t input_value = input_->GetFrameSlot(input_offset);
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
      PrintF("    [sp + %d] <- 0x%08x ; [sp + %d] (fixed part - %s)\n",
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
    output_[0]->SetPc(reinterpret_cast<uint32_t>(from_));
  } else {
    // Set up the frame pointer and the context pointer.
    output_[0]->SetRegister(fp.code(), input_->GetRegister(fp.code()));
    output_[0]->SetRegister(cp.code(), input_->GetRegister(cp.code()));

    unsigned pc_offset = data->OsrPcOffset()->value();
    uint32_t pc = reinterpret_cast<uint32_t>(
        compiled_code_->entry() + pc_offset);
    output_[0]->SetPc(pc);
  }
  Code* continuation = isolate_->builtins()->builtin(Builtins::kNotifyOSR);
  output_[0]->SetContinuation(
      reinterpret_cast<uint32_t>(continuation->entry()));

  if (FLAG_trace_osr) {
    PrintF("[on-stack replacement translation %s: 0x%08" V8PRIxPTR " ",
           ok ? "finished" : "aborted",
           reinterpret_cast<intptr_t>(function_));
    function_->PrintName();
    PrintF(" => pc=0x%0x]\n", output_[0]->GetPc());
  }
}


// This code is very similar to ia32/arm code, but relies on register names
// (fp, sp) and how the frame is laid out.
void Deoptimizer::DoComputeJSFrame(TranslationIterator* iterator,
                                   int frame_index) {
  // Read the ast node id, function, and frame height for this output frame.
  BailoutId node_id = BailoutId(iterator->Next());
  JSFunction* function;
  if (frame_index != 0) {
    function = JSFunction::cast(ComputeLiteral(iterator->Next()));
  } else {
    int closure_id = iterator->Next();
    USE(closure_id);
    ASSERT_EQ(Translation::kSelfLiteralId, closure_id);
    function = function_;
  }
  unsigned height = iterator->Next();
  unsigned height_in_bytes = height * kPointerSize;
  if (trace_) {
    PrintF("  translating ");
    function->PrintName();
    PrintF(" => node=%d, height=%d\n", node_id.ToInt(), height_in_bytes);
  }

  // The 'fixed' part of the frame consists of the incoming parameters and
  // the part described by JavaScriptFrameConstants.
  unsigned fixed_frame_size = ComputeFixedSize(function);
  unsigned input_frame_size = input_->GetFrameSize();
  unsigned output_frame_size = height_in_bytes + fixed_frame_size;

  // Allocate and store the output frame description.
  FrameDescription* output_frame =
      new(output_frame_size) FrameDescription(output_frame_size, function);
  output_frame->SetFrameType(StackFrame::JAVA_SCRIPT);

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
        input_->GetRegister(fp.code()) - (2 * kPointerSize) - height_in_bytes;
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
  if (trace_) {
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
  ASSERT(!is_bottommost || input_->GetRegister(fp.code()) == fp_value);
  output_frame->SetFp(fp_value);
  if (is_topmost) {
    output_frame->SetRegister(fp.code(), fp_value);
  }
  if (trace_) {
    PrintF("    0x%08x: [top + %d] <- 0x%08x ; caller's fp\n",
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
  output_frame->SetContext(value);
  if (is_topmost) output_frame->SetRegister(cp.code(), value);
  if (trace_) {
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
  if (trace_) {
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
  if (is_topmost && bailout_type_ != DEBUGGER) {
    Builtins* builtins = isolate_->builtins();
    Code* continuation = (bailout_type_ == EAGER)
        ? builtins->builtin(Builtins::kNotifyDeoptimized)
        : builtins->builtin(Builtins::kNotifyLazyDeoptimized);
    output_frame->SetContinuation(
        reinterpret_cast<uint32_t>(continuation->entry()));
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
    FrameDescription* output_frame, CodeStubInterfaceDescriptor* descriptor) {
  ApiFunction function(descriptor->deoptimization_handler_);
  ExternalReference xref(&function, ExternalReference::BUILTIN_CALL, isolate_);
  intptr_t handler = reinterpret_cast<intptr_t>(xref.address());
  int params = descriptor->register_param_count_;
  if (descriptor->stack_parameter_count_ != NULL) {
    params++;
  }
  output_frame->SetRegister(s0.code(), params);
  output_frame->SetRegister(s1.code(), (params - 1) * kPointerSize);
  output_frame->SetRegister(s2.code(), handler);
}


void Deoptimizer::CopyDoubleRegisters(FrameDescription* output_frame) {
  for (int i = 0; i < DoubleRegister::kMaxNumRegisters; ++i) {
    double double_value = input_->GetDoubleRegister(i);
    output_frame->SetDoubleRegister(i, double_value);
  }
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

  // Get the address of the location in the code object if possible (a3) (return
  // address for lazy deoptimization) and compute the fp-to-sp delta in
  // register t0.
  if (type() == EAGER) {
    __ mov(a3, zero_reg);
    // Correct one word for bailout id.
    __ Addu(t0, sp, Operand(kSavedRegistersAreaSize + (1 * kPointerSize)));
  } else if (type() == OSR) {
    __ mov(a3, ra);
    // Correct one word for bailout id.
    __ Addu(t0, sp, Operand(kSavedRegistersAreaSize + (1 * kPointerSize)));
  } else {
    __ mov(a3, ra);
    // Correct two words for bailout id and return address.
    __ Addu(t0, sp, Operand(kSavedRegistersAreaSize + (2 * kPointerSize)));
  }

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
  ASSERT(Register::kNumRegisters == kNumberOfRegisters);
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

  // Remove the bailout id, eventually return address, and the saved registers
  // from the stack.
  if (type() == EAGER || type() == OSR) {
    __ Addu(sp, sp, Operand(kSavedRegistersAreaSize + (1 * kPointerSize)));
  } else {
    __ Addu(sp, sp, Operand(kSavedRegistersAreaSize + (2 * kPointerSize)));
  }

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
  __ Branch(&pop_loop_header);
  __ bind(&pop_loop);
  __ pop(t0);
  __ sw(t0, MemOperand(a3, 0));
  __ addiu(a3, a3, sizeof(uint32_t));
  __ bind(&pop_loop_header);
  __ Branch(&pop_loop, ne, a2, Operand(sp));

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
  __ Branch(&inner_push_loop, ne, a3, Operand(zero_reg));

  __ Addu(t0, t0, Operand(kPointerSize));
  __ bind(&outer_loop_header);
  __ Branch(&outer_push_loop, lt, t0, Operand(a1));

  __ lw(a1, MemOperand(a0, Deoptimizer::input_offset()));
  for (int i = 0; i < FPURegister::kMaxNumAllocatableRegisters; ++i) {
    const FPURegister fpu_reg = FPURegister::FromAllocationIndex(i);
    int src_offset = i * kDoubleSize + double_regs_offset;
    __ ldc1(fpu_reg, MemOperand(a1, src_offset));
  }

  // Push state, pc, and continuation from the last output frame.
  if (type() != OSR) {
    __ lw(t2, MemOperand(a2, FrameDescription::state_offset()));
    __ push(t2);
  }

  __ lw(t2, MemOperand(a2, FrameDescription::pc_offset()));
  __ push(t2);
  __ lw(t2, MemOperand(a2, FrameDescription::continuation_offset()));
  __ push(t2);


  // Technically restoring 'at' should work unless zero_reg is also restored
  // but it's safer to check for this.
  ASSERT(!(at.bit() & restored_regs));
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
const int Deoptimizer::table_entry_size_ = 9 * Assembler::kInstrSize;

void Deoptimizer::TableEntryGenerator::GeneratePrologue() {
  Assembler::BlockTrampolinePoolScope block_trampoline_pool(masm());

  // Create a sequence of deoptimization entries. Note that any
  // registers may be still live.
  Label table_start;
  __ bind(&table_start);
  for (int i = 0; i < count(); i++) {
    Label start;
    __ bind(&start);
    if (type() != EAGER) {
      // Emulate ia32 like call by pushing return address to stack.
      __ addiu(sp, sp, -2 * kPointerSize);
      __ sw(ra, MemOperand(sp, 1 * kPointerSize));
    } else {
      __ addiu(sp, sp, -1 * kPointerSize);
    }
    // Jump over the remaining deopt entries (including this one).
    // This code is always reached by calling Jump, which puts the target (label
    // start) into t9.
    const int remaining_entries = (count() - i) * table_entry_size_;
    __ Addu(t9, t9, remaining_entries);
    // 'at' was clobbered so we can only load the current entry value here.
    __ li(at, i);
    __ jr(t9);  // Expose delay slot.
    __ sw(at, MemOperand(sp, 0 * kPointerSize));  // In the delay slot.

    // Pad the rest of the code.
    while (table_entry_size_ > (masm()->SizeOfCodeGeneratedSince(&start))) {
      __ nop();
    }

    ASSERT_EQ(table_entry_size_, masm()->SizeOfCodeGeneratedSince(&start));
  }

  ASSERT_EQ(masm()->SizeOfCodeGeneratedSince(&table_start),
      count() * table_entry_size_);
}

#undef __


} }  // namespace v8::internal
