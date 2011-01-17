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

#include "arm/lithium-codegen-arm.h"
#include "code-stubs.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {


class SafepointGenerator : public PostCallGenerator {
 public:
  SafepointGenerator(LCodeGen* codegen,
                     LPointerMap* pointers,
                     int deoptimization_index)
      : codegen_(codegen),
        pointers_(pointers),
        deoptimization_index_(deoptimization_index) { }
  virtual ~SafepointGenerator() { }

  virtual void Generate() {
    codegen_->RecordSafepoint(pointers_, deoptimization_index_);
  }

 private:
  LCodeGen* codegen_;
  LPointerMap* pointers_;
  int deoptimization_index_;
};


class LGapNode: public ZoneObject {
 public:
  explicit LGapNode(LOperand* operand)
      : operand_(operand), resolved_(false), visited_id_(-1) { }

  LOperand* operand() const { return operand_; }
  bool IsResolved() const { return !IsAssigned() || resolved_; }
  void MarkResolved() {
    ASSERT(!IsResolved());
    resolved_ = true;
  }
  int visited_id() const { return visited_id_; }
  void set_visited_id(int id) {
    ASSERT(id > visited_id_);
    visited_id_ = id;
  }

  bool IsAssigned() const { return assigned_from_.is_set(); }
  LGapNode* assigned_from() const { return assigned_from_.get(); }
  void set_assigned_from(LGapNode* n) { assigned_from_.set(n); }

 private:
  LOperand* operand_;
  SetOncePointer<LGapNode> assigned_from_;
  bool resolved_;
  int visited_id_;
};


LGapResolver::LGapResolver()
    : nodes_(32),
      identified_cycles_(4),
      result_(16),
      next_visited_id_(0) {
}


const ZoneList<LMoveOperands>* LGapResolver::Resolve(
    const ZoneList<LMoveOperands>* moves,
    LOperand* marker_operand) {
  nodes_.Rewind(0);
  identified_cycles_.Rewind(0);
  result_.Rewind(0);
  next_visited_id_ = 0;

  for (int i = 0; i < moves->length(); ++i) {
    LMoveOperands move = moves->at(i);
    if (!move.IsRedundant()) RegisterMove(move);
  }

  for (int i = 0; i < identified_cycles_.length(); ++i) {
    ResolveCycle(identified_cycles_[i], marker_operand);
  }

  int unresolved_nodes;
  do {
    unresolved_nodes = 0;
    for (int j = 0; j < nodes_.length(); j++) {
      LGapNode* node = nodes_[j];
      if (!node->IsResolved() && node->assigned_from()->IsResolved()) {
        AddResultMove(node->assigned_from(), node);
        node->MarkResolved();
      }
      if (!node->IsResolved()) ++unresolved_nodes;
    }
  } while (unresolved_nodes > 0);
  return &result_;
}


void LGapResolver::AddResultMove(LGapNode* from, LGapNode* to) {
  AddResultMove(from->operand(), to->operand());
}


void LGapResolver::AddResultMove(LOperand* from, LOperand* to) {
  result_.Add(LMoveOperands(from, to));
}


void LGapResolver::ResolveCycle(LGapNode* start, LOperand* marker_operand) {
  ZoneList<LOperand*> cycle_operands(8);
  cycle_operands.Add(marker_operand);
  LGapNode* cur = start;
  do {
    cur->MarkResolved();
    cycle_operands.Add(cur->operand());
    cur = cur->assigned_from();
  } while (cur != start);
  cycle_operands.Add(marker_operand);

  for (int i = cycle_operands.length() - 1; i > 0; --i) {
    LOperand* from = cycle_operands[i];
    LOperand* to = cycle_operands[i - 1];
    AddResultMove(from, to);
  }
}


bool LGapResolver::CanReach(LGapNode* a, LGapNode* b, int visited_id) {
  ASSERT(a != b);
  LGapNode* cur = a;
  while (cur != b && cur->visited_id() != visited_id && cur->IsAssigned()) {
    cur->set_visited_id(visited_id);
    cur = cur->assigned_from();
  }

  return cur == b;
}


bool LGapResolver::CanReach(LGapNode* a, LGapNode* b) {
  ASSERT(a != b);
  return CanReach(a, b, next_visited_id_++);
}


void LGapResolver::RegisterMove(LMoveOperands move) {
  if (move.from()->IsConstantOperand()) {
    // Constant moves should be last in the machine code. Therefore add them
    // first to the result set.
    AddResultMove(move.from(), move.to());
  } else {
    LGapNode* from = LookupNode(move.from());
    LGapNode* to = LookupNode(move.to());
    if (to->IsAssigned() && to->assigned_from() == from) {
      move.Eliminate();
      return;
    }
    ASSERT(!to->IsAssigned());
    if (CanReach(from, to)) {
      // This introduces a cycle. Save.
      identified_cycles_.Add(from);
    }
    to->set_assigned_from(from);
  }
}


LGapNode* LGapResolver::LookupNode(LOperand* operand) {
  for (int i = 0; i < nodes_.length(); ++i) {
    if (nodes_[i]->operand()->Equals(operand)) return nodes_[i];
  }

  // No node found => create a new one.
  LGapNode* result = new LGapNode(operand);
  nodes_.Add(result);
  return result;
}


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
      GenerateSafepointTable();
}


void LCodeGen::FinishCode(Handle<Code> code) {
  ASSERT(is_done());
  code->set_stack_slots(StackSlotCount());
  code->set_safepoint_table_start(safepoints_.GetCodeOffset());
  PopulateDeoptimizationData(code);
}


void LCodeGen::Abort(const char* format, ...) {
  if (FLAG_trace_bailout) {
    SmartPointer<char> debug_name = graph()->debug_name()->ToCString();
    PrintF("Aborting LCodeGen in @\"%s\": ", *debug_name);
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

  __ stm(db_w, sp, r1.bit() | cp.bit() | fp.bit() | lr.bit());
  __ add(fp, sp, Operand(2 * kPointerSize));  // Adjust FP to point to saved FP.

  // Reserve space for the stack slots needed by the code.
  int slots = StackSlotCount();
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
  return !is_aborted();
}


LInstruction* LCodeGen::GetNextInstruction() {
  if (current_instruction_ < instructions_->length() - 1) {
    return instructions_->at(current_instruction_ + 1);
  } else {
    return NULL;
  }
}


bool LCodeGen::GenerateDeferredCode() {
  ASSERT(is_generating());
  for (int i = 0; !is_aborted() && i < deferred_.length(); i++) {
    LDeferredCode* code = deferred_[i];
    __ bind(code->entry());
    code->Generate();
    __ jmp(code->exit());
  }

  // Deferred code is the last part of the instruction sequence. Mark
  // the generated code as done unless we bailed out.
  if (!is_aborted()) status_ = DONE;
  return !is_aborted();
}


bool LCodeGen::GenerateSafepointTable() {
  ASSERT(is_done());
  safepoints_.Emit(masm(), StackSlotCount());
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
  // TODO(regis): Revisit.
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
    int src_index = StackSlotCount() + op->index();
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
  if (instr != NULL) {
    LPointerMap* pointers = instr->pointer_map();
    RecordPosition(pointers->position());
    __ Call(code, mode);
    RegisterLazyDeoptimization(instr);
  } else {
    LPointerMap no_pointers(0);
    RecordPosition(no_pointers.position());
    __ Call(code, mode);
    RecordSafepoint(&no_pointers, Safepoint::kNoDeoptimizationIndex);
  }
}


void LCodeGen::CallRuntime(Runtime::Function* function,
                           int num_arguments,
                           LInstruction* instr) {
  ASSERT(instr != NULL);
  LPointerMap* pointers = instr->pointer_map();
  ASSERT(pointers != NULL);
  RecordPosition(pointers->position());

  __ CallRuntime(function, num_arguments);
  // Runtime calls to Throw are not supposed to ever return at the
  // call site, so don't register lazy deoptimization for these. We do
  // however have to record a safepoint since throwing exceptions can
  // cause garbage collections.
  if (!instr->IsThrow()) {
    RegisterLazyDeoptimization(instr);
  } else {
    RecordSafepoint(instr->pointer_map(), Safepoint::kNoDeoptimizationIndex);
  }
}


void LCodeGen::RegisterLazyDeoptimization(LInstruction* instr) {
  // Create the environment to bailout to. If the call has side effects
  // execution has to continue after the call otherwise execution can continue
  // from a previous bailout point repeating the call.
  LEnvironment* deoptimization_environment;
  if (instr->HasDeoptimizationEnvironment()) {
    deoptimization_environment = instr->deoptimization_environment();
  } else {
    deoptimization_environment = instr->environment();
  }

  RegisterEnvironmentForDeoptimization(deoptimization_environment);
  RecordSafepoint(instr->pointer_map(),
                  deoptimization_environment->deoptimization_index());
}


void LCodeGen::RegisterEnvironmentForDeoptimization(LEnvironment* environment) {
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
    environment->Register(deoptimization_index, translation.index());
    deoptimizations_.Add(environment);
  }
}


void LCodeGen::DeoptimizeIf(Condition cc, LEnvironment* environment) {
  RegisterEnvironmentForDeoptimization(environment);
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

  if (cc == no_condition) {
    if (FLAG_trap_on_deopt) __ stop("trap_on_deopt");
    __ Jump(entry, RelocInfo::RUNTIME_ENTRY);
  } else {
    if (FLAG_trap_on_deopt) {
      Label done;
      __ b(&done, NegateCondition(cc));
      __ stop("trap_on_deopt");
      __ Jump(entry, RelocInfo::RUNTIME_ENTRY);
      __ bind(&done);
    } else {
      __ Jump(entry, RelocInfo::RUNTIME_ENTRY, cc);
    }
  }
}


void LCodeGen::PopulateDeoptimizationData(Handle<Code> code) {
  int length = deoptimizations_.length();
  if (length == 0) return;
  ASSERT(FLAG_deopt);
  Handle<DeoptimizationInputData> data =
      Factory::NewDeoptimizationInputData(length, TENURED);

  data->SetTranslationByteArray(*translations_.CreateByteArray());
  data->SetInlinedFunctionCount(Smi::FromInt(inlined_function_count_));

  Handle<FixedArray> literals =
      Factory::NewFixedArray(deoptimization_literals_.length(), TENURED);
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


void LCodeGen::RecordSafepoint(LPointerMap* pointers,
                               int deoptimization_index) {
  const ZoneList<LOperand*>* operands = pointers->operands();
  Safepoint safepoint = safepoints_.DefineSafepoint(masm(),
                                                    deoptimization_index);
  for (int i = 0; i < operands->length(); i++) {
    LOperand* pointer = operands->at(i);
    if (pointer->IsStackSlot()) {
      safepoint.DefinePointerSlot(pointer->index());
    }
  }
}


void LCodeGen::RecordSafepointWithRegisters(LPointerMap* pointers,
                                            int arguments,
                                            int deoptimization_index) {
  const ZoneList<LOperand*>* operands = pointers->operands();
  Safepoint safepoint =
      safepoints_.DefineSafepointWithRegisters(
          masm(), arguments, deoptimization_index);
  for (int i = 0; i < operands->length(); i++) {
    LOperand* pointer = operands->at(i);
    if (pointer->IsStackSlot()) {
      safepoint.DefinePointerSlot(pointer->index());
    } else if (pointer->IsRegister()) {
      safepoint.DefinePointerRegister(ToRegister(pointer));
    }
  }
  // Register cp always contains a pointer to the context.
  safepoint.DefinePointerRegister(cp);
}


void LCodeGen::RecordSafepointWithRegistersAndDoubles(
    LPointerMap* pointers,
    int arguments,
    int deoptimization_index) {
  const ZoneList<LOperand*>* operands = pointers->operands();
  Safepoint safepoint =
      safepoints_.DefineSafepointWithRegistersAndDoubles(
          masm(), arguments, deoptimization_index);
  for (int i = 0; i < operands->length(); i++) {
    LOperand* pointer = operands->at(i);
    if (pointer->IsStackSlot()) {
      safepoint.DefinePointerSlot(pointer->index());
    } else if (pointer->IsRegister()) {
      safepoint.DefinePointerRegister(ToRegister(pointer));
    }
  }
  // Register cp always contains a pointer to the context.
  safepoint.DefinePointerRegister(cp);
}


void LCodeGen::RecordPosition(int position) {
  if (!FLAG_debug_info || position == RelocInfo::kNoPosition) return;
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
  LCodeGen::DoGap(label);
}


void LCodeGen::DoParallelMove(LParallelMove* move) {
  // d0 must always be a scratch register.
  DoubleRegister dbl_scratch = d0;
  LUnallocated marker_operand(LUnallocated::NONE);

  Register core_scratch = scratch0();
  bool destroys_core_scratch = false;

  const ZoneList<LMoveOperands>* moves =
      resolver_.Resolve(move->move_operands(), &marker_operand);
  for (int i = moves->length() - 1; i >= 0; --i) {
    LMoveOperands move = moves->at(i);
    LOperand* from = move.from();
    LOperand* to = move.to();
    ASSERT(!from->IsDoubleRegister() ||
           !ToDoubleRegister(from).is(dbl_scratch));
    ASSERT(!to->IsDoubleRegister() || !ToDoubleRegister(to).is(dbl_scratch));
    ASSERT(!from->IsRegister() || !ToRegister(from).is(core_scratch));
    ASSERT(!to->IsRegister() || !ToRegister(to).is(core_scratch));
    if (from == &marker_operand) {
      if (to->IsRegister()) {
        __ mov(ToRegister(to), core_scratch);
        ASSERT(destroys_core_scratch);
      } else if (to->IsStackSlot()) {
        __ str(core_scratch, ToMemOperand(to));
        ASSERT(destroys_core_scratch);
      } else if (to->IsDoubleRegister()) {
        __ vmov(ToDoubleRegister(to), dbl_scratch);
      } else {
        ASSERT(to->IsDoubleStackSlot());
        // TODO(regis): Why is vstr not taking a MemOperand?
        // __ vstr(dbl_scratch, ToMemOperand(to));
        MemOperand to_operand = ToMemOperand(to);
        __ vstr(dbl_scratch, to_operand.rn(), to_operand.offset());
      }
    } else if (to == &marker_operand) {
      if (from->IsRegister() || from->IsConstantOperand()) {
        __ mov(core_scratch, ToOperand(from));
        destroys_core_scratch = true;
      } else if (from->IsStackSlot()) {
        __ ldr(core_scratch, ToMemOperand(from));
        destroys_core_scratch = true;
      } else if (from->IsDoubleRegister()) {
        __ vmov(dbl_scratch, ToDoubleRegister(from));
      } else {
        ASSERT(from->IsDoubleStackSlot());
        // TODO(regis): Why is vldr not taking a MemOperand?
        // __ vldr(dbl_scratch, ToMemOperand(from));
        MemOperand from_operand = ToMemOperand(from);
        __ vldr(dbl_scratch, from_operand.rn(), from_operand.offset());
      }
    } else if (from->IsConstantOperand()) {
      if (to->IsRegister()) {
        __ mov(ToRegister(to), ToOperand(from));
      } else {
        ASSERT(to->IsStackSlot());
        __ mov(ip, ToOperand(from));
        __ str(ip, ToMemOperand(to));
      }
    } else if (from->IsRegister()) {
      if (to->IsRegister()) {
        __ mov(ToRegister(to), ToOperand(from));
      } else {
        ASSERT(to->IsStackSlot());
        __ str(ToRegister(from), ToMemOperand(to));
      }
    } else if (to->IsRegister()) {
      ASSERT(from->IsStackSlot());
      __ ldr(ToRegister(to), ToMemOperand(from));
    } else if (from->IsStackSlot()) {
      ASSERT(to->IsStackSlot());
      __ ldr(ip, ToMemOperand(from));
      __ str(ip, ToMemOperand(to));
    } else if (from->IsDoubleRegister()) {
      if (to->IsDoubleRegister()) {
      __ vmov(ToDoubleRegister(to), ToDoubleRegister(from));
      } else {
        ASSERT(to->IsDoubleStackSlot());
        // TODO(regis): Why is vstr not taking a MemOperand?
        // __ vstr(dbl_scratch, ToMemOperand(to));
        MemOperand to_operand = ToMemOperand(to);
        __ vstr(ToDoubleRegister(from), to_operand.rn(), to_operand.offset());
      }
    } else if (to->IsDoubleRegister()) {
      ASSERT(from->IsDoubleStackSlot());
      // TODO(regis): Why is vldr not taking a MemOperand?
      // __ vldr(ToDoubleRegister(to), ToMemOperand(from));
      MemOperand from_operand = ToMemOperand(from);
      __ vldr(ToDoubleRegister(to), from_operand.rn(), from_operand.offset());
    } else {
      ASSERT(to->IsDoubleStackSlot() && from->IsDoubleStackSlot());
      // TODO(regis): Why is vldr not taking a MemOperand?
      // __ vldr(dbl_scratch, ToMemOperand(from));
      MemOperand from_operand = ToMemOperand(from);
      __ vldr(dbl_scratch, from_operand.rn(), from_operand.offset());
      // TODO(regis): Why is vstr not taking a MemOperand?
      // __ vstr(dbl_scratch, ToMemOperand(to));
      MemOperand to_operand = ToMemOperand(to);
      __ vstr(dbl_scratch, to_operand.rn(), to_operand.offset());
    }
  }

  if (destroys_core_scratch) {
    __ ldr(core_scratch, MemOperand(fp, -kPointerSize));
  }

  LInstruction* next = GetNextInstruction();
  if (next != NULL && next->IsLazyBailout()) {
    int pc = masm()->pc_offset();
    safepoints_.SetPcAfterGap(pc);
  }
}


void LCodeGen::DoGap(LGap* gap) {
  for (int i = LGap::FIRST_INNER_POSITION;
       i <= LGap::LAST_INNER_POSITION;
       i++) {
    LGap::InnerPosition inner_pos = static_cast<LGap::InnerPosition>(i);
    LParallelMove* move = gap->GetParallelMove(inner_pos);
    if (move != NULL) DoParallelMove(move);
  }

  LInstruction* next = GetNextInstruction();
  if (next != NULL && next->IsLazyBailout()) {
    int pc = masm()->pc_offset();
    safepoints_.SetPcAfterGap(pc);
  }
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
    case CodeStub::StringCharAt: {
      Abort("StringCharAtStub unimplemented.");
      break;
    }
    case CodeStub::MathPow: {
      Abort("MathPowStub unimplemented.");
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
      TranscendentalCacheStub stub(instr->transcendental_type());
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
  Abort("ModI not implemented");
  class DeferredModI: public LDeferredCode {
   public:
    DeferredModI(LCodeGen* codegen, LModI* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() {
      codegen()->DoDeferredGenericBinaryStub(instr_, Token::MOD);
    }
   private:
    LModI* instr_;
  };
  // These registers hold untagged 32 bit values.
  Register left = ToRegister(instr->left());
  Register right = ToRegister(instr->right());
  Register result = ToRegister(instr->result());
  Register scratch = scratch0();

  Label deoptimize, done;
  // Check for x % 0.
  if (instr->hydrogen()->CheckFlag(HValue::kCanBeDivByZero)) {
    __ tst(right, Operand(right));
    __ b(eq, &deoptimize);
  }

  // Check for (0 % -x) that will produce negative zero.
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    Label ok;
    __ tst(left, Operand(left));
    __ b(ne, &ok);
    __ tst(right, Operand(right));
    __ b(pl, &ok);
    __ b(al, &deoptimize);
    __ bind(&ok);
  }

  // Call the generic stub. The numbers in r0 and r1 have
  // to be tagged to Smis. If that is not possible, deoptimize.
  DeferredModI* deferred = new DeferredModI(this, instr);
  __ TrySmiTag(left, &deoptimize, scratch);
  __ TrySmiTag(right, &deoptimize, scratch);

  __ b(al, deferred->entry());
  __ bind(deferred->exit());

  // If the result in r0 is a Smi, untag it, else deoptimize.
  __ BranchOnNotSmi(result, &deoptimize);
  __ mov(result, Operand(result, ASR, 1));

  __ b(al, &done);
  __ bind(&deoptimize);
  DeoptimizeIf(al, instr->environment());
  __ bind(&done);
}


void LCodeGen::DoDivI(LDivI* instr) {
  Abort("DivI not implemented");
  class DeferredDivI: public LDeferredCode {
   public:
    DeferredDivI(LCodeGen* codegen, LDivI* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() {
      codegen()->DoDeferredGenericBinaryStub(instr_, Token::DIV);
    }
   private:
    LDivI* instr_;
  };

  const Register left = ToRegister(instr->left());
  const Register right = ToRegister(instr->right());
  const Register scratch = scratch0();
  const Register result = ToRegister(instr->result());

  // Check for x / 0.
  if (instr->hydrogen()->CheckFlag(HValue::kCanBeDivByZero)) {
    __ tst(right, right);
    DeoptimizeIf(eq, instr->environment());
  }

  // Check for (0 / -x) that will produce negative zero.
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    Label left_not_zero;
    __ tst(left, Operand(left));
    __ b(ne, &left_not_zero);
    __ tst(right, Operand(right));
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

  // Call the generic stub. The numbers in r0 and r1 have
  // to be tagged to Smis. If that is not possible, deoptimize.
  DeferredDivI* deferred = new DeferredDivI(this, instr);

  __ TrySmiTag(left, &deoptimize, scratch);
  __ TrySmiTag(right, &deoptimize, scratch);

  __ b(al, deferred->entry());
  __ bind(deferred->exit());

  // If the result in r0 is a Smi, untag it, else deoptimize.
  __ BranchOnNotSmi(result, &deoptimize);
  __ SmiUntag(result);
  __ b(&done);

  __ bind(&deoptimize);
  DeoptimizeIf(al, instr->environment());
  __ bind(&done);
}


void LCodeGen::DoDeferredGenericBinaryStub(LBinaryOperation* instr,
                                           Token::Value op) {
  Register left = ToRegister(instr->left());
  Register right = ToRegister(instr->right());

  __ PushSafepointRegistersAndDoubles();
  GenericBinaryOpStub stub(op, OVERWRITE_LEFT, left, right);
  __ CallStub(&stub);
  RecordSafepointWithRegistersAndDoubles(instr->pointer_map(),
                                         0,
                                         Safepoint::kNoDeoptimizationIndex);
  // Overwrite the stored value of r0 with the result of the stub.
  __ str(r0, MemOperand(sp, DwVfpRegister::kNumAllocatableRegisters *
                        kDoubleSize));
  __ PopSafepointRegistersAndDoubles();
}


void LCodeGen::DoMulI(LMulI* instr) {
  Register scratch = scratch0();
  Register left = ToRegister(instr->left());
  Register right = EmitLoadRegister(instr->right(), scratch);

  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero) &&
      !instr->right()->IsConstantOperand()) {
    __ orr(ToRegister(instr->temp()), left, right);
  }

  if (instr->hydrogen()->CheckFlag(HValue::kCanOverflow)) {
    // scratch:left = left * right.
    __ smull(scratch, left, left, right);
    __ mov(ip, Operand(left, ASR, 31));
    __ cmp(ip, Operand(scratch));
    DeoptimizeIf(ne, instr->environment());
  } else {
    __ mul(left, left, right);
  }

  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    // Bail out if the result is supposed to be negative zero.
    Label done;
    __ tst(left, Operand(left));
    __ b(ne, &done);
    if (instr->right()->IsConstantOperand()) {
      if (ToInteger32(LConstantOperand::cast(instr->right())) < 0) {
        DeoptimizeIf(no_condition, instr->environment());
      }
    } else {
      // Test the non-zero operand for negative sign.
      __ cmp(ToRegister(instr->temp()), Operand(0));
      DeoptimizeIf(mi, instr->environment());
    }
    __ bind(&done);
  }
}


void LCodeGen::DoBitI(LBitI* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  ASSERT(left->Equals(instr->result()));
  ASSERT(left->IsRegister());
  Register result = ToRegister(left);
  Register right_reg = EmitLoadRegister(right, ip);
  switch (instr->op()) {
    case Token::BIT_AND:
      __ and_(result, ToRegister(left), Operand(right_reg));
      break;
    case Token::BIT_OR:
      __ orr(result, ToRegister(left), Operand(right_reg));
      break;
    case Token::BIT_XOR:
      __ eor(result, ToRegister(left), Operand(right_reg));
      break;
    default:
      UNREACHABLE();
      break;
  }
}


void LCodeGen::DoShiftI(LShiftI* instr) {
  Register scratch = scratch0();
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  ASSERT(left->Equals(instr->result()));
  ASSERT(left->IsRegister());
  Register result = ToRegister(left);
  if (right->IsRegister()) {
    // Mask the right operand.
    __ and_(scratch, ToRegister(right), Operand(0x1F));
    switch (instr->op()) {
      case Token::SAR:
        __ mov(result, Operand(result, ASR, scratch));
        break;
      case Token::SHR:
        if (instr->can_deopt()) {
          __ mov(result, Operand(result, LSR, scratch), SetCC);
          DeoptimizeIf(mi, instr->environment());
        } else {
          __ mov(result, Operand(result, LSR, scratch));
        }
        break;
      case Token::SHL:
        __ mov(result, Operand(result, LSL, scratch));
        break;
      default:
        UNREACHABLE();
        break;
    }
  } else {
    int value = ToInteger32(LConstantOperand::cast(right));
    uint8_t shift_count = static_cast<uint8_t>(value & 0x1F);
    switch (instr->op()) {
      case Token::SAR:
        if (shift_count != 0) {
          __ mov(result, Operand(result, ASR, shift_count));
        }
        break;
      case Token::SHR:
        if (shift_count == 0 && instr->can_deopt()) {
          __ tst(result, Operand(0x80000000));
          DeoptimizeIf(ne, instr->environment());
        } else {
          __ mov(result, Operand(result, LSR, shift_count));
        }
        break;
      case Token::SHL:
        if (shift_count != 0) {
          __ mov(result, Operand(result, LSL, shift_count));
        }
        break;
      default:
        UNREACHABLE();
        break;
    }
  }
}


void LCodeGen::DoSubI(LSubI* instr) {
  Register left = ToRegister(instr->left());
  Register right = EmitLoadRegister(instr->right(), ip);
  ASSERT(instr->left()->Equals(instr->result()));
  __ sub(left, left, right, SetCC);
  if (instr->hydrogen()->CheckFlag(HValue::kCanOverflow)) {
    DeoptimizeIf(vs, instr->environment());
  }
}


void LCodeGen::DoConstantI(LConstantI* instr) {
  ASSERT(instr->result()->IsRegister());
  __ mov(ToRegister(instr->result()), Operand(instr->value()));
}


void LCodeGen::DoConstantD(LConstantD* instr) {
  Abort("DoConstantD unimplemented.");
}


void LCodeGen::DoConstantT(LConstantT* instr) {
  ASSERT(instr->result()->IsRegister());
  __ mov(ToRegister(instr->result()), Operand(instr->value()));
}


void LCodeGen::DoJSArrayLength(LJSArrayLength* instr) {
  Register result = ToRegister(instr->result());
  Register array = ToRegister(instr->input());
  __ ldr(result, FieldMemOperand(array, JSArray::kLengthOffset));
}


void LCodeGen::DoFixedArrayLength(LFixedArrayLength* instr) {
  Register result = ToRegister(instr->result());
  Register array = ToRegister(instr->input());
  __ ldr(result, FieldMemOperand(array, FixedArray::kLengthOffset));
}


void LCodeGen::DoValueOf(LValueOf* instr) {
  Register input = ToRegister(instr->input());
  Register result = ToRegister(instr->result());
  Register map = ToRegister(instr->temporary());
  ASSERT(input.is(result));
  Label done;

  // If the object is a smi return the object.
  __ tst(input, Operand(kSmiTagMask));
  __ b(eq, &done);

  // If the object is not a value type, return the object.
  __ CompareObjectType(input, map, map, JS_VALUE_TYPE);
  __ b(ne, &done);
  __ ldr(result, FieldMemOperand(input, JSValue::kValueOffset));

  __ bind(&done);
}


void LCodeGen::DoBitNotI(LBitNotI* instr) {
  LOperand* input = instr->input();
  ASSERT(input->Equals(instr->result()));
  __ mvn(ToRegister(input), Operand(ToRegister(input)));
}


void LCodeGen::DoThrow(LThrow* instr) {
  Register input_reg = EmitLoadRegister(instr->input(), ip);
  __ push(input_reg);
  CallRuntime(Runtime::kThrow, 1, instr);

  if (FLAG_debug_code) {
    __ stop("Unreachable code.");
  }
}


void LCodeGen::DoAddI(LAddI* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  ASSERT(left->Equals(instr->result()));

  Register right_reg = EmitLoadRegister(right, ip);
  __ add(ToRegister(left), ToRegister(left), Operand(right_reg), SetCC);

  if (instr->hydrogen()->CheckFlag(HValue::kCanOverflow)) {
    DeoptimizeIf(vs, instr->environment());
  }
}


void LCodeGen::DoArithmeticD(LArithmeticD* instr) {
  DoubleRegister left = ToDoubleRegister(instr->left());
  DoubleRegister right = ToDoubleRegister(instr->right());
  switch (instr->op()) {
    case Token::ADD:
      __ vadd(left, left, right);
      break;
    case Token::SUB:
      __ vsub(left, left, right);
      break;
    case Token::MUL:
      __ vmul(left, left, right);
      break;
    case Token::DIV:
      __ vdiv(left, left, right);
      break;
    case Token::MOD: {
      Abort("DoArithmeticD unimplemented for MOD.");
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
}


void LCodeGen::DoArithmeticT(LArithmeticT* instr) {
  ASSERT(ToRegister(instr->left()).is(r1));
  ASSERT(ToRegister(instr->right()).is(r0));
  ASSERT(ToRegister(instr->result()).is(r0));

  // TODO(regis): Implement TypeRecordingBinaryOpStub and replace current
  // GenericBinaryOpStub:
  // TypeRecordingBinaryOpStub stub(instr->op(), NO_OVERWRITE);
  GenericBinaryOpStub stub(instr->op(), NO_OVERWRITE, r1, r0);
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
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

  Representation r = instr->hydrogen()->representation();
  if (r.IsInteger32()) {
    Register reg = ToRegister(instr->input());
    __ cmp(reg, Operand(0));
    EmitBranch(true_block, false_block, nz);
  } else if (r.IsDouble()) {
    DoubleRegister reg = ToDoubleRegister(instr->input());
    Register scratch = scratch0();

    // Test the double value. Zero and NaN are false.
    __ VFPCompareAndLoadFlags(reg, 0.0, scratch);
    __ tst(scratch, Operand(kVFPZConditionFlagBit | kVFPVConditionFlagBit));
    EmitBranch(true_block, false_block, ne);
  } else {
    ASSERT(r.IsTagged());
    Register reg = ToRegister(instr->input());
    if (instr->hydrogen()->type().IsBoolean()) {
      __ LoadRoot(ip, Heap::kTrueValueRootIndex);
      __ cmp(reg, ip);
      EmitBranch(true_block, false_block, eq);
    } else {
      Label* true_label = chunk_->GetAssemblyLabel(true_block);
      Label* false_label = chunk_->GetAssemblyLabel(false_block);

      __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
      __ cmp(reg, ip);
      __ b(eq, false_label);
      __ LoadRoot(ip, Heap::kTrueValueRootIndex);
      __ cmp(reg, ip);
      __ b(eq, true_label);
      __ LoadRoot(ip, Heap::kFalseValueRootIndex);
      __ cmp(reg, ip);
      __ b(eq, false_label);
      __ cmp(reg, Operand(0));
      __ b(eq, false_label);
      __ tst(reg, Operand(kSmiTagMask));
      __ b(eq, true_label);

      // Test double values. Zero and NaN are false.
      Label call_stub;
      DoubleRegister dbl_scratch = d0;
      Register scratch = scratch0();
      __ ldr(scratch, FieldMemOperand(reg, HeapObject::kMapOffset));
      __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
      __ cmp(scratch, Operand(ip));
      __ b(ne, &call_stub);
      __ sub(ip, reg, Operand(kHeapObjectTag));
      __ vldr(dbl_scratch, ip, HeapNumber::kValueOffset);
      __ VFPCompareAndLoadFlags(dbl_scratch, 0.0, scratch);
      __ tst(scratch, Operand(kVFPZConditionFlagBit | kVFPVConditionFlagBit));
      __ b(ne, false_label);
      __ b(true_label);

      // The conversion stub doesn't cause garbage collections so it's
      // safe to not record a safepoint after the call.
      __ bind(&call_stub);
      ToBooleanStub stub(reg);
      RegList saved_regs = kJSCallerSaved | kCalleeSaved;
      __ stm(db_w, sp, saved_regs);
      __ CallStub(&stub);
      __ cmp(reg, Operand(0));
      __ ldm(ia_w, sp, saved_regs);
      EmitBranch(true_block, false_block, nz);
    }
  }
}


void LCodeGen::EmitGoto(int block, LDeferredCode* deferred_stack_check) {
  block = chunk_->LookupDestination(block);
  int next_block = GetNextEmittedBlock(current_block_);
  if (block != next_block) {
    // Perform stack overflow check if this goto needs it before jumping.
    if (deferred_stack_check != NULL) {
      __ LoadRoot(ip, Heap::kStackLimitRootIndex);
      __ cmp(sp, Operand(ip));
      __ b(hs, chunk_->GetAssemblyLabel(block));
      __ jmp(deferred_stack_check->entry());
      deferred_stack_check->SetExit(chunk_->GetAssemblyLabel(block));
    } else {
      __ jmp(chunk_->GetAssemblyLabel(block));
    }
  }
}


void LCodeGen::DoDeferredStackCheck(LGoto* instr) {
  __ PushSafepointRegisters();
  __ CallRuntimeSaveDoubles(Runtime::kStackGuard);
  RecordSafepointWithRegisters(
      instr->pointer_map(), 0, Safepoint::kNoDeoptimizationIndex);
  __ PopSafepointRegisters();
}


void LCodeGen::DoGoto(LGoto* instr) {
  class DeferredStackCheck: public LDeferredCode {
   public:
    DeferredStackCheck(LCodeGen* codegen, LGoto* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() { codegen()->DoDeferredStackCheck(instr_); }
   private:
    LGoto* instr_;
  };

  DeferredStackCheck* deferred = NULL;
  if (instr->include_stack_check()) {
    deferred = new DeferredStackCheck(this, instr);
  }
  EmitGoto(instr->block_id(), deferred);
}


Condition LCodeGen::TokenToCondition(Token::Value op, bool is_unsigned) {
  Condition cond = no_condition;
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
  __ cmp(ToRegister(left), ToOperand(right));
  Abort("EmitCmpI untested.");
}


void LCodeGen::DoCmpID(LCmpID* instr) {
  Abort("DoCmpID unimplemented.");
}


void LCodeGen::DoCmpIDAndBranch(LCmpIDAndBranch* instr) {
  Abort("DoCmpIDAndBranch unimplemented.");
}


void LCodeGen::DoCmpJSObjectEq(LCmpJSObjectEq* instr) {
  Register left = ToRegister(instr->left());
  Register right = ToRegister(instr->right());
  Register result = ToRegister(instr->result());

  __ cmp(left, Operand(right));
  __ LoadRoot(result, Heap::kTrueValueRootIndex, eq);
  __ LoadRoot(result, Heap::kFalseValueRootIndex, ne);
  Abort("DoCmpJSObjectEq untested.");
}


void LCodeGen::DoCmpJSObjectEqAndBranch(LCmpJSObjectEqAndBranch* instr) {
  Abort("DoCmpJSObjectEqAndBranch unimplemented.");
}


void LCodeGen::DoIsNull(LIsNull* instr) {
  Register reg = ToRegister(instr->input());
  Register result = ToRegister(instr->result());

  __ LoadRoot(ip, Heap::kNullValueRootIndex);
  __ cmp(reg, ip);
  if (instr->is_strict()) {
    __ LoadRoot(result, Heap::kTrueValueRootIndex, eq);
    __ LoadRoot(result, Heap::kFalseValueRootIndex, ne);
  } else {
    Label true_value, false_value, done;
    __ b(eq, &true_value);
    __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
    __ cmp(ip, reg);
    __ b(eq, &true_value);
    __ tst(reg, Operand(kSmiTagMask));
    __ b(eq, &false_value);
    // Check for undetectable objects by looking in the bit field in
    // the map. The object has already been smi checked.
    Register scratch = result;
    __ ldr(scratch, FieldMemOperand(reg, HeapObject::kMapOffset));
    __ ldrb(scratch, FieldMemOperand(scratch, Map::kBitFieldOffset));
    __ tst(scratch, Operand(1 << Map::kIsUndetectable));
    __ b(ne, &true_value);
    __ bind(&false_value);
    __ LoadRoot(result, Heap::kFalseValueRootIndex);
    __ jmp(&done);
    __ bind(&true_value);
    __ LoadRoot(result, Heap::kTrueValueRootIndex);
    __ bind(&done);
  }
}


void LCodeGen::DoIsNullAndBranch(LIsNullAndBranch* instr) {
  Register scratch = scratch0();
  Register reg = ToRegister(instr->input());

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
    __ tst(reg, Operand(kSmiTagMask));
    __ b(eq, false_label);
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
                                 Register temp2,
                                 Label* is_not_object,
                                 Label* is_object) {
  Abort("EmitIsObject unimplemented.");
  return ne;
}


void LCodeGen::DoIsObject(LIsObject* instr) {
  Abort("DoIsObject unimplemented.");
}


void LCodeGen::DoIsObjectAndBranch(LIsObjectAndBranch* instr) {
  Abort("DoIsObjectAndBranch unimplemented.");
}


void LCodeGen::DoIsSmi(LIsSmi* instr) {
  ASSERT(instr->hydrogen()->value()->representation().IsTagged());
  Register result = ToRegister(instr->result());
  Register input_reg = EmitLoadRegister(instr->input(), ip);
  __ tst(input_reg, Operand(kSmiTagMask));
  __ LoadRoot(result, Heap::kTrueValueRootIndex);
  Label done;
  __ b(eq, &done);
  __ LoadRoot(result, Heap::kFalseValueRootIndex);
  __ bind(&done);
}


void LCodeGen::DoIsSmiAndBranch(LIsSmiAndBranch* instr) {
  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  Register input_reg = EmitLoadRegister(instr->input(), ip);
  __ tst(input_reg, Operand(kSmiTagMask));
  EmitBranch(true_block, false_block, eq);
}


InstanceType LHasInstanceType::TestType() {
  InstanceType from = hydrogen()->from();
  InstanceType to = hydrogen()->to();
  if (from == FIRST_TYPE) return to;
  ASSERT(from == to || to == LAST_TYPE);
  return from;
}


Condition LHasInstanceType::BranchCondition() {
  InstanceType from = hydrogen()->from();
  InstanceType to = hydrogen()->to();
  if (from == to) return eq;
  if (to == LAST_TYPE) return hs;
  if (from == FIRST_TYPE) return ls;
  UNREACHABLE();
  return eq;
}


void LCodeGen::DoHasInstanceType(LHasInstanceType* instr) {
  Abort("DoHasInstanceType unimplemented.");
}


void LCodeGen::DoHasInstanceTypeAndBranch(LHasInstanceTypeAndBranch* instr) {
  Register scratch = scratch0();
  Register input = ToRegister(instr->input());

  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  Label* false_label = chunk_->GetAssemblyLabel(false_block);

  __ tst(input, Operand(kSmiTagMask));
  __ b(eq, false_label);

  __ CompareObjectType(input, scratch, scratch, instr->TestType());
  EmitBranch(true_block, false_block, instr->BranchCondition());
}


void LCodeGen::DoHasCachedArrayIndex(LHasCachedArrayIndex* instr) {
  Abort("DoHasCachedArrayIndex unimplemented.");
}


void LCodeGen::DoHasCachedArrayIndexAndBranch(
    LHasCachedArrayIndexAndBranch* instr) {
  Abort("DoHasCachedArrayIndexAndBranch unimplemented.");
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
  __ tst(input, Operand(kSmiTagMask));
  __ b(eq, is_false);
  __ CompareObjectType(input, temp, temp2, FIRST_JS_OBJECT_TYPE);
  __ b(lt, is_false);

  // Map is now in temp.
  // Functions have class 'Function'.
  __ CompareInstanceType(temp, temp2, JS_FUNCTION_TYPE);
  if (class_name->IsEqualTo(CStrVector("Function"))) {
    __ b(eq, is_true);
  } else {
    __ b(eq, is_false);
  }

  // Check if the constructor in the map is a function.
  __ ldr(temp, FieldMemOperand(temp, Map::kConstructorOffset));

  // As long as JS_FUNCTION_TYPE is the last instance type and it is
  // right after LAST_JS_OBJECT_TYPE, we can avoid checking for
  // LAST_JS_OBJECT_TYPE.
  ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
  ASSERT(JS_FUNCTION_TYPE == LAST_JS_OBJECT_TYPE + 1);

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


void LCodeGen::DoClassOfTest(LClassOfTest* instr) {
  Register input = ToRegister(instr->input());
  Register result = ToRegister(instr->result());
  ASSERT(input.is(result));
  Handle<String> class_name = instr->hydrogen()->class_name();

  Label done, is_true, is_false;

  EmitClassOfTest(&is_true, &is_false, class_name, input, scratch0(), input);
  __ b(ne, &is_false);

  __ bind(&is_true);
  __ LoadRoot(result, Heap::kTrueValueRootIndex);
  __ jmp(&done);

  __ bind(&is_false);
  __ LoadRoot(result, Heap::kFalseValueRootIndex);
  __ bind(&done);
}


void LCodeGen::DoClassOfTestAndBranch(LClassOfTestAndBranch* instr) {
  Register input = ToRegister(instr->input());
  Register temp = scratch0();
  Register temp2 = ToRegister(instr->temporary());
  Handle<String> class_name = instr->hydrogen()->class_name();

  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  Label* true_label = chunk_->GetAssemblyLabel(true_block);
  Label* false_label = chunk_->GetAssemblyLabel(false_block);

  EmitClassOfTest(true_label, false_label, class_name, input, temp, temp2);

  EmitBranch(true_block, false_block, eq);
}


void LCodeGen::DoCmpMapAndBranch(LCmpMapAndBranch* instr) {
  Register reg = ToRegister(instr->input());
  Register temp = ToRegister(instr->temp());
  int true_block = instr->true_block_id();
  int false_block = instr->false_block_id();

  __ ldr(temp, FieldMemOperand(reg, HeapObject::kMapOffset));
  __ cmp(temp, Operand(instr->map()));
  EmitBranch(true_block, false_block, eq);
}


void LCodeGen::DoInstanceOf(LInstanceOf* instr) {
  ASSERT(ToRegister(instr->left()).is(r0));  // Object is in r0.
  ASSERT(ToRegister(instr->right()).is(r1));  // Function is in r1.

  InstanceofStub stub(InstanceofStub::kArgsInRegisters);
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);

  Label true_value, done;
  __ tst(r0, r0);
  __ mov(r0, Operand(Factory::false_value()), LeaveCC, ne);
  __ mov(r0, Operand(Factory::true_value()), LeaveCC, eq);
}


void LCodeGen::DoInstanceOfAndBranch(LInstanceOfAndBranch* instr) {
  Abort("DoInstanceOfAndBranch unimplemented.");
}


void LCodeGen::DoInstanceOfKnownGlobal(LInstanceOfKnownGlobal* instr) {
  Abort("DoInstanceOfKnownGlobal unimplemented.");
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
      return no_condition;
  }
}


void LCodeGen::DoCmpT(LCmpT* instr) {
  Token::Value op = instr->op();

  Handle<Code> ic = CompareIC::GetUninitialized(op);
  CallCode(ic, RelocInfo::CODE_TARGET, instr);

  Condition condition = ComputeCompareCondition(op);
  if (op == Token::GT || op == Token::LTE) {
    condition = ReverseCondition(condition);
  }
  __ cmp(r0, Operand(0));
  __ LoadRoot(ToRegister(instr->result()),
              Heap::kTrueValueRootIndex,
              condition);
  __ LoadRoot(ToRegister(instr->result()),
              Heap::kFalseValueRootIndex,
              NegateCondition(condition));
}


void LCodeGen::DoCmpTAndBranch(LCmpTAndBranch* instr) {
  Abort("DoCmpTAndBranch unimplemented.");
}


void LCodeGen::DoReturn(LReturn* instr) {
  if (FLAG_trace) {
    // Push the return value on the stack as the parameter.
    // Runtime::TraceExit returns its parameter in r0.
    __ push(r0);
    __ CallRuntime(Runtime::kTraceExit, 1);
  }
  int32_t sp_delta = (ParameterCount() + 1) * kPointerSize;
  __ mov(sp, fp);
  __ ldm(ia_w, sp, fp.bit() | lr.bit());
  __ add(sp, sp, Operand(sp_delta));
  __ Jump(lr);
}


void LCodeGen::DoLoadGlobal(LLoadGlobal* instr) {
  Register result = ToRegister(instr->result());
  __ mov(ip, Operand(Handle<Object>(instr->hydrogen()->cell())));
  __ ldr(result, FieldMemOperand(ip, JSGlobalPropertyCell::kValueOffset));
  if (instr->hydrogen()->check_hole_value()) {
    __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
    __ cmp(result, ip);
    DeoptimizeIf(eq, instr->environment());
  }
}


void LCodeGen::DoStoreGlobal(LStoreGlobal* instr) {
  Register value = ToRegister(instr->input());
  __ mov(ip, Operand(Handle<Object>(instr->hydrogen()->cell())));
  __ str(value, FieldMemOperand(ip, JSGlobalPropertyCell::kValueOffset));
}


void LCodeGen::DoLoadContextSlot(LLoadContextSlot* instr) {
  // TODO(antonm): load a context with a separate instruction.
  Register result = ToRegister(instr->result());
  __ LoadContext(result, instr->context_chain_length());
  __ ldr(result, ContextOperand(result, instr->slot_index()));
}


void LCodeGen::DoLoadNamedField(LLoadNamedField* instr) {
  Register object = ToRegister(instr->input());
  Register result = ToRegister(instr->result());
  if (instr->hydrogen()->is_in_object()) {
    __ ldr(result, FieldMemOperand(object, instr->hydrogen()->offset()));
  } else {
    __ ldr(result, FieldMemOperand(object, JSObject::kPropertiesOffset));
    __ ldr(result, FieldMemOperand(result, instr->hydrogen()->offset()));
  }
}


void LCodeGen::DoLoadNamedGeneric(LLoadNamedGeneric* instr) {
  ASSERT(ToRegister(instr->object()).is(r0));
  ASSERT(ToRegister(instr->result()).is(r0));

  // Name is always in r2.
  __ mov(r2, Operand(instr->name()));
  Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
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
  ASSERT(instr->result()->Equals(instr->input()));
  Register reg = ToRegister(instr->input());
  Register scratch = scratch0();

  __ ldr(reg, FieldMemOperand(reg, JSObject::kElementsOffset));
  if (FLAG_debug_code) {
    Label done;
    __ ldr(scratch, FieldMemOperand(reg, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kFixedArrayMapRootIndex);
    __ cmp(scratch, ip);
    __ b(eq, &done);
    __ LoadRoot(ip, Heap::kFixedCOWArrayMapRootIndex);
    __ cmp(scratch, ip);
    __ Check(eq, "Check for fast elements failed.");
    __ bind(&done);
  }
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
  ASSERT(result.is(elements));

  // Load the result.
  __ add(scratch, elements, Operand(key, LSL, kPointerSizeLog2));
  __ ldr(result, FieldMemOperand(scratch, FixedArray::kHeaderSize));

  // Check for the hole value.
  __ LoadRoot(scratch, Heap::kTheHoleValueRootIndex);
  __ cmp(result, scratch);
  DeoptimizeIf(eq, instr->environment());
}


void LCodeGen::DoLoadKeyedGeneric(LLoadKeyedGeneric* instr) {
  ASSERT(ToRegister(instr->object()).is(r1));
  ASSERT(ToRegister(instr->key()).is(r0));

  Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
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
  Register elem = ToRegister(instr->input());
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
  Register scratch = scratch0();

  ASSERT(receiver.is(r0));
  ASSERT(function.is(r1));
  ASSERT(ToRegister(instr->result()).is(r0));

  // If the receiver is null or undefined, we have to pass the
  // global object as a receiver.
  Label global_receiver, receiver_ok;
  __ LoadRoot(scratch, Heap::kNullValueRootIndex);
  __ cmp(receiver, scratch);
  __ b(eq, &global_receiver);
  __ LoadRoot(scratch, Heap::kUndefinedValueRootIndex);
  __ cmp(receiver, scratch);
  __ b(ne, &receiver_ok);
  __ bind(&global_receiver);
  __ ldr(receiver, GlobalObjectOperand());
  __ bind(&receiver_ok);

  Register length = ToRegister(instr->length());
  Register elements = ToRegister(instr->elements());

  Label invoke;

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
  Label loop;
  // length is a small non-negative integer, due to the test above.
  __ tst(length, Operand(length));
  __ b(eq, &invoke);
  __ bind(&loop);
  __ ldr(scratch, MemOperand(elements, length, LSL, 2));
  __ push(scratch);
  __ sub(length, length, Operand(1), SetCC);
  __ b(ne, &loop);

  __ bind(&invoke);
  // Invoke the function. The number of arguments is stored in receiver
  // which is r0, as expected by InvokeFunction.
  v8::internal::ParameterCount actual(receiver);
  SafepointGenerator safepoint_generator(this,
                                         instr->pointer_map(),
                                         Safepoint::kNoDeoptimizationIndex);
  __ InvokeFunction(function, actual, CALL_FUNCTION, &safepoint_generator);
}


void LCodeGen::DoPushArgument(LPushArgument* instr) {
  LOperand* argument = instr->input();
  if (argument->IsDoubleRegister() || argument->IsDoubleStackSlot()) {
    Abort("DoPushArgument not implemented for double type.");
  } else {
    Register argument_reg = EmitLoadRegister(argument, ip);
    __ push(argument_reg);
  }
}


void LCodeGen::DoGlobalObject(LGlobalObject* instr) {
  Register result = ToRegister(instr->result());
  __ ldr(result, ContextOperand(cp, Context::GLOBAL_INDEX));
}


void LCodeGen::DoGlobalReceiver(LGlobalReceiver* instr) {
  Register result = ToRegister(instr->result());
  __ ldr(result, ContextOperand(cp, Context::GLOBAL_INDEX));
  __ ldr(result, FieldMemOperand(result, GlobalObject::kGlobalReceiverOffset));
}


void LCodeGen::CallKnownFunction(Handle<JSFunction> function,
                                 int arity,
                                 LInstruction* instr) {
  // Change context if needed.
  bool change_context =
      (graph()->info()->closure()->context() != function->context()) ||
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
  __ ldr(ip, FieldMemOperand(r1, JSFunction::kCodeEntryOffset));
  __ Call(ip);

  // Setup deoptimization.
  RegisterLazyDeoptimization(instr);

  // Restore context.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallConstantFunction(LCallConstantFunction* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));
  __ mov(r1, Operand(instr->function()));
  CallKnownFunction(instr->function(), instr->arity(), instr);
}


void LCodeGen::DoDeferredMathAbsTaggedHeapNumber(LUnaryMathOperation* instr) {
  Abort("DoDeferredMathAbsTaggedHeapNumber unimplemented.");
}


void LCodeGen::DoMathAbs(LUnaryMathOperation* instr) {
  Abort("DoMathAbs unimplemented.");
}


void LCodeGen::DoMathFloor(LUnaryMathOperation* instr) {
  DoubleRegister input = ToDoubleRegister(instr->input());
  Register result = ToRegister(instr->result());
  Register prev_fpscr = ToRegister(instr->temp());
  SwVfpRegister single_scratch = double_scratch0().low();
  Register scratch = scratch0();

  // Set custom FPCSR:
  //  - Set rounding mode to "Round towards Minus Infinity".
  //  - Clear vfp cumulative exception flags.
  //  - Make sure Flush-to-zero mode control bit is unset.
  __ vmrs(prev_fpscr);
  __ bic(scratch, prev_fpscr,
      Operand(kVFPExceptionMask | kVFPRoundingModeMask | kVFPFlushToZeroMask));
  __ orr(scratch, scratch, Operand(kVFPRoundToMinusInfinityBits));
  __ vmsr(scratch);

  // Convert the argument to an integer.
  __ vcvt_s32_f64(single_scratch,
                  input,
                  Assembler::FPSCRRounding,
                  al);

  // Retrieve FPSCR and check for vfp exceptions.
  __ vmrs(scratch);
  // Restore FPSCR
  __ vmsr(prev_fpscr);
  __ tst(scratch, Operand(kVFPExceptionMask));
  DeoptimizeIf(ne, instr->environment());

  // Move the result back to general purpose register r0.
  __ vmov(result, single_scratch);
}


void LCodeGen::DoMathSqrt(LUnaryMathOperation* instr) {
  DoubleRegister input = ToDoubleRegister(instr->input());
  ASSERT(ToDoubleRegister(instr->result()).is(input));
  __ vsqrt(input, input);
}


void LCodeGen::DoUnaryMathOperation(LUnaryMathOperation* instr) {
  switch (instr->op()) {
    case kMathAbs:
      DoMathAbs(instr);
      break;
    case kMathFloor:
      DoMathFloor(instr);
      break;
    case kMathSqrt:
      DoMathSqrt(instr);
      break;
    default:
      Abort("Unimplemented type of LUnaryMathOperation.");
      UNREACHABLE();
  }
}


void LCodeGen::DoCallKeyed(LCallKeyed* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));

  int arity = instr->arity();
  Handle<Code> ic = StubCache::ComputeKeyedCallInitialize(arity, NOT_IN_LOOP);
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallNamed(LCallNamed* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));

  int arity = instr->arity();
  Handle<Code> ic = StubCache::ComputeCallInitialize(arity, NOT_IN_LOOP);
  __ mov(r2, Operand(instr->name()));
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
  // Restore context register.
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallFunction(LCallFunction* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));

  int arity = instr->arity();
  CallFunctionStub stub(arity, NOT_IN_LOOP, RECEIVER_MIGHT_BE_VALUE);
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
  __ Drop(1);
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallGlobal(LCallGlobal* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));

  int arity = instr->arity();
  Handle<Code> ic = StubCache::ComputeCallInitialize(arity, NOT_IN_LOOP);
  __ mov(r2, Operand(instr->name()));
  CallCode(ic, RelocInfo::CODE_TARGET_CONTEXT, instr);
  __ ldr(cp, MemOperand(fp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallKnownGlobal(LCallKnownGlobal* instr) {
  ASSERT(ToRegister(instr->result()).is(r0));
  __ mov(r1, Operand(instr->target()));
  CallKnownFunction(instr->target(), instr->arity(), instr);
}


void LCodeGen::DoCallNew(LCallNew* instr) {
  ASSERT(ToRegister(instr->input()).is(r1));
  ASSERT(ToRegister(instr->result()).is(r0));

  Handle<Code> builtin(Builtins::builtin(Builtins::JSConstructCall));
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
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
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


void LCodeGen::DoStoreKeyedGeneric(LStoreKeyedGeneric* instr) {
  ASSERT(ToRegister(instr->object()).is(r2));
  ASSERT(ToRegister(instr->key()).is(r1));
  ASSERT(ToRegister(instr->value()).is(r0));

  Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoInteger32ToDouble(LInteger32ToDouble* instr) {
  LOperand* input = instr->input();
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

  LOperand* input = instr->input();
  ASSERT(input->IsRegister() && input->Equals(instr->result()));
  Register reg = ToRegister(input);

  DeferredNumberTagI* deferred = new DeferredNumberTagI(this, instr);
  __ SmiTag(reg, SetCC);
  __ b(vs, deferred->entry());
  __ bind(deferred->exit());
}


void LCodeGen::DoDeferredNumberTagI(LNumberTagI* instr) {
  Label slow;
  Register reg = ToRegister(instr->input());
  DoubleRegister dbl_scratch = d0;
  SwVfpRegister flt_scratch = s0;

  // Preserve the value of all registers.
  __ PushSafepointRegisters();

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
  int reg_stack_index = __ SafepointRegisterStackIndex(reg.code());
  __ str(ip, MemOperand(sp, reg_stack_index * kPointerSize));

  __ CallRuntimeSaveDoubles(Runtime::kAllocateHeapNumber);
  RecordSafepointWithRegisters(
      instr->pointer_map(), 0, Safepoint::kNoDeoptimizationIndex);
  if (!reg.is(r0)) __ mov(reg, r0);

  // Done. Put the value in dbl_scratch into the value of the allocated heap
  // number.
  __ bind(&done);
  __ sub(ip, reg, Operand(kHeapObjectTag));
  __ vstr(dbl_scratch, ip, HeapNumber::kValueOffset);
  __ str(reg, MemOperand(sp, reg_stack_index * kPointerSize));
  __ PopSafepointRegisters();
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

  DoubleRegister input_reg = ToDoubleRegister(instr->input());
  Register scratch = scratch0();
  Register reg = ToRegister(instr->result());
  Register temp1 = ToRegister(instr->temp1());
  Register temp2 = ToRegister(instr->temp2());

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

  __ PushSafepointRegisters();
  __ CallRuntimeSaveDoubles(Runtime::kAllocateHeapNumber);
  RecordSafepointWithRegisters(
      instr->pointer_map(), 0, Safepoint::kNoDeoptimizationIndex);
  int reg_stack_index = __ SafepointRegisterStackIndex(reg.code());
  __ str(r0, MemOperand(sp, reg_stack_index * kPointerSize));
  __ PopSafepointRegisters();
}


void LCodeGen::DoSmiTag(LSmiTag* instr) {
  LOperand* input = instr->input();
  ASSERT(input->IsRegister() && input->Equals(instr->result()));
  ASSERT(!instr->hydrogen_value()->CheckFlag(HValue::kCanOverflow));
  __ SmiTag(ToRegister(input));
}


void LCodeGen::DoSmiUntag(LSmiUntag* instr) {
  LOperand* input = instr->input();
  ASSERT(input->IsRegister() && input->Equals(instr->result()));
  if (instr->needs_check()) {
    __ tst(ToRegister(input), Operand(kSmiTagMask));
    DeoptimizeIf(ne, instr->environment());
  }
  __ SmiUntag(ToRegister(input));
}


void LCodeGen::EmitNumberUntagD(Register input_reg,
                                DoubleRegister result_reg,
                                LEnvironment* env) {
  Register scratch = scratch0();
  SwVfpRegister flt_scratch = s0;
  ASSERT(!result_reg.is(d0));

  Label load_smi, heap_number, done;

  // Smi check.
  __ tst(input_reg, Operand(kSmiTagMask));
  __ b(eq, &load_smi);

  // Heap number map check.
  __ ldr(scratch, FieldMemOperand(input_reg, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
  __ cmp(scratch, Operand(ip));
  __ b(eq, &heap_number);

  __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
  __ cmp(input_reg, Operand(ip));
  DeoptimizeIf(ne, env);

  // Convert undefined to NaN.
  __ LoadRoot(ip, Heap::kNanValueRootIndex);
  __ sub(ip, ip, Operand(kHeapObjectTag));
  __ vldr(result_reg, ip, HeapNumber::kValueOffset);
  __ jmp(&done);

  // Heap number to double register conversion.
  __ bind(&heap_number);
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
  Label done;
  Register input_reg = ToRegister(instr->input());
  Register scratch = scratch0();
  DoubleRegister dbl_scratch = d0;
  SwVfpRegister flt_scratch = s0;
  DoubleRegister dbl_tmp = ToDoubleRegister(instr->temp());

  // Heap number map check.
  __ ldr(scratch, FieldMemOperand(input_reg, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
  __ cmp(scratch, Operand(ip));

  if (instr->truncating()) {
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
    __ sub(ip, input_reg, Operand(kHeapObjectTag));
    __ vldr(dbl_tmp, ip, HeapNumber::kValueOffset);
    __ vcmp(dbl_tmp, 0.0);  // Sets overflow bit in FPSCR flags if NaN.
    __ vcvt_s32_f64(flt_scratch, dbl_tmp);
    __ vmov(input_reg, flt_scratch);  // 32-bit result of conversion.
    __ vmrs(pc);  // Move vector status bits to normal status bits.
    // Overflow bit is set if dbl_tmp is Nan.
    __ cmn(input_reg, Operand(1), vc);  // 0x7fffffff + 1 -> overflow.
    __ cmp(input_reg, Operand(1), vc);  // 0x80000000 - 1 -> overflow.
    DeoptimizeIf(vs, instr->environment());  // Saturation may have occured.

  } else {
    // Deoptimize if we don't have a heap number.
    DeoptimizeIf(ne, instr->environment());

    __ sub(ip, input_reg, Operand(kHeapObjectTag));
    __ vldr(dbl_tmp, ip, HeapNumber::kValueOffset);
    __ vcvt_s32_f64(flt_scratch, dbl_tmp);
    __ vmov(input_reg, flt_scratch);  // 32-bit result of conversion.
    // Non-truncating conversion means that we cannot lose bits, so we convert
    // back to check; note that using non-overlapping s and d regs would be
    // slightly faster.
    __ vcvt_f64_s32(dbl_scratch, flt_scratch);
    __ VFPCompareAndSetFlags(dbl_scratch, dbl_tmp);
    DeoptimizeIf(ne, instr->environment());  // Not equal or unordered.
    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      __ tst(input_reg, Operand(input_reg));
      __ b(ne, &done);
      __ vmov(lr, ip, dbl_tmp);
      __ tst(ip, Operand(1 << 31));  // Test sign bit.
      DeoptimizeIf(ne, instr->environment());
    }
  }
  __ bind(&done);
}


void LCodeGen::DoTaggedToI(LTaggedToI* instr) {
  LOperand* input = instr->input();
  ASSERT(input->IsRegister());
  ASSERT(input->Equals(instr->result()));

  Register input_reg = ToRegister(input);

  DeferredTaggedToI* deferred = new DeferredTaggedToI(this, instr);

  // Smi check.
  __ tst(input_reg, Operand(kSmiTagMask));
  __ b(ne, deferred->entry());

  // Smi to int32 conversion
  __ SmiUntag(input_reg);  // Untag smi.

  __ bind(deferred->exit());
}


void LCodeGen::DoNumberUntagD(LNumberUntagD* instr) {
  LOperand* input = instr->input();
  ASSERT(input->IsRegister());
  LOperand* result = instr->result();
  ASSERT(result->IsDoubleRegister());

  Register input_reg = ToRegister(input);
  DoubleRegister result_reg = ToDoubleRegister(result);

  EmitNumberUntagD(input_reg, result_reg, instr->environment());
}


void LCodeGen::DoDoubleToI(LDoubleToI* instr) {
  Abort("DoDoubleToI unimplemented.");
}


void LCodeGen::DoCheckSmi(LCheckSmi* instr) {
  LOperand* input = instr->input();
  ASSERT(input->IsRegister());
  __ tst(ToRegister(input), Operand(kSmiTagMask));
  DeoptimizeIf(instr->condition(), instr->environment());
}


void LCodeGen::DoCheckInstanceType(LCheckInstanceType* instr) {
  Register input = ToRegister(instr->input());
  Register scratch = scratch0();
  InstanceType first = instr->hydrogen()->first();
  InstanceType last = instr->hydrogen()->last();

  __ ldr(scratch, FieldMemOperand(input, HeapObject::kMapOffset));
  __ ldrb(scratch, FieldMemOperand(scratch, Map::kInstanceTypeOffset));
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
}


void LCodeGen::DoCheckFunction(LCheckFunction* instr) {
  ASSERT(instr->input()->IsRegister());
  Register reg = ToRegister(instr->input());
  __ cmp(reg, Operand(instr->hydrogen()->target()));
  DeoptimizeIf(ne, instr->environment());
}


void LCodeGen::DoCheckMap(LCheckMap* instr) {
  Register scratch = scratch0();
  LOperand* input = instr->input();
  ASSERT(input->IsRegister());
  Register reg = ToRegister(input);
  __ ldr(scratch, FieldMemOperand(reg, HeapObject::kMapOffset));
  __ cmp(scratch, Operand(instr->hydrogen()->map()));
  DeoptimizeIf(ne, instr->environment());
}


void LCodeGen::LoadHeapObject(Register result,
                              Handle<HeapObject> object) {
  if (Heap::InNewSpace(*object)) {
    Handle<JSGlobalPropertyCell> cell =
        Factory::NewJSGlobalPropertyCell(object);
    __ mov(result, Operand(cell));
    __ ldr(result, FieldMemOperand(result, JSGlobalPropertyCell::kValueOffset));
  } else {
    __ mov(result, Operand(object));
  }
}


void LCodeGen::DoCheckPrototypeMaps(LCheckPrototypeMaps* instr) {
  Register temp1 = ToRegister(instr->temp1());
  Register temp2 = ToRegister(instr->temp2());

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
  bool pretenure = !instr->hydrogen()->pretenure();
  if (shared_info->num_literals() == 0 && !pretenure) {
    FastNewClosureStub stub;
    __ mov(r1, Operand(shared_info));
    __ push(r1);
    CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
  } else {
    __ mov(r2, Operand(shared_info));
    __ mov(r1, Operand(pretenure
                       ? Factory::true_value()
                       : Factory::false_value()));
    __ Push(cp, r2, r1);
    CallRuntime(Runtime::kNewClosure, 3, instr);
  }
}


void LCodeGen::DoTypeof(LTypeof* instr) {
  Register input = ToRegister(instr->input());
  __ push(input);
  CallRuntime(Runtime::kTypeof, 1, instr);
}


void LCodeGen::DoTypeofIs(LTypeofIs* instr) {
  Register input = ToRegister(instr->input());
  Register result = ToRegister(instr->result());
  Label true_label;
  Label false_label;
  Label done;

  Condition final_branch_condition = EmitTypeofIs(&true_label,
                                                  &false_label,
                                                  input,
                                                  instr->type_literal());
  __ b(final_branch_condition, &true_label);
  __ bind(&false_label);
  __ LoadRoot(result, Heap::kFalseValueRootIndex);
  __ b(&done);

  __ bind(&true_label);
  __ LoadRoot(result, Heap::kTrueValueRootIndex);

  __ bind(&done);
}


void LCodeGen::DoTypeofIsAndBranch(LTypeofIsAndBranch* instr) {
  Register input = ToRegister(instr->input());
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
  Condition final_branch_condition = no_condition;
  Register scratch = scratch0();
  if (type_name->Equals(Heap::number_symbol())) {
    __ tst(input, Operand(kSmiTagMask));
    __ b(eq, true_label);
    __ ldr(input, FieldMemOperand(input, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kHeapNumberMapRootIndex);
    __ cmp(input, Operand(ip));
    final_branch_condition = eq;

  } else if (type_name->Equals(Heap::string_symbol())) {
    __ tst(input, Operand(kSmiTagMask));
    __ b(eq, false_label);
    __ ldr(input, FieldMemOperand(input, HeapObject::kMapOffset));
    __ ldrb(ip, FieldMemOperand(input, Map::kBitFieldOffset));
    __ tst(ip, Operand(1 << Map::kIsUndetectable));
    __ b(ne, false_label);
    __ CompareInstanceType(input, scratch, FIRST_NONSTRING_TYPE);
    final_branch_condition = lo;

  } else if (type_name->Equals(Heap::boolean_symbol())) {
    __ LoadRoot(ip, Heap::kTrueValueRootIndex);
    __ cmp(input, ip);
    __ b(eq, true_label);
    __ LoadRoot(ip, Heap::kFalseValueRootIndex);
    __ cmp(input, ip);
    final_branch_condition = eq;

  } else if (type_name->Equals(Heap::undefined_symbol())) {
    __ LoadRoot(ip, Heap::kUndefinedValueRootIndex);
    __ cmp(input, ip);
    __ b(eq, true_label);
    __ tst(input, Operand(kSmiTagMask));
    __ b(eq, false_label);
    // Check for undetectable objects => true.
    __ ldr(input, FieldMemOperand(input, HeapObject::kMapOffset));
    __ ldrb(ip, FieldMemOperand(input, Map::kBitFieldOffset));
    __ tst(ip, Operand(1 << Map::kIsUndetectable));
    final_branch_condition = ne;

  } else if (type_name->Equals(Heap::function_symbol())) {
    __ tst(input, Operand(kSmiTagMask));
    __ b(eq, false_label);
    __ CompareObjectType(input, input, scratch, JS_FUNCTION_TYPE);
    __ b(eq, true_label);
    // Regular expressions => 'function' (they are callable).
    __ CompareInstanceType(input, scratch, JS_REGEXP_TYPE);
    final_branch_condition = eq;

  } else if (type_name->Equals(Heap::object_symbol())) {
    __ tst(input, Operand(kSmiTagMask));
    __ b(eq, false_label);
    __ LoadRoot(ip, Heap::kNullValueRootIndex);
    __ cmp(input, ip);
    __ b(eq, true_label);
    // Regular expressions => 'function', not 'object'.
    __ CompareObjectType(input, input, scratch, JS_REGEXP_TYPE);
    __ b(eq, false_label);
    // Check for undetectable objects => false.
    __ ldrb(ip, FieldMemOperand(input, Map::kBitFieldOffset));
    __ tst(ip, Operand(1 << Map::kIsUndetectable));
    __ b(ne, false_label);
    // Check for JS objects => true.
    __ CompareInstanceType(input, scratch, FIRST_JS_OBJECT_TYPE);
    __ b(lo, false_label);
    __ CompareInstanceType(input, scratch, LAST_JS_OBJECT_TYPE);
    final_branch_condition = ls;

  } else {
    final_branch_condition = ne;
    __ b(false_label);
    // A dead branch instruction will be generated after this point.
  }

  return final_branch_condition;
}


void LCodeGen::DoLazyBailout(LLazyBailout* instr) {
  // No code for lazy bailout instruction. Used to capture environment after a
  // call for populating the safepoint data with deoptimization data.
}


void LCodeGen::DoDeoptimize(LDeoptimize* instr) {
  DeoptimizeIf(no_condition, instr->environment());
}


void LCodeGen::DoDeleteProperty(LDeleteProperty* instr) {
  Register object = ToRegister(instr->object());
  Register key = ToRegister(instr->key());
  __ Push(object, key);
  RecordPosition(instr->pointer_map()->position());
  SafepointGenerator safepoint_generator(this,
                                         instr->pointer_map(),
                                         Safepoint::kNoDeoptimizationIndex);
  __ InvokeBuiltin(Builtins::DELETE, CALL_JS, &safepoint_generator);
}


void LCodeGen::DoStackCheck(LStackCheck* instr) {
  // Perform stack overflow check.
  Label ok;
  __ LoadRoot(ip, Heap::kStackLimitRootIndex);
  __ cmp(sp, Operand(ip));
  __ b(hs, &ok);
  StackCheckStub stub;
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
  __ bind(&ok);
}


void LCodeGen::DoOsrEntry(LOsrEntry* instr) {
  Abort("DoOsrEntry unimplemented.");
}


#undef __

} }  // namespace v8::internal
