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

#include "arm/lithium-codegen-arm.h"
#include "arm/lithium-gap-resolver-arm.h"
#include "code-stubs.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {


class SafepointGenerator : public CallWrapper {
 public:
  SafepointGenerator(LCodeGen* codegen,
                     LPointerMap* pointers,
                     Safepoint::DeoptMode mode)
      : codegen_(codegen),
        pointers_(pointers),
        deopt_mode_(mode) { }
  virtual ~SafepointGenerator() { }

  virtual void BeforeCall(int call_size) const { }

  virtual void AfterCall() const {
    codegen_->RecordSafepoint(pointers_, deopt_mode_);
  }

 private:
  LCodeGen* codegen_;
  LPointerMap* pointers_;
  Safepoint::DeoptMode deopt_mode_;
};


#define __ masm()->

bool LCodeGen::GenerateCode() {
  HPhase phase("Code generation", chunk());
  ASSERT(is_unused());
  status_ = GENERATING;
  CpuFeatures::Scope scope1(VFP3);
  CpuFeatures::Scope scope2(ARMv7);
  return GeneratePrologue() &&
      GenerateBody() &&
      GenerateDeferredCode() &&
      GenerateDeoptJumpTable() &&
      GenerateSafepointTable();
}


void LCodeGen::FinishCode(Handle<Code> code) {
  ASSERT(is_done());
  code->set_stack_slots(GetStackSlotCount());
  code->set_safepoint_table_offset(safepoints_.GetCodeOffset());
  PopulateDeoptimizationData(code);
}


void LCodeGen::Abort(const char* format, ...) {
  if (FLAG_trace_bailout) {
    SmartArrayPointer<char> name(
        info()->shared_info()->DebugName()->ToCString());
    PrintF("Aborting LCodeGen in @\"%s\": ", *name);
    va_list arguments;
    va_start(arguments, format);
    OS::VPrint(format, arguments);
    va_end(arguments);
    PrintF("\n");
  }
  status_ = ABORTED;
}


void LCodeGen::Comment(const char* format, ...) {
  if (!FLAG_code_comments) return;
  char buffer[4 * KB];
  StringBuilder builder(buffer, ARRAY_SIZE(buffer));
  va_list arguments;
  va_start(arguments, format);
  builder.AddFormattedList(format, arguments);
  va_end(arguments);

  // Copy the string before recording it in the assembler to avoid
  // issues when the stack allocated buffer goes out of scope.
  size_t length = builder.position();
  Vector<char> copy = Vector<char>::New(length + 1);
  memcpy(copy.start(), builder.Finalize(), copy.length());
  masm()->RecordComment(copy.start());
}


bool LCodeGen::GeneratePrologue() {
  ASSERT(is_generating());

#ifdef DEBUG
  if (strlen(FLAG_stop_at) > 0 &&
      info_->function()->name()->IsEqualTo(CStrVector(FLAG_stop_at))) {
    __ stop("stop_at");
  }
#endif

  // r1: Callee's JS function.
  // cp: Callee's context.
  // fp: Caller's frame pointer.
  // lr: Caller's pc.

  // Strict mode functions and builtins need to replace the receiver
  // with undefined when called as functions (without an explicit
  // receiver object). r5 is zero for method calls and non-zero for
  // function calls.
  if (info_->is_strict_mode() || info_->is_native()) {
    Label ok;
    __ cmp(r5, Operand(0));
    __ b(eq, &ok);
    int receiver_offset = scope()->num_parameters() * kPointerSize;
    __ LoadRoot(r2, Heap::kUndefinedValueRootIndex);
    __ str(r2, MemOperand(sp, receiver_offset));
    __ bind(&ok);
  }

  __ stm(db_w, sp, r1.bit() | cp.bit() | fp.bit() | lr.bit());
  __ add(fp, sp, Operand(2 * kPointerSize));  // Adjust FP to point to saved FP.

  // Reserve space for the stack slots needed by the code.
  int slots = GetStackSlotCount();
  if (slots > 0) {
    if (FLAG_debug_code) {
      __ mov(r0, Operand(slots));
      __ mov(r2, Operand(kSlotsZapValue));
      Label loop;
      __ bind(&loop);
      __ push(r2);
      __ sub(r0, r0, Operand(1), SetCC);
      __ b(ne, &loop);
    } else {
      __ sub(sp,  sp, Operand(slots * kPointerSize));
    }
  }

  // Possibly allocate a local context.
  int heap_slots = scope()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
  if (heap_slots > 0) {
    Comment(";;; Allocate local context");
    // Argument to NewContext is the function, which is in r1.
    __ push(r1);
    if (heap_slots <= FastNewContextStub::kMaximumSlots) {
      FastNewContextStub stub(heap_slots);
      __ CallStub(&stub);
    } else {
      __ CallRuntime(Runtime::kNewFunctionContext, 1);
    }
    RecordSafepoint(Safepoint::kNoLazyDeopt);
    // Context is returned in both r0 and cp.  It replaces the context
    // passed to us.  It's saved in the stack and kept live in cp.
    __ str(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
    // Copy any necessary parameters into the context.
    int num_parameters = scope()->num_parameters();
    for (int i = 0; i < num_parameters; i++) {
      Variable* var = scope()->parameter(i);
      if (var->IsContextSlot()) {
        int parameter_offset = StandardFrameConstants::kCallerSPOffset +
            (num_parameters - 1 - i) * kPointerSize;
        // Load parameter from stack.
        __ ldr(r0, MemOperand(fp, parameter_offset));
        // Store it in the context.
        __ mov(r1, Operand(Context::SlotOffset(var->index())));
        __ str(r0, MemOperand(cp, r1));
        // Update the write barrier. This clobbers all involved
        // registers, so we have to use two more registers to avoid
        // clobbering cp.
        __ mov(r2, Operand(cp));
        __ RecordWrite(r2, Operand(r1), r3, r0);
      }
    }
    Comment(";;; End allocate local context");
  }

  // Trace the call.
  if (FLAG_trace) {
    __ CallRuntime(Runtime::kTraceEnter, 0);
  }
  return !is_aborted();
}


bool LCodeGen::GenerateBody() {
  ASSERT(is_generating());
  bool emit_instructions = true;
  for (current_instruction_ = 0;
       !is_aborted() && current_instruction_ < instructions_->length();
       current_instruction_++) {
    LInstruction* instr = instructions_->at(current_instruction_);
    if (instr->IsLabel()) {
      LLabel* label = LLabel::cast(instr);
      emit_instructions = !label->HasReplacement();
    }

    if (emit_instructions) {
      Comment(";;; @%d: %s.", current_instruction_, instr->Mnemonic());
      instr->CompileToNative(this);
    }
  }
  EnsureSpaceForLazyDeopt();
  return !is_aborted();
}


bool LCodeGen::GenerateDeferredCode() {
  ASSERT(is_generating());
  if (deferred_.length() > 0) {
    for (int i = 0; !is_aborted() && i < deferred_.length(); i++) {
      LDeferredCode* code = deferred_[i];
      __ bind(code->entry());
      code->Generate();
      __ jmp(code->exit());
    }
  }

  // Force constant pool emission at the end of the deferred code to make
  // sure that no constant pools are emitted after.
  masm()->CheckConstPool(true, false);

  return !is_aborted();
}


bool LCodeGen::GenerateDeoptJumpTable() {
  // Check that the jump table is accessible from everywhere in the function
  // code, ie that offsets to the table can be encoded in the 24bit signed
  // immediate of a branch instruction.
  // To simplify we consider the code size from the first instruction to the
  // end of the jump table. We also don't consider the pc load delta.
  // Each entry in the jump table generates one instruction and inlines one
  // 32bit data after it.
  if (!is_int24((masm()->pc_offset() / Assembler::kInstrSize) +
      deopt_jump_table_.length() * 2)) {
    Abort("Generated code is too large");
  }

  // Block the constant pool emission during the jump table emission.
  __ BlockConstPoolFor(deopt_jump_table_.length());
  __ RecordComment("[ Deoptimisation jump table");
  Label table_start;
  __ bind(&table_start);
  for (int i = 0; i < deopt_jump_table_.length(); i++) {
    __ bind(&deopt_jump_table_[i].label);
    __ ldr(pc, MemOperand(pc, Assembler::kInstrSize - Assembler::kPcLoadDelta));
    __ dd(reinterpret_cast<uint32_t>(deopt_jump_table_[i].address));
  }
  ASSERT(masm()->InstructionsGeneratedSince(&table_start) ==
      deopt_jump_table_.length() * 2);
  __ RecordComment("]");

  // The deoptimization jump table is the last part of the instruction
  // sequence. Mark the generated code as done unless we bailed out.
  if (!is_aborted()) status_ = DONE;
  return !is_aborted();
}


bool LCodeGen::GenerateSafepointTable() {
  ASSERT(is_done());
  safepoints_.Emit(masm(), GetStackSlotCount());
  return !is_aborted();
}


Register LCodeGen::ToRegister(int index) const {
  return Register::FromAllocationIndex(index);
}


DoubleRegister LCodeGen::ToDoubleRegister(int index) const {
  return DoubleRegister::FromAllocationIndex(index);
}


Register LCodeGen::ToRegister(LOperand* op) const {
  ASSERT(op->IsRegister());
  return ToRegister(op->index());
}


Register LCodeGen::EmitLoadRegister(LOperand* op, Register scratch) {
  if (op->IsRegister()) {
    return ToRegister(op->index());
  } else if (op->IsConstantOperand()) {
    __ mov(scratch, ToOperand(op));
    return scratch;
  } else if (op->IsStackSlot() || op->IsArgument()) {
    __ ldr(scratch, ToMemOperand(op));
    return scratch;
  }
  UNREACHABLE();
  return scratch;
}


DoubleRegister LCodeGen::ToDoubleRegister(LOperand* op) const {
  ASSERT(op->IsDoubleRegister());
  return ToDoubleRegister(op->index());
}


DoubleRegister LCodeGen::EmitLoadDoubleRegister(LOperand* op,
                                                SwVfpRegister flt_scratch,
                                                DoubleRegister dbl_scratch) {
  if (op->IsDoubleRegister()) {
    return ToDoubleRegister(op->index());
  } else if (op->IsConstantOperand()) {
    LConstantOperand* const_op = LConstantOperand::cast(op);
    Handle<Object> literal = chunk_->LookupLiteral(const_op);
    Representation r = chunk_->LookupLiteralRepresentation(const_op);
    if (r.IsInteger32()) {
      ASSERT(literal->IsNumber());
      __ mov(ip, Operand(static_cast<int32_t>(literal->Number())));
      __ vmov(flt_scratch, ip);
      __ vcvt_f64_s32(dbl_scratch, flt_scratch);
      return dbl_scratch;
    } else if (r.IsDouble()) {
      Abort("unsupported double immediate");
    } else if (r.IsTagged()) {
      Abort("unsupported tagged immediate");
    }
  } else if (op->IsStackSlot() || op->IsArgument()) {
    // TODO(regis): Why is vldr not taking a MemOperand?
    // __ vldr(dbl_scratch, ToMemOperand(op));
    MemOperand mem_op = ToMemOperand(op);
    __ vldr(dbl_scratch, mem_op.rn(), mem_op.offset());
    return dbl_scratch;
  }
  UNREACHABLE();
  return dbl_scratch;
}


int LCodeGen::ToInteger32(LConstantOperand* op) const {
  Handle<Object> value = chunk_->LookupLiteral(op);
  ASSERT(chunk_->LookupLiteralRepresentation(op).IsInteger32());
  ASSERT(static_cast<double>(static_cast<int32_t>(value->Number())) ==
      value->Number());
  return static_cast<int32_t>(value->Number());
}


Operand LCodeGen::ToOperand(LOperand* op) {
  if (op->IsConstantOperand()) {
    LConstantOperand* const_op = LConstantOperand::cast(op);
    Handle<Object> literal = chunk_->LookupLiteral(const_op);
    Representation r = chunk_->LookupLiteralRepresentation(const_op);
    if (r.IsInteger32()) {
      ASSERT(literal->IsNumber());
      return Operand(static_cast<int32_t>(literal->Number()));
    } else if (r.IsDouble()) {
      Abort("ToOperand Unsupported double immediate.");
    }
    ASSERT(r.IsTagged());
    return Operand(literal);
  } else if (op->IsRegister()) {
    return Operand(ToRegister(op));
  } else if (op->IsDoubleRegister()) {
    Abort("ToOperand IsDoubleRegister unimplemented");
    return Operand(0);
  }
  // Stack slots not implemented, use ToMemOperand instead.
  UNREACHABLE();
  return Operand(0);
}


MemOperand LCodeGen::ToMemOperand(LOperand* op) const {
  ASSERT(!op->IsRegister());
  ASSERT(!op->IsDoubleRegister());
  ASSERT(op->IsStackSlot() || op->IsDoubleStackSlot());
  int index = op->index();
  if (index >= 0) {
    // Local or spill slot. Skip the frame pointer, function, and
    // context in the fixed part of the frame.
    return MemOperand(fp, -(index + 3) * kPointerSize);
  } else {
    // Incoming parameter. Skip the return address.
    return MemOperand(fp, -(index - 1) * kPointerSize);
  }
}


MemOperand LCodeGen::ToHighMemOperand(LOperand* op) const {
  ASSERT(op->IsDoubleStackSlot());
  int index = op->index();
  if (index >= 0) {
    // Local or spill slot. Skip the frame pointer, function, context,
    // and the first word of the double in the fixed part of the frame.
    return MemOperand(fp, -(index + 3) * kPointerSize + kPointerSize);
  } else {
    // Incoming parameter. Skip the return address and the first word of
    // the double.
    return MemOperand(fp, -(index - 1) * kPointerSize + kPointerSize);
  }
}


void LCodeGen::WriteTranslation(LEnvironment* environment,
                                Translation* translation) {
  if (environment == NULL) return;

  // The translation includes one command per value in the environment.
  int translation_size = environment->values()->length();
  // The output frame height does not include the parameters.
  int height = translation_size - environment->parameter_count();

  WriteTranslation(environment->outer(), translation);
  int closure_id = DefineDeoptimizationLiteral(environment->closure());
  translation->BeginFrame(environment->ast_id(), closure_id, height);
  for (int i = 0; i < translation_size; ++i) {
    LOperand* value = environment->values()->at(i);
    // spilled_registers_ and spilled_double_registers_ are either
    // both NULL or both set.
    if (environment->spilled_registers() != NULL && value != NULL) {
      if (value->IsRegister() &&
          environment->spilled_registers()[value->index()] != NULL) {
        translation->MarkDuplicate();
        AddToTranslation(translation,
                         environment->spilled_registers()[value->index()],
                         environment->HasTaggedValueAt(i));
      } else if (
          value->IsDoubleRegister() &&
          environment->spilled_double_registers()[value->index()] != NULL) {
        translation->MarkDuplicate();
        AddToTranslation(
            translation,
            environment->spilled_double_registers()[value->index()],
            false);
      }
    }

    AddToTranslation(translation, value, environment->HasTaggedValueAt(i));
  }
}


void LCodeGen::AddToTranslation(Translation* translation,
                                LOperand* op,
                                bool is_tagged) {
  if (op == NULL) {
    // TODO(twuerthinger): Introduce marker operands to indicate that this value
    // is not present and must be reconstructed from the deoptimizer. Currently
    // this is only used for the arguments object.
    translation->StoreArgumentsObject();
  } else if (op->IsStackSlot()) {
    if (is_tagged) {
      translation->StoreStackSlot(op->index());
    } else {
      translation->StoreInt32StackSlot(op->index());
    }
  } else if (op->IsDoubleStackSlot()) {
    translation->StoreDoubleStackSlot(op->index());
  } else if (op->IsArgument()) {
    ASSERT(is_tagged);
    int src_index = GetStackSlotCount() + op->index();
    translation->StoreStackSlot(src_index);
  } else if (op->IsRegister()) {
    Register reg = ToRegister(op);
    if (is_tagged) {
      translation->StoreRegister(reg);
    } else {
      translation->StoreInt32Register(reg);
    }
  } else if (op->IsDoubleRegister()) {
    DoubleRegister reg = ToDoubleRegister(op);
    translation->StoreDoubleRegister(reg);
  } else if (op->IsConstantOperand()) {
    Handle<Object> literal = chunk()->LookupLiteral(LConstantOperand::cast(op));
    int src_index = DefineDeoptimizationLiteral(literal);
    translation->StoreLiteral(src_index);
  } else {
    UNREACHABLE();
  }
}


void LCodeGen::CallCode(Handle<Code> code,
                        RelocInfo::Mode mode,
                        LInstruction* instr) {
  CallCodeGeneric(code, mode, instr, RECORD_SIMPLE_SAFEPOINT);
}


void LCodeGen::CallCodeGeneric(Handle<Code> code,
                               RelocInfo::Mode mode,
                               LInstruction* instr,
                               SafepointMode safepoint_mode) {
  ASSERT(instr != NULL);
  LPointerMap* pointers = instr->pointer_map();
  RecordPosition(pointers->position());
  __ Call(code, mode);
  RecordSafepointWithLazyDeopt(instr, safepoint_mode);

  // Signal that we don't inline smi code before these stubs in the
  // optimizing code generator.
  if (code->kind() == Code::BINARY_OP_IC ||
      code->kind() == Code::COMPARE_IC) {
    __ nop();
  }
}


void LCodeGen::CallRuntime(const Runtime::Function* function,
                           int num_arguments,
                           LInstruction* instr) {
  ASSERT(instr != NULL);
  LPointerMap* pointers = instr->pointer_map();
  ASSERT(pointers != NULL);
  RecordPosition(pointers->position());

  __ CallRuntime(function, num_arguments);
  RecordSafepointWithLazyDeopt(instr, RECORD_SIMPLE_SAFEPOINT);
}


void LCodeGen::CallRuntimeFromDeferred(Runtime::FunctionId id,
                                       int argc,
                                       LInstruction* instr) {
  __ CallRuntimeSaveDoubles(id);
  RecordSafepointWithRegisters(
      instr->pointer_map(), argc, Safepoint::kNoLazyDeopt);
}


void LCodeGen::RegisterEnvironmentForDeoptimization(LEnvironment* environment,
                                                    Safepoint::DeoptMode mode) {
  if (!environment->HasBeenRegistered()) {
    // Physical stack frame layout:
    // -x ............. -4  0 ..................................... y
    // [incoming arguments] [spill slots] [pushed outgoing arguments]

    // Layout of the environment:
    // 0 ..................................................... size-1
    // [parameters] [locals] [expression stack including arguments]

    // Layout of the translation:
    // 0 ........................................................ size - 1 + 4
    // [expression stack including arguments] [locals] [4 words] [parameters]
    // |>------------  translation_size ------------<|

    int frame_count = 0;
    for (LEnvironment* e = environment; e != NULL; e = e->outer()) {
      ++frame_count;
    }
    Translation translation(&translations_, frame_count);
    WriteTranslation(environment, &translation);
    int deoptimization_index = deoptimizations_.length();
    int pc_offset = masm()->pc_offset();
    environment->Register(deoptimization_index,
                          translation.index(),
                          (mode == Safepoint::kLazyDeopt) ? pc_offset : -1);
    deoptimizations_.Add(environment);
  }
}


void LCodeGen::DeoptimizeIf(Condition cc, LEnvironment* environment) {
  RegisterEnvironmentForDeoptimization(environment, Safepoint::kNoLazyDeopt);
  ASSERT(environment->HasBeenRegistered());
  int id = environment->deoptimization_index();
  Address entry = Deoptimizer::GetDeoptimizationEntry(id, Deoptimizer::EAGER);
  ASSERT(entry != NULL);
  if (entry == NULL) {
    Abort("bailout was not prepared");
    return;
  }

  ASSERT(FLAG_deopt_every_n_times < 2);  // Other values not supported on ARM.

  if (FLAG_deopt_every_n_times == 1 &&
      info_->shared_info()->opt_count() == id) {
    __ Jump(entry, RelocInfo::RUNTIME_ENTRY);
    return;
  }

  if (FLAG_trap_on_deopt) __ stop("trap_on_deopt", cc);

  if (cc == al) {
    __ Jump(entry, RelocInfo::RUNTIME_ENTRY);
  } else {
    // We often have several deopts to the same entry, reuse the last
    // jump entry if this is the case.
    if (deopt_jump_table_.is_empty() ||
        (deopt_jump_table_.last().address != entry)) {
      deopt_jump_table_.Add(JumpTableEntry(entry));
    }
    __ b(cc, &deopt_jump_table_.last().label);
  }
}


void LCodeGen::PopulateDeoptimizationData(Handle<Code> code) {
  int length = deoptimizations_.length();
  if (length == 0) return;
  ASSERT(FLAG_deopt);
  Handle<DeoptimizationInputData> data =
      factory()->NewDeoptimizationInputData(length, TENURED);

  Handle<ByteArray> translations = translations_.CreateByteArray();
  data->SetTranslationByteArray(*translations);
  data->SetInlinedFunctionCount(Smi::FromInt(inlined_function_count_));

  Handle<FixedArray> literals =
      factory()->NewFixedArray(deoptimization_literals_.length(), TENURED);
  for (int i = 0; i < deoptimization_literals_.length(); i++) {
    literals->set(i, *deoptimization_literals_[i]);
  }
  data->SetLiteralArray(*literals);

  data->SetOsrAstId(Smi::FromInt(info_->osr_ast_id()));
  data->SetOsrPcOffset(Smi::FromInt(osr_pc_offset_));

  // Populate the deoptimization entries.
  for (int i = 0; i < length; i++) {
    LEnvironment* env = deoptimizations_[i];
    data->SetAstId(i, Smi::FromInt(env->ast_id()));
    data->SetTranslationIndex(i, Smi::FromInt(env->translation_index()));
    data->SetArgumentsStackHeight(i,
                                  Smi::FromInt(env->arguments_stack_height()));
    data->SetPc(i, Smi::FromInt(env->pc_offset()));
  }
  code->set_deoptimization_data(*data);
}


int LCodeGen::DefineDeoptimizationLiteral(Handle<Object> literal) {
  int result = deoptimization_literals_.length();
  for (int i = 0; i < deoptimization_literals_.length(); ++i) {
    if (deoptimization_literals_[i].is_identical_to(literal)) return i;
  }
  deoptimization_literals_.Add(literal);
  return result;
}


void LCodeGen::PopulateDeoptimizationLiteralsWithInlinedFunctions() {
  ASSERT(deoptimization_literals_.length() == 0);

  const ZoneList<Handle<JSFunction> >* inlined_closures =
      chunk()->inlined_closures();

  for (int i = 0, length = inlined_closures->length();
       i < length;
       i++) {
    DefineDeoptimizationLiteral(inlined_closures->at(i));
  }

  inlined_function_count_ = deoptimization_literals_.length();
}


void LCodeGen::RecordSafepointWithLazyDeopt(
    LInstruction* instr, SafepointMode safepoint_mode) {
  if (safepoint_mode == RECORD_SIMPLE_SAFEPOINT) {
    RecordSafepoint(instr->pointer_map(), Safepoint::kLazyDeopt);
  } else {
    ASSERT(safepoint_mode == RECORD_SAFEPOINT_WITH_REGISTERS_AND_NO_ARGUMENTS);
    RecordSafepointWithRegisters(
        instr->pointer_map(), 0, Safepoint::kLazyDeopt);
  }
}


void LCodeGen::RecordSafepoint(
    LPointerMap* pointers,
    Safepoint::Kind kind,
    int arguments,
    Safepoint::DeoptMode deopt_mode) {
  ASSERT(expected_safepoint_kind_ == kind);

  const ZoneList<LOperand*>* operands = pointers->operands();
  Safepoint safepoint = safepoints_.DefineSafepoint(masm(),
      kind, arguments, deopt_mode);
  for (int i = 0; i < operands->length(); i++) {
    LOperand* pointer = operands->at(i);
    if (pointer->IsStackSlot()) {
      safepoint.DefinePointerSlot(pointer->index());
    } else if (pointer->IsRegister() && (kind & Safepoint::kWithRegisters)) {
      safepoint.DefinePointerRegister(ToRegister(pointer));
    }
  }
  if (kind & Safepoint::kWithRegisters) {
    // Register cp always contains a pointer to the context.
    safepoint.DefinePointerRegister(cp);
  }
}


void LCodeGen::RecordSafepoint(LPointerMap* pointers,
                               Safepoint::DeoptMode deopt_mode) {
  RecordSafepoint(pointers, Safepoint::kSimple, 0, deopt_mode);
}


void LCodeGen::RecordSafepoint(Safepoint::DeoptMode deopt_mode) {
  LPointerMap empty_pointers(RelocInfo::kNoPosition);
  RecordSafepoint(&empty_pointers, deopt_mode);
}


void LCodeGen::RecordSafepointWithRegisters(LPointerMap* pointers,
                                            int arguments,
                                            Safepoint::DeoptMode deopt_mode) {
  RecordSafepoint(
      pointers, Safepoint::kWithRegisters, arguments, deopt_mode);
}


void LCodeGen::RecordSafepointWithRegistersAndDoubles(
    LPointerMap* pointers,
    int arguments,
    Safepoint::DeoptMode deopt_mode) {
  RecordSafepoint(
      pointers, Safepoint::kWithRegistersAndDoubles, arguments, deopt_mode);
}


void LCodeGen::RecordPosition(int position) {
  if (position == RelocInfo::kNoPosition) return;
  masm()->positions_recorder()->RecordPosition(position);
}


void LCodeGen::DoLabel(LLabel* label) {
  if (label->is_loop_header()) {
    Comment(";;; B%d - LOOP entry", label->block_id());
  } else {
    Comment(";;; B%d", label->block_id());
  }
  __ bind(label->label());
  current_block_ = label->block_id();
  DoGap(label);
}


void LCodeGen::DoParallelMove(LParallelMove* move) {
  resolver_.Resolve(move);
}


void LCodeGen::DoGap(LGap* gap) {
  for (int i = LGap::FIRST_INNER_POSITION;
       i <= LGap::LAST_INNER_POSITION;
       i++) {
    LGap::InnerPosition inner_pos = static_cast<LGap::InnerPosition>(i);
    LParallelMove* move = gap->GetParallelMove(inner_pos);
    if (move != NULL) DoParallelMove(move);
  }
}


void LCodeGen::DoInstructionGap(LInstructionGap* instr) {
  DoGap(instr);
}


void LCodeGen::DoParameter(LParameter* instr) {
  // Nothing to do.
}


void LCodeGen::DoCallStub(LCallStub* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));
  switch (instr->hydrogen()->major_key()) {
    case CodeStub::RegExpConstructResult: {
      RegExpConstructResultStub stub;
      CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
      break;
    }
    case CodeStub::RegExpExec: {
      RegExpExecStub stub;
      CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
      break;
    }
    case CodeStub::SubString: {
      SubStringStub stub;
      CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
      break;
    }
    case CodeStub::NumberToString: {
      NumberToStringStub stub;
      CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
      break;
    }
    case CodeStub::StringAdd: {
      StringAddStub stub(NO_STRING_ADD_FLAGS);
      CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
      break;
    }
    case CodeStub::StringCompare: {
      StringCompareStub stub;
      CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
      break;
    }
    case CodeStub::TranscendentalCache: {
      __ ldr(r0, MemOperand(sp, 0));
      TranscendentalCacheStub stub(instr->transcendental_type(),
                                   TranscendentalCacheStub::TAGGED);
      CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
      break;
    }
    default:
      UNREACHABLE();
  }
}


void LCodeGen::DoUnknownOSRValue(LUnknownOSRValue* instr) {
  // Nothing to do.
}


void LCodeGen::DoModI(LModI* instr) {
  if (instr->hydrogen()->HasPowerOf2Divisor()) {
    Register dividend = ToRegister(instr->InputAt(0));
    Register result = ToRegister(instr->result());

    int32_t divisor =
        HConstant::cast(instr->hydrogen()->right())->Integer32Value();

    if (divisor < 0) divisor = -divisor;

    Label positive_dividend, done;
    __ cmp(dividend, Operand(0));
    __ b(pl, &positive_dividend);
    __ rsb(result, dividend, Operand(0));
    __ and_(result, result, Operand(divisor - 1), SetCC);
    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      DeoptimizeIf(eq, instr->environment());
    }
    __ rsb(result, result, Operand(0));
    __ b(&done);
    __ bind(&positive_dividend);
    __ and_(result, dividend, Operand(divisor - 1));
    __ bind(&done);
    return;
  }

  // These registers hold untagged 32 bit values.
  Register left = ToRegister(instr->InputAt(0));
  Register right = ToRegister(instr->InputAt(1));
  Register result = ToRegister(instr->result());

  Register scratch = scratch0();
  Register scratch2 = ToRegister(instr->TempAt(0));
  DwVfpRegister dividend = ToDoubleRegister(instr->TempAt(1));
  DwVfpRegister divisor = ToDoubleRegister(instr->TempAt(2));
  DwVfpRegister quotient = double_scratch0();

  ASSERT(!dividend.is(divisor));
  ASSERT(!dividend.is(quotient));
  ASSERT(!divisor.is(quotient));
  ASSERT(!scratch.is(left));
  ASSERT(!scratch.is(right));
  ASSERT(!scratch.is(result));

  Label done, vfp_modulo, both_positive, right_negative;

  // Check for x % 0.
  if (instr->hydrogen()->CheckFlag(HValue::kCanBeDivByZero)) {
    __ cmp(right, Operand(0));
    DeoptimizeIf(eq, instr->environment());
  }

  __ Move(result, left);

  // (0 % x) must yield 0 (if x is finite, which is the case here).
  __ cmp(left, Operand(0));
  __ b(eq, &done);
  // Preload right in a vfp register.
  __ vmov(divisor.low(), right);
  __ b(lt, &vfp_modulo);

  __ cmp(left, Operand(right));
  __ b(lt, &done);

  // Check for (positive) power of two on the right hand side.
  __ JumpIfNotPowerOfTwoOrZeroAndNeg(right,
                                     scratch,
                                     &right_negative,
                                     &both_positive);
  // Perform modulo operation (scratch contains right - 1).
  __ and_(result, scratch, Operand(left));
  __ b(&done);

  __ bind(&right_negative);
  // Negate right. The sign of the divisor does not matter.
  __ rsb(right, right, Operand(0));

  __ bind(&both_positive);
  const int kUnfolds = 3;
  // If the right hand side is smaller than the (nonnegative)
  // left hand side, the left hand side is the result.
  // Else try a few subtractions of the left hand side.
  __ mov(scratch, left);
  for (int i = 0; i < kUnfolds; i++) {
    // Check if the left hand side is less or equal than the
    // the right hand side.
    __ cmp(scratch, Operand(right));
    __ mov(result, scratch, LeaveCC, lt);
    __ b(lt, &done);
    // If not, reduce the left hand side by the right hand
    // side and check again.
    if (i < kUnfolds - 1) __ sub(scratch, scratch, right);
  }

  __ bind(&vfp_modulo);
  // Load the arguments in VFP registers.
  // The divisor value is preloaded before. Be careful that 'right' is only live
  // on entry.
  __ vmov(dividend.low(), left);
  // From here on don't use right as it may have been reallocated (for example
  // to scratch2).
  right = no_reg;

  __ vcvt_f64_s32(dividend, dividend.low());
  __ vcvt_f64_s32(divisor, divisor.low());

  // We do not care about the sign of the divisor.
  __ vabs(divisor, divisor);
  // Compute the quotient and round it to a 32bit integer.
  __ vdiv(quotient, dividend, divisor);
  __ vcvt_s32_f64(quotient.low(), quotient);
  __ vcvt_f64_s32(quotient, quotient.low());

  // Compute the remainder in result.
  DwVfpRegister double_scratch = dividend;
  __ vmul(double_scratch, divisor, quotient);
  __ vcvt_s32_f64(double_scratch.low(), double_scratch);
  __ vmov(scratch, double_scratch.low());

  if (!instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    __ sub(result, left, scratch);
  } else {
    Label ok;
    // Check for -0.
    __ sub(scratch2, left, scratch, SetCC);
    __ b(ne, &ok);
    __ cmp(left, Operand(0));
    DeoptimizeIf(mi, instr->environment());
    __ bind(&ok);
    // Load the result and we are done.
    __ mov(result, scratch2);
  }

  __ bind(&done);
}


void LCodeGen::DoDivI(LDivI* instr) {
  class DeferredDivI: public LDeferredCode {
   public:
    DeferredDivI(LCodeGen* codegen, LDivI* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() {
      codegen()->DoDeferredBinaryOpStub(instr_, Token::DIV);
    }
   private:
    LDivI* instr_;
  };

  const Register left = ToRegister(instr->InputAt(0));
  const Register right = ToRegister(instr->InputAt(1));
  const Register scratch = scratch0();
  const Register result = ToRegister(instr->result());

  // Check for x / 0.
  if (instr->hydrogen()->CheckFlag(HValue::kCanBeDivByZero)) {
    __ cmp(right, Operand(0));
    DeoptimizeIf(eq, instr->environment());
  }

  // Check for (0 / -x) that will produce negative zero.
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    Label left_not_zero;
    __ cmp(left, Operand(0));
    __ b(ne, &left_not_zero);
    __ cmp(right, Operand(0));
    DeoptimizeIf(mi, instr->environment());
    __ bind(&left_not_zero);
  }

  // Check for (-kMinInt / -1).
  if (instr->hydrogen()->CheckFlag(HValue::kCanOverflow)) {
    Label left_not_min_int;
    __ cmp(left, Operand(kMinInt));
    __ b(ne, &left_not_min_int);
    __ cmp(right, Operand(-1));
    DeoptimizeIf(eq, instr->environment());
    __ bind(&left_not_min_int);
  }

  Label done, deoptimize;
  // Test for a few common cases first.
  __ cmp(right, Operand(1));
  __ mov(result, left, LeaveCC, eq);
  __ b(eq, &done);

  __ cmp(right, Operand(2));
  __ tst(left, Operand(1), eq);
  __ mov(result, Operand(left, ASR, 1), LeaveCC, eq);
  __ b(eq, &done);

  __ cmp(right, Operand(4));
  __ tst(left, Operand(3), eq);
  __ mov(result, Operand(left, ASR, 2), LeaveCC, eq);
  __ b(eq, &done);

  // Call the stub. The numbers in r0 and r1 have
  // to be tagged to Smis. If that is not possible, deoptimize.
  DeferredDivI* deferred = new DeferredDivI(this, instr);

  __ TrySmiTag(left, &deoptimize, scratch);
  __ TrySmiTag(right, &deoptimize, scratch);

  __ b(al, deferred->entry());
  __ bind(deferred->exit());

  // If the result in r0 is a Smi, untag it, else deoptimize.
  __ JumpIfNotSmi(result, &deoptimize);
  __ SmiUntag(result);
  __ b(&done);

  __ bind(&deoptimize);
  DeoptimizeIf(al, instr->environment());
  __ bind(&done);
}


template<int T>
void LCodeGen::DoDeferredBinaryOpStub(LTemplateInstruction<1, 2, T>* instr,
                                      Token::Value op) {
  Register left = ToRegister(instr->InputAt(0));
  Register right = ToRegister(instr->InputAt(1));

  PushSafepointRegistersScope scope(this, Safepoint::kWithRegistersAndDoubles);
  // Move left to r1 and right to r0 for the stub call.
  if (left.is(r1)) {
    __ Move(r0, right);
  } else if (left.is(r0) && right.is(r1)) {
    __ Swap(r0, r1, r2);
  } else if (left.is(r0)) {
    ASSERT(!right.is(r1));
    __ mov(r1, r0);
    __ mov(r0, right);
  } else {
    ASSERT(!left.is(r0) && !right.is(r0));
    __ mov(r0, right);
    __ mov(r1, left);
  }
  BinaryOpStub stub(op, OVERWRITE_LEFT);
  __ CallStub(&stub);
  RecordSafepointWithRegistersAndDoubles(instr->pointer_map(),
                                         0,
                                         Safepoint::kNoLazyDeopt);
  // Overwrite the stored value of r0 with the result of the stub.
  __ StoreToSafepointRegistersAndDoublesSlot(r0, r0);
}


void LCodeGen::DoMulI(LMulI* instr) {
  Register scratch = scratch0();
  Register result = ToRegister(instr->result());
  // Note that result may alias left.
  Register left = ToRegister(instr->InputAt(0));
  LOperand* right_op = instr->InputAt(1);

  bool can_overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);
  bool bailout_on_minus_zero =
    instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero);

  if (right_op->IsConstantOperand() && !can_overflow) {
    // Use optimized code for specific constants.
    int32_t constant = ToInteger32(LConstantOperand::cast(right_op));

    if (bailout_on_minus_zero && (constant < 0)) {
      // The case of a null constant will be handled separately.
      // If constant is negative and left is null, the result should be -0.
      __ cmp(left, Operand(0));
      DeoptimizeIf(eq, instr->environment());
    }

    switch (constant) {
      case -1:
        __ rsb(result, left, Operand(0));
        break;
      case 0:
        if (bailout_on_minus_zero) {
          // If left is strictly negative and the constant is null, the
          // result is -0. Deoptimize if required, otherwise return 0.
          __ cmp(left, Operand(0));
          DeoptimizeIf(mi, instr->environment());
        }
        __ mov(result, Operand(0));
        break;
      case 1:
        __ Move(result, left);
        break;
      default:
        // Multiplying by powers of two and powers of two plus or minus
        // one can be done faster with shifted operands.
        // For other constants we emit standard code.
        int32_t mask = constant >> 31;
        uint32_t constant_abs = (constant + mask) ^ mask;

        if (IsPowerOf2(constant_abs) ||
            IsPowerOf2(constant_abs - 1) ||
            IsPowerOf2(constant_abs + 1)) {
          if (IsPowerOf2(constant_abs)) {
            int32_t shift = WhichPowerOf2(constant_abs);
            __ mov(result, Operand(left, LSL, shift));
          } else if (IsPowerOf2(constant_abs - 1)) {
            int32_t shift = WhichPowerOf2(constant_abs - 1);
            __ add(result, left, Operand(left, LSL, shift));
          } else if (IsPowerOf2(constant_abs + 1)) {
            int32_t shift = WhichPowerOf2(constant_abs + 1);
            __ rsb(result, left, Operand(left, LSL, shift));
          }

          // Correct the sign of the result is the constant is negative.
          if (constant < 0)  __ rsb(result, result, Operand(0));

        } else {
          // Generate standard code.
          __ mov(ip, Operand(constant));
          __ mul(result, left, ip);
        }
    }

  } else {
    Register right = EmitLoadRegister(right_op, scratch);
    if (bailout_on_minus_zero) {
      __ orr(ToRegister(instr->TempAt(0)), left, right);
    }

    if (can_overflow) {
      // scratch:result = left * right.
      __ smull(result, scratch, left, right);
      __ cmp(scratch, Operand(result, ASR, 31));
      DeoptimizeIf(ne, instr->environment());
    } else {
      __ mul(result, left, right);
    }

    if (bailout_on_minus_zero) {
      // Bail out if the result is supposed to be negative zero.
      Label done;
      __ cmp(result, Operand(0));
      __ b(ne, &done);
      __ cmp(ToRegister(instr->TempAt(0)), Operand(0));
      DeoptimizeIf(mi, instr->environment());
      __ bind(&done);
    }
  }
}


void LCodeGen::DoBitI(LBitI* instr) {
  LOperand* left_op = instr->InputAt(0);
  LOperand* right_op = instr->InputAt(1);
  ASSERT(left_op->IsRegister());
  Register left = ToRegister(left_op);
  Register result = ToRegister(instr->result());
  Operand right(no_reg);

  if (right_op->IsStackSlot() || right_op->IsArgument()) {
    right = Operand(EmitLoadRegister(right_op, ip));
  } else {
    ASSERT(right_op->IsRegister() || right_op->IsConstantOperand());
    right = ToOperand(right_op);
  }

  switch (instr->op()) {
    case Token::BIT_AND:
      __ and_(result, left, right);
      break;
    case Token::BIT_OR:
      __ orr(result, left, right);
      break;
    case Token::BIT_XOR:
      __ eor(result, left, right);
      break;
    default:
      UNREACHABLE();
      break;
  }
}


void LCodeGen::DoShiftI(LShiftI* instr) {
  // Both 'left' and 'right' are "used at start" (see LCodeGen::DoShift), so
  // result may alias either of them.
  LOperand* right_op = instr->InputAt(1);
  Register left = ToRegister(instr->InputAt(0));
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();
  if (right_op->IsRegister()) {
    // Mask the right_op operand.
    __ and_(scratch, ToRegister(right_op), Operand(0x1F));
    switch (instr->op()) {
      case Token::SAR:
        __ mov(result, Operand(left, ASR, scratch));
        break;
      case Token::SHR:
        if (instr->can_deopt()) {
          __ mov(result, Operand(left, LSR, scratch), SetCC);
          DeoptimizeIf(mi, instr->environment());
        } else {
          __ mov(result, Operand(left, LSR, scratch));
        }
        break;
      case Token::SHL:
        __ mov(result, Operand(left, LSL, scratch));
        break;
      default:
        UNREACHABLE();
        break;
    }
  } else {
    // Mask the right_op operand.
    int value = ToInteger32(LConstantOperand::cast(right_op));
    uint8_t shift_count = static_cast<uint8_t>(value & 0x1F);
    switch (instr->op()) {
      case Token::SAR:
        if (shift_count != 0) {
          __ mov(result, Operand(left, ASR, shift_count));
        } else {
          __ Move(result, left);
        }
        break;
      case Token::SHR:
        if (shift_count != 0) {
          __ mov(result, Operand(left, LSR, shift_count));
        } else {
          if (instr->can_deopt()) {
            __ tst(left, Operand(0x80000000));
            DeoptimizeIf(ne, instr->environment());
          }
          __ Move(result, left);
        }
        break;
      case Token::SHL:
        if (shift_count != 0) {
          __ mov(result, Operand(left, LSL, shift_count));
        } else {
          __ Move(result, left);
        }
        break;
      default:
        UNREACHABLE();
        break;
    }
  }
}


void LCodeGen::DoSubI(LSubI* instr) {
  LOperand* left = instr->InputAt(0);
  LOperand* right = instr->InputAt(1);
  LOperand* result = instr->result();
  bool can_overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);
  SBit set_cond = can_overflow ? SetCC : LeaveCC;

  if (right->IsStackSlot() || right->IsArgument()) {
    Register right_reg = EmitLoadRegister(right, ip);
    __ sub(ToRegister(result), ToRegister(left), Operand(right_reg), set_cond);
  } else {
    ASSERT(right->IsRegister() || right->IsConstantOperand());
    __ sub(ToRegister(result), ToRegister(left), ToOperand(right), set_cond);
  }

  if (can_overflow) {
    DeoptimizeIf(vs, instr->environment());
  }
}


void LCodeGen::DoConstantI(LConstantI* instr) {
  ASSERT(instr->result()->IsRegister());
  __ mov(ToRegister(instr->result()), Operand(instr->value()));
}


void LCodeGen::DoConstantD(LConstantD* instr) {
  ASSERT(instr->result()->IsDoubleRegister());
  DwVfpRegister result = ToDoubleRegister(instr->result());
  double v = instr->value();
  __ Vmov(result, v);
}


void LCodeGen::DoConstantT(LConstantT* instr) {
  ASSERT(instr->result()->IsRegister());
  __ mov(ToRegister(instr->result()), Operand(instr->value()));
}


void LCodeGen::DoJSArrayLength(LJSArrayLength* instr) {
  Register result = ToRegister(instr->result());
  Register array = ToRegister(instr->InputAt(0));
  __ ldr(result, FieldMemOperand(array, JSArray::kLengthOffset));
}


void LCodeGen::DoFixedArrayBaseLength(LFixedArrayBaseLength* instr) {
  Register result = ToRegister(instr->result());
  Register array = ToRegister(instr->InputAt(0));
  __ ldr(result, FieldMemOperand(array, FixedArrayBase::kLengthOffset));
}


void LCodeGen::DoElementsKind(LElementsKind* instr) {
  Register result = ToRegister(instr->result());
  Register input = ToRegister(instr->InputAt(0));

  // Load map into |result|.
  __ ldr(result, FieldMemOperand(input, HeapObject::kMapOffset));
  // Load the map's "bit field 2" into |result|. We only need the first byte,
  // but the following bit field extraction takes care of that anyway.
  __ ldr(result, FieldMemOperand(result, Map::kBitField2Offset));
  // Retrieve elements_kind from bit field 2.
  __ ubfx(result, result, Map::kElementsKindShift, Map::kElementsKindBitCount);
}


void LCodeGen::DoValueOf(LValueOf* instr) {
  Register input = ToRegister(instr->InputAt(0));
  Register result = ToRegister(instr->result());
  Register map = ToRegister(instr->TempAt(0));
  Label done;

  // If the object is a smi return the object.
  __ tst(input, Operand(kSmiTagMask));
  __ Move(result, input, eq);
  __ b(eq, &done);

  // If the object is not a value type, return the object.
  __ CompareObjectType(input, map, map, JS_VALUE_TYPE);
  __ Move(result, input, ne);
  __ b(ne, &done);
  __ ldr(result, FieldMemOperand(input, JSValue::kValueOffset));

  __ bind(&done);
}


void LCodeGen::DoBitNotI(LBitNotI* instr) {
  Register input = ToRegister(instr->InputAt(0));
  Register result = ToRegister(instr->result());
  __ mvn(result, Operand(input));
}


void LCodeGen::DoThrow(LThrow* instr) {
  Register input_reg = EmitLoadRegister(instr->InputAt(0), ip);
  __ push(input_reg);
  CallRuntime(Runtime::kThrow, 1, instr);

  if (FLAG_debug_code) {
    __ stop("Unreachable code.");
  }
}


void LCodeGen::DoAddI(LAddI* instr) {
  LOperand* left = instr->InputAt(0);
  LOperand* right = instr->InputAt(1);
  LOperand* result = instr->result();
  bool can_overflow = instr->hydrogen()->CheckFlag(HValue::kCanOverflow);
  SBit set_cond = can_overflow ? SetCC : LeaveCC;

  if (right->IsStackSlot() || right->IsArgument()) {
    Register right_reg = EmitLoadRegister(right, ip);
    __ add(ToRegister(result), ToRegister(left), Operand(right_reg), set_cond);
  } else {
    ASSERT(right->IsRegister() || right->IsConstantOperand());
    __ add(ToRegister(result), ToRegister(left), ToOperand(right), set_cond);
  }

  if (can_overflow) {
    DeoptimizeIf(vs, instr->environment());
  }
}


void LCodeGen::DoArithmeticD(LArithmeticD* instr) {
  DoubleRegister left = ToDoubleRegister(instr->InputAt(0));
  DoubleRegister right = ToDoubleRegister(instr->InputAt(1));
  DoubleRegister result = ToDoubleRegister(instr->result());
  switch (instr->op()) {
    case Token::ADD:
      __ vadd(result, left, right);
      break;
    case Token::SUB:
      __ vsub(result, left, right);
      break;
    case Token::MUL:
      __ vmul(result, left, right);
      break;
    case Token::DIV:
      __ vdiv(result, left, right);
      break;
    case Token::MOD: {
      // Save r0-r3 on the stack.
      __ stm(db_w, sp, r0.bit() | r1.bit() | r2.bit() | r3.bit());

      __ PrepareCallCFunction(0, 2, scratch0());
      __ SetCallCDoubleArguments(left, right);
      __ CallCFunction(
          ExternalReference::double_fp_operation(Token::MOD, isolate()),
          0, 2);
      // Move the result in the double result register.
      __ GetCFunctionDoubleResult(result);

      // Restore r0-r3.
      __ ldm(ia_w, sp, r0.bit() | r1.bit() | r2.bit() | r3.bit());
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
}


void LCodeGen::DoArithmeticT(LArithmeticT* instr) {
  ASSERT(ToRegister(instr->InputAt(0)).is(r1));
  ASSERT(ToRegister(instr->InputAt(1)).is(r0));
  ASSERT(ToRegister(instr->result()).is(r0));

  BinaryOpStub stub(instr->op(), NO_OVERWRITE);
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
  __ nop();  // Signals no inlined code.
}


int LCodeGen::GetNextEmittedBlock(int block) {
  for (int i = block + 1; i < graph()->blocks()->length(); ++i) {
    LLabel* label = chunk_->GetLabel(i);
    if (!label->HasReplacement()) return i;
  }
  return -1;
}


void LCodeGen::EmitBranch(int left_block, int right_block, Condition cc) {
  int next_block = GetNextEmittedBlock(current_block_);
  right_block = chunk_->LookupDestination(right_block);
  left_block = chunk_->LookupDestination(left_block);

  if (right_block == left_block) {
    EmitGoto(left_block);
  } else if (left_block == next_block) {
    __ b(NegateCondition(cc), chunk_->GetAssemblyLabel(right_block));
  } else if (right_block == next_block) {
    __ b(cc, chunk_->GetAssemblyLabel(left_block));
  } else {
    __ b(cc, chunk_->GetAssemblyLabel(left_block));
    __ b(chunk_->GetAssemblyLabel(right_block));
  }
}


void LCodeGen::DoBranch(LBranch* instr) {
  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  Representation r = instr->hydrogen()->value()->representation();
  if (r.IsInteger32()) {
    Register reg = ToRegister(instr->InputAt(0));
    __ cmp(reg, Operand(0));
    EmitBranch(true_block, false_block, ne);
  } else if (r.IsDouble()) {
    DoubleRegister reg = ToDoubleRegister(instr->InputAt(0));
    Register scratch = scratch0();

    // Test the double value. Zero and NaN are false.
    __ VFPCompareAndLoadFlags(reg, 0.0, scratch);
    __ tst(scratch, Operand(kVFPZConditionFlagBit | kVFPVConditionFlagBit));
    EmitBranch(true_block, false_block, eq);
  } else {
    ASSERT(r.IsTagged());
    Register reg = ToRegister(instr->InputAt(0));
    HType type = instr->hydrogen()->value()->type();
    if (type.IsBoolean()) {
      __ CompareRoot(reg, Heap::kTrueValueRootIndex);
      EmitBranch(true_block, false_block, eq);
    } else if (type.IsSmi()) {
      __ cmp(reg, Operand(0));
      EmitBranch(true_block, false_block, ne);
    } else {
      Label* true_label = chunk_->GetAssemblyLabel(true_block);
      Label* false_label = chunk_->GetAssemblyLabel(false_block);

      ToBooleanStub::Types expected = instr->hydrogen()->expected_input_types();
      // Avoid deopts in the case where we've never executed this path before.
      if (expected.IsEmpty()) expected = ToBooleanStub::all_types();

      if (expected.Contains(ToBooleanStub::UNDEFINED)) {
        // undefined -> false.
        __ CompareRoot(reg, Heap::kUndefinedValueRootIndex);
        __ b(eq, false_label);
      }
      if (expected.Contains(ToBooleanStub::BOOLEAN)) {
        // Boolean -> its value.
        __ CompareRoot(reg, Heap::kTrueValueRootIndex);
        __ b(eq, true_label);
        __ CompareRoot(reg, Heap::kFalseValueRootIndex);
        __ b(eq, false_label);
      }
      if (expected.Contains(ToBooleanStub::NULL_TYPE)) {
        // 'null' -> false.
        __ CompareRoot(reg, Heap::kNullValueRootIndex);
        __ b(eq, false_label);
      }

      if (expected.Contains(ToBooleanStub::SMI)) {
        // Smis: 0 -> false, all other -> true.
        __ cmp(reg, Operand(0));
        __ b(eq, false_label);
        __ JumpIfSmi(reg, true_label);
      } else if (expected.NeedsMap()) {
        // If we need a map later and have a Smi -> deopt.
        __ tst(reg, Operand(kSmiTagMask));
        DeoptimizeIf(eq, instr->environment());
      }

      const Register map = scratch0();
      if (expected.NeedsMap()) {
        __ ldr(map, FieldMemOperand(reg, HeapObject::kMapOffset));

        if (expected.CanBeUndetectable()) {
          // Undetectable -> false.
          __ ldrb(ip, FieldMemOperand(map, Map::kBitFieldOffset));
          __ tst(ip, Operand(1 << Map::kIsUndetectable));
          __ b(ne, false_label);
        }
      }

      if (expected.Contains(ToBooleanStub::SPEC_OBJECT)) {
        // spec object -> true.
        __ CompareInstanceType(map, ip, FIRST_SPEC_OBJECT_TYPE);
        __ b(ge, true_label);
      }

      if (expected.Contains(ToBooleanStub::STRING)) {
        // String value -> false iff empty.
        Label not_string;
        __ CompareInstanceType(map, ip, FIRST_NONSTRING_TYPE);
        __ b(ge, &not_string);
        __ ldr(ip, FieldMemOperand(reg, String::kLengthOffset));
        __ cmp(ip, Operand(0));
        __ b(ne, true_label);
        __ b(false_label);
        __ bind(&not_string);
      }

      if (expected.Contains(ToBooleanStub::HEAP_NUMBER)) {
        // heap number -> false iff +0, -0, or NaN.
        DoubleRegister dbl_scratch = double_scratch0();
        Label not_heap_number;
        __ CompareRoot(map, Heap::kHeapNumberMapRootIndex);
        __ b(ne, &not_heap_number);
        __ vldr(dbl_scratch, FieldMemOperand(reg, HeapNumber::kValueOffset));
        __ VFPCompareAndSetFlags(dbl_scratch, 0.0);
        __ b(vs, false_label);  // NaN -> false.
        __ b(eq, false_label);  // +0, -0 -> false.
        __ b(true_label);
        __ bind(&not_heap_number);
      }

      // We've seen something for the first time -> deopt.
      DeoptimizeIf(al, instr->environment());
    }
  }
}


void LCodeGen::EmitGoto(int block) {
  block = chunk_->LookupDestination(block);
  int next_block = GetNextEmittedBlock(current_block_);
  if (block != next_block) {
    __ jmp(chunk_->GetAssemblyLabel(block));
  }
}


void LCodeGen::DoGoto(LGoto* instr) {
  EmitGoto(instr->block_id());
}


Condition LCodeGen::TokenToCondition(Token::Value op, bool is_unsigned) {
  Condition cond = kNoCondition;
  switch (op) {
    case Token::EQ:
    case Token::EQ_STRICT:
      cond = eq;
      break;
    case Token::LT:
      cond = is_unsigned ? lo : lt;
      break;
    case Token::GT:
      cond = is_unsigned ? hi : gt;
      break;
    case Token::LTE:
      cond = is_unsigned ? ls : le;
      break;
    case Token::GTE:
      cond = is_unsigned ? hs : ge;
      break;
    case Token::IN:
    case Token::INSTANCEOF:
    default:
      UNREACHABLE();
  }
  return cond;
}


void LCodeGen::EmitCmpI(LOperand* left, LOperand* right) {
  __ cmp(ToRegister(left), ToRegister(right));
}


void LCodeGen::DoCmpIDAndBranch(LCmpIDAndBranch* instr) {
  LOperand* left = instr->InputAt(0);
  LOperand* right = instr->InputAt(1);
  int false_block = chunk_->LookupDestination(instr->false_block_id());
  int true_block = chunk_->LookupDestination(instr->true_block_id());

  if (instr->is_double()) {
    // Compare left and right as doubles and load the
    // resulting flags into the normal status register.
    __ VFPCompareAndSetFlags(ToDoubleRegister(left), ToDoubleRegister(right));
    // If a NaN is involved, i.e. the result is unordered (V set),
    // jump to false block label.
    __ b(vs, chunk_->GetAssemblyLabel(false_block));
  } else {
    EmitCmpI(left, right);
  }

  Condition cc = TokenToCondition(instr->op(), instr->is_double());
  EmitBranch(true_block, false_block, cc);
}


void LCodeGen::DoCmpObjectEqAndBranch(LCmpObjectEqAndBranch* instr) {
  Register left = ToRegister(instr->InputAt(0));
  Register right = ToRegister(instr->InputAt(1));
  int false_block = chunk_->LookupDestination(instr->false_block_id());
  int true_block = chunk_->LookupDestination(instr->true_block_id());

  __ cmp(left, Operand(right));
  EmitBranch(true_block, false_block, eq);
}


void LCodeGen::DoCmpConstantEqAndBranch(LCmpConstantEqAndBranch* instr) {
  Register left = ToRegister(instr->InputAt(0));
  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  __ cmp(left, Operand(instr->hydrogen()->right()));
  EmitBranch(true_block, false_block, eq);
}


void LCodeGen::DoIsNullAndBranch(LIsNullAndBranch* instr) {
  Register scratch = scratch0();
  Register reg = ToRegister(instr->InputAt(0));

  // TODO(fsc): If the expression is known to be a smi, then it's
  // definitely not null. Jump to the false block.

  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  __ LoadRoot(ip, Heap::kNullValueRootIndex);
  __ cmp(reg, ip);
  if (instr->is_strict()) {
    EmitBranch(true_block, false_block, eq);
  } else {
    Label* true_label = chunk_->GetAssemblyLabel(true_block);
    Label* false_label = chunk_->GetAssemblyLabel(false_block);
    __ b(eq, true_label);
    __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
    __ cmp(reg, ip);
    __ b(eq, true_label);
    __ JumpIfSmi(reg, false_label);
    // Check for undetectable objects by looking in the bit field in
    // the map. The object has already been smi checked.
    __ ldr(scratch, FieldMemOperand(reg, HeapObject::kMapOffset));
    __ ldrb(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ tst(scratch, Operand(1 << Map::kIsUndetectable));
    EmitBranch(true_block, false_block, ne);
  }
}


Condition LCodeGen::EmitIsObject(Register input,
                                 Register temp1,
                                 Label* is_not_object,
                                 Label* is_object) {
  Register temp2 = scratch0();
  __ JumpIfSmi(input, is_not_object);

  __ LoadRoot(temp2, Heap::kNullValueRootIndex);
  __ cmp(input, temp2);
  __ b(eq, is_object);

  // Load map.
  __ ldr(temp1, FieldMemOperand(input, HeapObject::kMapOffset));
  // Undetectable objects behave like undefined.
  __ ldrb(temp2, FieldMemOperand(temp1, Map::kBitFieldOffset));
  __ tst(temp2, Operand(1 << Map::kIsUndetectable));
  __ b(ne, is_not_object);

  // Load instance type and check that it is in object type range.
  __ ldrb(temp2, FieldMemOperand(temp1, Map::kInstanceTypeOffset));
  __ cmp(temp2, Operand(FIRST_NONCALLABLE_SPEC_OBJECT_TYPE));
  __ b(lt, is_not_object);
  __ cmp(temp2, Operand(LAST_NONCALLABLE_SPEC_OBJECT_TYPE));
  return le;
}


void LCodeGen::DoIsObjectAndBranch(LIsObjectAndBranch* instr) {
  Register reg = ToRegister(instr->InputAt(0));
  Register temp1 = ToRegister(instr->TempAt(0));

  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());
  Label* true_label = chunk_->GetAssemblyLabel(true_block);
  Label* false_label = chunk_->GetAssemblyLabel(false_block);

  Condition true_cond =
      EmitIsObject(reg, temp1, false_label, true_label);

  EmitBranch(true_block, false_block, true_cond);
}


void LCodeGen::DoIsSmiAndBranch(LIsSmiAndBranch* instr) {
  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  Register input_reg = EmitLoadRegister(instr->InputAt(0), ip);
  __ tst(input_reg, Operand(kSmiTagMask));
  EmitBranch(true_block, false_block, eq);
}


void LCodeGen::DoIsUndetectableAndBranch(LIsUndetectableAndBranch* instr) {
  Register input = ToRegister(instr->InputAt(0));
  Register temp = ToRegister(instr->TempAt(0));

  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  __ JumpIfSmi(input, chunk_->GetAssemblyLabel(false_block));
  __ ldr(temp, FieldMemOperand(input, HeapObject::kMapOffset));
  __ ldrb(temp, FieldMemOperand(temp, Map::kBitFieldOffset));
  __ tst(temp, Operand(1 << Map::kIsUndetectable));
  EmitBranch(true_block, false_block, ne);
}


static InstanceType TestType(HHasInstanceTypeAndBranch* instr) {
  InstanceType from = instr->from();
  InstanceType to = instr->to();
  if (from == FIRST_TYPE) return to;
  ASSERT(from == to || to == LAST_TYPE);
  return from;
}


static Condition BranchCondition(HHasInstanceTypeAndBranch* instr) {
  InstanceType from = instr->from();
  InstanceType to = instr->to();
  if (from == to) return eq;
  if (to == LAST_TYPE) return hs;
  if (from == FIRST_TYPE) return ls;
  UNREACHABLE();
  return eq;
}


void LCodeGen::DoHasInstanceTypeAndBranch(LHasInstanceTypeAndBranch* instr) {
  Register scratch = scratch0();
  Register input = ToRegister(instr->InputAt(0));

  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  Label* false_label = chunk_->GetAssemblyLabel(false_block);

  __ JumpIfSmi(input, false_label);

  __ CompareObjectType(input, scratch, scratch, TestType(instr->hydrogen()));
  EmitBranch(true_block, false_block, BranchCondition(instr->hydrogen()));
}


void LCodeGen::DoGetCachedArrayIndex(LGetCachedArrayIndex* instr) {
  Register input = ToRegister(instr->InputAt(0));
  Register result = ToRegister(instr->result());

  if (FLAG_debug_code) {
    __ AbortIfNotString(input);
  }

  __ ldr(result, FieldMemOperand(input, String::kHashFieldOffset));
  __ IndexFromHash(result, result);
}


void LCodeGen::DoHasCachedArrayIndexAndBranch(
    LHasCachedArrayIndexAndBranch* instr) {
  Register input = ToRegister(instr->InputAt(0));
  Register scratch = scratch0();

  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  __ ldr(scratch,
         FieldMemOperand(input, String::kHashFieldOffset));
  __ tst(scratch, Operand(String::kContainsCachedArrayIndexMask));
  EmitBranch(true_block, false_block, eq);
}


// Branches to a label or falls through with the answer in flags.  Trashes
// the temp registers, but not the input.  Only input and temp2 may alias.
void LCodeGen::EmitClassOfTest(Label* is_true,
                               Label* is_false,
                               Handle<String>class_name,
                               Register input,
                               Register temp,
                               Register temp2) {
  ASSERT(!input.is(temp));
  ASSERT(!temp.is(temp2));  // But input and temp2 may be the same register.
  __ JumpIfSmi(input, is_false);
  __ CompareObjectType(input, temp, temp2, FIRST_SPEC_OBJECT_TYPE);
  __ b(lt, is_false);

  // Map is now in temp.
  // Functions have class 'Function'.
  __ CompareInstanceType(temp, temp2, FIRST_CALLABLE_SPEC_OBJECT_TYPE);
  if (class_name->IsEqualTo(CStrVector("Function"))) {
    __ b(ge, is_true);
  } else {
    __ b(ge, is_false);
  }

  // Check if the constructor in the map is a function.
  __ ldr(temp, FieldMemOperand(temp, Map::kConstructorOffset));

  // As long as LAST_CALLABLE_SPEC_OBJECT_TYPE is the last instance type and
  // FIRST_CALLABLE_SPEC_OBJECT_TYPE comes right after
  // LAST_NONCALLABLE_SPEC_OBJECT_TYPE, we can avoid checking for the latter.
  STATIC_ASSERT(LAST_TYPE == LAST_CALLABLE_SPEC_OBJECT_TYPE);
  STATIC_ASSERT(FIRST_CALLABLE_SPEC_OBJECT_TYPE ==
                LAST_NONCALLABLE_SPEC_OBJECT_TYPE + 1);

  // Objects with a non-function constructor have class 'Object'.
  __ CompareObjectType(temp, temp2, temp2, JS_FUNCTION_TYPE);
  if (class_name->IsEqualTo(CStrVector("Object"))) {
    __ b(ne, is_true);
  } else {
    __ b(ne, is_false);
  }

  // temp now contains the constructor function. Grab the
  // instance class name from there.
  __ ldr(temp, FieldMemOperand(temp, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(temp, FieldMemOperand(temp,
                               SharedFunctionInfo::kInstanceClassNameOffset));
  // The class name we are testing against is a symbol because it's a literal.
  // The name in the constructor is a symbol because of the way the context is
  // booted.  This routine isn't expected to work for random API-created
  // classes and it doesn't have to because you can't access it with natives
  // syntax.  Since both sides are symbols it is sufficient to use an identity
  // comparison.
  __ cmp(temp, Operand(class_name));
  // End with the answer in flags.
}


void LCodeGen::DoClassOfTestAndBranch(LClassOfTestAndBranch* instr) {
  Register input = ToRegister(instr->InputAt(0));
  Register temp = scratch0();
  Register temp2 = ToRegister(instr->TempAt(0));
  Handle<String> class_name = instr->hydrogen()->class_name();

  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  Label* true_label = chunk_->GetAssemblyLabel(true_block);
  Label* false_label = chunk_->GetAssemblyLabel(false_block);

  EmitClassOfTest(true_label, false_label, class_name, input, temp, temp2);

  EmitBranch(true_block, false_block, eq);
}


void LCodeGen::DoCmpMapAndBranch(LCmpMapAndBranch* instr) {
  Register reg = ToRegister(instr->InputAt(0));
  Register temp = ToRegister(instr->TempAt(0));
  int true_block = instr->true_block_id();
  int false_block = instr->false_block_id();

  __ ldr(temp, FieldMemOperand(reg, HeapObject::kMapOffset));
  __ cmp(temp, Operand(instr->map()));
  EmitBranch(true_block, false_block, eq);
}


void LCodeGen::DoInstanceOf(LInstanceOf* instr) {
  ASSERT(ToRegister(instr->InputAt(0)).is(r0));  // Object is in r0.
  ASSERT(ToRegister(instr->InputAt(1)).is(r1));  // Function is in r1.

  InstanceofStub stub(InstanceofStub::kArgsInRegisters);
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);

  __ cmp(r0, Operand(0));
  __ mov(r0, Operand(factory()->false_value()), LeaveCC, ne);
  __ mov(r0, Operand(factory()->true_value()), LeaveCC, eq);
}


void LCodeGen::DoInstanceOfKnownGlobal(LInstanceOfKnownGlobal* instr) {
  class DeferredInstanceOfKnownGlobal: public LDeferredCode {
   public:
    DeferredInstanceOfKnownGlobal(LCodeGen* codegen,
                                  LInstanceOfKnownGlobal* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() {
      codegen()->DoDeferredInstanceOfKnownGlobal(instr_, &map_check_);
    }

    Label* map_check() { return &map_check_; }

   private:
    LInstanceOfKnownGlobal* instr_;
    Label map_check_;
  };

  DeferredInstanceOfKnownGlobal* deferred;
  deferred = new DeferredInstanceOfKnownGlobal(this, instr);

  Label done, false_result;
  Register object = ToRegister(instr->InputAt(0));
  Register temp = ToRegister(instr->TempAt(0));
  Register result = ToRegister(instr->result());

  ASSERT(object.is(r0));
  ASSERT(result.is(r0));

  // A Smi is not instance of anything.
  __ JumpIfSmi(object, &false_result);

  // This is the inlined call site instanceof cache. The two occurences of the
  // hole value will be patched to the last map/result pair generated by the
  // instanceof stub.
  Label cache_miss;
  Register map = temp;
  __ ldr(map, FieldMemOperand(object, HeapObject::kMapOffset));
  __ bind(deferred->map_check());  // Label for calculating code patching.
  // We use Factory::the_hole_value() on purpose instead of loading from the
  // root array to force relocation to be able to later patch with
  // the cached map.
  __ mov(ip, Operand(factory()->the_hole_value()));
  __ cmp(map, Operand(ip));
  __ b(ne, &cache_miss);
  // We use Factory::the_hole_value() on purpose instead of loading from the
  // root array to force relocation to be able to later patch
  // with true or false.
  __ mov(result, Operand(factory()->the_hole_value()));
  __ b(&done);

  // The inlined call site cache did not match. Check null and string before
  // calling the deferred code.
  __ bind(&cache_miss);
  // Null is not instance of anything.
  __ LoadRoot(ip, Heap::kNullValueRootIndex);
  __ cmp(object, Operand(ip));
  __ b(eq, &false_result);

  // String values is not instance of anything.
  Condition is_string = masm_->IsObjectStringType(object, temp);
  __ b(is_string, &false_result);

  // Go to the deferred code.
  __ b(deferred->entry());

  __ bind(&false_result);
  __ LoadRoot(result, Heap::kFalseValueRootIndex);

  // Here result has either true or false. Deferred code also produces true or
  // false object.
  __ bind(deferred->exit());
  __ bind(&done);
}


void LCodeGen::DoDeferredInstanceOfKnownGlobal(LInstanceOfKnownGlobal* instr,
                                               Label* map_check) {
  Register result = ToRegister(instr->result());
  ASSERT(result.is(r0));

  InstanceofStub::Flags flags = InstanceofStub::kNoFlags;
  flags = static_cast<InstanceofStub::Flags>(
      flags | InstanceofStub::kArgsInRegisters);
  flags = static_cast<InstanceofStub::Flags>(
      flags | InstanceofStub::kCallSiteInlineCheck);
  flags = static_cast<InstanceofStub::Flags>(
      flags | InstanceofStub::kReturnTrueFalseObject);
  InstanceofStub stub(flags);

  PushSafepointRegistersScope scope(this, Safepoint::kWithRegisters);

  // Get the temp register reserved by the instruction. This needs to be r4 as
  // its slot of the pushing of safepoint registers is used to communicate the
  // offset to the location of the map check.
  Register temp = ToRegister(instr->TempAt(0));
  ASSERT(temp.is(r4));
  __ mov(InstanceofStub::right(), Operand(instr->function()));
  static const int kAdditionalDelta = 4;
  int delta = masm_->InstructionsGeneratedSince(map_check) + kAdditionalDelta;
  Label before_push_delta;
  __ bind(&before_push_delta);
  __ BlockConstPoolFor(kAdditionalDelta);
  __ mov(temp, Operand(delta * kPointerSize));
  __ StoreToSafepointRegisterSlot(temp, temp);
  CallCodeGeneric(stub.GetCode(),
                  RelocInfo::CODE_TARGET,
                  instr,
                  RECORD_SAFEPOINT_WITH_REGISTERS_AND_NO_ARGUMENTS);
  ASSERT(instr->HasDeoptimizationEnvironment());
  LEnvironment* env = instr->deoptimization_environment();
  safepoints_.RecordLazyDeoptimizationIndex(env->deoptimization_index());
  // Put the result value into the result register slot and
  // restore all registers.
  __ StoreToSafepointRegisterSlot(result, result);
}


static Condition ComputeCompareCondition(Token::Value op) {
  switch (op) {
    case Token::EQ_STRICT:
    case Token::EQ:
      return eq;
    case Token::LT:
      return lt;
    case Token::GT:
      return gt;
    case Token::LTE:
      return le;
    case Token::GTE:
      return ge;
    default:
      UNREACHABLE();
      return kNoCondition;
  }
}


void LCodeGen::DoCmpT(LCmpT* instr) {
  Token::Value op = instr->op();

  Handle<Code> ic = CompareIC::GetUninitialized(op);
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
  __ cmp(r0, Operand(0));  // This instruction also signals no smi code inlined.

  Condition condition = ComputeCompareCondition(op);
  if (op == Token::GT || op == Token::LTE) {
    condition = ReverseCondition(condition);
  }
  __ LoadRoot(ToRegister(instr->result()),
              Heap::kTrueValueRootIndex,
              condition);
  __ LoadRoot(ToRegister(instr->result()),
              Heap::kFalseValueRootIndex,
              NegateCondition(condition));
}


void LCodeGen::DoReturn(LReturn* instr) {
  if (FLAG_trace) {
    // Push the return value on the stack as the parameter.
    // Runtime::TraceExit returns its parameter in r0.
    __ push(r0);
    __ CallRuntime(Runtime::kTraceExit, 1);
  }
  int32_t sp_delta = (GetParameterCount() + 1) * kPointerSize;
  __ mov(sp, fp);
  __ ldm(ia_w, sp, fp.bit() | lr.bit());
  __ add(sp, sp, Operand(sp_delta));
  __ Jump(lr);
}


void LCodeGen::DoLoadGlobalCell(LLoadGlobalCell* instr) {
  Register result = ToRegister(instr->result());
  __ mov(ip, Operand(Handle<Object>(instr->hydrogen()->cell())));
  __ ldr(result, FieldMemOperand(ip, JSGlobalPropertyCell::kValueOffset));
  if (instr->hydrogen()->check_hole_value()) {
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
    __ cmp(result, ip);
    DeoptimizeIf(eq, instr->environment());
  }
}


void LCodeGen::DoLoadGlobalGeneric(LLoadGlobalGeneric* instr) {
  ASSERT(ToRegister(instr->global_object()).is(r0));
  ASSERT(ToRegister(instr->result()).is(r0));

  __ mov(r2, Operand(instr->name()));
  RelocInfo::Mode mode = instr->for_typeof() ? RelocInfo::CODE_TARGET
                                             : RelocInfo::CODE_TARGET_CONTEXT;
  Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
  CallCode(ic, mode, instr);
}


void LCodeGen::DoStoreGlobalCell(LStoreGlobalCell* instr) {
  Register value = ToRegister(instr->InputAt(0));
  Register scratch = scratch0();

  // Load the cell.
  __ mov(scratch, Operand(Handle<Object>(instr->hydrogen()->cell())));

  // If the cell we are storing to contains the hole it could have
  // been deleted from the property dictionary. In that case, we need
  // to update the property details in the property dictionary to mark
  // it as no longer deleted.
  if (instr->hydrogen()->check_hole_value()) {
    Register scratch2 = ToRegister(instr->TempAt(0));
    __ ldr(scratch2,
           FieldMemOperand(scratch, JSGlobalPropertyCell::kValueOffset));
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
    __ cmp(scratch2, ip);
    DeoptimizeIf(eq, instr->environment());
  }

  // Store the value.
  __ str(value, FieldMemOperand(scratch, JSGlobalPropertyCell::kValueOffset));
}


void LCodeGen::DoStoreGlobalGeneric(LStoreGlobalGeneric* instr) {
  ASSERT(ToRegister(instr->global_object()).is(r1));
  ASSERT(ToRegister(instr->value()).is(r0));

  __ mov(r2, Operand(instr->name()));
  Handle<Code> ic = instr->strict_mode()
      ? isolate()->builtins()->StoreIC_Initialize_Strict()
      : isolate()->builtins()->StoreIC_Initialize();
  CallCode(ic, RelocInfo::CODE_TARGET_CONTEXT, instr);
}


void LCodeGen::DoLoadContextSlot(LLoadContextSlot* instr) {
  Register context = ToRegister(instr->context());
  Register result = ToRegister(instr->result());
  __ ldr(result, ContextOperand(context, instr->slot_index()));
}


void LCodeGen::DoStoreContextSlot(LStoreContextSlot* instr) {
  Register context = ToRegister(instr->context());
  Register value = ToRegister(instr->value());
  __ str(value, ContextOperand(context, instr->slot_index()));
  if (instr->needs_write_barrier()) {
    int offset = Context::SlotOffset(instr->slot_index());
    __ RecordWrite(context, Operand(offset), value, scratch0());
  }
}


void LCodeGen::DoLoadNamedField(LLoadNamedField* instr) {
  Register object = ToRegister(instr->InputAt(0));
  Register result = ToRegister(instr->result());
  if (instr->hydrogen()->is_in_object()) {
    __ ldr(result, FieldMemOperand(object, instr->hydrogen()->offset()));
  } else {
    __ ldr(result, FieldMemOperand(object, JSObject::kPropertiesOffset));
    __ ldr(result, FieldMemOperand(result, instr->hydrogen()->offset()));
  }
}


void LCodeGen::EmitLoadFieldOrConstantFunction(Register result,
                                               Register object,
                                               Handle<Map> type,
                                               Handle<String> name) {
  LookupResult lookup;
  type->LookupInDescriptors(NULL, *name, &lookup);
  ASSERT(lookup.IsProperty() &&
         (lookup.type() == FIELD || lookup.type() == CONSTANT_FUNCTION));
  if (lookup.type() == FIELD) {
    int index = lookup.GetLocalFieldIndexFromMap(*type);
    int offset = index * kPointerSize;
    if (index < 0) {
      // Negative property indices are in-object properties, indexed
      // from the end of the fixed part of the object.
      __ ldr(result, FieldMemOperand(object, offset + type->instance_size()));
    } else {
      // Non-negative property indices are in the properties array.
      __ ldr(result, FieldMemOperand(object, JSObject::kPropertiesOffset));
      __ ldr(result, FieldMemOperand(result, offset + FixedArray::kHeaderSize));
    }
  } else {
    Handle<JSFunction> function(lookup.GetConstantFunctionFromMap(*type));
    LoadHeapObject(result, Handle<HeapObject>::cast(function));
  }
}


void LCodeGen::DoLoadNamedFieldPolymorphic(LLoadNamedFieldPolymorphic* instr) {
  Register object = ToRegister(instr->object());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();
  int map_count = instr->hydrogen()->types()->length();
  Handle<String> name = instr->hydrogen()->name();
  if (map_count == 0) {
    ASSERT(instr->hydrogen()->need_generic());
    __ mov(r2, Operand(name));
    Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
    CallCode(ic, RelocInfo::CODE_TARGET, instr);
  } else {
    Label done;
    __ ldr(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
    for (int i = 0; i < map_count - 1; ++i) {
      Handle<Map> map = instr->hydrogen()->types()->at(i);
      Label next;
      __ cmp(scratch, Operand(map));
      __ b(ne, &next);
      EmitLoadFieldOrConstantFunction(result, object, map, name);
      __ b(&done);
      __ bind(&next);
    }
    Handle<Map> map = instr->hydrogen()->types()->last();
    __ cmp(scratch, Operand(map));
    if (instr->hydrogen()->need_generic()) {
      Label generic;
      __ b(ne, &generic);
      EmitLoadFieldOrConstantFunction(result, object, map, name);
      __ b(&done);
      __ bind(&generic);
      __ mov(r2, Operand(name));
      Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
      CallCode(ic, RelocInfo::CODE_TARGET, instr);
    } else {
      DeoptimizeIf(ne, instr->environment());
      EmitLoadFieldOrConstantFunction(result, object, map, name);
    }
    __ bind(&done);
  }
}


void LCodeGen::DoLoadNamedGeneric(LLoadNamedGeneric* instr) {
  ASSERT(ToRegister(instr->object()).is(r0));
  ASSERT(ToRegister(instr->result()).is(r0));

  // Name is always in r2.
  __ mov(r2, Operand(instr->name()));
  Handle<Code> ic = isolate()->builtins()->LoadIC_Initialize();
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoLoadFunctionPrototype(LLoadFunctionPrototype* instr) {
  Register scratch = scratch0();
  Register function = ToRegister(instr->function());
  Register result = ToRegister(instr->result());

  // Check that the function really is a function. Load map into the
  // result register.
  __ CompareObjectType(function, result, scratch, JS_FUNCTION_TYPE);
  DeoptimizeIf(ne, instr->environment());

  // Make sure that the function has an instance prototype.
  Label non_instance;
  __ ldrb(scratch, FieldMemOperand(result, Map::kBitFieldOffset));
  __ tst(scratch, Operand(1 << Map::kHasNonInstancePrototype));
  __ b(ne, &non_instance);

  // Get the prototype or initial map from the function.
  __ ldr(result,
         FieldMemOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // Check that the function has a prototype or an initial map.
  __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
  __ cmp(result, ip);
  DeoptimizeIf(eq, instr->environment());

  // If the function does not have an initial map, we're done.
  Label done;
  __ CompareObjectType(result, scratch, scratch, MAP_TYPE);
  __ b(ne, &done);

  // Get the prototype from the initial map.
  __ ldr(result, FieldMemOperand(result, Map::kPrototypeOffset));
  __ jmp(&done);

  // Non-instance prototype: Fetch prototype from constructor field
  // in initial map.
  __ bind(&non_instance);
  __ ldr(result, FieldMemOperand(result, Map::kConstructorOffset));

  // All done.
  __ bind(&done);
}


void LCodeGen::DoLoadElements(LLoadElements* instr) {
  Register result = ToRegister(instr->result());
  Register input = ToRegister(instr->InputAt(0));
  Register scratch = scratch0();

  __ ldr(result, FieldMemOperand(input, JSObject::kElementsOffset));
  if (FLAG_debug_code) {
    Label done, fail;
    __ ldr(scratch, FieldMemOperand(result, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kFixedArrayMapRootIndex);
    __ cmp(scratch, ip);
    __ b(eq, &done);
    __ LoadRoot(ip, Heap::kFixedCOWArrayMapRootIndex);
    __ cmp(scratch, ip);
    __ b(eq, &done);
    // |scratch| still contains |input|'s map.
    __ ldr(scratch, FieldMemOperand(scratch, Map::kBitField2Offset));
    __ ubfx(scratch, scratch, Map::kElementsKindShift,
            Map::kElementsKindBitCount);
    __ cmp(scratch, Operand(FAST_ELEMENTS));
    __ b(eq, &done);
    __ cmp(scratch, Operand(FIRST_EXTERNAL_ARRAY_ELEMENTS_KIND));
    __ b(lt, &fail);
    __ cmp(scratch, Operand(LAST_EXTERNAL_ARRAY_ELEMENTS_KIND));
    __ b(le, &done);
    __ bind(&fail);
    __ Abort("Check for fast or external elements failed.");
    __ bind(&done);
  }
}


void LCodeGen::DoLoadExternalArrayPointer(
    LLoadExternalArrayPointer* instr) {
  Register to_reg = ToRegister(instr->result());
  Register from_reg  = ToRegister(instr->InputAt(0));
  __ ldr(to_reg, FieldMemOperand(from_reg,
                                 ExternalArray::kExternalPointerOffset));
}


void LCodeGen::DoAccessArgumentsAt(LAccessArgumentsAt* instr) {
  Register arguments = ToRegister(instr->arguments());
  Register length = ToRegister(instr->length());
  Register index = ToRegister(instr->index());
  Register result = ToRegister(instr->result());

  // Bailout index is not a valid argument index. Use unsigned check to get
  // negative check for free.
  __ sub(length, length, index, SetCC);
  DeoptimizeIf(ls, instr->environment());

  // There are two words between the frame pointer and the last argument.
  // Subtracting from length accounts for one of them add one more.
  __ add(length, length, Operand(1));
  __ ldr(result, MemOperand(arguments, length, LSL, kPointerSizeLog2));
}


void LCodeGen::DoLoadKeyedFastElement(LLoadKeyedFastElement* instr) {
  Register elements = ToRegister(instr->elements());
  Register key = EmitLoadRegister(instr->key(), scratch0());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();

  // Load the result.
  __ add(scratch, elements, Operand(key, LSL, kPointerSizeLog2));
  __ ldr(result, FieldMemOperand(scratch, FixedArray::kHeaderSize));

  // Check for the hole value.
  if (instr->hydrogen()->RequiresHoleCheck()) {
    __ LoadRoot(scratch, Heap::kTheHoleValueRootIndex);
    __ cmp(result, scratch);
    DeoptimizeIf(eq, instr->environment());
  }
}


void LCodeGen::DoLoadKeyedFastDoubleElement(
    LLoadKeyedFastDoubleElement* instr) {
  Register elements = ToRegister(instr->elements());
  bool key_is_constant = instr->key()->IsConstantOperand();
  Register key = no_reg;
  DwVfpRegister result = ToDoubleRegister(instr->result());
  Register scratch = scratch0();

  int shift_size =
      ElementsKindToShiftSize(FAST_DOUBLE_ELEMENTS);
  int constant_key = 0;
  if (key_is_constant) {
    constant_key = ToInteger32(LConstantOperand::cast(instr->key()));
    if (constant_key & 0xF0000000) {
      Abort("array index constant value too big.");
    }
  } else {
    key = ToRegister(instr->key());
  }

  Operand operand = key_is_constant
      ? Operand(constant_key * (1 << shift_size) +
                FixedDoubleArray::kHeaderSize - kHeapObjectTag)
      : Operand(key, LSL, shift_size);
  __ add(elements, elements, operand);
  if (!key_is_constant) {
    __ add(elements, elements,
           Operand(FixedDoubleArray::kHeaderSize - kHeapObjectTag));
  }

  if (instr->hydrogen()->RequiresHoleCheck()) {
    // TODO(danno): If no hole check is required, there is no need to allocate
    // elements into a temporary register, instead scratch can be used.
    __ ldr(scratch, MemOperand(elements, sizeof(kHoleNanLower32)));
    __ cmp(scratch, Operand(kHoleNanUpper32));
    DeoptimizeIf(eq, instr->environment());
  }

  __ vldr(result, elements, 0);
}


void LCodeGen::DoLoadKeyedSpecializedArrayElement(
    LLoadKeyedSpecializedArrayElement* instr) {
  Register external_pointer = ToRegister(instr->external_pointer());
  Register key = no_reg;
  ElementsKind elements_kind = instr->elements_kind();
  bool key_is_constant = instr->key()->IsConstantOperand();
  int constant_key = 0;
  if (key_is_constant) {
    constant_key = ToInteger32(LConstantOperand::cast(instr->key()));
    if (constant_key & 0xF0000000) {
      Abort("array index constant value too big.");
    }
  } else {
    key = ToRegister(instr->key());
  }
  int shift_size = ElementsKindToShiftSize(elements_kind);

  if (elements_kind == EXTERNAL_FLOAT_ELEMENTS ||
      elements_kind == EXTERNAL_DOUBLE_ELEMENTS) {
    CpuFeatures::Scope scope(VFP3);
    DwVfpRegister result = ToDoubleRegister(instr->result());
    Operand operand = key_is_constant
        ? Operand(constant_key * (1 << shift_size))
        : Operand(key, LSL, shift_size);
    __ add(scratch0(), external_pointer, operand);
    if (elements_kind == EXTERNAL_FLOAT_ELEMENTS) {
      __ vldr(result.low(), scratch0(), 0);
      __ vcvt_f64_f32(result, result.low());
    } else  {  // i.e. elements_kind == EXTERNAL_DOUBLE_ELEMENTS
      __ vldr(result, scratch0(), 0);
    }
  } else {
    Register result = ToRegister(instr->result());
    MemOperand mem_operand(key_is_constant
        ? MemOperand(external_pointer, constant_key * (1 << shift_size))
        : MemOperand(external_pointer, key, LSL, shift_size));
    switch (elements_kind) {
      case EXTERNAL_BYTE_ELEMENTS:
        __ ldrsb(result, mem_operand);
        break;
      case EXTERNAL_PIXEL_ELEMENTS:
      case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
        __ ldrb(result, mem_operand);
        break;
      case EXTERNAL_SHORT_ELEMENTS:
        __ ldrsh(result, mem_operand);
        break;
      case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
        __ ldrh(result, mem_operand);
        break;
      case EXTERNAL_INT_ELEMENTS:
        __ ldr(result, mem_operand);
        break;
      case EXTERNAL_UNSIGNED_INT_ELEMENTS:
        __ ldr(result, mem_operand);
        __ cmp(result, Operand(0x80000000));
        // TODO(danno): we could be more clever here, perhaps having a special
        // version of the stub that detects if the overflow case actually
        // happens, and generate code that returns a double rather than int.
        DeoptimizeIf(cs, instr->environment());
        break;
      case EXTERNAL_FLOAT_ELEMENTS:
      case EXTERNAL_DOUBLE_ELEMENTS:
      case FAST_DOUBLE_ELEMENTS:
      case FAST_ELEMENTS:
      case DICTIONARY_ELEMENTS:
      case NON_STRICT_ARGUMENTS_ELEMENTS:
        UNREACHABLE();
        break;
    }
  }
}


void LCodeGen::DoLoadKeyedGeneric(LLoadKeyedGeneric* instr) {
  ASSERT(ToRegister(instr->object()).is(r1));
  ASSERT(ToRegister(instr->key()).is(r0));

  Handle<Code> ic = isolate()->builtins()->KeyedLoadIC_Initialize();
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoArgumentsElements(LArgumentsElements* instr) {
  Register scratch = scratch0();
  Register result = ToRegister(instr->result());

  // Check if the calling frame is an arguments adaptor frame.
  Label done, adapted;
  __ ldr(scratch, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ldr(result, MemOperand(scratch, StandardFrameConstants::kContextOffset));
  __ cmp(result, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));

  // Result is the frame pointer for the frame if not adapted and for the real
  // frame below the adaptor frame if adapted.
  __ mov(result, fp, LeaveCC, ne);
  __ mov(result, scratch, LeaveCC, eq);
}


void LCodeGen::DoArgumentsLength(LArgumentsLength* instr) {
  Register elem = ToRegister(instr->InputAt(0));
  Register result = ToRegister(instr->result());

  Label done;

  // If no arguments adaptor frame the number of arguments is fixed.
  __ cmp(fp, elem);
  __ mov(result, Operand(scope()->num_parameters()));
  __ b(eq, &done);

  // Arguments adaptor frame present. Get argument length from there.
  __ ldr(result, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));
  __ ldr(result,
         MemOperand(result, ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(result);

  // Argument length is in result register.
  __ bind(&done);
}


void LCodeGen::DoApplyArguments(LApplyArguments* instr) {
  Register receiver = ToRegister(instr->receiver());
  Register function = ToRegister(instr->function());
  Register length = ToRegister(instr->length());
  Register elements = ToRegister(instr->elements());
  Register scratch = scratch0();
  ASSERT(receiver.is(r0));  // Used for parameter count.
  ASSERT(function.is(r1));  // Required by InvokeFunction.
  ASSERT(ToRegister(instr->result()).is(r0));

  // If the receiver is null or undefined, we have to pass the global
  // object as a receiver to normal functions. Values have to be
  // passed unchanged to builtins and strict-mode functions.
  Label global_object, receiver_ok;

  // Do not transform the receiver to object for strict mode
  // functions.
  __ ldr(scratch,
         FieldMemOperand(function, JSFunction::kSharedFunctionInfoOffset));
  __ ldr(scratch,
         FieldMemOperand(scratch, SharedFunctionInfo::kCompilerHintsOffset));
  __ tst(scratch,
         Operand(1 << (SharedFunctionInfo::kStrictModeFunction + kSmiTagSize)));
  __ b(ne, &receiver_ok);

  // Do not transform the receiver to object for builtins.
  __ tst(scratch, Operand(1 << (SharedFunctionInfo::kNative + kSmiTagSize)));
  __ b(ne, &receiver_ok);

  // Normal function. Replace undefined or null with global receiver.
  __ LoadRoot(scratch, Heap::kNullValueRootIndex);
  __ cmp(receiver, scratch);
  __ b(eq, &global_object);
  __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  __ cmp(receiver, scratch);
  __ b(eq, &global_object);

  // Deoptimize if the receiver is not a JS object.
  __ tst(receiver, Operand(kSmiTagMask));
  DeoptimizeIf(eq, instr->environment());
  __ CompareObjectType(receiver, scratch, scratch, FIRST_SPEC_OBJECT_TYPE);
  DeoptimizeIf(lt, instr->environment());
  __ jmp(&receiver_ok);

  __ bind(&global_object);
  __ ldr(receiver, GlobalObjectOperand());
  __ ldr(receiver,
         FieldMemOperand(receiver, JSGlobalObject::kGlobalReceiverOffset));
  __ bind(&receiver_ok);

  // Copy the arguments to this function possibly from the
  // adaptor frame below it.
  const uint32_t kArgumentsLimit = 1 * KB;
  __ cmp(length, Operand(kArgumentsLimit));
  DeoptimizeIf(hi, instr->environment());

  // Push the receiver and use the register to keep the original
  // number of arguments.
  __ push(receiver);
  __ mov(receiver, length);
  // The arguments are at a one pointer size offset from elements.
  __ add(elements, elements, Operand(1 * kPointerSize));

  // Loop through the arguments pushing them onto the execution
  // stack.
  Label invoke, loop;
  // length is a small non-negative integer, due to the test above.
  __ cmp(length, Operand(0));
  __ b(eq, &invoke);
  __ bind(&loop);
  __ ldr(scratch, MemOperand(elements, length, LSL, 2));
  __ push(scratch);
  __ sub(length, length, Operand(1), SetCC);
  __ b(ne, &loop);

  __ bind(&invoke);
  ASSERT(instr->HasPointerMap() && instr->HasDeoptimizationEnvironment());
  LPointerMap* pointers = instr->pointer_map();
  RecordPosition(pointers->position());
  SafepointGenerator safepoint_generator(
      this, pointers, Safepoint::kLazyDeopt);
  // The number of arguments is stored in receiver which is r0, as expected
  // by InvokeFunction.
  v8::internal::ParameterCount actual(receiver);
  __ InvokeFunction(function, actual, CALL_FUNCTION,
                    safepoint_generator, CALL_AS_METHOD);
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoPushArgument(LPushArgument* instr) {
  LOperand* argument = instr->InputAt(0);
  if (argument->IsDoubleRegister() || argument->IsDoubleStackSlot()) {
    Abort("DoPushArgument not implemented for double type.");
  } else {
    Register argument_reg = EmitLoadRegister(argument, ip);
    __ push(argument_reg);
  }
}


void LCodeGen::DoThisFunction(LThisFunction* instr) {
  Register result = ToRegister(instr->result());
  __ ldr(result, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
}


void LCodeGen::DoContext(LContext* instr) {
  Register result = ToRegister(instr->result());
  __ mov(result, cp);
}


void LCodeGen::DoOuterContext(LOuterContext* instr) {
  Register context = ToRegister(instr->context());
  Register result = ToRegister(instr->result());
  __ ldr(result,
         MemOperand(context, Context::SlotOffset(Context::PREVIOUS_INDEX)));
}


void LCodeGen::DoGlobalObject(LGlobalObject* instr) {
  Register result = ToRegister(instr->result());
  __ ldr(result, ContextOperand(cp, Context::GLOBAL_INDEX));
}


void LCodeGen::DoGlobalReceiver(LGlobalReceiver* instr) {
  Register global = ToRegister(instr->global());
  Register result = ToRegister(instr->result());
  __ ldr(result, FieldMemOperand(global, GlobalObject::kGlobalReceiverOffset));
}


void LCodeGen::CallKnownFunction(Handle<JSFunction> function,
                                 int arity,
                                 LInstruction* instr,
                                 CallKind call_kind) {
  // Change context if needed.
  bool change_context =
      (info()->closure()->context() != function->context()) ||
      scope()->contains_with() ||
      (scope()->num_heap_slots() > 0);
  if (change_context) {
    __ ldr(cp, FieldMemOperand(r1, JSFunction::kContextOffset));
  }

  // Set r0 to arguments count if adaption is not needed. Assumes that r0
  // is available to write to at this point.
  if (!function->NeedsArgumentsAdaption()) {
    __ mov(r0, Operand(arity));
  }

  LPointerMap* pointers = instr->pointer_map();
  RecordPosition(pointers->position());

  // Invoke function.
  __ SetCallKind(r5, call_kind);
  __ ldr(ip, FieldMemOperand(r1, JSFunction::kCodeEntryOffset));
  __ Call(ip);

  // Setup deoptimization.
  RecordSafepointWithLazyDeopt(instr, RECORD_SIMPLE_SAFEPOINT);

  // Restore context.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallConstantFunction(LCallConstantFunction* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));
  __ mov(r1, Operand(instr->function()));
  CallKnownFunction(instr->function(),
                    instr->arity(),
                    instr,
                    CALL_AS_METHOD);
}


void LCodeGen::DoDeferredMathAbsTaggedHeapNumber(LUnaryMathOperation* instr) {
  Register input = ToRegister(instr->InputAt(0));
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();

  // Deoptimize if not a heap number.
  __ ldr(scratch, FieldMemOperand(input, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
  __ cmp(scratch, Operand(ip));
  DeoptimizeIf(ne, instr->environment());

  Label done;
  Register exponent = scratch0();
  scratch = no_reg;
  __ ldr(exponent, FieldMemOperand(input, HeapNumber::kExponentOffset));
  // Check the sign of the argument. If the argument is positive, just
  // return it.
  __ tst(exponent, Operand(HeapNumber::kSignMask));
  // Move the input to the result if necessary.
  __ Move(result, input);
  __ b(eq, &done);

  // Input is negative. Reverse its sign.
  // Preserve the value of all registers.
  {
    PushSafepointRegistersScope scope(this, Safepoint::kWithRegisters);

    // Registers were saved at the safepoint, so we can use
    // many scratch registers.
    Register tmp1 = input.is(r1) ? r0 : r1;
    Register tmp2 = input.is(r2) ? r0 : r2;
    Register tmp3 = input.is(r3) ? r0 : r3;
    Register tmp4 = input.is(r4) ? r0 : r4;

    // exponent: floating point exponent value.

    Label allocated, slow;
    __ LoadRoot(tmp4, Heap::kHeapNumberMapRootIndex);
    __ AllocateHeapNumber(tmp1, tmp2, tmp3, tmp4, &slow);
    __ b(&allocated);

    // Slow case: Call the runtime system to do the number allocation.
    __ bind(&slow);

    CallRuntimeFromDeferred(Runtime::kAllocateHeapNumber, 0, instr);
    // Set the pointer to the new heap number in tmp.
    if (!tmp1.is(r0)) __ mov(tmp1, Operand(r0));
    // Restore input_reg after call to runtime.
    __ LoadFromSafepointRegisterSlot(input, input);
    __ ldr(exponent, FieldMemOperand(input, HeapNumber::kExponentOffset));

    __ bind(&allocated);
    // exponent: floating point exponent value.
    // tmp1: allocated heap number.
    __ bic(exponent, exponent, Operand(HeapNumber::kSignMask));
    __ str(exponent, FieldMemOperand(tmp1, HeapNumber::kExponentOffset));
    __ ldr(tmp2, FieldMemOperand(input, HeapNumber::kMantissaOffset));
    __ str(tmp2, FieldMemOperand(tmp1, HeapNumber::kMantissaOffset));

    __ StoreToSafepointRegisterSlot(tmp1, result);
  }

  __ bind(&done);
}


void LCodeGen::EmitIntegerMathAbs(LUnaryMathOperation* instr) {
  Register input = ToRegister(instr->InputAt(0));
  Register result = ToRegister(instr->result());
  __ cmp(input, Operand(0));
  __ Move(result, input, pl);
  // We can make rsb conditional because the previous cmp instruction
  // will clear the V (overflow) flag and rsb won't set this flag
  // if input is positive.
  __ rsb(result, input, Operand(0), SetCC, mi);
  // Deoptimize on overflow.
  DeoptimizeIf(vs, instr->environment());
}


void LCodeGen::DoMathAbs(LUnaryMathOperation* instr) {
  // Class for deferred case.
  class DeferredMathAbsTaggedHeapNumber: public LDeferredCode {
   public:
    DeferredMathAbsTaggedHeapNumber(LCodeGen* codegen,
                                    LUnaryMathOperation* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() {
      codegen()->DoDeferredMathAbsTaggedHeapNumber(instr_);
    }
   private:
    LUnaryMathOperation* instr_;
  };

  Representation r = instr->hydrogen()->value()->representation();
  if (r.IsDouble()) {
    DwVfpRegister input = ToDoubleRegister(instr->InputAt(0));
    DwVfpRegister result = ToDoubleRegister(instr->result());
    __ vabs(result, input);
  } else if (r.IsInteger32()) {
    EmitIntegerMathAbs(instr);
  } else {
    // Representation is tagged.
    DeferredMathAbsTaggedHeapNumber* deferred =
        new DeferredMathAbsTaggedHeapNumber(this, instr);
    Register input = ToRegister(instr->InputAt(0));
    // Smi check.
    __ JumpIfNotSmi(input, deferred->entry());
    // If smi, handle it directly.
    EmitIntegerMathAbs(instr);
    __ bind(deferred->exit());
  }
}


void LCodeGen::DoMathFloor(LUnaryMathOperation* instr) {
  DoubleRegister input = ToDoubleRegister(instr->InputAt(0));
  Register result = ToRegister(instr->result());
  SwVfpRegister single_scratch = double_scratch0().low();
  Register scratch1 = scratch0();
  Register scratch2 = ToRegister(instr->TempAt(0));

  __ EmitVFPTruncate(kRoundToMinusInf,
                     single_scratch,
                     input,
                     scratch1,
                     scratch2);
  DeoptimizeIf(ne, instr->environment());

  // Move the result back to general purpose register r0.
  __ vmov(result, single_scratch);

  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    // Test for -0.
    Label done;
    __ cmp(result, Operand(0));
    __ b(ne, &done);
    __ vmov(scratch1, input.high());
    __ tst(scratch1, Operand(HeapNumber::kSignMask));
    DeoptimizeIf(ne, instr->environment());
    __ bind(&done);
  }
}


void LCodeGen::DoMathRound(LUnaryMathOperation* instr) {
  DoubleRegister input = ToDoubleRegister(instr->InputAt(0));
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();
  Label done, check_sign_on_zero;

  // Extract exponent bits.
  __ vmov(result, input.high());
  __ ubfx(scratch,
          result,
          HeapNumber::kExponentShift,
          HeapNumber::kExponentBits);

  // If the number is in ]-0.5, +0.5[, the result is +/- 0.
  __ cmp(scratch, Operand(HeapNumber::kExponentBias - 2));
  __ mov(result, Operand(0), LeaveCC, le);
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    __ b(le, &check_sign_on_zero);
  } else {
    __ b(le, &done);
  }

  // The following conversion will not work with numbers
  // outside of ]-2^32, 2^32[.
  __ cmp(scratch, Operand(HeapNumber::kExponentBias + 32));
  DeoptimizeIf(ge, instr->environment());

  // Save the original sign for later comparison.
  __ and_(scratch, result, Operand(HeapNumber::kSignMask));

  __ Vmov(double_scratch0(), 0.5);
  __ vadd(double_scratch0(), input, double_scratch0());

  // Check sign of the result: if the sign changed, the input
  // value was in ]0.5, 0[ and the result should be -0.
  __ vmov(result, double_scratch0().high());
  __ eor(result, result, Operand(scratch), SetCC);
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    DeoptimizeIf(mi, instr->environment());
  } else {
    __ mov(result, Operand(0), LeaveCC, mi);
    __ b(mi, &done);
  }

  __ EmitVFPTruncate(kRoundToMinusInf,
                     double_scratch0().low(),
                     double_scratch0(),
                     result,
                     scratch);
  DeoptimizeIf(ne, instr->environment());
  __ vmov(result, double_scratch0().low());

  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    // Test for -0.
    __ cmp(result, Operand(0));
    __ b(ne, &done);
    __ bind(&check_sign_on_zero);
    __ vmov(scratch, input.high());
    __ tst(scratch, Operand(HeapNumber::kSignMask));
    DeoptimizeIf(ne, instr->environment());
  }
  __ bind(&done);
}


void LCodeGen::DoMathSqrt(LUnaryMathOperation* instr) {
  DoubleRegister input = ToDoubleRegister(instr->InputAt(0));
  DoubleRegister result = ToDoubleRegister(instr->result());
  __ vsqrt(result, input);
}


void LCodeGen::DoMathPowHalf(LUnaryMathOperation* instr) {
  DoubleRegister input = ToDoubleRegister(instr->InputAt(0));
  DoubleRegister result = ToDoubleRegister(instr->result());
  // Add +0 to convert -0 to +0.
  __ vadd(result, input, kDoubleRegZero);
  __ vsqrt(result, result);
}


void LCodeGen::DoPower(LPower* instr) {
  LOperand* left = instr->InputAt(0);
  LOperand* right = instr->InputAt(1);
  Register scratch = scratch0();
  DoubleRegister result_reg = ToDoubleRegister(instr->result());
  Representation exponent_type = instr->hydrogen()->right()->representation();
  if (exponent_type.IsDouble()) {
    // Prepare arguments and call C function.
    __ PrepareCallCFunction(0, 2, scratch);
    __ SetCallCDoubleArguments(ToDoubleRegister(left),
                               ToDoubleRegister(right));
    __ CallCFunction(
        ExternalReference::power_double_double_function(isolate()), 0, 2);
  } else if (exponent_type.IsInteger32()) {
    ASSERT(ToRegister(right).is(r0));
    // Prepare arguments and call C function.
    __ PrepareCallCFunction(1, 1, scratch);
    __ SetCallCDoubleArguments(ToDoubleRegister(left), ToRegister(right));
    __ CallCFunction(
        ExternalReference::power_double_int_function(isolate()), 1, 1);
  } else {
    ASSERT(exponent_type.IsTagged());
    ASSERT(instr->hydrogen()->left()->representation().IsDouble());

    Register right_reg = ToRegister(right);

    // Check for smi on the right hand side.
    Label non_smi, call;
    __ JumpIfNotSmi(right_reg, &non_smi);

    // Untag smi and convert it to a double.
    __ SmiUntag(right_reg);
    SwVfpRegister single_scratch = double_scratch0().low();
    __ vmov(single_scratch, right_reg);
    __ vcvt_f64_s32(result_reg, single_scratch);
    __ jmp(&call);

    // Heap number map check.
    __ bind(&non_smi);
    __ ldr(scratch, FieldMemOperand(right_reg, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
    __ cmp(scratch, Operand(ip));
    DeoptimizeIf(ne, instr->environment());
    int32_t value_offset = HeapNumber::kValueOffset - kHeapObjectTag;
    __ add(scratch, right_reg, Operand(value_offset));
    __ vldr(result_reg, scratch, 0);

    // Prepare arguments and call C function.
    __ bind(&call);
    __ PrepareCallCFunction(0, 2, scratch);
    __ SetCallCDoubleArguments(ToDoubleRegister(left), result_reg);
    __ CallCFunction(
        ExternalReference::power_double_double_function(isolate()), 0, 2);
  }
  // Store the result in the result register.
  __ GetCFunctionDoubleResult(result_reg);
}


void LCodeGen::DoMathLog(LUnaryMathOperation* instr) {
  ASSERT(ToDoubleRegister(instr->result()).is(d2));
  TranscendentalCacheStub stub(TranscendentalCache::LOG,
                               TranscendentalCacheStub::UNTAGGED);
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoMathCos(LUnaryMathOperation* instr) {
  ASSERT(ToDoubleRegister(instr->result()).is(d2));
  TranscendentalCacheStub stub(TranscendentalCache::COS,
                               TranscendentalCacheStub::UNTAGGED);
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoMathSin(LUnaryMathOperation* instr) {
  ASSERT(ToDoubleRegister(instr->result()).is(d2));
  TranscendentalCacheStub stub(TranscendentalCache::SIN,
                               TranscendentalCacheStub::UNTAGGED);
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoUnaryMathOperation(LUnaryMathOperation* instr) {
  switch (instr->op()) {
    case kMathAbs:
      DoMathAbs(instr);
      break;
    case kMathFloor:
      DoMathFloor(instr);
      break;
    case kMathRound:
      DoMathRound(instr);
      break;
    case kMathSqrt:
      DoMathSqrt(instr);
      break;
    case kMathPowHalf:
      DoMathPowHalf(instr);
      break;
    case kMathCos:
      DoMathCos(instr);
      break;
    case kMathSin:
      DoMathSin(instr);
      break;
    case kMathLog:
      DoMathLog(instr);
      break;
    default:
      Abort("Unimplemented type of LUnaryMathOperation.");
      UNREACHABLE();
  }
}


void LCodeGen::DoInvokeFunction(LInvokeFunction* instr) {
  ASSERT(ToRegister(instr->function()).is(r1));
  ASSERT(instr->HasPointerMap());
  ASSERT(instr->HasDeoptimizationEnvironment());
  LPointerMap* pointers = instr->pointer_map();
  RecordPosition(pointers->position());
  SafepointGenerator generator(this, pointers, Safepoint::kLazyDeopt);
  ParameterCount count(instr->arity());
  __ InvokeFunction(r1, count, CALL_FUNCTION, generator, CALL_AS_METHOD);
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallKeyed(LCallKeyed* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));

  int arity = instr->arity();
  Handle<Code> ic =
      isolate()->stub_cache()->ComputeKeyedCallInitialize(arity);
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallNamed(LCallNamed* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));

  int arity = instr->arity();
  RelocInfo::Mode mode = RelocInfo::CODE_TARGET;
  Handle<Code> ic =
      isolate()->stub_cache()->ComputeCallInitialize(arity, mode);
  __ mov(r2, Operand(instr->name()));
  CallCode(ic, mode, instr);
  // Restore context register.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallFunction(LCallFunction* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));

  int arity = instr->arity();
  CallFunctionStub stub(arity, RECEIVER_MIGHT_BE_IMPLICIT);
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
  __ Drop(1);
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallGlobal(LCallGlobal* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));

  int arity = instr->arity();
  RelocInfo::Mode mode = RelocInfo::CODE_TARGET_CONTEXT;
  Handle<Code> ic =
      isolate()->stub_cache()->ComputeCallInitialize(arity, mode);
  __ mov(r2, Operand(instr->name()));
  CallCode(ic, mode, instr);
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallKnownGlobal(LCallKnownGlobal* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));
  __ mov(r1, Operand(instr->target()));
  CallKnownFunction(instr->target(), instr->arity(), instr, CALL_AS_FUNCTION);
}


void LCodeGen::DoCallNew(LCallNew* instr) {
  ASSERT(ToRegister(instr->InputAt(0)).is(r1));
  ASSERT(ToRegister(instr->result()).is(r0));

  Handle<Code> builtin = isolate()->builtins()->JSConstructCall();
  __ mov(r0, Operand(instr->arity()));
  CallCode(builtin, RelocInfo::CONSTRUCT_CALL, instr);
}


void LCodeGen::DoCallRuntime(LCallRuntime* instr) {
  CallRuntime(instr->function(), instr->arity(), instr);
}


void LCodeGen::DoStoreNamedField(LStoreNamedField* instr) {
  Register object = ToRegister(instr->object());
  Register value = ToRegister(instr->value());
  Register scratch = scratch0();
  int offset = instr->offset();

  ASSERT(!object.is(value));

  if (!instr->transition().is_null()) {
    __ mov(scratch, Operand(instr->transition()));
    __ str(scratch, FieldMemOperand(object, HeapObject::kMapOffset));
  }

  // Do the store.
  if (instr->is_in_object()) {
    __ str(value, FieldMemOperand(object, offset));
    if (instr->needs_write_barrier()) {
      // Update the write barrier for the object for in-object properties.
      __ RecordWrite(object, Operand(offset), value, scratch);
    }
  } else {
    __ ldr(scratch, FieldMemOperand(object, JSObject::kPropertiesOffset));
    __ str(value, FieldMemOperand(scratch, offset));
    if (instr->needs_write_barrier()) {
      // Update the write barrier for the properties array.
      // object is used as a scratch register.
      __ RecordWrite(scratch, Operand(offset), value, object);
    }
  }
}


void LCodeGen::DoStoreNamedGeneric(LStoreNamedGeneric* instr) {
  ASSERT(ToRegister(instr->object()).is(r1));
  ASSERT(ToRegister(instr->value()).is(r0));

  // Name is always in r2.
  __ mov(r2, Operand(instr->name()));
  Handle<Code> ic = instr->strict_mode()
      ? isolate()->builtins()->StoreIC_Initialize_Strict()
      : isolate()->builtins()->StoreIC_Initialize();
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoBoundsCheck(LBoundsCheck* instr) {
  __ cmp(ToRegister(instr->index()), ToRegister(instr->length()));
  DeoptimizeIf(hs, instr->environment());
}


void LCodeGen::DoStoreKeyedFastElement(LStoreKeyedFastElement* instr) {
  Register value = ToRegister(instr->value());
  Register elements = ToRegister(instr->object());
  Register key = instr->key()->IsRegister() ? ToRegister(instr->key()) : no_reg;
  Register scratch = scratch0();

  // Do the store.
  if (instr->key()->IsConstantOperand()) {
    ASSERT(!instr->hydrogen()->NeedsWriteBarrier());
    LConstantOperand* const_operand = LConstantOperand::cast(instr->key());
    int offset =
        ToInteger32(const_operand) * kPointerSize + FixedArray::kHeaderSize;
    __ str(value, FieldMemOperand(elements, offset));
  } else {
    __ add(scratch, elements, Operand(key, LSL, kPointerSizeLog2));
    __ str(value, FieldMemOperand(scratch, FixedArray::kHeaderSize));
  }

  if (instr->hydrogen()->NeedsWriteBarrier()) {
    // Compute address of modified element and store it into key register.
    __ add(key, scratch, Operand(FixedArray::kHeaderSize));
    __ RecordWrite(elements, key, value);
  }
}


void LCodeGen::DoStoreKeyedFastDoubleElement(
    LStoreKeyedFastDoubleElement* instr) {
  DwVfpRegister value = ToDoubleRegister(instr->value());
  Register elements = ToRegister(instr->elements());
  Register key = no_reg;
  Register scratch = scratch0();
  bool key_is_constant = instr->key()->IsConstantOperand();
  int constant_key = 0;
  Label not_nan;

  // Calculate the effective address of the slot in the array to store the
  // double value.
  if (key_is_constant) {
    constant_key = ToInteger32(LConstantOperand::cast(instr->key()));
    if (constant_key & 0xF0000000) {
      Abort("array index constant value too big.");
    }
  } else {
    key = ToRegister(instr->key());
  }
  int shift_size = ElementsKindToShiftSize(FAST_DOUBLE_ELEMENTS);
  Operand operand = key_is_constant
      ? Operand(constant_key * (1 << shift_size) +
                FixedDoubleArray::kHeaderSize - kHeapObjectTag)
      : Operand(key, LSL, shift_size);
  __ add(scratch, elements, operand);
  if (!key_is_constant) {
    __ add(scratch, scratch,
           Operand(FixedDoubleArray::kHeaderSize - kHeapObjectTag));
  }

  // Check for NaN. All NaNs must be canonicalized.
  __ VFPCompareAndSetFlags(value, value);

  // Only load canonical NaN if the comparison above set the overflow.
  __ Vmov(value, FixedDoubleArray::canonical_not_the_hole_nan_as_double(), vs);

  __ bind(&not_nan);
  __ vstr(value, scratch, 0);
}


void LCodeGen::DoStoreKeyedSpecializedArrayElement(
    LStoreKeyedSpecializedArrayElement* instr) {

  Register external_pointer = ToRegister(instr->external_pointer());
  Register key = no_reg;
  ElementsKind elements_kind = instr->elements_kind();
  bool key_is_constant = instr->key()->IsConstantOperand();
  int constant_key = 0;
  if (key_is_constant) {
    constant_key = ToInteger32(LConstantOperand::cast(instr->key()));
    if (constant_key & 0xF0000000) {
      Abort("array index constant value too big.");
    }
  } else {
    key = ToRegister(instr->key());
  }
  int shift_size = ElementsKindToShiftSize(elements_kind);

  if (elements_kind == EXTERNAL_FLOAT_ELEMENTS ||
      elements_kind == EXTERNAL_DOUBLE_ELEMENTS) {
    CpuFeatures::Scope scope(VFP3);
    DwVfpRegister value(ToDoubleRegister(instr->value()));
    Operand operand(key_is_constant ? Operand(constant_key * (1 << shift_size))
                                    : Operand(key, LSL, shift_size));
    __ add(scratch0(), external_pointer, operand);
    if (elements_kind == EXTERNAL_FLOAT_ELEMENTS) {
      __ vcvt_f32_f64(double_scratch0().low(), value);
      __ vstr(double_scratch0().low(), scratch0(), 0);
    } else {  // i.e. elements_kind == EXTERNAL_DOUBLE_ELEMENTS
      __ vstr(value, scratch0(), 0);
    }
  } else {
    Register value(ToRegister(instr->value()));
    MemOperand mem_operand(key_is_constant
        ? MemOperand(external_pointer, constant_key * (1 << shift_size))
        : MemOperand(external_pointer, key, LSL, shift_size));
    switch (elements_kind) {
      case EXTERNAL_PIXEL_ELEMENTS:
      case EXTERNAL_BYTE_ELEMENTS:
      case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
        __ strb(value, mem_operand);
        break;
      case EXTERNAL_SHORT_ELEMENTS:
      case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
        __ strh(value, mem_operand);
        break;
      case EXTERNAL_INT_ELEMENTS:
      case EXTERNAL_UNSIGNED_INT_ELEMENTS:
        __ str(value, mem_operand);
        break;
      case EXTERNAL_FLOAT_ELEMENTS:
      case EXTERNAL_DOUBLE_ELEMENTS:
      case FAST_DOUBLE_ELEMENTS:
      case FAST_ELEMENTS:
      case DICTIONARY_ELEMENTS:
      case NON_STRICT_ARGUMENTS_ELEMENTS:
        UNREACHABLE();
        break;
    }
  }
}


void LCodeGen::DoStoreKeyedGeneric(LStoreKeyedGeneric* instr) {
  ASSERT(ToRegister(instr->object()).is(r2));
  ASSERT(ToRegister(instr->key()).is(r1));
  ASSERT(ToRegister(instr->value()).is(r0));

  Handle<Code> ic = instr->strict_mode()
      ? isolate()->builtins()->KeyedStoreIC_Initialize_Strict()
      : isolate()->builtins()->KeyedStoreIC_Initialize();
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoStringAdd(LStringAdd* instr) {
  __ push(ToRegister(instr->left()));
  __ push(ToRegister(instr->right()));
  StringAddStub stub(NO_STRING_CHECK_IN_STUB);
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoStringCharCodeAt(LStringCharCodeAt* instr) {
  class DeferredStringCharCodeAt: public LDeferredCode {
   public:
    DeferredStringCharCodeAt(LCodeGen* codegen, LStringCharCodeAt* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() { codegen()->DoDeferredStringCharCodeAt(instr_); }
   private:
    LStringCharCodeAt* instr_;
  };

  Register string = ToRegister(instr->string());
  Register index = ToRegister(instr->index());
  Register result = ToRegister(instr->result());

  DeferredStringCharCodeAt* deferred =
      new DeferredStringCharCodeAt(this, instr);

  // Fetch the instance type of the receiver into result register.
  __ ldr(result, FieldMemOperand(string, HeapObject::kMapOffset));
  __ ldrb(result, FieldMemOperand(result, Map::kInstanceTypeOffset));

  // We need special handling for indirect strings.
  Label check_sequential;
  __ tst(result, Operand(kIsIndirectStringMask));
  __ b(eq, &check_sequential);

  // Dispatch on the indirect string shape: slice or cons.
  Label cons_string;
  __ tst(result, Operand(kSlicedNotConsMask));
  __ b(eq, &cons_string);

  // Handle slices.
  Label indirect_string_loaded;
  __ ldr(result, FieldMemOperand(string, SlicedString::kOffsetOffset));
  __ add(index, index, Operand(result, ASR, kSmiTagSize));
  __ ldr(string, FieldMemOperand(string, SlicedString::kParentOffset));
  __ jmp(&indirect_string_loaded);

  // Handle conses.
  // Check whether the right hand side is the empty string (i.e. if
  // this is really a flat string in a cons string). If that is not
  // the case we would rather go to the runtime system now to flatten
  // the string.
  __ bind(&cons_string);
  __ ldr(result, FieldMemOperand(string, ConsString::kSecondOffset));
  __ LoadRoot(ip, Heap::kEmptyStringRootIndex);
  __ cmp(result, ip);
  __ b(ne, deferred->entry());
  // Get the first of the two strings and load its instance type.
  __ ldr(string, FieldMemOperand(string, ConsString::kFirstOffset));

  __ bind(&indirect_string_loaded);
  __ ldr(result, FieldMemOperand(string, HeapObject::kMapOffset));
  __ ldrb(result, FieldMemOperand(result, Map::kInstanceTypeOffset));

  // Check whether the string is sequential. The only non-sequential
  // shapes we support have just been unwrapped above.
  __ bind(&check_sequential);
  STATIC_ASSERT(kSeqStringTag == 0);
  __ tst(result, Operand(kStringRepresentationMask));
  __ b(ne, deferred->entry());

  // Dispatch on the encoding: ASCII or two-byte.
  Label ascii_string;
  STATIC_ASSERT((kStringEncodingMask & kAsciiStringTag) != 0);
  STATIC_ASSERT((kStringEncodingMask & kTwoByteStringTag) == 0);
  __ tst(result, Operand(kStringEncodingMask));
  __ b(ne, &ascii_string);

  // Two-byte string.
  // Load the two-byte character code into the result register.
  Label done;
  __ add(result,
         string,
         Operand(SeqTwoByteString::kHeaderSize - kHeapObjectTag));
  __ ldrh(result, MemOperand(result, index, LSL, 1));
  __ jmp(&done);

  // ASCII string.
  // Load the byte into the result register.
  __ bind(&ascii_string);
  __ add(result,
         string,
         Operand(SeqAsciiString::kHeaderSize - kHeapObjectTag));
  __ ldrb(result, MemOperand(result, index));

  __ bind(&done);
  __ bind(deferred->exit());
}


void LCodeGen::DoDeferredStringCharCodeAt(LStringCharCodeAt* instr) {
  Register string = ToRegister(instr->string());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();

  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  __ mov(result, Operand(0));

  PushSafepointRegistersScope scope(this, Safepoint::kWithRegisters);
  __ push(string);
  // Push the index as a smi. This is safe because of the checks in
  // DoStringCharCodeAt above.
  if (instr->index()->IsConstantOperand()) {
    int const_index = ToInteger32(LConstantOperand::cast(instr->index()));
    __ mov(scratch, Operand(Smi::FromInt(const_index)));
    __ push(scratch);
  } else {
    Register index = ToRegister(instr->index());
    __ SmiTag(index);
    __ push(index);
  }
  CallRuntimeFromDeferred(Runtime::kStringCharCodeAt, 2, instr);
  if (FLAG_debug_code) {
    __ AbortIfNotSmi(r0);
  }
  __ SmiUntag(r0);
  __ StoreToSafepointRegisterSlot(r0, result);
}


void LCodeGen::DoStringCharFromCode(LStringCharFromCode* instr) {
  class DeferredStringCharFromCode: public LDeferredCode {
   public:
    DeferredStringCharFromCode(LCodeGen* codegen, LStringCharFromCode* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() { codegen()->DoDeferredStringCharFromCode(instr_); }
   private:
    LStringCharFromCode* instr_;
  };

  DeferredStringCharFromCode* deferred =
      new DeferredStringCharFromCode(this, instr);

  ASSERT(instr->hydrogen()->value()->representation().IsInteger32());
  Register char_code = ToRegister(instr->char_code());
  Register result = ToRegister(instr->result());
  ASSERT(!char_code.is(result));

  __ cmp(char_code, Operand(String::kMaxAsciiCharCode));
  __ b(hi, deferred->entry());
  __ LoadRoot(result, Heap::kSingleCharacterStringCacheRootIndex);
  __ add(result, result, Operand(char_code, LSL, kPointerSizeLog2));
  __ ldr(result, FieldMemOperand(result, FixedArray::kHeaderSize));
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(result, ip);
  __ b(eq, deferred->entry());
  __ bind(deferred->exit());
}


void LCodeGen::DoDeferredStringCharFromCode(LStringCharFromCode* instr) {
  Register char_code = ToRegister(instr->char_code());
  Register result = ToRegister(instr->result());

  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  __ mov(result, Operand(0));

  PushSafepointRegistersScope scope(this, Safepoint::kWithRegisters);
  __ SmiTag(char_code);
  __ push(char_code);
  CallRuntimeFromDeferred(Runtime::kCharFromCode, 1, instr);
  __ StoreToSafepointRegisterSlot(r0, result);
}


void LCodeGen::DoStringLength(LStringLength* instr) {
  Register string = ToRegister(instr->InputAt(0));
  Register result = ToRegister(instr->result());
  __ ldr(result, FieldMemOperand(string, String::kLengthOffset));
}


void LCodeGen::DoInteger32ToDouble(LInteger32ToDouble* instr) {
  LOperand* input = instr->InputAt(0);
  ASSERT(input->IsRegister() || input->IsStackSlot());
  LOperand* output = instr->result();
  ASSERT(output->IsDoubleRegister());
  SwVfpRegister single_scratch = double_scratch0().low();
  if (input->IsStackSlot()) {
    Register scratch = scratch0();
    __ ldr(scratch, ToMemOperand(input));
    __ vmov(single_scratch, scratch);
  } else {
    __ vmov(single_scratch, ToRegister(input));
  }
  __ vcvt_f64_s32(ToDoubleRegister(output), single_scratch);
}


void LCodeGen::DoNumberTagI(LNumberTagI* instr) {
  class DeferredNumberTagI: public LDeferredCode {
   public:
    DeferredNumberTagI(LCodeGen* codegen, LNumberTagI* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() { codegen()->DoDeferredNumberTagI(instr_); }
   private:
    LNumberTagI* instr_;
  };

  LOperand* input = instr->InputAt(0);
  ASSERT(input->IsRegister() && input->Equals(instr->result()));
  Register reg = ToRegister(input);

  DeferredNumberTagI* deferred = new DeferredNumberTagI(this, instr);
  __ SmiTag(reg, SetCC);
  __ b(vs, deferred->entry());
  __ bind(deferred->exit());
}


void LCodeGen::DoDeferredNumberTagI(LNumberTagI* instr) {
  Label slow;
  Register reg = ToRegister(instr->InputAt(0));
  DoubleRegister dbl_scratch = double_scratch0();
  SwVfpRegister flt_scratch = dbl_scratch.low();

  // Preserve the value of all registers.
  PushSafepointRegistersScope scope(this, Safepoint::kWithRegisters);

  // There was overflow, so bits 30 and 31 of the original integer
  // disagree. Try to allocate a heap number in new space and store
  // the value in there. If that fails, call the runtime system.
  Label done;
  __ SmiUntag(reg);
  __ eor(reg, reg, Operand(0x80000000));
  __ vmov(flt_scratch, reg);
  __ vcvt_f64_s32(dbl_scratch, flt_scratch);
  if (FLAG_inline_new) {
    __ LoadRoot(r6, Heap::kHeapNumberMapRootIndex);
    __ AllocateHeapNumber(r5, r3, r4, r6, &slow);
    if (!reg.is(r5)) __ mov(reg, r5);
    __ b(&done);
  }

  // Slow case: Call the runtime system to do the number allocation.
  __ bind(&slow);

  // TODO(3095996): Put a valid pointer value in the stack slot where the result
  // register is stored, as this register is in the pointer map, but contains an
  // integer value.
  __ mov(ip, Operand(0));
  __ StoreToSafepointRegisterSlot(ip, reg);
  CallRuntimeFromDeferred(Runtime::kAllocateHeapNumber, 0, instr);
  if (!reg.is(r0)) __ mov(reg, r0);

  // Done. Put the value in dbl_scratch into the value of the allocated heap
  // number.
  __ bind(&done);
  __ sub(ip, reg, Operand(kHeapObjectTag));
  __ vstr(dbl_scratch, ip, HeapNumber::kValueOffset);
  __ StoreToSafepointRegisterSlot(reg, reg);
}


void LCodeGen::DoNumberTagD(LNumberTagD* instr) {
  class DeferredNumberTagD: public LDeferredCode {
   public:
    DeferredNumberTagD(LCodeGen* codegen, LNumberTagD* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() { codegen()->DoDeferredNumberTagD(instr_); }
   private:
    LNumberTagD* instr_;
  };

  DoubleRegister input_reg = ToDoubleRegister(instr->InputAt(0));
  Register scratch = scratch0();
  Register reg = ToRegister(instr->result());
  Register temp1 = ToRegister(instr->TempAt(0));
  Register temp2 = ToRegister(instr->TempAt(1));

  DeferredNumberTagD* deferred = new DeferredNumberTagD(this, instr);
  if (FLAG_inline_new) {
    __ LoadRoot(scratch, Heap::kHeapNumberMapRootIndex);
    __ AllocateHeapNumber(reg, temp1, temp2, scratch, deferred->entry());
  } else {
    __ jmp(deferred->entry());
  }
  __ bind(deferred->exit());
  __ sub(ip, reg, Operand(kHeapObjectTag));
  __ vstr(input_reg, ip, HeapNumber::kValueOffset);
}


void LCodeGen::DoDeferredNumberTagD(LNumberTagD* instr) {
  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  Register reg = ToRegister(instr->result());
  __ mov(reg, Operand(0));

  PushSafepointRegistersScope scope(this, Safepoint::kWithRegisters);
  CallRuntimeFromDeferred(Runtime::kAllocateHeapNumber, 0, instr);
  __ StoreToSafepointRegisterSlot(r0, reg);
}


void LCodeGen::DoSmiTag(LSmiTag* instr) {
  LOperand* input = instr->InputAt(0);
  ASSERT(input->IsRegister() && input->Equals(instr->result()));
  ASSERT(!instr->hydrogen_value()->CheckFlag(HValue::kCanOverflow));
  __ SmiTag(ToRegister(input));
}


void LCodeGen::DoSmiUntag(LSmiUntag* instr) {
  LOperand* input = instr->InputAt(0);
  ASSERT(input->IsRegister() && input->Equals(instr->result()));
  if (instr->needs_check()) {
    STATIC_ASSERT(kHeapObjectTag == 1);
    // If the input is a HeapObject, SmiUntag will set the carry flag.
    __ SmiUntag(ToRegister(input), SetCC);
    DeoptimizeIf(cs, instr->environment());
  } else {
    __ SmiUntag(ToRegister(input));
  }
}


void LCodeGen::EmitNumberUntagD(Register input_reg,
                                DoubleRegister result_reg,
                                bool deoptimize_on_undefined,
                                LEnvironment* env) {
  Register scratch = scratch0();
  SwVfpRegister flt_scratch = double_scratch0().low();
  ASSERT(!result_reg.is(double_scratch0()));

  Label load_smi, heap_number, done;

  // Smi check.
  __ JumpIfSmi(input_reg, &load_smi);

  // Heap number map check.
  __ ldr(scratch, FieldMemOperand(input_reg, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
  __ cmp(scratch, Operand(ip));
  if (deoptimize_on_undefined) {
    DeoptimizeIf(ne, env);
  } else {
    Label heap_number;
    __ b(eq, &heap_number);

    __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
    __ cmp(input_reg, Operand(ip));
    DeoptimizeIf(ne, env);

    // Convert undefined to NaN.
    __ LoadRoot(ip, Heap::kNanValueRootIndex);
    __ sub(ip, ip, Operand(kHeapObjectTag));
    __ vldr(result_reg, ip, HeapNumber::kValueOffset);
    __ jmp(&done);

    __ bind(&heap_number);
  }
  // Heap number to double register conversion.
  __ sub(ip, input_reg, Operand(kHeapObjectTag));
  __ vldr(result_reg, ip, HeapNumber::kValueOffset);
  __ jmp(&done);

  // Smi to double register conversion
  __ bind(&load_smi);
  __ SmiUntag(input_reg);  // Untag smi before converting to float.
  __ vmov(flt_scratch, input_reg);
  __ vcvt_f64_s32(result_reg, flt_scratch);
  __ SmiTag(input_reg);  // Retag smi.
  __ bind(&done);
}


class DeferredTaggedToI: public LDeferredCode {
 public:
  DeferredTaggedToI(LCodeGen* codegen, LTaggedToI* instr)
      : LDeferredCode(codegen), instr_(instr) { }
  virtual void Generate() { codegen()->DoDeferredTaggedToI(instr_); }
 private:
  LTaggedToI* instr_;
};


void LCodeGen::DoDeferredTaggedToI(LTaggedToI* instr) {
  Register input_reg = ToRegister(instr->InputAt(0));
  Register scratch1 = scratch0();
  Register scratch2 = ToRegister(instr->TempAt(0));
  DwVfpRegister double_scratch = double_scratch0();
  SwVfpRegister single_scratch = double_scratch.low();

  ASSERT(!scratch1.is(input_reg) && !scratch1.is(scratch2));
  ASSERT(!scratch2.is(input_reg) && !scratch2.is(scratch1));

  Label done;

  // The input was optimistically untagged; revert it.
  // The carry flag is set when we reach this deferred code as we just executed
  // SmiUntag(heap_object, SetCC)
  STATIC_ASSERT(kHeapObjectTag == 1);
  __ adc(input_reg, input_reg, Operand(input_reg));

  // Heap number map check.
  __ ldr(scratch1, FieldMemOperand(input_reg, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
  __ cmp(scratch1, Operand(ip));

  if (instr->truncating()) {
    Register scratch3 = ToRegister(instr->TempAt(1));
    DwVfpRegister double_scratch2 = ToDoubleRegister(instr->TempAt(2));
    ASSERT(!scratch3.is(input_reg) &&
           !scratch3.is(scratch1) &&
           !scratch3.is(scratch2));
    // Performs a truncating conversion of a floating point number as used by
    // the JS bitwise operations.
    Label heap_number;
    __ b(eq, &heap_number);
    // Check for undefined. Undefined is converted to zero for truncating
    // conversions.
    __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
    __ cmp(input_reg, Operand(ip));
    DeoptimizeIf(ne, instr->environment());
    __ mov(input_reg, Operand(0));
    __ b(&done);

    __ bind(&heap_number);
    __ sub(scratch1, input_reg, Operand(kHeapObjectTag));
    __ vldr(double_scratch2, scratch1, HeapNumber::kValueOffset);

    __ EmitECMATruncate(input_reg,
                        double_scratch2,
                        single_scratch,
                        scratch1,
                        scratch2,
                        scratch3);

  } else {
    CpuFeatures::Scope scope(VFP3);
    // Deoptimize if we don't have a heap number.
    DeoptimizeIf(ne, instr->environment());

    __ sub(ip, input_reg, Operand(kHeapObjectTag));
    __ vldr(double_scratch, ip, HeapNumber::kValueOffset);
    __ EmitVFPTruncate(kRoundToZero,
                       single_scratch,
                       double_scratch,
                       scratch1,
                       scratch2,
                       kCheckForInexactConversion);
    DeoptimizeIf(ne, instr->environment());
    // Load the result.
    __ vmov(input_reg, single_scratch);

    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      __ cmp(input_reg, Operand(0));
      __ b(ne, &done);
      __ vmov(scratch1, double_scratch.high());
      __ tst(scratch1, Operand(HeapNumber::kSignMask));
      DeoptimizeIf(ne, instr->environment());
    }
  }
  __ bind(&done);
}


void LCodeGen::DoTaggedToI(LTaggedToI* instr) {
  LOperand* input = instr->InputAt(0);
  ASSERT(input->IsRegister());
  ASSERT(input->Equals(instr->result()));

  Register input_reg = ToRegister(input);

  DeferredTaggedToI* deferred = new DeferredTaggedToI(this, instr);

  // Optimistically untag the input.
  // If the input is a HeapObject, SmiUntag will set the carry flag.
  __ SmiUntag(input_reg, SetCC);
  // Branch to deferred code if the input was tagged.
  // The deferred code will take care of restoring the tag.
  __ b(cs, deferred->entry());
  __ bind(deferred->exit());
}


void LCodeGen::DoNumberUntagD(LNumberUntagD* instr) {
  LOperand* input = instr->InputAt(0);
  ASSERT(input->IsRegister());
  LOperand* result = instr->result();
  ASSERT(result->IsDoubleRegister());

  Register input_reg = ToRegister(input);
  DoubleRegister result_reg = ToDoubleRegister(result);

  EmitNumberUntagD(input_reg, result_reg,
                   instr->hydrogen()->deoptimize_on_undefined(),
                   instr->environment());
}


void LCodeGen::DoDoubleToI(LDoubleToI* instr) {
  Register result_reg = ToRegister(instr->result());
  Register scratch1 = scratch0();
  Register scratch2 = ToRegister(instr->TempAt(0));
  DwVfpRegister double_input = ToDoubleRegister(instr->InputAt(0));
  SwVfpRegister single_scratch = double_scratch0().low();

  Label done;

  if (instr->truncating()) {
    Register scratch3 = ToRegister(instr->TempAt(1));
    __ EmitECMATruncate(result_reg,
                        double_input,
                        single_scratch,
                        scratch1,
                        scratch2,
                        scratch3);
  } else {
    VFPRoundingMode rounding_mode = kRoundToMinusInf;
    __ EmitVFPTruncate(rounding_mode,
                       single_scratch,
                       double_input,
                       scratch1,
                       scratch2,
                       kCheckForInexactConversion);
    // Deoptimize if we had a vfp invalid exception,
    // including inexact operation.
    DeoptimizeIf(ne, instr->environment());
    // Retrieve the result.
    __ vmov(result_reg, single_scratch);
  }
    __ bind(&done);
}


void LCodeGen::DoCheckSmi(LCheckSmi* instr) {
  LOperand* input = instr->InputAt(0);
  __ tst(ToRegister(input), Operand(kSmiTagMask));
  DeoptimizeIf(ne, instr->environment());
}


void LCodeGen::DoCheckNonSmi(LCheckNonSmi* instr) {
  LOperand* input = instr->InputAt(0);
  __ tst(ToRegister(input), Operand(kSmiTagMask));
  DeoptimizeIf(eq, instr->environment());
}


void LCodeGen::DoCheckInstanceType(LCheckInstanceType* instr) {
  Register input = ToRegister(instr->InputAt(0));
  Register scratch = scratch0();

  __ ldr(scratch, FieldMemOperand(input, HeapObject::kMapOffset));
  __ ldrb(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));

  if (instr->hydrogen()->is_interval_check()) {
    InstanceType first;
    InstanceType last;
    instr->hydrogen()->GetCheckInterval(&first, &last);

    __ cmp(scratch, Operand(first));

    // If there is only one type in the interval check for equality.
    if (first == last) {
      DeoptimizeIf(ne, instr->environment());
    } else {
      DeoptimizeIf(lo, instr->environment());
      // Omit check for the last type.
      if (last != LAST_TYPE) {
        __ cmp(scratch, Operand(last));
        DeoptimizeIf(hi, instr->environment());
      }
    }
  } else {
    uint8_t mask;
    uint8_t tag;
    instr->hydrogen()->GetCheckMaskAndTag(&mask, &tag);

    if (IsPowerOf2(mask)) {
      ASSERT(tag == 0 || IsPowerOf2(tag));
      __ tst(scratch, Operand(mask));
      DeoptimizeIf(tag == 0 ? ne : eq, instr->environment());
    } else {
      __ and_(scratch, scratch, Operand(mask));
      __ cmp(scratch, Operand(tag));
      DeoptimizeIf(ne, instr->environment());
    }
  }
}


void LCodeGen::DoCheckFunction(LCheckFunction* instr) {
  ASSERT(instr->InputAt(0)->IsRegister());
  Register reg = ToRegister(instr->InputAt(0));
  __ cmp(reg, Operand(instr->hydrogen()->target()));
  DeoptimizeIf(ne, instr->environment());
}


void LCodeGen::DoCheckMap(LCheckMap* instr) {
  Register scratch = scratch0();
  LOperand* input = instr->InputAt(0);
  ASSERT(input->IsRegister());
  Register reg = ToRegister(input);
  __ ldr(scratch, FieldMemOperand(reg, HeapObject::kMapOffset));
  __ cmp(scratch, Operand(instr->hydrogen()->map()));
  DeoptimizeIf(ne, instr->environment());
}


void LCodeGen::DoClampDToUint8(LClampDToUint8* instr) {
  DoubleRegister value_reg = ToDoubleRegister(instr->unclamped());
  Register result_reg = ToRegister(instr->result());
  DoubleRegister temp_reg = ToDoubleRegister(instr->TempAt(0));
  __ ClampDoubleToUint8(result_reg, value_reg, temp_reg);
}


void LCodeGen::DoClampIToUint8(LClampIToUint8* instr) {
  Register unclamped_reg = ToRegister(instr->unclamped());
  Register result_reg = ToRegister(instr->result());
  __ ClampUint8(result_reg, unclamped_reg);
}


void LCodeGen::DoClampTToUint8(LClampTToUint8* instr) {
  Register scratch = scratch0();
  Register input_reg = ToRegister(instr->unclamped());
  Register result_reg = ToRegister(instr->result());
  DoubleRegister temp_reg = ToDoubleRegister(instr->TempAt(0));
  Label is_smi, done, heap_number;

  // Both smi and heap number cases are handled.
  __ JumpIfSmi(input_reg, &is_smi);

  // Check for heap number
  __ ldr(scratch, FieldMemOperand(input_reg, HeapObject::kMapOffset));
  __ cmp(scratch, Operand(factory()->heap_number_map()));
  __ b(eq, &heap_number);

  // Check for undefined. Undefined is converted to zero for clamping
  // conversions.
  __ cmp(input_reg, Operand(factory()->undefined_value()));
  DeoptimizeIf(ne, instr->environment());
  __ mov(result_reg, Operand(0));
  __ jmp(&done);

  // Heap number
  __ bind(&heap_number);
  __ vldr(double_scratch0(), FieldMemOperand(input_reg,
                                             HeapNumber::kValueOffset));
  __ ClampDoubleToUint8(result_reg, double_scratch0(), temp_reg);
  __ jmp(&done);

  // smi
  __ bind(&is_smi);
  __ SmiUntag(result_reg, input_reg);
  __ ClampUint8(result_reg, result_reg);

  __ bind(&done);
}


void LCodeGen::LoadHeapObject(Register result,
                              Handle<HeapObject> object) {
  if (heap()->InNewSpace(*object)) {
    Handle<JSGlobalPropertyCell> cell =
        factory()->NewJSGlobalPropertyCell(object);
    __ mov(result, Operand(cell));
    __ ldr(result, FieldMemOperand(result, JSGlobalPropertyCell::kValueOffset));
  } else {
    __ mov(result, Operand(object));
  }
}


void LCodeGen::DoCheckPrototypeMaps(LCheckPrototypeMaps* instr) {
  Register temp1 = ToRegister(instr->TempAt(0));
  Register temp2 = ToRegister(instr->TempAt(1));

  Handle<JSObject> holder = instr->holder();
  Handle<JSObject> current_prototype = instr->prototype();

  // Load prototype object.
  LoadHeapObject(temp1, current_prototype);

  // Check prototype maps up to the holder.
  while (!current_prototype.is_identical_to(holder)) {
    __ ldr(temp2, FieldMemOperand(temp1, HeapObject::kMapOffset));
    __ cmp(temp2, Operand(Handle<Map>(current_prototype->map())));
    DeoptimizeIf(ne, instr->environment());
    current_prototype =
        Handle<JSObject>(JSObject::cast(current_prototype->GetPrototype()));
    // Load next prototype object.
    LoadHeapObject(temp1, current_prototype);
  }

  // Check the holder map.
  __ ldr(temp2, FieldMemOperand(temp1, HeapObject::kMapOffset));
  __ cmp(temp2, Operand(Handle<Map>(current_prototype->map())));
  DeoptimizeIf(ne, instr->environment());
}


void LCodeGen::DoArrayLiteral(LArrayLiteral* instr) {
  __ ldr(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ ldr(r3, FieldMemOperand(r3, JSFunction::kLiteralsOffset));
  __ mov(r2, Operand(Smi::FromInt(instr->hydrogen()->literal_index())));
  __ mov(r1, Operand(instr->hydrogen()->constant_elements()));
  __ Push(r3, r2, r1);

  // Pick the right runtime function or stub to call.
  int length = instr->hydrogen()->length();
  if (instr->hydrogen()->IsCopyOnWrite()) {
    ASSERT(instr->hydrogen()->depth() == 1);
    FastCloneShallowArrayStub::Mode mode =
        FastCloneShallowArrayStub::COPY_ON_WRITE_ELEMENTS;
    FastCloneShallowArrayStub stub(mode, length);
    CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
  } else if (instr->hydrogen()->depth() > 1) {
    CallRuntime(Runtime::kCreateArrayLiteral, 3, instr);
  } else if (length > FastCloneShallowArrayStub::kMaximumClonedLength) {
    CallRuntime(Runtime::kCreateArrayLiteralShallow, 3, instr);
  } else {
    FastCloneShallowArrayStub::Mode mode =
        FastCloneShallowArrayStub::CLONE_ELEMENTS;
    FastCloneShallowArrayStub stub(mode, length);
    CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
  }
}


void LCodeGen::DoObjectLiteral(LObjectLiteral* instr) {
  __ ldr(r4, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ ldr(r4, FieldMemOperand(r4, JSFunction::kLiteralsOffset));
  __ mov(r3, Operand(Smi::FromInt(instr->hydrogen()->literal_index())));
  __ mov(r2, Operand(instr->hydrogen()->constant_properties()));
  __ mov(r1, Operand(Smi::FromInt(instr->hydrogen()->fast_elements() ? 1 : 0)));
  __ Push(r4, r3, r2, r1);

  // Pick the right runtime function to call.
  if (instr->hydrogen()->depth() > 1) {
    CallRuntime(Runtime::kCreateObjectLiteral, 4, instr);
  } else {
    CallRuntime(Runtime::kCreateObjectLiteralShallow, 4, instr);
  }
}


void LCodeGen::DoToFastProperties(LToFastProperties* instr) {
  ASSERT(ToRegister(instr->InputAt(0)).is(r0));
  __ push(r0);
  CallRuntime(Runtime::kToFastProperties, 1, instr);
}


void LCodeGen::DoRegExpLiteral(LRegExpLiteral* instr) {
  Label materialized;
  // Registers will be used as follows:
  // r3 = JS function.
  // r7 = literals array.
  // r1 = regexp literal.
  // r0 = regexp literal clone.
  // r2 and r4-r6 are used as temporaries.
  __ ldr(r3, MemOperand(fp, JavaScriptFrameConstants::kFunctionOffset));
  __ ldr(r7, FieldMemOperand(r3, JSFunction::kLiteralsOffset));
  int literal_offset = FixedArray::kHeaderSize +
      instr->hydrogen()->literal_index() * kPointerSize;
  __ ldr(r1, FieldMemOperand(r7, literal_offset));
  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(r1, ip);
  __ b(ne, &materialized);

  // Create regexp literal using runtime function
  // Result will be in r0.
  __ mov(r6, Operand(Smi::FromInt(instr->hydrogen()->literal_index())));
  __ mov(r5, Operand(instr->hydrogen()->pattern()));
  __ mov(r4, Operand(instr->hydrogen()->flags()));
  __ Push(r7, r6, r5, r4);
  CallRuntime(Runtime::kMaterializeRegExpLiteral, 4, instr);
  __ mov(r1, r0);

  __ bind(&materialized);
  int size = JSRegExp::kSize + JSRegExp::kInObjectFieldCount * kPointerSize;
  Label allocated, runtime_allocate;

  __ AllocateInNewSpace(size, r0, r2, r3, &runtime_allocate, TAG_OBJECT);
  __ jmp(&allocated);

  __ bind(&runtime_allocate);
  __ mov(r0, Operand(Smi::FromInt(size)));
  __ Push(r1, r0);
  CallRuntime(Runtime::kAllocateInNewSpace, 1, instr);
  __ pop(r1);

  __ bind(&allocated);
  // Copy the content into the newly allocated memory.
  // (Unroll copy loop once for better throughput).
  for (int i = 0; i < size - kPointerSize; i += 2 * kPointerSize) {
    __ ldr(r3, FieldMemOperand(r1, i));
    __ ldr(r2, FieldMemOperand(r1, i + kPointerSize));
    __ str(r3, FieldMemOperand(r0, i));
    __ str(r2, FieldMemOperand(r0, i + kPointerSize));
  }
  if ((size % (2 * kPointerSize)) != 0) {
    __ ldr(r3, FieldMemOperand(r1, size - kPointerSize));
    __ str(r3, FieldMemOperand(r0, size - kPointerSize));
  }
}


void LCodeGen::DoFunctionLiteral(LFunctionLiteral* instr) {
  // Use the fast case closure allocation code that allocates in new
  // space for nested functions that don't need literals cloning.
  Handle<SharedFunctionInfo> shared_info = instr->shared_info();
  bool pretenure = instr->hydrogen()->pretenure();
  if (!pretenure && shared_info->num_literals() == 0) {
    FastNewClosureStub stub(
        shared_info->strict_mode() ? kStrictMode : kNonStrictMode);
    __ mov(r1, Operand(shared_info));
    __ push(r1);
    CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
  } else {
    __ mov(r2, Operand(shared_info));
    __ mov(r1, Operand(pretenure
                       ? factory()->true_value()
                       : factory()->false_value()));
    __ Push(cp, r2, r1);
    CallRuntime(Runtime::kNewClosure, 3, instr);
  }
}


void LCodeGen::DoTypeof(LTypeof* instr) {
  Register input = ToRegister(instr->InputAt(0));
  __ push(input);
  CallRuntime(Runtime::kTypeof, 1, instr);
}


void LCodeGen::DoTypeofIsAndBranch(LTypeofIsAndBranch* instr) {
  Register input = ToRegister(instr->InputAt(0));
  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());
  Label* true_label = chunk_->GetAssemblyLabel(true_block);
  Label* false_label = chunk_->GetAssemblyLabel(false_block);

  Condition final_branch_condition = EmitTypeofIs(true_label,
                                                  false_label,
                                                  input,
                                                  instr->type_literal());

  EmitBranch(true_block, false_block, final_branch_condition);
}


Condition LCodeGen::EmitTypeofIs(Label* true_label,
                                 Label* false_label,
                                 Register input,
                                 Handle<String> type_name) {
  Condition final_branch_condition = kNoCondition;
  Register scratch = scratch0();
  if (type_name->Equals(heap()->number_symbol())) {
    __ JumpIfSmi(input, true_label);
    __ ldr(input, FieldMemOperand(input, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
    __ cmp(input, Operand(ip));
    final_branch_condition = eq;

  } else if (type_name->Equals(heap()->string_symbol())) {
    __ JumpIfSmi(input, false_label);
    __ CompareObjectType(input, input, scratch, FIRST_NONSTRING_TYPE);
    __ b(ge, false_label);
    __ ldrb(ip, FieldMemOperand(input, Map::kBitFieldOffset));
    __ tst(ip, Operand(1 << Map::kIsUndetectable));
    final_branch_condition = eq;

  } else if (type_name->Equals(heap()->boolean_symbol())) {
    __ CompareRoot(input, Heap::kTrueValueRootIndex);
    __ b(eq, true_label);
    __ CompareRoot(input, Heap::kFalseValueRootIndex);
    final_branch_condition = eq;

  } else if (FLAG_harmony_typeof && type_name->Equals(heap()->null_symbol())) {
    __ CompareRoot(input, Heap::kNullValueRootIndex);
    final_branch_condition = eq;

  } else if (type_name->Equals(heap()->undefined_symbol())) {
    __ CompareRoot(input, Heap::kUndefinedValueRootIndex);
    __ b(eq, true_label);
    __ JumpIfSmi(input, false_label);
    // Check for undetectable objects => true.
    __ ldr(input, FieldMemOperand(input, HeapObject::kMapOffset));
    __ ldrb(ip, FieldMemOperand(input, Map::kBitFieldOffset));
    __ tst(ip, Operand(1 << Map::kIsUndetectable));
    final_branch_condition = ne;

  } else if (type_name->Equals(heap()->function_symbol())) {
    __ JumpIfSmi(input, false_label);
    __ CompareObjectType(input, input, scratch,
                         FIRST_CALLABLE_SPEC_OBJECT_TYPE);
    final_branch_condition = ge;

  } else if (type_name->Equals(heap()->object_symbol())) {
    __ JumpIfSmi(input, false_label);
    if (!FLAG_harmony_typeof) {
      __ CompareRoot(input, Heap::kNullValueRootIndex);
      __ b(eq, true_label);
    }
    __ CompareObjectType(input, input, scratch,
                         FIRST_NONCALLABLE_SPEC_OBJECT_TYPE);
    __ b(lt, false_label);
    __ CompareInstanceType(input, scratch, LAST_NONCALLABLE_SPEC_OBJECT_TYPE);
    __ b(gt, false_label);
    // Check for undetectable objects => false.
    __ ldrb(ip, FieldMemOperand(input, Map::kBitFieldOffset));
    __ tst(ip, Operand(1 << Map::kIsUndetectable));
    final_branch_condition = eq;

  } else {
    final_branch_condition = ne;
    __ b(false_label);
    // A dead branch instruction will be generated after this point.
  }

  return final_branch_condition;
}


void LCodeGen::DoIsConstructCallAndBranch(LIsConstructCallAndBranch* instr) {
  Register temp1 = ToRegister(instr->TempAt(0));
  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  EmitIsConstructCall(temp1, scratch0());
  EmitBranch(true_block, false_block, eq);
}


void LCodeGen::EmitIsConstructCall(Register temp1, Register temp2) {
  ASSERT(!temp1.is(temp2));
  // Get the frame pointer for the calling frame.
  __ ldr(temp1, MemOperand(fp, StandardFrameConstants::kCallerFPOffset));

  // Skip the arguments adaptor frame if it exists.
  Label check_frame_marker;
  __ ldr(temp2, MemOperand(temp1, StandardFrameConstants::kContextOffset));
  __ cmp(temp2, Operand(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ b(ne, &check_frame_marker);
  __ ldr(temp1, MemOperand(temp1, StandardFrameConstants::kCallerFPOffset));

  // Check the marker in the calling frame.
  __ bind(&check_frame_marker);
  __ ldr(temp1, MemOperand(temp1, StandardFrameConstants::kMarkerOffset));
  __ cmp(temp1, Operand(Smi::FromInt(StackFrame::CONSTRUCT)));
}


void LCodeGen::EnsureSpaceForLazyDeopt() {
  // Ensure that we have enough space after the previous lazy-bailout
  // instruction for patching the code here.
  int current_pc = masm()->pc_offset();
  int patch_size = Deoptimizer::patch_size();
  if (current_pc < last_lazy_deopt_pc_ + patch_size) {
    int padding_size = last_lazy_deopt_pc_ + patch_size - current_pc;
    ASSERT_EQ(0, padding_size % Assembler::kInstrSize);
    while (padding_size > 0) {
      __ nop();
      padding_size -= Assembler::kInstrSize;
    }
  }
  last_lazy_deopt_pc_ = masm()->pc_offset();
}


void LCodeGen::DoLazyBailout(LLazyBailout* instr) {
  EnsureSpaceForLazyDeopt();
  ASSERT(instr->HasEnvironment());
  LEnvironment* env = instr->environment();
  RegisterEnvironmentForDeoptimization(env, Safepoint::kLazyDeopt);
  safepoints_.RecordLazyDeoptimizationIndex(env->deoptimization_index());
}


void LCodeGen::DoDeoptimize(LDeoptimize* instr) {
  DeoptimizeIf(al, instr->environment());
}


void LCodeGen::DoDeleteProperty(LDeleteProperty* instr) {
  Register object = ToRegister(instr->object());
  Register key = ToRegister(instr->key());
  Register strict = scratch0();
  __ mov(strict, Operand(Smi::FromInt(strict_mode_flag())));
  __ Push(object, key, strict);
  ASSERT(instr->HasPointerMap() && instr->HasDeoptimizationEnvironment());
  LPointerMap* pointers = instr->pointer_map();
  RecordPosition(pointers->position());
  SafepointGenerator safepoint_generator(
      this, pointers, Safepoint::kLazyDeopt);
  __ InvokeBuiltin(Builtins::DELETE, CALL_FUNCTION, safepoint_generator);
}


void LCodeGen::DoIn(LIn* instr) {
  Register obj = ToRegister(instr->object());
  Register key = ToRegister(instr->key());
  __ Push(key, obj);
  ASSERT(instr->HasPointerMap() && instr->HasDeoptimizationEnvironment());
  LPointerMap* pointers = instr->pointer_map();
  RecordPosition(pointers->position());
  SafepointGenerator safepoint_generator(this, pointers, Safepoint::kLazyDeopt);
  __ InvokeBuiltin(Builtins::IN, CALL_FUNCTION, safepoint_generator);
}


void LCodeGen::DoDeferredStackCheck(LStackCheck* instr) {
  PushSafepointRegistersScope scope(this, Safepoint::kWithRegisters);
  __ CallRuntimeSaveDoubles(Runtime::kStackGuard);
  RecordSafepointWithLazyDeopt(
      instr, RECORD_SAFEPOINT_WITH_REGISTERS_AND_NO_ARGUMENTS);
  ASSERT(instr->HasEnvironment());
  LEnvironment* env = instr->environment();
  safepoints_.RecordLazyDeoptimizationIndex(env->deoptimization_index());
}


void LCodeGen::DoStackCheck(LStackCheck* instr) {
  class DeferredStackCheck: public LDeferredCode {
   public:
    DeferredStackCheck(LCodeGen* codegen, LStackCheck* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() { codegen()->DoDeferredStackCheck(instr_); }
   private:
    LStackCheck* instr_;
  };

  ASSERT(instr->HasEnvironment());
  LEnvironment* env = instr->environment();
  // There is no LLazyBailout instruction for stack-checks. We have to
  // prepare for lazy deoptimization explicitly here.
  if (instr->hydrogen()->is_function_entry()) {
    // Perform stack overflow check.
    Label done;
    __ LoadRoot(ip, Heap::kStackLimitRootIndex);
    __ cmp(sp, Operand(ip));
    __ b(hs, &done);
    StackCheckStub stub;
    CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
    EnsureSpaceForLazyDeopt();
    __ bind(&done);
    RegisterEnvironmentForDeoptimization(env, Safepoint::kLazyDeopt);
    safepoints_.RecordLazyDeoptimizationIndex(env->deoptimization_index());
  } else {
    ASSERT(instr->hydrogen()->is_backwards_branch());
    // Perform stack overflow check if this goto needs it before jumping.
    DeferredStackCheck* deferred_stack_check =
        new DeferredStackCheck(this, instr);
    __ LoadRoot(ip, Heap::kStackLimitRootIndex);
    __ cmp(sp, Operand(ip));
    __ b(lo, deferred_stack_check->entry());
    EnsureSpaceForLazyDeopt();
    __ bind(instr->done_label());
    deferred_stack_check->SetExit(instr->done_label());
    RegisterEnvironmentForDeoptimization(env, Safepoint::kLazyDeopt);
    // Don't record a deoptimization index for the safepoint here.
    // This will be done explicitly when emitting call and the safepoint in
    // the deferred code.
  }
}


void LCodeGen::DoOsrEntry(LOsrEntry* instr) {
  // This is a pseudo-instruction that ensures that the environment here is
  // properly registered for deoptimization and records the assembler's PC
  // offset.
  LEnvironment* environment = instr->environment();
  environment->SetSpilledRegisters(instr->SpilledRegisterArray(),
                                   instr->SpilledDoubleRegisterArray());

  // If the environment were already registered, we would have no way of
  // backpatching it with the spill slot operands.
  ASSERT(!environment->HasBeenRegistered());
  RegisterEnvironmentForDeoptimization(environment, Safepoint::kNoLazyDeopt);
  ASSERT(osr_pc_offset_ == -1);
  osr_pc_offset_ = masm()->pc_offset();
}




#undef __

} }  // namespace v8::internal
