// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api.h"
#include "src/arm64/assembler-arm64-inl.h"
#include "src/arm64/frames-arm64.h"
#include "src/arm64/macro-assembler-arm64-inl.h"
#include "src/codegen.h"
#include "src/deoptimizer.h"
#include "src/full-codegen/full-codegen.h"
#include "src/register-configuration.h"
#include "src/safepoint-table.h"


namespace v8 {
namespace internal {


int Deoptimizer::patch_size() {
  // Size of the code used to patch lazy bailout points.
  // Patching is done by Deoptimizer::DeoptimizeFunction.
  return 4 * kInstructionSize;
}


void Deoptimizer::EnsureRelocSpaceForLazyDeoptimization(Handle<Code> code) {
  // Empty because there is no need for relocation information for the code
  // patching in Deoptimizer::PatchCodeForDeoptimization below.
}


void Deoptimizer::PatchCodeForDeoptimization(Isolate* isolate, Code* code) {
  Address code_start_address = code->instruction_start();
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

  {
    PatchingAssembler patcher(Assembler::IsolateData(isolate), pointer, 1);
    patcher.brk(0);
  }

  DeoptimizationInputData* data =
      DeoptimizationInputData::cast(code->deoptimization_data());
  int osr_offset = data->OsrPcOffset()->value();
  if (osr_offset > 0) {
    PatchingAssembler patcher(Assembler::IsolateData(isolate),
                              code_start_address + osr_offset, 1);
    patcher.brk(0);
  }

  DeoptimizationInputData* deopt_data =
      DeoptimizationInputData::cast(code->deoptimization_data());
#ifdef DEBUG
  Address prev_call_address = NULL;
#endif
  // For each LLazyBailout instruction insert a call to the corresponding
  // deoptimization entry.
  for (int i = 0; i < deopt_data->DeoptCount(); i++) {
    if (deopt_data->Pc(i)->value() == -1) continue;

    Address call_address = code_start_address + deopt_data->Pc(i)->value();
    Address deopt_entry = GetDeoptimizationEntry(isolate, i, LAZY);

    PatchingAssembler patcher(isolate, call_address,
                              patch_size() / kInstructionSize);
    patcher.ldr_pcrel(ip0, (2 * kInstructionSize) >> kLoadLiteralScaleLog2);
    patcher.blr(ip0);
    patcher.dc64(reinterpret_cast<intptr_t>(deopt_entry));

    DCHECK((prev_call_address == NULL) ||
           (call_address >= prev_call_address + patch_size()));
    DCHECK(call_address + patch_size() <= code->instruction_end());
#ifdef DEBUG
    prev_call_address = call_address;
#endif
  }
}


void Deoptimizer::SetPlatformCompiledStubRegisters(
    FrameDescription* output_frame, CodeStubDescriptor* descriptor) {
  ApiFunction function(descriptor->deoptimization_handler());
  ExternalReference xref(&function, ExternalReference::BUILTIN_CALL, isolate_);
  intptr_t handler = reinterpret_cast<intptr_t>(xref.address());
  int params = descriptor->GetHandlerParameterCount();
  output_frame->SetRegister(x0.code(), params);
  output_frame->SetRegister(x1.code(), handler);
}


void Deoptimizer::CopyDoubleRegisters(FrameDescription* output_frame) {
  for (int i = 0; i < DoubleRegister::kMaxNumRegisters; ++i) {
    Float64 double_value = input_->GetDoubleRegister(i);
    output_frame->SetDoubleRegister(i, double_value);
  }
}



#define __ masm()->

void Deoptimizer::TableEntryGenerator::Generate() {
  GeneratePrologue();

  // TODO(all): This code needs to be revisited. We probably only need to save
  // caller-saved registers here. Callee-saved registers can be stored directly
  // in the input frame.

  // Save all allocatable double registers.
  CPURegList saved_double_registers(
      CPURegister::kFPRegister, kDRegSizeInBits,
      RegisterConfiguration::Crankshaft()->allocatable_double_codes_mask());
  __ PushCPURegList(saved_double_registers);

  // Save all allocatable float registers.
  CPURegList saved_float_registers(
      CPURegister::kFPRegister, kSRegSizeInBits,
      RegisterConfiguration::Crankshaft()->allocatable_float_codes_mask());
  __ PushCPURegList(saved_float_registers);

  // We save all the registers expcept jssp, sp and lr.
  CPURegList saved_registers(CPURegister::kRegister, kXRegSizeInBits, 0, 27);
  saved_registers.Combine(fp);
  __ PushCPURegList(saved_registers);

  __ Mov(x3, Operand(ExternalReference(Isolate::kCEntryFPAddress, isolate())));
  __ Str(fp, MemOperand(x3));

  const int kSavedRegistersAreaSize =
      (saved_registers.Count() * kXRegSize) +
      (saved_double_registers.Count() * kDRegSize) +
      (saved_float_registers.Count() * kSRegSize);

  // Floating point registers are saved on the stack above core registers.
  const int kFloatRegistersOffset = saved_registers.Count() * kXRegSize;
  const int kDoubleRegistersOffset =
      kFloatRegistersOffset + saved_float_registers.Count() * kSRegSize;

  // Get the bailout id from the stack.
  Register bailout_id = x2;
  __ Peek(bailout_id, kSavedRegistersAreaSize);

  Register code_object = x3;
  Register fp_to_sp = x4;
  // Get the address of the location in the code object. This is the return
  // address for lazy deoptimization.
  __ Mov(code_object, lr);
  // Compute the fp-to-sp delta, and correct one word for bailout id.
  __ Add(fp_to_sp, __ StackPointer(),
         kSavedRegistersAreaSize + (1 * kPointerSize));
  __ Sub(fp_to_sp, fp, fp_to_sp);

  // Allocate a new deoptimizer object.
  __ Mov(x0, 0);
  Label context_check;
  __ Ldr(x1, MemOperand(fp, CommonFrameConstants::kContextOrFrameTypeOffset));
  __ JumpIfSmi(x1, &context_check);
  __ Ldr(x0, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ bind(&context_check);
  __ Mov(x1, type());
  // Following arguments are already loaded:
  //  - x2: bailout id
  //  - x3: code object address
  //  - x4: fp-to-sp delta
  __ Mov(x5, ExternalReference::isolate_address(isolate()));

  {
    // Call Deoptimizer::New().
    AllowExternalCallThatCantCauseGC scope(masm());
    __ CallCFunction(ExternalReference::new_deoptimizer_function(isolate()), 6);
  }

  // Preserve "deoptimizer" object in register x0.
  Register deoptimizer = x0;

  // Get the input frame descriptor pointer.
  __ Ldr(x1, MemOperand(deoptimizer, Deoptimizer::input_offset()));

  // Copy core registers into the input frame.
  CPURegList copy_to_input = saved_registers;
  for (int i = 0; i < saved_registers.Count(); i++) {
    __ Peek(x2, i * kPointerSize);
    CPURegister current_reg = copy_to_input.PopLowestIndex();
    int offset = (current_reg.code() * kPointerSize) +
        FrameDescription::registers_offset();
    __ Str(x2, MemOperand(x1, offset));
  }

  // Copy double registers to the input frame.
  CPURegList copy_double_to_input = saved_double_registers;
  for (int i = 0; i < saved_double_registers.Count(); i++) {
    int src_offset = kDoubleRegistersOffset + (i * kDoubleSize);
    __ Peek(x2, src_offset);
    CPURegister reg = copy_double_to_input.PopLowestIndex();
    int dst_offset = FrameDescription::double_registers_offset() +
                     (reg.code() * kDoubleSize);
    __ Str(x2, MemOperand(x1, dst_offset));
  }

  // Copy float registers to the input frame.
  CPURegList copy_float_to_input = saved_float_registers;
  for (int i = 0; i < saved_float_registers.Count(); i++) {
    int src_offset = kFloatRegistersOffset + (i * kFloatSize);
    __ Peek(w2, src_offset);
    CPURegister reg = copy_float_to_input.PopLowestIndex();
    int dst_offset =
        FrameDescription::float_registers_offset() + (reg.code() * kFloatSize);
    __ Str(w2, MemOperand(x1, dst_offset));
  }

  // Remove the bailout id and the saved registers from the stack.
  __ Drop(1 + (kSavedRegistersAreaSize / kXRegSize));

  // Compute a pointer to the unwinding limit in register x2; that is
  // the first stack slot not part of the input frame.
  Register unwind_limit = x2;
  __ Ldr(unwind_limit, MemOperand(x1, FrameDescription::frame_size_offset()));
  __ Add(unwind_limit, unwind_limit, __ StackPointer());

  // Unwind the stack down to - but not including - the unwinding
  // limit and copy the contents of the activation frame to the input
  // frame description.
  __ Add(x3, x1, FrameDescription::frame_content_offset());
  Label pop_loop;
  Label pop_loop_header;
  __ B(&pop_loop_header);
  __ Bind(&pop_loop);
  __ Pop(x4);
  __ Str(x4, MemOperand(x3, kPointerSize, PostIndex));
  __ Bind(&pop_loop_header);
  __ Cmp(unwind_limit, __ StackPointer());
  __ B(ne, &pop_loop);

  // Compute the output frame in the deoptimizer.
  __ Push(x0);  // Preserve deoptimizer object across call.

  {
    // Call Deoptimizer::ComputeOutputFrames().
    AllowExternalCallThatCantCauseGC scope(masm());
    __ CallCFunction(
        ExternalReference::compute_output_frames_function(isolate()), 1);
  }
  __ Pop(x4);  // Restore deoptimizer object (class Deoptimizer).

  __ Ldr(__ StackPointer(),
         MemOperand(x4, Deoptimizer::caller_frame_top_offset()));

  // Replace the current (input) frame with the output frames.
  Label outer_push_loop, inner_push_loop,
      outer_loop_header, inner_loop_header;
  __ Ldrsw(x1, MemOperand(x4, Deoptimizer::output_count_offset()));
  __ Ldr(x0, MemOperand(x4, Deoptimizer::output_offset()));
  __ Add(x1, x0, Operand(x1, LSL, kPointerSizeLog2));
  __ B(&outer_loop_header);

  __ Bind(&outer_push_loop);
  Register current_frame = x2;
  __ Ldr(current_frame, MemOperand(x0, 0));
  __ Ldr(x3, MemOperand(current_frame, FrameDescription::frame_size_offset()));
  __ B(&inner_loop_header);

  __ Bind(&inner_push_loop);
  __ Sub(x3, x3, kPointerSize);
  __ Add(x6, current_frame, x3);
  __ Ldr(x7, MemOperand(x6, FrameDescription::frame_content_offset()));
  __ Push(x7);
  __ Bind(&inner_loop_header);
  __ Cbnz(x3, &inner_push_loop);

  __ Add(x0, x0, kPointerSize);
  __ Bind(&outer_loop_header);
  __ Cmp(x0, x1);
  __ B(lt, &outer_push_loop);

  __ Ldr(x1, MemOperand(x4, Deoptimizer::input_offset()));
  DCHECK(!saved_double_registers.IncludesAliasOf(crankshaft_fp_scratch) &&
         !saved_double_registers.IncludesAliasOf(fp_zero) &&
         !saved_double_registers.IncludesAliasOf(fp_scratch));
  while (!saved_double_registers.IsEmpty()) {
    const CPURegister reg = saved_double_registers.PopLowestIndex();
    int src_offset = FrameDescription::double_registers_offset() +
                     (reg.code() * kDoubleSize);
    __ Ldr(reg, MemOperand(x1, src_offset));
  }

  // Push state from the last output frame.
  __ Ldr(x6, MemOperand(current_frame, FrameDescription::state_offset()));
  __ Push(x6);

  // TODO(all): ARM copies a lot (if not all) of the last output frame onto the
  // stack, then pops it all into registers. Here, we try to load it directly
  // into the relevant registers. Is this correct? If so, we should improve the
  // ARM code.

  // TODO(all): This code needs to be revisited, We probably don't need to
  // restore all the registers as fullcodegen does not keep live values in
  // registers (note that at least fp must be restored though).

  // Restore registers from the last output frame.
  // Note that lr is not in the list of saved_registers and will be restored
  // later. We can use it to hold the address of last output frame while
  // reloading the other registers.
  DCHECK(!saved_registers.IncludesAliasOf(lr));
  Register last_output_frame = lr;
  __ Mov(last_output_frame, current_frame);

  // We don't need to restore x7 as it will be clobbered later to hold the
  // continuation address.
  Register continuation = x7;
  saved_registers.Remove(continuation);

  while (!saved_registers.IsEmpty()) {
    // TODO(all): Look for opportunities to optimize this by using ldp.
    CPURegister current_reg = saved_registers.PopLowestIndex();
    int offset = (current_reg.code() * kPointerSize) +
        FrameDescription::registers_offset();
    __ Ldr(current_reg, MemOperand(last_output_frame, offset));
  }

  __ Ldr(continuation, MemOperand(last_output_frame,
                                  FrameDescription::continuation_offset()));
  __ Ldr(lr, MemOperand(last_output_frame, FrameDescription::pc_offset()));
  __ InitializeRootRegister();
  __ Br(continuation);
}


// Size of an entry of the second level deopt table.
// This is the code size generated by GeneratePrologue for one entry.
const int Deoptimizer::table_entry_size_ = 2 * kInstructionSize;


void Deoptimizer::TableEntryGenerator::GeneratePrologue() {
  UseScratchRegisterScope temps(masm());
  Register entry_id = temps.AcquireX();

  // Create a sequence of deoptimization entries.
  // Note that registers are still live when jumping to an entry.
  Label done;
  {
    InstructionAccurateScope scope(masm());

    // The number of entry will never exceed kMaxNumberOfEntries.
    // As long as kMaxNumberOfEntries is a valid 16 bits immediate you can use
    // a movz instruction to load the entry id.
    DCHECK(is_uint16(Deoptimizer::kMaxNumberOfEntries));

    for (int i = 0; i < count(); i++) {
      int start = masm()->pc_offset();
      USE(start);
      __ movz(entry_id, i);
      __ b(&done);
      DCHECK(masm()->pc_offset() - start == table_entry_size_);
    }
  }
  __ Bind(&done);
  __ Push(entry_id);
}


void FrameDescription::SetCallerPc(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}


void FrameDescription::SetCallerFp(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}


void FrameDescription::SetCallerConstantPool(unsigned offset, intptr_t value) {
  // No embedded constant pool support.
  UNREACHABLE();
}


#undef __

}  // namespace internal
}  // namespace v8
