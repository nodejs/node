// Copyright 2010 the V8 project authors. All rights reserved.
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

int Deoptimizer::table_entry_size_ = 16;

void Deoptimizer::DeoptimizeFunction(JSFunction* function) {
  AssertNoAllocation no_allocation;

  if (!function->IsOptimized()) return;

  // Get the optimized code.
  Code* code = function->code();

  // Invalidate the relocation information, as it will become invalid by the
  // code patching below, and is not needed any more.
  code->InvalidateRelocation();

  // For each return after a safepoint insert an absolute call to the
  // corresponding deoptimization entry.
  unsigned last_pc_offset = 0;
  SafepointTable table(function->code());
  for (unsigned i = 0; i < table.length(); i++) {
    unsigned pc_offset = table.GetPcOffset(i);
    SafepointEntry safepoint_entry = table.GetEntry(i);
    int deoptimization_index = safepoint_entry.deoptimization_index();
    int gap_code_size = safepoint_entry.gap_code_size();
    // Check that we did not shoot past next safepoint.
    // TODO(srdjan): How do we guarantee that safepoint code does not
    // overlap other safepoint patching code?
    CHECK(pc_offset >= last_pc_offset);
#ifdef DEBUG
    // Destroy the code which is not supposed to be run again.
    int instructions = (pc_offset - last_pc_offset) / Assembler::kInstrSize;
    CodePatcher destroyer(code->instruction_start() + last_pc_offset,
                          instructions);
    for (int x = 0; x < instructions; x++) {
      destroyer.masm()->bkpt(0);
    }
#endif
    last_pc_offset = pc_offset;
    if (deoptimization_index != Safepoint::kNoDeoptimizationIndex) {
      const int kCallInstructionSizeInWords = 3;
      CodePatcher patcher(code->instruction_start() + pc_offset + gap_code_size,
                          kCallInstructionSizeInWords);
      Address deoptimization_entry = Deoptimizer::GetDeoptimizationEntry(
          deoptimization_index, Deoptimizer::LAZY);
      patcher.masm()->Call(deoptimization_entry, RelocInfo::NONE);
      last_pc_offset +=
          gap_code_size + kCallInstructionSizeInWords * Assembler::kInstrSize;
    }
  }


#ifdef DEBUG
  // Destroy the code which is not supposed to be run again.
  int instructions =
      (code->safepoint_table_start() - last_pc_offset) / Assembler::kInstrSize;
  CodePatcher destroyer(code->instruction_start() + last_pc_offset,
                        instructions);
  for (int x = 0; x < instructions; x++) {
    destroyer.masm()->bkpt(0);
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
  UNIMPLEMENTED();
}


void Deoptimizer::RevertStackCheckCode(RelocInfo* rinfo, Code* check_code) {
  UNIMPLEMENTED();
}


void Deoptimizer::DoComputeOsrOutputFrame() {
  UNIMPLEMENTED();
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

  // The context can be gotten from the function so long as we don't
  // optimize functions that need local contexts.
  output_offset -= kPointerSize;
  input_offset -= kPointerSize;
  value = reinterpret_cast<intptr_t>(function->context());
  // The context for the bottommost output frame should also agree with the
  // input frame.
  ASSERT(!is_bottommost || input_->GetFrameSlot(input_offset) == value);
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


// This code tries to be close to ia32 code so that any changes can be
// easily ported.
void Deoptimizer::EntryGenerator::Generate() {
  GeneratePrologue();
  // TOS: bailout-id; TOS+1: return address if not EAGER.
  CpuFeatures::Scope scope(VFP3);
  // Save all general purpose registers before messing with them.
  const int kNumberOfRegisters = Register::kNumRegisters;

  // Everything but pc, lr and ip which will be saved but not restored.
  RegList restored_regs = kJSCallerSaved | kCalleeSaved | ip.bit();

  const int kDoubleRegsSize =
      kDoubleSize * DwVfpRegister::kNumAllocatableRegisters;

  // Save all general purpose registers before messing with them.
  __ sub(sp, sp, Operand(kDoubleRegsSize));
  for (int i = 0; i < DwVfpRegister::kNumAllocatableRegisters; ++i) {
    DwVfpRegister vfp_reg = DwVfpRegister::FromAllocationIndex(i);
    int offset = i * kDoubleSize;
    __ vstr(vfp_reg, sp, offset);
  }

  // Push all 16 registers (needed to populate FrameDescription::registers_).
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
  } else {
    __ mov(r3, lr);
    // Correct two words for bailout id and return address.
    __ add(r4, sp, Operand(kSavedRegistersAreaSize + (2 * kPointerSize)));
  }
  __ sub(r4, fp, r4);

  // Allocate a new deoptimizer object.
  // Pass four arguments in r0 to r3 and fifth argument on stack.
  __ PrepareCallCFunction(5, r5);
  __ ldr(r0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ mov(r1, Operand(type()));  // bailout type,
  // r2: bailout id already loaded.
  // r3: code address or 0 already loaded.
  __ str(r4, MemOperand(sp, 0 * kPointerSize));  // Fp-to-sp delta.
  // Call Deoptimizer::New().
  __ CallCFunction(ExternalReference::new_deoptimizer_function(), 5);

  // Preserve "deoptimizer" object in register r0 and get the input
  // frame descriptor pointer to r1 (deoptimizer->input_);
  __ ldr(r1, MemOperand(r0, Deoptimizer::input_offset()));


  // Copy core registers into FrameDescription::registers_[kNumRegisters].
  ASSERT(Register::kNumRegisters == kNumberOfRegisters);
  for (int i = 0; i < kNumberOfRegisters; i++) {
    int offset = (i * kIntSize) + FrameDescription::registers_offset();
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
  if (type() == EAGER) {
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
  __ CallCFunction(ExternalReference::compute_output_frames_function(), 1);
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

  // In case of OSR, we have to restore the XMM registers.
  if (type() == OSR) {
    UNIMPLEMENTED();
  }

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
    int offset = (i * kIntSize) + FrameDescription::registers_offset();
    __ ldr(r6, MemOperand(r2, offset));
    __ push(r6);
  }

  // Restore the registers from the stack.
  __ ldm(ia_w, sp, restored_regs);  // all but pc registers.
  __ pop(ip);  // remove sp
  __ pop(ip);  // remove lr

  // Set up the roots register.
  ExternalReference roots_address = ExternalReference::roots_address();
  __ mov(r10, Operand(roots_address));

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
