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

const int Deoptimizer::table_entry_size_ = 16;


int Deoptimizer::patch_size() {
  const int kCallInstructionSizeInWords = 3;
  return kCallInstructionSizeInWords * Assembler::kInstrSize;
}


void Deoptimizer::DeoptimizeFunction(JSFunction* function) {
  HandleScope scope;
  AssertNoAllocation no_allocation;

  if (!function->IsOptimized()) return;

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
    Address deopt_entry = GetDeoptimizationEntry(i, LAZY);
    int call_size_in_bytes = MacroAssembler::CallSize(deopt_entry,
                                                      RelocInfo::NONE);
    int call_size_in_words = call_size_in_bytes / Assembler::kInstrSize;
    ASSERT(call_size_in_bytes % Assembler::kInstrSize == 0);
    ASSERT(call_size_in_bytes <= patch_size());
    CodePatcher patcher(call_address, call_size_in_words);
    patcher.masm()->Call(deopt_entry, RelocInfo::NONE);
    ASSERT(prev_call_address == NULL ||
           call_address >= prev_call_address + patch_size());
    ASSERT(call_address + patch_size() <= code->instruction_end());

#ifdef DEBUG
    prev_call_address = call_address;
#endif
  }

  // Add the deoptimizing code to the list.
  DeoptimizingCodeListNode* node = new DeoptimizingCodeListNode(code);
  DeoptimizerData* data = code->GetIsolate()->deoptimizer_data();
  node->set_next(data->deoptimizing_code_list_);
  data->deoptimizing_code_list_ = node;

  // Set the code for the function to non-optimized version.
  function->ReplaceCode(function->shared()->code());

  if (FLAG_trace_deopt) {
    PrintF("[forced deoptimization: ");
    function->PrintName();
    PrintF(" / %x]\n", reinterpret_cast<uint32_t>(function));
  }
}


void Deoptimizer::PatchStackCheckCodeAt(Address pc_after,
                                        Code* check_code,
                                        Code* replacement_code) {
  const int kInstrSize = Assembler::kInstrSize;
  // The call of the stack guard check has the following form:
  //  e1 5d 00 0c       cmp sp, <limit>
  //  2a 00 00 01       bcs ok
  //  e5 9f c? ??       ldr ip, [pc, <stack guard address>]
  //  e1 2f ff 3c       blx ip
  ASSERT(Memory::int32_at(pc_after - kInstrSize) ==
      (al | B24 | B21 | 15*B16 | 15*B12 | 15*B8 | BLX | ip.code()));
  ASSERT(Assembler::IsLdrPcImmediateOffset(
      Assembler::instr_at(pc_after - 2 * kInstrSize)));

  // We patch the code to the following form:
  //  e1 5d 00 0c       cmp sp, <limit>
  //  e1 a0 00 00       mov r0, r0 (NOP)
  //  e5 9f c? ??       ldr ip, [pc, <on-stack replacement address>]
  //  e1 2f ff 3c       blx ip
  // and overwrite the constant containing the
  // address of the stack check stub.

  // Replace conditional jump with NOP.
  CodePatcher patcher(pc_after - 3 * kInstrSize, 1);
  patcher.masm()->nop();

  // Replace the stack check address in the constant pool
  // with the entry address of the replacement code.
  uint32_t stack_check_address_offset = Memory::uint16_at(pc_after -
      2 * kInstrSize) & 0xfff;
  Address stack_check_address_pointer = pc_after + stack_check_address_offset;
  ASSERT(Memory::uint32_at(stack_check_address_pointer) ==
         reinterpret_cast<uint32_t>(check_code->entry()));
  Memory::uint32_at(stack_check_address_pointer) =
      reinterpret_cast<uint32_t>(replacement_code->entry());
}


void Deoptimizer::RevertStackCheckCodeAt(Address pc_after,
                                         Code* check_code,
                                         Code* replacement_code) {
  const int kInstrSize = Assembler::kInstrSize;
  ASSERT(Memory::uint32_at(pc_after - kInstrSize) == 0xe12fff3c);
  ASSERT(Memory::uint8_at(pc_after - kInstrSize - 1) == 0xe5);
  ASSERT(Memory::uint8_at(pc_after - kInstrSize - 2) == 0x9f);

  // Replace NOP with conditional jump.
  CodePatcher patcher(pc_after - 3 * kInstrSize, 1);
  patcher.masm()->b(+4, cs);

  // Replace the stack check address in the constant pool
  // with the entry address of the replacement code.
  uint32_t stack_check_address_offset = Memory::uint16_at(pc_after -
      2 * kInstrSize) & 0xfff;
  Address stack_check_address_pointer = pc_after + stack_check_address_offset;
  ASSERT(Memory::uint32_at(stack_check_address_pointer) ==
         reinterpret_cast<uint32_t>(replacement_code->entry()));
  Memory::uint32_at(stack_check_address_pointer) =
      reinterpret_cast<uint32_t>(check_code->entry());
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
#ifdef DEBUG
  output_[0]->SetKind(Code::OPTIMIZED_FUNCTION);
#endif

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
    // Setup the frame pointer and the context pointer.
    output_[0]->SetRegister(fp.code(), input_->GetRegister(fp.code()));
    output_[0]->SetRegister(cp.code(), input_->GetRegister(cp.code()));

    unsigned pc_offset = data->OsrPcOffset()->value();
    uint32_t pc = reinterpret_cast<uint32_t>(
        optimized_code_->entry() + pc_offset);
    output_[0]->SetPc(pc);
  }
  Code* continuation = isolate_->builtins()->builtin(Builtins::kNotifyOSR);
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


// This code is very similar to ia32 code, but relies on register names (fp, sp)
// and how the frame is laid out.
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
#ifdef DEBUG
  output_frame->SetKind(Code::FUNCTION);
#endif

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
  ASSERT(!is_bottommost || input_->GetRegister(fp.code()) == fp_value);
  output_frame->SetFp(fp_value);
  if (is_topmost) {
    output_frame->SetRegister(fp.code(), fp_value);
  }
  if (FLAG_trace_deopt) {
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
  if (is_topmost) {
    output_frame->SetRegister(cp.code(), value);
  }
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
  if (is_topmost) {
    output_frame->SetRegister(pc.code(), pc_value);
  }

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
  for (int i = 0; i < DoubleRegister::kNumAllocatableRegisters; i++) {
    input_->SetDoubleRegister(i, 0.0);
  }

  // Fill the frame content from the actual data on the frame.
  for (unsigned i = 0; i < input_->GetFrameSize(); i += kPointerSize) {
    input_->SetFrameSlot(i, Memory::uint32_at(tos + i));
  }
}


#define __ masm()->

// This code tries to be close to ia32 code so that any changes can be
// easily ported.
void Deoptimizer::EntryGenerator::Generate() {
  GeneratePrologue();

  Isolate* isolate = masm()->isolate();

  CpuFeatures::Scope scope(VFP3);
  // Save all general purpose registers before messing with them.
  const int kNumberOfRegisters = Register::kNumRegisters;

  // Everything but pc, lr and ip which will be saved but not restored.
  RegList restored_regs = kJSCallerSaved | kCalleeSaved | ip.bit();

  const int kDoubleRegsSize =
      kDoubleSize * DwVfpRegister::kNumAllocatableRegisters;

  // Save all VFP registers before messing with them.
  DwVfpRegister first = DwVfpRegister::FromAllocationIndex(0);
  DwVfpRegister last =
      DwVfpRegister::FromAllocationIndex(
          DwVfpRegister::kNumAllocatableRegisters - 1);
  ASSERT(last.code() > first.code());
  ASSERT((last.code() - first.code()) ==
      (DwVfpRegister::kNumAllocatableRegisters - 1));
#ifdef DEBUG
  for (int i = 0; i <= (DwVfpRegister::kNumAllocatableRegisters - 1); i++) {
    ASSERT((DwVfpRegister::FromAllocationIndex(i).code() <= last.code()) &&
           (DwVfpRegister::FromAllocationIndex(i).code() >= first.code()));
  }
#endif
  __ vstm(db_w, sp, first, last);

  // Push all 16 registers (needed to populate FrameDescription::registers_).
  // TODO(1588) Note that using pc with stm is deprecated, so we should perhaps
  // handle this a bit differently.
  __ stm(db_w, sp, restored_regs  | sp.bit() | lr.bit() | pc.bit());

  const int kSavedRegistersAreaSize =
      (kNumberOfRegisters * kPointerSize) + kDoubleRegsSize;

  // Get the bailout id from the stack.
  __ ldr(r2, MemOperand(sp, kSavedRegistersAreaSize));

  // Get the address of the location in the code object if possible (r3) (return
  // address for lazy deoptimization) and compute the fp-to-sp delta in
  // register r4.
  if (type() == EAGER) {
    __ mov(r3, Operand(0));
    // Correct one word for bailout id.
    __ add(r4, sp, Operand(kSavedRegistersAreaSize + (1 * kPointerSize)));
  } else if (type() == OSR) {
    __ mov(r3, lr);
    // Correct one word for bailout id.
    __ add(r4, sp, Operand(kSavedRegistersAreaSize + (1 * kPointerSize)));
  } else {
    __ mov(r3, lr);
    // Correct two words for bailout id and return address.
    __ add(r4, sp, Operand(kSavedRegistersAreaSize + (2 * kPointerSize)));
  }
  __ sub(r4, fp, r4);

  // Allocate a new deoptimizer object.
  // Pass four arguments in r0 to r3 and fifth argument on stack.
  __ PrepareCallCFunction(6, r5);
  __ ldr(r0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ mov(r1, Operand(type()));  // bailout type,
  // r2: bailout id already loaded.
  // r3: code address or 0 already loaded.
  __ str(r4, MemOperand(sp, 0 * kPointerSize));  // Fp-to-sp delta.
  __ mov(r5, Operand(ExternalReference::isolate_address()));
  __ str(r5, MemOperand(sp, 1 * kPointerSize));  // Isolate.
  // Call Deoptimizer::New().
  __ CallCFunction(ExternalReference::new_deoptimizer_function(isolate), 6);

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
  // double_registers_[DoubleRegister::kNumAllocatableRegisters]
  int double_regs_offset = FrameDescription::double_registers_offset();
  for (int i = 0; i < DwVfpRegister::kNumAllocatableRegisters; ++i) {
    int dst_offset = i * kDoubleSize + double_regs_offset;
    int src_offset = i * kDoubleSize + kNumberOfRegisters * kPointerSize;
    __ vldr(d0, sp, src_offset);
    __ vstr(d0, r1, dst_offset);
  }

  // Remove the bailout id, eventually return address, and the saved registers
  // from the stack.
  if (type() == EAGER || type() == OSR) {
    __ add(sp, sp, Operand(kSavedRegistersAreaSize + (1 * kPointerSize)));
  } else {
    __ add(sp, sp, Operand(kSavedRegistersAreaSize + (2 * kPointerSize)));
  }

  // Compute a pointer to the unwinding limit in register r2; that is
  // the first stack slot not part of the input frame.
  __ ldr(r2, MemOperand(r1, FrameDescription::frame_size_offset()));
  __ add(r2, r2, sp);

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ add(r3,  r1, Operand(FrameDescription::frame_content_offset()));
  Label pop_loop;
  __ bind(&pop_loop);
  __ pop(r4);
  __ str(r4, MemOperand(r3, 0));
  __ add(r3, r3, Operand(sizeof(uint32_t)));
  __ cmp(r2, sp);
  __ b(ne, &pop_loop);

  // Compute the output frame in the deoptimizer.
  __ push(r0);  // Preserve deoptimizer object across call.
  // r0: deoptimizer object; r1: scratch.
  __ PrepareCallCFunction(1, r1);
  // Call Deoptimizer::ComputeOutputFrames().
  __ CallCFunction(
      ExternalReference::compute_output_frames_function(isolate), 1);
  __ pop(r0);  // Restore deoptimizer object (class Deoptimizer).

  // Replace the current (input) frame with the output frames.
  Label outer_push_loop, inner_push_loop;
  // Outer loop state: r0 = current "FrameDescription** output_",
  // r1 = one past the last FrameDescription**.
  __ ldr(r1, MemOperand(r0, Deoptimizer::output_count_offset()));
  __ ldr(r0, MemOperand(r0, Deoptimizer::output_offset()));  // r0 is output_.
  __ add(r1, r0, Operand(r1, LSL, 2));
  __ bind(&outer_push_loop);
  // Inner loop state: r2 = current FrameDescription*, r3 = loop index.
  __ ldr(r2, MemOperand(r0, 0));  // output_[ix]
  __ ldr(r3, MemOperand(r2, FrameDescription::frame_size_offset()));
  __ bind(&inner_push_loop);
  __ sub(r3, r3, Operand(sizeof(uint32_t)));
  // __ add(r6, r2, Operand(r3, LSL, 1));
  __ add(r6, r2, Operand(r3));
  __ ldr(r7, MemOperand(r6, FrameDescription::frame_content_offset()));
  __ push(r7);
  __ cmp(r3, Operand(0));
  __ b(ne, &inner_push_loop);  // test for gt?
  __ add(r0, r0, Operand(kPointerSize));
  __ cmp(r0, r1);
  __ b(lt, &outer_push_loop);

  // Push state, pc, and continuation from the last output frame.
  if (type() != OSR) {
    __ ldr(r6, MemOperand(r2, FrameDescription::state_offset()));
    __ push(r6);
  }

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
  // Create a sequence of deoptimization entries. Note that any
  // registers may be still live.
  Label done;
  for (int i = 0; i < count(); i++) {
    int start = masm()->pc_offset();
    USE(start);
    if (type() == EAGER) {
      __ nop();
    } else {
      // Emulate ia32 like call by pushing return address to stack.
      __ push(lr);
    }
    __ mov(ip, Operand(i));
    __ push(ip);
    __ b(&done);
    ASSERT(masm()->pc_offset() - start == table_entry_size_);
  }
  __ bind(&done);
}

#undef __

} }  // namespace v8::internal
