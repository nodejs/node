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

#include "ia32/lithium-codegen-ia32.h"
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
  CpuFeatures::Scope scope(SSE2);
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
    __ int3();
  }
#endif

  __ push(ebp);  // Caller's frame pointer.
  __ mov(ebp, esp);
  __ push(esi);  // Callee's context.
  __ push(edi);  // Callee's JS function.

  // Reserve space for the stack slots needed by the code.
  int slots = StackSlotCount();
  if (slots > 0) {
    if (FLAG_debug_code) {
      __ mov(Operand(eax), Immediate(slots));
      Label loop;
      __ bind(&loop);
      __ push(Immediate(kSlotsZapValue));
      __ dec(eax);
      __ j(not_zero, &loop);
    } else {
      __ sub(Operand(esp), Immediate(slots * kPointerSize));
#ifdef _MSC_VER
      // On windows, you may not access the stack more than one page below
      // the most recently mapped page. To make the allocated area randomly
      // accessible, we write to each page in turn (the value is irrelevant).
      const int kPageSize = 4 * KB;
      for (int offset = slots * kPointerSize - kPageSize;
           offset > 0;
           offset -= kPageSize) {
        __ mov(Operand(esp, offset), eax);
      }
#endif
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


XMMRegister LCodeGen::ToDoubleRegister(int index) const {
  return XMMRegister::FromAllocationIndex(index);
}


Register LCodeGen::ToRegister(LOperand* op) const {
  ASSERT(op->IsRegister());
  return ToRegister(op->index());
}


XMMRegister LCodeGen::ToDoubleRegister(LOperand* op) const {
  ASSERT(op->IsDoubleRegister());
  return ToDoubleRegister(op->index());
}


int LCodeGen::ToInteger32(LConstantOperand* op) const {
  Handle<Object> value = chunk_->LookupLiteral(op);
  ASSERT(chunk_->LookupLiteralRepresentation(op).IsInteger32());
  ASSERT(static_cast<double>(static_cast<int32_t>(value->Number())) ==
      value->Number());
  return static_cast<int32_t>(value->Number());
}


Immediate LCodeGen::ToImmediate(LOperand* op) {
  LConstantOperand* const_op = LConstantOperand::cast(op);
  Handle<Object> literal = chunk_->LookupLiteral(const_op);
  Representation r = chunk_->LookupLiteralRepresentation(const_op);
  if (r.IsInteger32()) {
    ASSERT(literal->IsNumber());
    return Immediate(static_cast<int32_t>(literal->Number()));
  } else if (r.IsDouble()) {
    Abort("unsupported double immediate");
  }
  ASSERT(r.IsTagged());
  return Immediate(literal);
}


Operand LCodeGen::ToOperand(LOperand* op) const {
  if (op->IsRegister()) return Operand(ToRegister(op));
  if (op->IsDoubleRegister()) return Operand(ToDoubleRegister(op));
  ASSERT(op->IsStackSlot() || op->IsDoubleStackSlot());
  int index = op->index();
  if (index >= 0) {
    // Local or spill slot. Skip the frame pointer, function, and
    // context in the fixed part of the frame.
    return Operand(ebp, -(index + 3) * kPointerSize);
  } else {
    // Incoming parameter. Skip the return address.
    return Operand(ebp, -(index - 1) * kPointerSize);
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
    XMMRegister reg = ToDoubleRegister(op);
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
    __ call(code, mode);
    RegisterLazyDeoptimization(instr);
  } else {
    LPointerMap no_pointers(0);
    RecordPosition(no_pointers.position());
    __ call(code, mode);
    RecordSafepoint(&no_pointers, Safepoint::kNoDeoptimizationIndex);
  }

  // Signal that we don't inline smi code before these stubs in the
  // optimizing code generator.
  if (code->kind() == Code::TYPE_RECORDING_BINARY_OP_IC ||
      code->kind() == Code::COMPARE_IC) {
    __ nop();
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
  // BUG(3243555): register a lazy deoptimization point at throw. We need
  // it to be able to inline functions containing a throw statement.
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

  if (FLAG_deopt_every_n_times != 0) {
    Handle<SharedFunctionInfo> shared(info_->shared_info());
    Label no_deopt;
    __ pushfd();
    __ push(eax);
    __ push(ebx);
    __ mov(ebx, shared);
    __ mov(eax, FieldOperand(ebx, SharedFunctionInfo::kDeoptCounterOffset));
    __ sub(Operand(eax), Immediate(Smi::FromInt(1)));
    __ j(not_zero, &no_deopt);
    if (FLAG_trap_on_deopt) __ int3();
    __ mov(eax, Immediate(Smi::FromInt(FLAG_deopt_every_n_times)));
    __ mov(FieldOperand(ebx, SharedFunctionInfo::kDeoptCounterOffset), eax);
    __ pop(ebx);
    __ pop(eax);
    __ popfd();
    __ jmp(entry, RelocInfo::RUNTIME_ENTRY);

    __ bind(&no_deopt);
    __ mov(FieldOperand(ebx, SharedFunctionInfo::kDeoptCounterOffset), eax);
    __ pop(ebx);
    __ pop(eax);
    __ popfd();
  }

  if (cc == no_condition) {
    if (FLAG_trap_on_deopt) __ int3();
    __ jmp(entry, RelocInfo::RUNTIME_ENTRY);
  } else {
    if (FLAG_trap_on_deopt) {
      NearLabel done;
      __ j(NegateCondition(cc), &done);
      __ int3();
      __ jmp(entry, RelocInfo::RUNTIME_ENTRY);
      __ bind(&done);
    } else {
      __ j(cc, entry, RelocInfo::RUNTIME_ENTRY, not_taken);
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
  // Register esi always contains a pointer to the context.
  safepoint.DefinePointerRegister(esi);
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
  // xmm0 must always be a scratch register.
  XMMRegister xmm_scratch = xmm0;
  LUnallocated marker_operand(LUnallocated::NONE);

  Register cpu_scratch = esi;
  bool destroys_cpu_scratch = false;

  const ZoneList<LMoveOperands>* moves =
      resolver_.Resolve(move->move_operands(), &marker_operand);
  for (int i = moves->length() - 1; i >= 0; --i) {
    LMoveOperands move = moves->at(i);
    LOperand* from = move.from();
    LOperand* to = move.to();
    ASSERT(!from->IsDoubleRegister() ||
           !ToDoubleRegister(from).is(xmm_scratch));
    ASSERT(!to->IsDoubleRegister() || !ToDoubleRegister(to).is(xmm_scratch));
    ASSERT(!from->IsRegister() || !ToRegister(from).is(cpu_scratch));
    ASSERT(!to->IsRegister() || !ToRegister(to).is(cpu_scratch));
    if (from->IsConstantOperand()) {
      __ mov(ToOperand(to), ToImmediate(from));
    } else if (from == &marker_operand) {
      if (to->IsRegister() || to->IsStackSlot()) {
        __ mov(ToOperand(to), cpu_scratch);
        ASSERT(destroys_cpu_scratch);
      } else {
        ASSERT(to->IsDoubleRegister() || to->IsDoubleStackSlot());
        __ movdbl(ToOperand(to), xmm_scratch);
      }
    } else if (to == &marker_operand) {
      if (from->IsRegister() || from->IsStackSlot()) {
        __ mov(cpu_scratch, ToOperand(from));
        destroys_cpu_scratch = true;
      } else {
        ASSERT(from->IsDoubleRegister() || from->IsDoubleStackSlot());
        __ movdbl(xmm_scratch, ToOperand(from));
      }
    } else if (from->IsRegister()) {
      __ mov(ToOperand(to), ToRegister(from));
    } else if (to->IsRegister()) {
      __ mov(ToRegister(to), ToOperand(from));
    } else if (from->IsStackSlot()) {
      ASSERT(to->IsStackSlot());
      __ push(eax);
      __ mov(eax, ToOperand(from));
      __ mov(ToOperand(to), eax);
      __ pop(eax);
    } else if (from->IsDoubleRegister()) {
      __ movdbl(ToOperand(to), ToDoubleRegister(from));
    } else if (to->IsDoubleRegister()) {
      __ movdbl(ToDoubleRegister(to), ToOperand(from));
    } else {
      ASSERT(to->IsDoubleStackSlot() && from->IsDoubleStackSlot());
      __ movdbl(xmm_scratch, ToOperand(from));
      __ movdbl(ToOperand(to), xmm_scratch);
    }
  }

  if (destroys_cpu_scratch) {
    __ mov(cpu_scratch, Operand(ebp, -kPointerSize));
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
  ASSERT(ToRegister(instr->result()).is(eax));
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
      StringCharAtStub stub;
      CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
      break;
    }
    case CodeStub::MathPow: {
      MathPowStub stub;
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
  LOperand* right = instr->right();
  ASSERT(ToRegister(instr->result()).is(edx));
  ASSERT(ToRegister(instr->left()).is(eax));
  ASSERT(!ToRegister(instr->right()).is(eax));
  ASSERT(!ToRegister(instr->right()).is(edx));

  Register right_reg = ToRegister(right);

  // Check for x % 0.
  if (instr->hydrogen()->CheckFlag(HValue::kCanBeDivByZero)) {
    __ test(right_reg, ToOperand(right));
    DeoptimizeIf(zero, instr->environment());
  }

  // Sign extend to edx.
  __ cdq();

  // Check for (0 % -x) that will produce negative zero.
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    NearLabel positive_left;
    NearLabel done;
    __ test(eax, Operand(eax));
    __ j(not_sign, &positive_left);
    __ idiv(right_reg);

    // Test the remainder for 0, because then the result would be -0.
    __ test(edx, Operand(edx));
    __ j(not_zero, &done);

    DeoptimizeIf(no_condition, instr->environment());
    __ bind(&positive_left);
    __ idiv(right_reg);
    __ bind(&done);
  } else {
    __ idiv(right_reg);
  }
}


void LCodeGen::DoDivI(LDivI* instr) {
  LOperand* right = instr->right();
  ASSERT(ToRegister(instr->result()).is(eax));
  ASSERT(ToRegister(instr->left()).is(eax));
  ASSERT(!ToRegister(instr->right()).is(eax));
  ASSERT(!ToRegister(instr->right()).is(edx));

  Register left_reg = eax;

  // Check for x / 0.
  Register right_reg = ToRegister(right);
  if (instr->hydrogen()->CheckFlag(HValue::kCanBeDivByZero)) {
    __ test(right_reg, ToOperand(right));
    DeoptimizeIf(zero, instr->environment());
  }

  // Check for (0 / -x) that will produce negative zero.
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    NearLabel left_not_zero;
    __ test(left_reg, Operand(left_reg));
    __ j(not_zero, &left_not_zero);
    __ test(right_reg, ToOperand(right));
    DeoptimizeIf(sign, instr->environment());
    __ bind(&left_not_zero);
  }

  // Check for (-kMinInt / -1).
  if (instr->hydrogen()->CheckFlag(HValue::kCanOverflow)) {
    NearLabel left_not_min_int;
    __ cmp(left_reg, kMinInt);
    __ j(not_zero, &left_not_min_int);
    __ cmp(right_reg, -1);
    DeoptimizeIf(zero, instr->environment());
    __ bind(&left_not_min_int);
  }

  // Sign extend to edx.
  __ cdq();
  __ idiv(right_reg);

  // Deoptimize if remainder is not 0.
  __ test(edx, Operand(edx));
  DeoptimizeIf(not_zero, instr->environment());
}


void LCodeGen::DoMulI(LMulI* instr) {
  Register left = ToRegister(instr->left());
  LOperand* right = instr->right();

  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    __ mov(ToRegister(instr->temp()), left);
  }

  if (right->IsConstantOperand()) {
    __ imul(left, left, ToInteger32(LConstantOperand::cast(right)));
  } else {
    __ imul(left, ToOperand(right));
  }

  if (instr->hydrogen()->CheckFlag(HValue::kCanOverflow)) {
    DeoptimizeIf(overflow, instr->environment());
  }

  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    // Bail out if the result is supposed to be negative zero.
    NearLabel done;
    __ test(left, Operand(left));
    __ j(not_zero, &done);
    if (right->IsConstantOperand()) {
      if (ToInteger32(LConstantOperand::cast(right)) < 0) {
        DeoptimizeIf(no_condition, instr->environment());
      }
    } else {
      // Test the non-zero operand for negative sign.
      __ or_(ToRegister(instr->temp()), ToOperand(right));
      DeoptimizeIf(sign, instr->environment());
    }
    __ bind(&done);
  }
}


void LCodeGen::DoBitI(LBitI* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  ASSERT(left->Equals(instr->result()));
  ASSERT(left->IsRegister());

  if (right->IsConstantOperand()) {
    int right_operand = ToInteger32(LConstantOperand::cast(right));
    switch (instr->op()) {
      case Token::BIT_AND:
        __ and_(ToRegister(left), right_operand);
        break;
      case Token::BIT_OR:
        __ or_(ToRegister(left), right_operand);
        break;
      case Token::BIT_XOR:
        __ xor_(ToRegister(left), right_operand);
        break;
      default:
        UNREACHABLE();
        break;
    }
  } else {
    switch (instr->op()) {
      case Token::BIT_AND:
        __ and_(ToRegister(left), ToOperand(right));
        break;
      case Token::BIT_OR:
        __ or_(ToRegister(left), ToOperand(right));
        break;
      case Token::BIT_XOR:
        __ xor_(ToRegister(left), ToOperand(right));
        break;
      default:
        UNREACHABLE();
        break;
    }
  }
}


void LCodeGen::DoShiftI(LShiftI* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  ASSERT(left->Equals(instr->result()));
  ASSERT(left->IsRegister());
  if (right->IsRegister()) {
    ASSERT(ToRegister(right).is(ecx));

    switch (instr->op()) {
      case Token::SAR:
        __ sar_cl(ToRegister(left));
        break;
      case Token::SHR:
        __ shr_cl(ToRegister(left));
        if (instr->can_deopt()) {
          __ test(ToRegister(left), Immediate(0x80000000));
          DeoptimizeIf(not_zero, instr->environment());
        }
        break;
      case Token::SHL:
        __ shl_cl(ToRegister(left));
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
          __ sar(ToRegister(left), shift_count);
        }
        break;
      case Token::SHR:
        if (shift_count == 0 && instr->can_deopt()) {
          __ test(ToRegister(left), Immediate(0x80000000));
          DeoptimizeIf(not_zero, instr->environment());
        } else {
          __ shr(ToRegister(left), shift_count);
        }
        break;
      case Token::SHL:
        if (shift_count != 0) {
          __ shl(ToRegister(left), shift_count);
        }
        break;
      default:
        UNREACHABLE();
        break;
    }
  }
}


void LCodeGen::DoSubI(LSubI* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  ASSERT(left->Equals(instr->result()));

  if (right->IsConstantOperand()) {
    __ sub(ToOperand(left), ToImmediate(right));
  } else {
    __ sub(ToRegister(left), ToOperand(right));
  }
  if (instr->hydrogen()->CheckFlag(HValue::kCanOverflow)) {
    DeoptimizeIf(overflow, instr->environment());
  }
}


void LCodeGen::DoConstantI(LConstantI* instr) {
  ASSERT(instr->result()->IsRegister());
  __ Set(ToRegister(instr->result()), Immediate(instr->value()));
}


void LCodeGen::DoConstantD(LConstantD* instr) {
  ASSERT(instr->result()->IsDoubleRegister());
  XMMRegister res = ToDoubleRegister(instr->result());
  double v = instr->value();
  // Use xor to produce +0.0 in a fast and compact way, but avoid to
  // do so if the constant is -0.0.
  if (BitCast<uint64_t, double>(v) == 0) {
    __ xorpd(res, res);
  } else {
    int32_t v_int32 = static_cast<int32_t>(v);
    if (static_cast<double>(v_int32) == v) {
      __ push_imm32(v_int32);
      __ cvtsi2sd(res, Operand(esp, 0));
      __ add(Operand(esp), Immediate(kPointerSize));
    } else {
      uint64_t int_val = BitCast<uint64_t, double>(v);
      int32_t lower = static_cast<int32_t>(int_val);
      int32_t upper = static_cast<int32_t>(int_val >> (kBitsPerInt));
      __ push_imm32(upper);
      __ push_imm32(lower);
      __ movdbl(res, Operand(esp, 0));
      __ add(Operand(esp), Immediate(2 * kPointerSize));
    }
  }
}


void LCodeGen::DoConstantT(LConstantT* instr) {
  ASSERT(instr->result()->IsRegister());
  __ Set(ToRegister(instr->result()), Immediate(instr->value()));
}


void LCodeGen::DoJSArrayLength(LJSArrayLength* instr) {
  Register result = ToRegister(instr->result());
  Register array = ToRegister(instr->input());
  __ mov(result, FieldOperand(array, JSArray::kLengthOffset));
}


void LCodeGen::DoFixedArrayLength(LFixedArrayLength* instr) {
  Register result = ToRegister(instr->result());
  Register array = ToRegister(instr->input());
  __ mov(result, FieldOperand(array, FixedArray::kLengthOffset));
}


void LCodeGen::DoValueOf(LValueOf* instr) {
  Register input = ToRegister(instr->input());
  Register result = ToRegister(instr->result());
  Register map = ToRegister(instr->temporary());
  ASSERT(input.is(result));
  NearLabel done;
  // If the object is a smi return the object.
  __ test(input, Immediate(kSmiTagMask));
  __ j(zero, &done);

  // If the object is not a value type, return the object.
  __ CmpObjectType(input, JS_VALUE_TYPE, map);
  __ j(not_equal, &done);
  __ mov(result, FieldOperand(input, JSValue::kValueOffset));

  __ bind(&done);
}


void LCodeGen::DoBitNotI(LBitNotI* instr) {
  LOperand* input = instr->input();
  ASSERT(input->Equals(instr->result()));
  __ not_(ToRegister(input));
}


void LCodeGen::DoThrow(LThrow* instr) {
  __ push(ToOperand(instr->input()));
  CallRuntime(Runtime::kThrow, 1, instr);

  if (FLAG_debug_code) {
    Comment("Unreachable code.");
    __ int3();
  }
}


void LCodeGen::DoAddI(LAddI* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  ASSERT(left->Equals(instr->result()));

  if (right->IsConstantOperand()) {
    __ add(ToOperand(left), ToImmediate(right));
  } else {
    __ add(ToRegister(left), ToOperand(right));
  }

  if (instr->hydrogen()->CheckFlag(HValue::kCanOverflow)) {
    DeoptimizeIf(overflow, instr->environment());
  }
}


void LCodeGen::DoArithmeticD(LArithmeticD* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  // Modulo uses a fixed result register.
  ASSERT(instr->op() == Token::MOD || left->Equals(instr->result()));
  switch (instr->op()) {
    case Token::ADD:
      __ addsd(ToDoubleRegister(left), ToDoubleRegister(right));
      break;
    case Token::SUB:
       __ subsd(ToDoubleRegister(left), ToDoubleRegister(right));
       break;
    case Token::MUL:
      __ mulsd(ToDoubleRegister(left), ToDoubleRegister(right));
      break;
    case Token::DIV:
      __ divsd(ToDoubleRegister(left), ToDoubleRegister(right));
      break;
    case Token::MOD: {
      // Pass two doubles as arguments on the stack.
      __ PrepareCallCFunction(4, eax);
      __ movdbl(Operand(esp, 0 * kDoubleSize), ToDoubleRegister(left));
      __ movdbl(Operand(esp, 1 * kDoubleSize), ToDoubleRegister(right));
      __ CallCFunction(ExternalReference::double_fp_operation(Token::MOD), 4);

      // Return value is in st(0) on ia32.
      // Store it into the (fixed) result register.
      __ sub(Operand(esp), Immediate(kDoubleSize));
      __ fstp_d(Operand(esp, 0));
      __ movdbl(ToDoubleRegister(instr->result()), Operand(esp, 0));
      __ add(Operand(esp), Immediate(kDoubleSize));
      break;
    }
    default:
      UNREACHABLE();
      break;
  }
}


void LCodeGen::DoArithmeticT(LArithmeticT* instr) {
  ASSERT(ToRegister(instr->left()).is(edx));
  ASSERT(ToRegister(instr->right()).is(eax));
  ASSERT(ToRegister(instr->result()).is(eax));

  TypeRecordingBinaryOpStub stub(instr->op(), NO_OVERWRITE);
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
    __ j(NegateCondition(cc), chunk_->GetAssemblyLabel(right_block));
  } else if (right_block == next_block) {
    __ j(cc, chunk_->GetAssemblyLabel(left_block));
  } else {
    __ j(cc, chunk_->GetAssemblyLabel(left_block));
    __ jmp(chunk_->GetAssemblyLabel(right_block));
  }
}


void LCodeGen::DoBranch(LBranch* instr) {
  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  Representation r = instr->hydrogen()->representation();
  if (r.IsInteger32()) {
    Register reg = ToRegister(instr->input());
    __ test(reg, Operand(reg));
    EmitBranch(true_block, false_block, not_zero);
  } else if (r.IsDouble()) {
    XMMRegister reg = ToDoubleRegister(instr->input());
    __ xorpd(xmm0, xmm0);
    __ ucomisd(reg, xmm0);
    EmitBranch(true_block, false_block, not_equal);
  } else {
    ASSERT(r.IsTagged());
    Register reg = ToRegister(instr->input());
    if (instr->hydrogen()->type().IsBoolean()) {
      __ cmp(reg, Factory::true_value());
      EmitBranch(true_block, false_block, equal);
    } else {
      Label* true_label = chunk_->GetAssemblyLabel(true_block);
      Label* false_label = chunk_->GetAssemblyLabel(false_block);

      __ cmp(reg, Factory::undefined_value());
      __ j(equal, false_label);
      __ cmp(reg, Factory::true_value());
      __ j(equal, true_label);
      __ cmp(reg, Factory::false_value());
      __ j(equal, false_label);
      __ test(reg, Operand(reg));
      __ j(equal, false_label);
      __ test(reg, Immediate(kSmiTagMask));
      __ j(zero, true_label);

      // Test for double values. Zero is false.
      NearLabel call_stub;
      __ cmp(FieldOperand(reg, HeapObject::kMapOffset),
             Factory::heap_number_map());
      __ j(not_equal, &call_stub);
      __ fldz();
      __ fld_d(FieldOperand(reg, HeapNumber::kValueOffset));
      __ FCmp();
      __ j(zero, false_label);
      __ jmp(true_label);

      // The conversion stub doesn't cause garbage collections so it's
      // safe to not record a safepoint after the call.
      __ bind(&call_stub);
      ToBooleanStub stub;
      __ pushad();
      __ push(reg);
      __ CallStub(&stub);
      __ test(eax, Operand(eax));
      __ popad();
      EmitBranch(true_block, false_block, not_zero);
    }
  }
}


void LCodeGen::EmitGoto(int block, LDeferredCode* deferred_stack_check) {
  block = chunk_->LookupDestination(block);
  int next_block = GetNextEmittedBlock(current_block_);
  if (block != next_block) {
    // Perform stack overflow check if this goto needs it before jumping.
    if (deferred_stack_check != NULL) {
      ExternalReference stack_limit =
          ExternalReference::address_of_stack_limit();
      __ cmp(esp, Operand::StaticVariable(stack_limit));
      __ j(above_equal, chunk_->GetAssemblyLabel(block));
      __ jmp(deferred_stack_check->entry());
      deferred_stack_check->SetExit(chunk_->GetAssemblyLabel(block));
    } else {
      __ jmp(chunk_->GetAssemblyLabel(block));
    }
  }
}


void LCodeGen::DoDeferredStackCheck(LGoto* instr) {
  __ pushad();
  __ CallRuntimeSaveDoubles(Runtime::kStackGuard);
  RecordSafepointWithRegisters(
      instr->pointer_map(), 0, Safepoint::kNoDeoptimizationIndex);
  __ popad();
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
      cond = equal;
      break;
    case Token::LT:
      cond = is_unsigned ? below : less;
      break;
    case Token::GT:
      cond = is_unsigned ? above : greater;
      break;
    case Token::LTE:
      cond = is_unsigned ? below_equal : less_equal;
      break;
    case Token::GTE:
      cond = is_unsigned ? above_equal : greater_equal;
      break;
    case Token::IN:
    case Token::INSTANCEOF:
    default:
      UNREACHABLE();
  }
  return cond;
}


void LCodeGen::EmitCmpI(LOperand* left, LOperand* right) {
  if (right->IsConstantOperand()) {
    __ cmp(ToOperand(left), ToImmediate(right));
  } else {
    __ cmp(ToRegister(left), ToOperand(right));
  }
}


void LCodeGen::DoCmpID(LCmpID* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  LOperand* result = instr->result();

  NearLabel unordered;
  if (instr->is_double()) {
    // Don't base result on EFLAGS when a NaN is involved. Instead
    // jump to the unordered case, which produces a false value.
    __ ucomisd(ToDoubleRegister(left), ToDoubleRegister(right));
    __ j(parity_even, &unordered, not_taken);
  } else {
    EmitCmpI(left, right);
  }

  NearLabel done;
  Condition cc = TokenToCondition(instr->op(), instr->is_double());
  __ mov(ToRegister(result), Handle<Object>(Heap::true_value()));
  __ j(cc, &done);

  __ bind(&unordered);
  __ mov(ToRegister(result), Handle<Object>(Heap::false_value()));
  __ bind(&done);
}


void LCodeGen::DoCmpIDAndBranch(LCmpIDAndBranch* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  int false_block = chunk_->LookupDestination(instr->false_block_id());
  int true_block = chunk_->LookupDestination(instr->true_block_id());

  if (instr->is_double()) {
    // Don't base result on EFLAGS when a NaN is involved. Instead
    // jump to the false block.
    __ ucomisd(ToDoubleRegister(left), ToDoubleRegister(right));
    __ j(parity_even, chunk_->GetAssemblyLabel(false_block));
  } else {
    EmitCmpI(left, right);
  }

  Condition cc = TokenToCondition(instr->op(), instr->is_double());
  EmitBranch(true_block, false_block, cc);
}


void LCodeGen::DoCmpJSObjectEq(LCmpJSObjectEq* instr) {
  Register left = ToRegister(instr->left());
  Register right = ToRegister(instr->right());
  Register result = ToRegister(instr->result());

  __ cmp(left, Operand(right));
  __ mov(result, Handle<Object>(Heap::true_value()));
  NearLabel done;
  __ j(equal, &done);
  __ mov(result, Handle<Object>(Heap::false_value()));
  __ bind(&done);
}


void LCodeGen::DoCmpJSObjectEqAndBranch(LCmpJSObjectEqAndBranch* instr) {
  Register left = ToRegister(instr->left());
  Register right = ToRegister(instr->right());
  int false_block = chunk_->LookupDestination(instr->false_block_id());
  int true_block = chunk_->LookupDestination(instr->true_block_id());

  __ cmp(left, Operand(right));
  EmitBranch(true_block, false_block, equal);
}


void LCodeGen::DoIsNull(LIsNull* instr) {
  Register reg = ToRegister(instr->input());
  Register result = ToRegister(instr->result());

  // TODO(fsc): If the expression is known to be a smi, then it's
  // definitely not null. Materialize false.

  __ cmp(reg, Factory::null_value());
  if (instr->is_strict()) {
    __ mov(result, Handle<Object>(Heap::true_value()));
    NearLabel done;
    __ j(equal, &done);
    __ mov(result, Handle<Object>(Heap::false_value()));
    __ bind(&done);
  } else {
    NearLabel true_value, false_value, done;
    __ j(equal, &true_value);
    __ cmp(reg, Factory::undefined_value());
    __ j(equal, &true_value);
    __ test(reg, Immediate(kSmiTagMask));
    __ j(zero, &false_value);
    // Check for undetectable objects by looking in the bit field in
    // the map. The object has already been smi checked.
    Register scratch = result;
    __ mov(scratch, FieldOperand(reg, HeapObject::kMapOffset));
    __ movzx_b(scratch, FieldOperand(scratch, Map::kBitFieldOffset));
    __ test(scratch, Immediate(1 << Map::kIsUndetectable));
    __ j(not_zero, &true_value);
    __ bind(&false_value);
    __ mov(result, Handle<Object>(Heap::false_value()));
    __ jmp(&done);
    __ bind(&true_value);
    __ mov(result, Handle<Object>(Heap::true_value()));
    __ bind(&done);
  }
}


void LCodeGen::DoIsNullAndBranch(LIsNullAndBranch* instr) {
  Register reg = ToRegister(instr->input());

  // TODO(fsc): If the expression is known to be a smi, then it's
  // definitely not null. Jump to the false block.

  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  __ cmp(reg, Factory::null_value());
  if (instr->is_strict()) {
    EmitBranch(true_block, false_block, equal);
  } else {
    Label* true_label = chunk_->GetAssemblyLabel(true_block);
    Label* false_label = chunk_->GetAssemblyLabel(false_block);
    __ j(equal, true_label);
    __ cmp(reg, Factory::undefined_value());
    __ j(equal, true_label);
    __ test(reg, Immediate(kSmiTagMask));
    __ j(zero, false_label);
    // Check for undetectable objects by looking in the bit field in
    // the map. The object has already been smi checked.
    Register scratch = ToRegister(instr->temp());
    __ mov(scratch, FieldOperand(reg, HeapObject::kMapOffset));
    __ movzx_b(scratch, FieldOperand(scratch, Map::kBitFieldOffset));
    __ test(scratch, Immediate(1 << Map::kIsUndetectable));
    EmitBranch(true_block, false_block, not_zero);
  }
}


Condition LCodeGen::EmitIsObject(Register input,
                                 Register temp1,
                                 Register temp2,
                                 Label* is_not_object,
                                 Label* is_object) {
  ASSERT(!input.is(temp1));
  ASSERT(!input.is(temp2));
  ASSERT(!temp1.is(temp2));

  __ test(input, Immediate(kSmiTagMask));
  __ j(equal, is_not_object);

  __ cmp(input, Factory::null_value());
  __ j(equal, is_object);

  __ mov(temp1, FieldOperand(input, HeapObject::kMapOffset));
  // Undetectable objects behave like undefined.
  __ movzx_b(temp2, FieldOperand(temp1, Map::kBitFieldOffset));
  __ test(temp2, Immediate(1 << Map::kIsUndetectable));
  __ j(not_zero, is_not_object);

  __ movzx_b(temp2, FieldOperand(temp1, Map::kInstanceTypeOffset));
  __ cmp(temp2, FIRST_JS_OBJECT_TYPE);
  __ j(below, is_not_object);
  __ cmp(temp2, LAST_JS_OBJECT_TYPE);
  return below_equal;
}


void LCodeGen::DoIsObject(LIsObject* instr) {
  Register reg = ToRegister(instr->input());
  Register result = ToRegister(instr->result());
  Register temp = ToRegister(instr->temp());
  Label is_false, is_true, done;

  Condition true_cond = EmitIsObject(reg, result, temp, &is_false, &is_true);
  __ j(true_cond, &is_true);

  __ bind(&is_false);
  __ mov(result, Handle<Object>(Heap::false_value()));
  __ jmp(&done);

  __ bind(&is_true);
  __ mov(result, Handle<Object>(Heap::true_value()));

  __ bind(&done);
}


void LCodeGen::DoIsObjectAndBranch(LIsObjectAndBranch* instr) {
  Register reg = ToRegister(instr->input());
  Register temp = ToRegister(instr->temp());
  Register temp2 = ToRegister(instr->temp2());

  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());
  Label* true_label = chunk_->GetAssemblyLabel(true_block);
  Label* false_label = chunk_->GetAssemblyLabel(false_block);

  Condition true_cond = EmitIsObject(reg, temp, temp2, false_label, true_label);

  EmitBranch(true_block, false_block, true_cond);
}


void LCodeGen::DoIsSmi(LIsSmi* instr) {
  Operand input = ToOperand(instr->input());
  Register result = ToRegister(instr->result());

  ASSERT(instr->hydrogen()->value()->representation().IsTagged());
  __ test(input, Immediate(kSmiTagMask));
  __ mov(result, Handle<Object>(Heap::true_value()));
  NearLabel done;
  __ j(zero, &done);
  __ mov(result, Handle<Object>(Heap::false_value()));
  __ bind(&done);
}


void LCodeGen::DoIsSmiAndBranch(LIsSmiAndBranch* instr) {
  Operand input = ToOperand(instr->input());

  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  __ test(input, Immediate(kSmiTagMask));
  EmitBranch(true_block, false_block, zero);
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
  if (from == to) return equal;
  if (to == LAST_TYPE) return above_equal;
  if (from == FIRST_TYPE) return below_equal;
  UNREACHABLE();
  return equal;
}


void LCodeGen::DoHasInstanceType(LHasInstanceType* instr) {
  Register input = ToRegister(instr->input());
  Register result = ToRegister(instr->result());

  ASSERT(instr->hydrogen()->value()->representation().IsTagged());
  __ test(input, Immediate(kSmiTagMask));
  NearLabel done, is_false;
  __ j(zero, &is_false);
  __ CmpObjectType(input, instr->TestType(), result);
  __ j(NegateCondition(instr->BranchCondition()), &is_false);
  __ mov(result, Handle<Object>(Heap::true_value()));
  __ jmp(&done);
  __ bind(&is_false);
  __ mov(result, Handle<Object>(Heap::false_value()));
  __ bind(&done);
}


void LCodeGen::DoHasInstanceTypeAndBranch(LHasInstanceTypeAndBranch* instr) {
  Register input = ToRegister(instr->input());
  Register temp = ToRegister(instr->temp());

  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  Label* false_label = chunk_->GetAssemblyLabel(false_block);

  __ test(input, Immediate(kSmiTagMask));
  __ j(zero, false_label);

  __ CmpObjectType(input, instr->TestType(), temp);
  EmitBranch(true_block, false_block, instr->BranchCondition());
}


void LCodeGen::DoHasCachedArrayIndex(LHasCachedArrayIndex* instr) {
  Register input = ToRegister(instr->input());
  Register result = ToRegister(instr->result());

  ASSERT(instr->hydrogen()->value()->representation().IsTagged());
  __ mov(result, Handle<Object>(Heap::true_value()));
  __ test(FieldOperand(input, String::kHashFieldOffset),
          Immediate(String::kContainsCachedArrayIndexMask));
  NearLabel done;
  __ j(not_zero, &done);
  __ mov(result, Handle<Object>(Heap::false_value()));
  __ bind(&done);
}


void LCodeGen::DoHasCachedArrayIndexAndBranch(
    LHasCachedArrayIndexAndBranch* instr) {
  Register input = ToRegister(instr->input());

  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  __ test(FieldOperand(input, String::kHashFieldOffset),
          Immediate(String::kContainsCachedArrayIndexMask));
  EmitBranch(true_block, false_block, not_equal);
}


// Branches to a label or falls through with the answer in the z flag.  Trashes
// the temp registers, but not the input.  Only input and temp2 may alias.
void LCodeGen::EmitClassOfTest(Label* is_true,
                               Label* is_false,
                               Handle<String>class_name,
                               Register input,
                               Register temp,
                               Register temp2) {
  ASSERT(!input.is(temp));
  ASSERT(!temp.is(temp2));  // But input and temp2 may be the same register.
  __ test(input, Immediate(kSmiTagMask));
  __ j(zero, is_false);
  __ CmpObjectType(input, FIRST_JS_OBJECT_TYPE, temp);
  __ j(below, is_false);

  // Map is now in temp.
  // Functions have class 'Function'.
  __ CmpInstanceType(temp, JS_FUNCTION_TYPE);
  if (class_name->IsEqualTo(CStrVector("Function"))) {
    __ j(equal, is_true);
  } else {
    __ j(equal, is_false);
  }

  // Check if the constructor in the map is a function.
  __ mov(temp, FieldOperand(temp, Map::kConstructorOffset));

  // As long as JS_FUNCTION_TYPE is the last instance type and it is
  // right after LAST_JS_OBJECT_TYPE, we can avoid checking for
  // LAST_JS_OBJECT_TYPE.
  ASSERT(LAST_TYPE == JS_FUNCTION_TYPE);
  ASSERT(JS_FUNCTION_TYPE == LAST_JS_OBJECT_TYPE + 1);

  // Objects with a non-function constructor have class 'Object'.
  __ CmpObjectType(temp, JS_FUNCTION_TYPE, temp2);
  if (class_name->IsEqualTo(CStrVector("Object"))) {
    __ j(not_equal, is_true);
  } else {
    __ j(not_equal, is_false);
  }

  // temp now contains the constructor function. Grab the
  // instance class name from there.
  __ mov(temp, FieldOperand(temp, JSFunction::kSharedFunctionInfoOffset));
  __ mov(temp, FieldOperand(temp,
                            SharedFunctionInfo::kInstanceClassNameOffset));
  // The class name we are testing against is a symbol because it's a literal.
  // The name in the constructor is a symbol because of the way the context is
  // booted.  This routine isn't expected to work for random API-created
  // classes and it doesn't have to because you can't access it with natives
  // syntax.  Since both sides are symbols it is sufficient to use an identity
  // comparison.
  __ cmp(temp, class_name);
  // End with the answer in the z flag.
}


void LCodeGen::DoClassOfTest(LClassOfTest* instr) {
  Register input = ToRegister(instr->input());
  Register result = ToRegister(instr->result());
  ASSERT(input.is(result));
  Register temp = ToRegister(instr->temporary());
  Handle<String> class_name = instr->hydrogen()->class_name();
  NearLabel done;
  Label is_true, is_false;

  EmitClassOfTest(&is_true, &is_false, class_name, input, temp, input);

  __ j(not_equal, &is_false);

  __ bind(&is_true);
  __ mov(result, Handle<Object>(Heap::true_value()));
  __ jmp(&done);

  __ bind(&is_false);
  __ mov(result, Handle<Object>(Heap::false_value()));
  __ bind(&done);
}


void LCodeGen::DoClassOfTestAndBranch(LClassOfTestAndBranch* instr) {
  Register input = ToRegister(instr->input());
  Register temp = ToRegister(instr->temporary());
  Register temp2 = ToRegister(instr->temporary2());
  if (input.is(temp)) {
    // Swap.
    Register swapper = temp;
    temp = temp2;
    temp2 = swapper;
  }
  Handle<String> class_name = instr->hydrogen()->class_name();

  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  Label* true_label = chunk_->GetAssemblyLabel(true_block);
  Label* false_label = chunk_->GetAssemblyLabel(false_block);

  EmitClassOfTest(true_label, false_label, class_name, input, temp, temp2);

  EmitBranch(true_block, false_block, equal);
}


void LCodeGen::DoCmpMapAndBranch(LCmpMapAndBranch* instr) {
  Register reg = ToRegister(instr->input());
  int true_block = instr->true_block_id();
  int false_block = instr->false_block_id();

  __ cmp(FieldOperand(reg, HeapObject::kMapOffset), instr->map());
  EmitBranch(true_block, false_block, equal);
}


void LCodeGen::DoInstanceOf(LInstanceOf* instr) {
  // Object and function are in fixed registers defined by the stub.
  InstanceofStub stub(InstanceofStub::kArgsInRegisters);
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);

  NearLabel true_value, done;
  __ test(eax, Operand(eax));
  __ j(zero, &true_value);
  __ mov(ToRegister(instr->result()), Factory::false_value());
  __ jmp(&done);
  __ bind(&true_value);
  __ mov(ToRegister(instr->result()), Factory::true_value());
  __ bind(&done);
}


void LCodeGen::DoInstanceOfAndBranch(LInstanceOfAndBranch* instr) {
  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  InstanceofStub stub(InstanceofStub::kArgsInRegisters);
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
  __ test(eax, Operand(eax));
  EmitBranch(true_block, false_block, zero);
}


void LCodeGen::DoInstanceOfKnownGlobal(LInstanceOfKnownGlobal* instr) {
  class DeferredInstanceOfKnownGlobal: public LDeferredCode {
   public:
    DeferredInstanceOfKnownGlobal(LCodeGen* codegen,
                                  LInstanceOfKnownGlobal* instr)
        : LDeferredCode(codegen), instr_(instr) { }
    virtual void Generate() {
      codegen()->DoDeferredLInstanceOfKnownGlobal(instr_, &map_check_);
    }

    Label* map_check() { return &map_check_; }

   private:
    LInstanceOfKnownGlobal* instr_;
    Label map_check_;
  };

  DeferredInstanceOfKnownGlobal* deferred;
  deferred = new DeferredInstanceOfKnownGlobal(this, instr);

  Label done, false_result;
  Register object = ToRegister(instr->input());
  Register temp = ToRegister(instr->temp());

  // A Smi is not instance of anything.
  __ test(object, Immediate(kSmiTagMask));
  __ j(zero, &false_result, not_taken);

  // This is the inlined call site instanceof cache. The two occourences of the
  // hole value will be patched to the last map/result pair generated by the
  // instanceof stub.
  NearLabel cache_miss;
  Register map = ToRegister(instr->temp());
  __ mov(map, FieldOperand(object, HeapObject::kMapOffset));
  __ bind(deferred->map_check());  // Label for calculating code patching.
  __ cmp(map, Factory::the_hole_value());  // Patched to cached map.
  __ j(not_equal, &cache_miss, not_taken);
  __ mov(eax, Factory::the_hole_value());  // Patched to either true or false.
  __ jmp(&done);

  // The inlined call site cache did not match. Check null and string before
  // calling the deferred code.
  __ bind(&cache_miss);
  // Null is not instance of anything.
  __ cmp(object, Factory::null_value());
  __ j(equal, &false_result);

  // String values are not instances of anything.
  Condition is_string = masm_->IsObjectStringType(object, temp, temp);
  __ j(is_string, &false_result);

  // Go to the deferred code.
  __ jmp(deferred->entry());

  __ bind(&false_result);
  __ mov(ToRegister(instr->result()), Factory::false_value());

  // Here result has either true or false. Deferred code also produces true or
  // false object.
  __ bind(deferred->exit());
  __ bind(&done);
}


void LCodeGen::DoDeferredLInstanceOfKnownGlobal(LInstanceOfKnownGlobal* instr,
                                                Label* map_check) {
  __ PushSafepointRegisters();

  InstanceofStub::Flags flags = InstanceofStub::kNoFlags;
  flags = static_cast<InstanceofStub::Flags>(
      flags | InstanceofStub::kArgsInRegisters);
  flags = static_cast<InstanceofStub::Flags>(
      flags | InstanceofStub::kCallSiteInlineCheck);
  flags = static_cast<InstanceofStub::Flags>(
      flags | InstanceofStub::kReturnTrueFalseObject);
  InstanceofStub stub(flags);

  // Get the temp register reserved by the instruction. This needs to be edi as
  // its slot of the pushing of safepoint registers is used to communicate the
  // offset to the location of the map check.
  Register temp = ToRegister(instr->temp());
  ASSERT(temp.is(edi));
  __ mov(InstanceofStub::right(), Immediate(instr->function()));
  static const int kAdditionalDelta = 13;
  int delta = masm_->SizeOfCodeGeneratedSince(map_check) + kAdditionalDelta;
  Label before_push_delta;
  __ bind(&before_push_delta);
  __ mov(temp, Immediate(delta));
  __ mov(Operand(esp, EspIndexForPushAll(temp) * kPointerSize), temp);
  __ call(stub.GetCode(), RelocInfo::CODE_TARGET);
  ASSERT_EQ(kAdditionalDelta,
            masm_->SizeOfCodeGeneratedSince(&before_push_delta));
  RecordSafepointWithRegisters(
      instr->pointer_map(), 0, Safepoint::kNoDeoptimizationIndex);
  // Put the result value into the eax slot and restore all registers.
  __ mov(Operand(esp, EspIndexForPushAll(eax) * kPointerSize), eax);

  __ PopSafepointRegisters();
}


static Condition ComputeCompareCondition(Token::Value op) {
  switch (op) {
    case Token::EQ_STRICT:
    case Token::EQ:
      return equal;
    case Token::LT:
      return less;
    case Token::GT:
      return greater;
    case Token::LTE:
      return less_equal;
    case Token::GTE:
      return greater_equal;
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
  NearLabel true_value, done;
  __ test(eax, Operand(eax));
  __ j(condition, &true_value);
  __ mov(ToRegister(instr->result()), Factory::false_value());
  __ jmp(&done);
  __ bind(&true_value);
  __ mov(ToRegister(instr->result()), Factory::true_value());
  __ bind(&done);
}


void LCodeGen::DoCmpTAndBranch(LCmpTAndBranch* instr) {
  Token::Value op = instr->op();
  int true_block = chunk_->LookupDestination(instr->true_block_id());
  int false_block = chunk_->LookupDestination(instr->false_block_id());

  Handle<Code> ic = CompareIC::GetUninitialized(op);
  CallCode(ic, RelocInfo::CODE_TARGET, instr);

  // The compare stub expects compare condition and the input operands
  // reversed for GT and LTE.
  Condition condition = ComputeCompareCondition(op);
  if (op == Token::GT || op == Token::LTE) {
    condition = ReverseCondition(condition);
  }
  __ test(eax, Operand(eax));
  EmitBranch(true_block, false_block, condition);
}


void LCodeGen::DoReturn(LReturn* instr) {
  if (FLAG_trace) {
    // Preserve the return value on the stack and rely on the runtime
    // call to return the value in the same register.
    __ push(eax);
    __ CallRuntime(Runtime::kTraceExit, 1);
  }
  __ mov(esp, ebp);
  __ pop(ebp);
  __ ret((ParameterCount() + 1) * kPointerSize);
}


void LCodeGen::DoLoadGlobal(LLoadGlobal* instr) {
  Register result = ToRegister(instr->result());
  __ mov(result, Operand::Cell(instr->hydrogen()->cell()));
  if (instr->hydrogen()->check_hole_value()) {
    __ cmp(result, Factory::the_hole_value());
    DeoptimizeIf(equal, instr->environment());
  }
}


void LCodeGen::DoStoreGlobal(LStoreGlobal* instr) {
  Register value = ToRegister(instr->input());
  __ mov(Operand::Cell(instr->hydrogen()->cell()), value);
}


void LCodeGen::DoLoadContextSlot(LLoadContextSlot* instr) {
  // TODO(antonm): load a context with a separate instruction.
  Register result = ToRegister(instr->result());
  __ LoadContext(result, instr->context_chain_length());
  __ mov(result, ContextOperand(result, instr->slot_index()));
}


void LCodeGen::DoLoadNamedField(LLoadNamedField* instr) {
  Register object = ToRegister(instr->input());
  Register result = ToRegister(instr->result());
  if (instr->hydrogen()->is_in_object()) {
    __ mov(result, FieldOperand(object, instr->hydrogen()->offset()));
  } else {
    __ mov(result, FieldOperand(object, JSObject::kPropertiesOffset));
    __ mov(result, FieldOperand(result, instr->hydrogen()->offset()));
  }
}


void LCodeGen::DoLoadNamedGeneric(LLoadNamedGeneric* instr) {
  ASSERT(ToRegister(instr->object()).is(eax));
  ASSERT(ToRegister(instr->result()).is(eax));

  __ mov(ecx, instr->name());
  Handle<Code> ic(Builtins::builtin(Builtins::LoadIC_Initialize));
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoLoadFunctionPrototype(LLoadFunctionPrototype* instr) {
  Register function = ToRegister(instr->function());
  Register temp = ToRegister(instr->temporary());
  Register result = ToRegister(instr->result());

  // Check that the function really is a function.
  __ CmpObjectType(function, JS_FUNCTION_TYPE, result);
  DeoptimizeIf(not_equal, instr->environment());

  // Check whether the function has an instance prototype.
  NearLabel non_instance;
  __ test_b(FieldOperand(result, Map::kBitFieldOffset),
            1 << Map::kHasNonInstancePrototype);
  __ j(not_zero, &non_instance);

  // Get the prototype or initial map from the function.
  __ mov(result,
         FieldOperand(function, JSFunction::kPrototypeOrInitialMapOffset));

  // Check that the function has a prototype or an initial map.
  __ cmp(Operand(result), Immediate(Factory::the_hole_value()));
  DeoptimizeIf(equal, instr->environment());

  // If the function does not have an initial map, we're done.
  NearLabel done;
  __ CmpObjectType(result, MAP_TYPE, temp);
  __ j(not_equal, &done);

  // Get the prototype from the initial map.
  __ mov(result, FieldOperand(result, Map::kPrototypeOffset));
  __ jmp(&done);

  // Non-instance prototype: Fetch prototype from constructor field
  // in the function's map.
  __ bind(&non_instance);
  __ mov(result, FieldOperand(result, Map::kConstructorOffset));

  // All done.
  __ bind(&done);
}


void LCodeGen::DoLoadElements(LLoadElements* instr) {
  ASSERT(instr->result()->Equals(instr->input()));
  Register reg = ToRegister(instr->input());
  __ mov(reg, FieldOperand(reg, JSObject::kElementsOffset));
  if (FLAG_debug_code) {
    NearLabel done;
    __ cmp(FieldOperand(reg, HeapObject::kMapOffset),
           Immediate(Factory::fixed_array_map()));
    __ j(equal, &done);
    __ cmp(FieldOperand(reg, HeapObject::kMapOffset),
           Immediate(Factory::fixed_cow_array_map()));
    __ Check(equal, "Check for fast elements failed.");
    __ bind(&done);
  }
}


void LCodeGen::DoAccessArgumentsAt(LAccessArgumentsAt* instr) {
  Register arguments = ToRegister(instr->arguments());
  Register length = ToRegister(instr->length());
  Operand index = ToOperand(instr->index());
  Register result = ToRegister(instr->result());

  __ sub(length, index);
  DeoptimizeIf(below_equal, instr->environment());

  // There are two words between the frame pointer and the last argument.
  // Subtracting from length accounts for one of them add one more.
  __ mov(result, Operand(arguments, length, times_4, kPointerSize));
}


void LCodeGen::DoLoadKeyedFastElement(LLoadKeyedFastElement* instr) {
  Register elements = ToRegister(instr->elements());
  Register key = ToRegister(instr->key());
  Register result = ToRegister(instr->result());
  ASSERT(result.is(elements));

  // Load the result.
  __ mov(result, FieldOperand(elements, key, times_4, FixedArray::kHeaderSize));

  // Check for the hole value.
  __ cmp(result, Factory::the_hole_value());
  DeoptimizeIf(equal, instr->environment());
}


void LCodeGen::DoLoadKeyedGeneric(LLoadKeyedGeneric* instr) {
  ASSERT(ToRegister(instr->object()).is(edx));
  ASSERT(ToRegister(instr->key()).is(eax));

  Handle<Code> ic(Builtins::builtin(Builtins::KeyedLoadIC_Initialize));
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoArgumentsElements(LArgumentsElements* instr) {
  Register result = ToRegister(instr->result());

  // Check for arguments adapter frame.
  NearLabel done, adapted;
  __ mov(result, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ mov(result, Operand(result, StandardFrameConstants::kContextOffset));
  __ cmp(Operand(result),
         Immediate(Smi::FromInt(StackFrame::ARGUMENTS_ADAPTOR)));
  __ j(equal, &adapted);

  // No arguments adaptor frame.
  __ mov(result, Operand(ebp));
  __ jmp(&done);

  // Arguments adaptor frame present.
  __ bind(&adapted);
  __ mov(result, Operand(ebp, StandardFrameConstants::kCallerFPOffset));

  // Result is the frame pointer for the frame if not adapted and for the real
  // frame below the adaptor frame if adapted.
  __ bind(&done);
}


void LCodeGen::DoArgumentsLength(LArgumentsLength* instr) {
  Operand elem = ToOperand(instr->input());
  Register result = ToRegister(instr->result());

  NearLabel done;

  // If no arguments adaptor frame the number of arguments is fixed.
  __ cmp(ebp, elem);
  __ mov(result, Immediate(scope()->num_parameters()));
  __ j(equal, &done);

  // Arguments adaptor frame present. Get argument length from there.
  __ mov(result, Operand(ebp, StandardFrameConstants::kCallerFPOffset));
  __ mov(result, Operand(result,
                         ArgumentsAdaptorFrameConstants::kLengthOffset));
  __ SmiUntag(result);

  // Argument length is in result register.
  __ bind(&done);
}


void LCodeGen::DoApplyArguments(LApplyArguments* instr) {
  Register receiver = ToRegister(instr->receiver());
  ASSERT(ToRegister(instr->function()).is(edi));
  ASSERT(ToRegister(instr->result()).is(eax));

  // If the receiver is null or undefined, we have to pass the
  // global object as a receiver.
  NearLabel global_receiver, receiver_ok;
  __ cmp(receiver, Factory::null_value());
  __ j(equal, &global_receiver);
  __ cmp(receiver, Factory::undefined_value());
  __ j(not_equal, &receiver_ok);
  __ bind(&global_receiver);
  __ mov(receiver, GlobalObjectOperand());
  __ bind(&receiver_ok);

  Register length = ToRegister(instr->length());
  Register elements = ToRegister(instr->elements());

  Label invoke;

  // Copy the arguments to this function possibly from the
  // adaptor frame below it.
  const uint32_t kArgumentsLimit = 1 * KB;
  __ cmp(length, kArgumentsLimit);
  DeoptimizeIf(above, instr->environment());

  __ push(receiver);
  __ mov(receiver, length);

  // Loop through the arguments pushing them onto the execution
  // stack.
  Label loop;
  // length is a small non-negative integer, due to the test above.
  __ test(length, Operand(length));
  __ j(zero, &invoke);
  __ bind(&loop);
  __ push(Operand(elements, length, times_pointer_size, 1 * kPointerSize));
  __ dec(length);
  __ j(not_zero, &loop);

  // Invoke the function.
  __ bind(&invoke);
  ASSERT(receiver.is(eax));
  v8::internal::ParameterCount actual(eax);
  SafepointGenerator safepoint_generator(this,
                                         instr->pointer_map(),
                                         Safepoint::kNoDeoptimizationIndex);
  __ InvokeFunction(edi, actual, CALL_FUNCTION, &safepoint_generator);
}


void LCodeGen::DoPushArgument(LPushArgument* instr) {
  LOperand* argument = instr->input();
  if (argument->IsConstantOperand()) {
    __ push(ToImmediate(argument));
  } else {
    __ push(ToOperand(argument));
  }
}


void LCodeGen::DoGlobalObject(LGlobalObject* instr) {
  Register result = ToRegister(instr->result());
  __ mov(result, Operand(esi, Context::SlotOffset(Context::GLOBAL_INDEX)));
}


void LCodeGen::DoGlobalReceiver(LGlobalReceiver* instr) {
  Register result = ToRegister(instr->result());
  __ mov(result, Operand(esi, Context::SlotOffset(Context::GLOBAL_INDEX)));
  __ mov(result, FieldOperand(result, GlobalObject::kGlobalReceiverOffset));
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
    __ mov(esi, FieldOperand(edi, JSFunction::kContextOffset));
  }

  // Set eax to arguments count if adaption is not needed. Assumes that eax
  // is available to write to at this point.
  if (!function->NeedsArgumentsAdaption()) {
    __ mov(eax, arity);
  }

  LPointerMap* pointers = instr->pointer_map();
  RecordPosition(pointers->position());

  // Invoke function.
  if (*function == *graph()->info()->closure()) {
    __ CallSelf();
  } else {
    __ call(FieldOperand(edi, JSFunction::kCodeEntryOffset));
  }

  // Setup deoptimization.
  RegisterLazyDeoptimization(instr);

  // Restore context.
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallConstantFunction(LCallConstantFunction* instr) {
  ASSERT(ToRegister(instr->result()).is(eax));
  __ mov(edi, instr->function());
  CallKnownFunction(instr->function(), instr->arity(), instr);
}


void LCodeGen::DoDeferredMathAbsTaggedHeapNumber(LUnaryMathOperation* instr) {
  Register input_reg = ToRegister(instr->input());
  __ cmp(FieldOperand(input_reg, HeapObject::kMapOffset),
         Factory::heap_number_map());
  DeoptimizeIf(not_equal, instr->environment());

  Label done;
  Register tmp = input_reg.is(eax) ? ecx : eax;
  Register tmp2 = tmp.is(ecx) ? edx : input_reg.is(ecx) ? edx : ecx;

  // Preserve the value of all registers.
  __ PushSafepointRegisters();

  Label negative;
  __ mov(tmp, FieldOperand(input_reg, HeapNumber::kExponentOffset));
  // Check the sign of the argument. If the argument is positive,
  // just return it.
  __ test(tmp, Immediate(HeapNumber::kSignMask));
  __ j(not_zero, &negative);
  __ mov(tmp, input_reg);
  __ jmp(&done);

  __ bind(&negative);

  Label allocated, slow;
  __ AllocateHeapNumber(tmp, tmp2, no_reg, &slow);
  __ jmp(&allocated);

  // Slow case: Call the runtime system to do the number allocation.
  __ bind(&slow);

  __ CallRuntimeSaveDoubles(Runtime::kAllocateHeapNumber);
  RecordSafepointWithRegisters(
      instr->pointer_map(), 0, Safepoint::kNoDeoptimizationIndex);
  // Set the pointer to the new heap number in tmp.
  if (!tmp.is(eax)) __ mov(tmp, eax);

  // Restore input_reg after call to runtime.
  __ mov(input_reg, Operand(esp, EspIndexForPushAll(input_reg) * kPointerSize));

  __ bind(&allocated);
  __ mov(tmp2, FieldOperand(input_reg, HeapNumber::kExponentOffset));
  __ and_(tmp2, ~HeapNumber::kSignMask);
  __ mov(FieldOperand(tmp, HeapNumber::kExponentOffset), tmp2);
  __ mov(tmp2, FieldOperand(input_reg, HeapNumber::kMantissaOffset));
  __ mov(FieldOperand(tmp, HeapNumber::kMantissaOffset), tmp2);

  __ bind(&done);
  __ mov(Operand(esp, EspIndexForPushAll(input_reg) * kPointerSize), tmp);

  __ PopSafepointRegisters();
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

  ASSERT(instr->input()->Equals(instr->result()));
  Representation r = instr->hydrogen()->value()->representation();

  if (r.IsDouble()) {
    XMMRegister  scratch = xmm0;
    XMMRegister input_reg = ToDoubleRegister(instr->input());
    __ pxor(scratch, scratch);
    __ subsd(scratch, input_reg);
    __ pand(input_reg, scratch);
  } else if (r.IsInteger32()) {
    Register input_reg = ToRegister(instr->input());
    __ test(input_reg, Operand(input_reg));
    Label is_positive;
    __ j(not_sign, &is_positive);
    __ neg(input_reg);
    __ test(input_reg, Operand(input_reg));
    DeoptimizeIf(negative, instr->environment());
    __ bind(&is_positive);
  } else {  // Tagged case.
    DeferredMathAbsTaggedHeapNumber* deferred =
        new DeferredMathAbsTaggedHeapNumber(this, instr);
    Label not_smi;
    Register input_reg = ToRegister(instr->input());
    // Smi check.
    __ test(input_reg, Immediate(kSmiTagMask));
    __ j(not_zero, deferred->entry());
    __ test(input_reg, Operand(input_reg));
    Label is_positive;
    __ j(not_sign, &is_positive);
    __ neg(input_reg);

    __ test(input_reg, Operand(input_reg));
    DeoptimizeIf(negative, instr->environment());

    __ bind(&is_positive);
    __ bind(deferred->exit());
  }
}


void LCodeGen::DoMathFloor(LUnaryMathOperation* instr) {
  XMMRegister xmm_scratch = xmm0;
  Register output_reg = ToRegister(instr->result());
  XMMRegister input_reg = ToDoubleRegister(instr->input());
  __ xorpd(xmm_scratch, xmm_scratch);  // Zero the register.
  __ ucomisd(input_reg, xmm_scratch);

  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    DeoptimizeIf(below_equal, instr->environment());
  } else {
    DeoptimizeIf(below, instr->environment());
  }

  // Use truncating instruction (OK because input is positive).
  __ cvttsd2si(output_reg, Operand(input_reg));

  // Overflow is signalled with minint.
  __ cmp(output_reg, 0x80000000u);
  DeoptimizeIf(equal, instr->environment());
}


void LCodeGen::DoMathRound(LUnaryMathOperation* instr) {
  XMMRegister xmm_scratch = xmm0;
  Register output_reg = ToRegister(instr->result());
  XMMRegister input_reg = ToDoubleRegister(instr->input());

  // xmm_scratch = 0.5
  ExternalReference one_half = ExternalReference::address_of_one_half();
  __ movdbl(xmm_scratch, Operand::StaticVariable(one_half));

  // input = input + 0.5
  __ addsd(input_reg, xmm_scratch);

  // We need to return -0 for the input range [-0.5, 0[, otherwise
  // compute Math.floor(value + 0.5).
  if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
    __ ucomisd(input_reg, xmm_scratch);
    DeoptimizeIf(below_equal, instr->environment());
  } else {
    // If we don't need to bailout on -0, we check only bailout
    // on negative inputs.
    __ xorpd(xmm_scratch, xmm_scratch);  // Zero the register.
    __ ucomisd(input_reg, xmm_scratch);
    DeoptimizeIf(below, instr->environment());
  }

  // Compute Math.floor(value + 0.5).
  // Use truncating instruction (OK because input is positive).
  __ cvttsd2si(output_reg, Operand(input_reg));

  // Overflow is signalled with minint.
  __ cmp(output_reg, 0x80000000u);
  DeoptimizeIf(equal, instr->environment());
}


void LCodeGen::DoMathSqrt(LUnaryMathOperation* instr) {
  XMMRegister input_reg = ToDoubleRegister(instr->input());
  ASSERT(ToDoubleRegister(instr->result()).is(input_reg));
  __ sqrtsd(input_reg, input_reg);
}


void LCodeGen::DoMathPowHalf(LUnaryMathOperation* instr) {
  XMMRegister xmm_scratch = xmm0;
  XMMRegister input_reg = ToDoubleRegister(instr->input());
  ASSERT(ToDoubleRegister(instr->result()).is(input_reg));
  ExternalReference negative_infinity =
      ExternalReference::address_of_negative_infinity();
  __ movdbl(xmm_scratch, Operand::StaticVariable(negative_infinity));
  __ ucomisd(xmm_scratch, input_reg);
  DeoptimizeIf(equal, instr->environment());
  __ sqrtsd(input_reg, input_reg);
}


void LCodeGen::DoPower(LPower* instr) {
  LOperand* left = instr->left();
  LOperand* right = instr->right();
  DoubleRegister result_reg = ToDoubleRegister(instr->result());
  Representation exponent_type = instr->hydrogen()->right()->representation();
  if (exponent_type.IsDouble()) {
    // It is safe to use ebx directly since the instruction is marked
    // as a call.
    __ PrepareCallCFunction(4, ebx);
    __ movdbl(Operand(esp, 0 * kDoubleSize), ToDoubleRegister(left));
    __ movdbl(Operand(esp, 1 * kDoubleSize), ToDoubleRegister(right));
    __ CallCFunction(ExternalReference::power_double_double_function(), 4);
  } else if (exponent_type.IsInteger32()) {
    // It is safe to use ebx directly since the instruction is marked
    // as a call.
    ASSERT(!ToRegister(right).is(ebx));
    __ PrepareCallCFunction(4, ebx);
    __ movdbl(Operand(esp, 0 * kDoubleSize), ToDoubleRegister(left));
    __ mov(Operand(esp, 1 * kDoubleSize), ToRegister(right));
    __ CallCFunction(ExternalReference::power_double_int_function(), 4);
  } else {
    ASSERT(exponent_type.IsTagged());
    CpuFeatures::Scope scope(SSE2);
    Register right_reg = ToRegister(right);

    Label non_smi, call;
    __ test(right_reg, Immediate(kSmiTagMask));
    __ j(not_zero, &non_smi);
    __ SmiUntag(right_reg);
    __ cvtsi2sd(result_reg, Operand(right_reg));
    __ jmp(&call);

    __ bind(&non_smi);
    // It is safe to use ebx directly since the instruction is marked
    // as a call.
    ASSERT(!right_reg.is(ebx));
    __ CmpObjectType(right_reg, HEAP_NUMBER_TYPE , ebx);
    DeoptimizeIf(not_equal, instr->environment());
    __ movdbl(result_reg, FieldOperand(right_reg, HeapNumber::kValueOffset));

    __ bind(&call);
    __ PrepareCallCFunction(4, ebx);
    __ movdbl(Operand(esp, 0 * kDoubleSize), ToDoubleRegister(left));
    __ movdbl(Operand(esp, 1 * kDoubleSize), result_reg);
    __ CallCFunction(ExternalReference::power_double_double_function(), 4);
  }

  // Return value is in st(0) on ia32.
  // Store it into the (fixed) result register.
  __ sub(Operand(esp), Immediate(kDoubleSize));
  __ fstp_d(Operand(esp, 0));
  __ movdbl(result_reg, Operand(esp, 0));
  __ add(Operand(esp), Immediate(kDoubleSize));
}


void LCodeGen::DoMathLog(LUnaryMathOperation* instr) {
  ASSERT(ToDoubleRegister(instr->result()).is(xmm1));
  TranscendentalCacheStub stub(TranscendentalCache::LOG,
                               TranscendentalCacheStub::UNTAGGED);
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoMathCos(LUnaryMathOperation* instr) {
  ASSERT(ToDoubleRegister(instr->result()).is(xmm1));
  TranscendentalCacheStub stub(TranscendentalCache::COS,
                               TranscendentalCacheStub::UNTAGGED);
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoMathSin(LUnaryMathOperation* instr) {
  ASSERT(ToDoubleRegister(instr->result()).is(xmm1));
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
      UNREACHABLE();
  }
}


void LCodeGen::DoCallKeyed(LCallKeyed* instr) {
  ASSERT(ToRegister(instr->result()).is(eax));

  int arity = instr->arity();
  Handle<Code> ic = StubCache::ComputeKeyedCallInitialize(arity, NOT_IN_LOOP);
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallNamed(LCallNamed* instr) {
  ASSERT(ToRegister(instr->result()).is(eax));

  int arity = instr->arity();
  Handle<Code> ic = StubCache::ComputeCallInitialize(arity, NOT_IN_LOOP);
  __ mov(ecx, instr->name());
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallFunction(LCallFunction* instr) {
  ASSERT(ToRegister(instr->result()).is(eax));

  int arity = instr->arity();
  CallFunctionStub stub(arity, NOT_IN_LOOP, RECEIVER_MIGHT_BE_VALUE);
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
  __ Drop(1);
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallGlobal(LCallGlobal* instr) {
  ASSERT(ToRegister(instr->result()).is(eax));

  int arity = instr->arity();
  Handle<Code> ic = StubCache::ComputeCallInitialize(arity, NOT_IN_LOOP);
  __ mov(ecx, instr->name());
  CallCode(ic, RelocInfo::CODE_TARGET_CONTEXT, instr);
  __ mov(esi, Operand(ebp, StandardFrameConstants::kContextOffset));
}


void LCodeGen::DoCallKnownGlobal(LCallKnownGlobal* instr) {
  ASSERT(ToRegister(instr->result()).is(eax));
  __ mov(edi, instr->target());
  CallKnownFunction(instr->target(), instr->arity(), instr);
}


void LCodeGen::DoCallNew(LCallNew* instr) {
  ASSERT(ToRegister(instr->input()).is(edi));
  ASSERT(ToRegister(instr->result()).is(eax));

  Handle<Code> builtin(Builtins::builtin(Builtins::JSConstructCall));
  __ Set(eax, Immediate(instr->arity()));
  CallCode(builtin, RelocInfo::CONSTRUCT_CALL, instr);
}


void LCodeGen::DoCallRuntime(LCallRuntime* instr) {
  CallRuntime(instr->function(), instr->arity(), instr);
}


void LCodeGen::DoStoreNamedField(LStoreNamedField* instr) {
  Register object = ToRegister(instr->object());
  Register value = ToRegister(instr->value());
  int offset = instr->offset();

  if (!instr->transition().is_null()) {
    __ mov(FieldOperand(object, HeapObject::kMapOffset), instr->transition());
  }

  // Do the store.
  if (instr->is_in_object()) {
    __ mov(FieldOperand(object, offset), value);
    if (instr->needs_write_barrier()) {
      Register temp = ToRegister(instr->temp());
      // Update the write barrier for the object for in-object properties.
      __ RecordWrite(object, offset, value, temp);
    }
  } else {
    Register temp = ToRegister(instr->temp());
    __ mov(temp, FieldOperand(object, JSObject::kPropertiesOffset));
    __ mov(FieldOperand(temp, offset), value);
    if (instr->needs_write_barrier()) {
      // Update the write barrier for the properties array.
      // object is used as a scratch register.
      __ RecordWrite(temp, offset, value, object);
    }
  }
}


void LCodeGen::DoStoreNamedGeneric(LStoreNamedGeneric* instr) {
  ASSERT(ToRegister(instr->object()).is(edx));
  ASSERT(ToRegister(instr->value()).is(eax));

  __ mov(ecx, instr->name());
  Handle<Code> ic(Builtins::builtin(Builtins::StoreIC_Initialize));
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoBoundsCheck(LBoundsCheck* instr) {
  __ cmp(ToRegister(instr->index()), ToOperand(instr->length()));
  DeoptimizeIf(above_equal, instr->environment());
}


void LCodeGen::DoStoreKeyedFastElement(LStoreKeyedFastElement* instr) {
  Register value = ToRegister(instr->value());
  Register elements = ToRegister(instr->object());
  Register key = instr->key()->IsRegister() ? ToRegister(instr->key()) : no_reg;

  // Do the store.
  if (instr->key()->IsConstantOperand()) {
    ASSERT(!instr->hydrogen()->NeedsWriteBarrier());
    LConstantOperand* const_operand = LConstantOperand::cast(instr->key());
    int offset =
        ToInteger32(const_operand) * kPointerSize + FixedArray::kHeaderSize;
    __ mov(FieldOperand(elements, offset), value);
  } else {
    __ mov(FieldOperand(elements, key, times_4, FixedArray::kHeaderSize),
           value);
  }

  if (instr->hydrogen()->NeedsWriteBarrier()) {
    // Compute address of modified element and store it into key register.
    __ lea(key, FieldOperand(elements, key, times_4, FixedArray::kHeaderSize));
    __ RecordWrite(elements, key, value);
  }
}


void LCodeGen::DoStoreKeyedGeneric(LStoreKeyedGeneric* instr) {
  ASSERT(ToRegister(instr->object()).is(edx));
  ASSERT(ToRegister(instr->key()).is(ecx));
  ASSERT(ToRegister(instr->value()).is(eax));

  Handle<Code> ic(Builtins::builtin(Builtins::KeyedStoreIC_Initialize));
  CallCode(ic, RelocInfo::CODE_TARGET, instr);
}


void LCodeGen::DoInteger32ToDouble(LInteger32ToDouble* instr) {
  LOperand* input = instr->input();
  ASSERT(input->IsRegister() || input->IsStackSlot());
  LOperand* output = instr->result();
  ASSERT(output->IsDoubleRegister());
  __ cvtsi2sd(ToDoubleRegister(output), ToOperand(input));
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
  __ SmiTag(reg);
  __ j(overflow, deferred->entry());
  __ bind(deferred->exit());
}


void LCodeGen::DoDeferredNumberTagI(LNumberTagI* instr) {
  Label slow;
  Register reg = ToRegister(instr->input());
  Register tmp = reg.is(eax) ? ecx : eax;

  // Preserve the value of all registers.
  __ PushSafepointRegisters();

  // There was overflow, so bits 30 and 31 of the original integer
  // disagree. Try to allocate a heap number in new space and store
  // the value in there. If that fails, call the runtime system.
  NearLabel done;
  __ SmiUntag(reg);
  __ xor_(reg, 0x80000000);
  __ cvtsi2sd(xmm0, Operand(reg));
  if (FLAG_inline_new) {
    __ AllocateHeapNumber(reg, tmp, no_reg, &slow);
    __ jmp(&done);
  }

  // Slow case: Call the runtime system to do the number allocation.
  __ bind(&slow);

  // TODO(3095996): Put a valid pointer value in the stack slot where the result
  // register is stored, as this register is in the pointer map, but contains an
  // integer value.
  __ mov(Operand(esp, EspIndexForPushAll(reg) * kPointerSize), Immediate(0));

  __ CallRuntimeSaveDoubles(Runtime::kAllocateHeapNumber);
  RecordSafepointWithRegisters(
      instr->pointer_map(), 0, Safepoint::kNoDeoptimizationIndex);
  if (!reg.is(eax)) __ mov(reg, eax);

  // Done. Put the value in xmm0 into the value of the allocated heap
  // number.
  __ bind(&done);
  __ movdbl(FieldOperand(reg, HeapNumber::kValueOffset), xmm0);
  __ mov(Operand(esp, EspIndexForPushAll(reg) * kPointerSize), reg);
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

  XMMRegister input_reg = ToDoubleRegister(instr->input());
  Register reg = ToRegister(instr->result());
  Register tmp = ToRegister(instr->temp());

  DeferredNumberTagD* deferred = new DeferredNumberTagD(this, instr);
  if (FLAG_inline_new) {
    __ AllocateHeapNumber(reg, tmp, no_reg, deferred->entry());
  } else {
    __ jmp(deferred->entry());
  }
  __ bind(deferred->exit());
  __ movdbl(FieldOperand(reg, HeapNumber::kValueOffset), input_reg);
}


void LCodeGen::DoDeferredNumberTagD(LNumberTagD* instr) {
  // TODO(3095996): Get rid of this. For now, we need to make the
  // result register contain a valid pointer because it is already
  // contained in the register pointer map.
  Register reg = ToRegister(instr->result());
  __ Set(reg, Immediate(0));

  __ PushSafepointRegisters();
  __ CallRuntimeSaveDoubles(Runtime::kAllocateHeapNumber);
  RecordSafepointWithRegisters(
      instr->pointer_map(), 0, Safepoint::kNoDeoptimizationIndex);
  __ mov(Operand(esp, EspIndexForPushAll(reg) * kPointerSize), eax);
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
    __ test(ToRegister(input), Immediate(kSmiTagMask));
    DeoptimizeIf(not_zero, instr->environment());
  }
  __ SmiUntag(ToRegister(input));
}


void LCodeGen::EmitNumberUntagD(Register input_reg,
                                XMMRegister result_reg,
                                LEnvironment* env) {
  NearLabel load_smi, heap_number, done;

  // Smi check.
  __ test(input_reg, Immediate(kSmiTagMask));
  __ j(zero, &load_smi, not_taken);

  // Heap number map check.
  __ cmp(FieldOperand(input_reg, HeapObject::kMapOffset),
         Factory::heap_number_map());
  __ j(equal, &heap_number);

  __ cmp(input_reg, Factory::undefined_value());
  DeoptimizeIf(not_equal, env);

  // Convert undefined to NaN.
  __ push(input_reg);
  __ mov(input_reg, Factory::nan_value());
  __ movdbl(result_reg, FieldOperand(input_reg, HeapNumber::kValueOffset));
  __ pop(input_reg);
  __ jmp(&done);

  // Heap number to XMM conversion.
  __ bind(&heap_number);
  __ movdbl(result_reg, FieldOperand(input_reg, HeapNumber::kValueOffset));
  __ jmp(&done);

  // Smi to XMM conversion
  __ bind(&load_smi);
  __ SmiUntag(input_reg);  // Untag smi before converting to float.
  __ cvtsi2sd(result_reg, Operand(input_reg));
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
  NearLabel done, heap_number;
  Register input_reg = ToRegister(instr->input());

  // Heap number map check.
  __ cmp(FieldOperand(input_reg, HeapObject::kMapOffset),
         Factory::heap_number_map());

  if (instr->truncating()) {
    __ j(equal, &heap_number);
    // Check for undefined. Undefined is converted to zero for truncating
    // conversions.
    __ cmp(input_reg, Factory::undefined_value());
    DeoptimizeIf(not_equal, instr->environment());
    __ mov(input_reg, 0);
    __ jmp(&done);

    __ bind(&heap_number);
    if (CpuFeatures::IsSupported(SSE3)) {
      CpuFeatures::Scope scope(SSE3);
      NearLabel convert;
      // Use more powerful conversion when sse3 is available.
      // Load x87 register with heap number.
      __ fld_d(FieldOperand(input_reg, HeapNumber::kValueOffset));
      // Get exponent alone and check for too-big exponent.
      __ mov(input_reg, FieldOperand(input_reg, HeapNumber::kExponentOffset));
      __ and_(input_reg, HeapNumber::kExponentMask);
      const uint32_t kTooBigExponent =
          (HeapNumber::kExponentBias + 63) << HeapNumber::kExponentShift;
      __ cmp(Operand(input_reg), Immediate(kTooBigExponent));
      __ j(less, &convert);
      // Pop FPU stack before deoptimizing.
      __ ffree(0);
      __ fincstp();
      DeoptimizeIf(no_condition, instr->environment());

      // Reserve space for 64 bit answer.
      __ bind(&convert);
      __ sub(Operand(esp), Immediate(kDoubleSize));
      // Do conversion, which cannot fail because we checked the exponent.
      __ fisttp_d(Operand(esp, 0));
      __ mov(input_reg, Operand(esp, 0));  // Low word of answer is the result.
      __ add(Operand(esp), Immediate(kDoubleSize));
    } else {
      NearLabel deopt;
      XMMRegister xmm_temp = ToDoubleRegister(instr->temp());
      __ movdbl(xmm0, FieldOperand(input_reg, HeapNumber::kValueOffset));
      __ cvttsd2si(input_reg, Operand(xmm0));
      __ cmp(input_reg, 0x80000000u);
      __ j(not_equal, &done);
      // Check if the input was 0x8000000 (kMinInt).
      // If no, then we got an overflow and we deoptimize.
      ExternalReference min_int = ExternalReference::address_of_min_int();
      __ movdbl(xmm_temp, Operand::StaticVariable(min_int));
      __ ucomisd(xmm_temp, xmm0);
      DeoptimizeIf(not_equal, instr->environment());
      DeoptimizeIf(parity_even, instr->environment());  // NaN.
    }
  } else {
    // Deoptimize if we don't have a heap number.
    DeoptimizeIf(not_equal, instr->environment());

    XMMRegister xmm_temp = ToDoubleRegister(instr->temp());
    __ movdbl(xmm0, FieldOperand(input_reg, HeapNumber::kValueOffset));
    __ cvttsd2si(input_reg, Operand(xmm0));
    __ cvtsi2sd(xmm_temp, Operand(input_reg));
    __ ucomisd(xmm0, xmm_temp);
    DeoptimizeIf(not_equal, instr->environment());
    DeoptimizeIf(parity_even, instr->environment());  // NaN.
    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      __ test(input_reg, Operand(input_reg));
      __ j(not_zero, &done);
      __ movmskpd(input_reg, xmm0);
      __ and_(input_reg, 1);
      DeoptimizeIf(not_zero, instr->environment());
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
  __ test(input_reg, Immediate(kSmiTagMask));
  __ j(not_zero, deferred->entry());

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
  XMMRegister result_reg = ToDoubleRegister(result);

  EmitNumberUntagD(input_reg, result_reg, instr->environment());
}


void LCodeGen::DoDoubleToI(LDoubleToI* instr) {
  LOperand* input = instr->input();
  ASSERT(input->IsDoubleRegister());
  LOperand* result = instr->result();
  ASSERT(result->IsRegister());

  XMMRegister input_reg = ToDoubleRegister(input);
  Register result_reg = ToRegister(result);

  if (instr->truncating()) {
    // Performs a truncating conversion of a floating point number as used by
    // the JS bitwise operations.
    __ cvttsd2si(result_reg, Operand(input_reg));
    __ cmp(result_reg, 0x80000000u);
    if (CpuFeatures::IsSupported(SSE3)) {
      // This will deoptimize if the exponent of the input in out of range.
      CpuFeatures::Scope scope(SSE3);
      NearLabel convert, done;
      __ j(not_equal, &done);
      __ sub(Operand(esp), Immediate(kDoubleSize));
      __ movdbl(Operand(esp, 0), input_reg);
      // Get exponent alone and check for too-big exponent.
      __ mov(result_reg, Operand(esp, sizeof(int32_t)));
      __ and_(result_reg, HeapNumber::kExponentMask);
      const uint32_t kTooBigExponent =
          (HeapNumber::kExponentBias + 63) << HeapNumber::kExponentShift;
      __ cmp(Operand(result_reg), Immediate(kTooBigExponent));
      __ j(less, &convert);
      __ add(Operand(esp), Immediate(kDoubleSize));
      DeoptimizeIf(no_condition, instr->environment());
      __ bind(&convert);
      // Do conversion, which cannot fail because we checked the exponent.
      __ fld_d(Operand(esp, 0));
      __ fisttp_d(Operand(esp, 0));
      __ mov(result_reg, Operand(esp, 0));  // Low word of answer is the result.
      __ add(Operand(esp), Immediate(kDoubleSize));
      __ bind(&done);
    } else {
      NearLabel done;
      Register temp_reg = ToRegister(instr->temporary());
      XMMRegister xmm_scratch = xmm0;

      // If cvttsd2si succeeded, we're done. Otherwise, we attempt
      // manual conversion.
      __ j(not_equal, &done);

      // Get high 32 bits of the input in result_reg and temp_reg.
      __ pshufd(xmm_scratch, input_reg, 1);
      __ movd(Operand(temp_reg), xmm_scratch);
      __ mov(result_reg, temp_reg);

      // Prepare negation mask in temp_reg.
      __ sar(temp_reg, kBitsPerInt - 1);

      // Extract the exponent from result_reg and subtract adjusted
      // bias from it. The adjustment is selected in a way such that
      // when the difference is zero, the answer is in the low 32 bits
      // of the input, otherwise a shift has to be performed.
      __ shr(result_reg, HeapNumber::kExponentShift);
      __ and_(result_reg,
              HeapNumber::kExponentMask >> HeapNumber::kExponentShift);
      __ sub(Operand(result_reg),
             Immediate(HeapNumber::kExponentBias +
                       HeapNumber::kExponentBits +
                       HeapNumber::kMantissaBits));
      // Don't handle big (> kMantissaBits + kExponentBits == 63) or
      // special exponents.
      DeoptimizeIf(greater, instr->environment());

      // Zero out the sign and the exponent in the input (by shifting
      // it to the left) and restore the implicit mantissa bit,
      // i.e. convert the input to unsigned int64 shifted left by
      // kExponentBits.
      ExternalReference minus_zero = ExternalReference::address_of_minus_zero();
      // Minus zero has the most significant bit set and the other
      // bits cleared.
      __ movdbl(xmm_scratch, Operand::StaticVariable(minus_zero));
      __ psllq(input_reg, HeapNumber::kExponentBits);
      __ por(input_reg, xmm_scratch);

      // Get the amount to shift the input right in xmm_scratch.
      __ neg(result_reg);
      __ movd(xmm_scratch, Operand(result_reg));

      // Shift the input right and extract low 32 bits.
      __ psrlq(input_reg, xmm_scratch);
      __ movd(Operand(result_reg), input_reg);

      // Use the prepared mask in temp_reg to negate the result if necessary.
      __ xor_(result_reg, Operand(temp_reg));
      __ sub(result_reg, Operand(temp_reg));
      __ bind(&done);
    }
  } else {
    NearLabel done;
    __ cvttsd2si(result_reg, Operand(input_reg));
    __ cvtsi2sd(xmm0, Operand(result_reg));
    __ ucomisd(xmm0, input_reg);
    DeoptimizeIf(not_equal, instr->environment());
    DeoptimizeIf(parity_even, instr->environment());  // NaN.
    if (instr->hydrogen()->CheckFlag(HValue::kBailoutOnMinusZero)) {
      // The integer converted back is equal to the original. We
      // only have to test if we got -0 as an input.
      __ test(result_reg, Operand(result_reg));
      __ j(not_zero, &done);
      __ movmskpd(result_reg, input_reg);
      // Bit 0 contains the sign of the double in input_reg.
      // If input was positive, we are ok and return 0, otherwise
      // deoptimize.
      __ and_(result_reg, 1);
      DeoptimizeIf(not_zero, instr->environment());
    }
    __ bind(&done);
  }
}


void LCodeGen::DoCheckSmi(LCheckSmi* instr) {
  LOperand* input = instr->input();
  ASSERT(input->IsRegister());
  __ test(ToRegister(input), Immediate(kSmiTagMask));
  DeoptimizeIf(instr->condition(), instr->environment());
}


void LCodeGen::DoCheckInstanceType(LCheckInstanceType* instr) {
  Register input = ToRegister(instr->input());
  Register temp = ToRegister(instr->temp());
  InstanceType first = instr->hydrogen()->first();
  InstanceType last = instr->hydrogen()->last();

  __ mov(temp, FieldOperand(input, HeapObject::kMapOffset));
  __ cmpb(FieldOperand(temp, Map::kInstanceTypeOffset),
          static_cast<int8_t>(first));

  // If there is only one type in the interval check for equality.
  if (first == last) {
    DeoptimizeIf(not_equal, instr->environment());
  } else {
    DeoptimizeIf(below, instr->environment());
    // Omit check for the last type.
    if (last != LAST_TYPE) {
      __ cmpb(FieldOperand(temp, Map::kInstanceTypeOffset),
              static_cast<int8_t>(last));
      DeoptimizeIf(above, instr->environment());
    }
  }
}


void LCodeGen::DoCheckFunction(LCheckFunction* instr) {
  ASSERT(instr->input()->IsRegister());
  Register reg = ToRegister(instr->input());
  __ cmp(reg, instr->hydrogen()->target());
  DeoptimizeIf(not_equal, instr->environment());
}


void LCodeGen::DoCheckMap(LCheckMap* instr) {
  LOperand* input = instr->input();
  ASSERT(input->IsRegister());
  Register reg = ToRegister(input);
  __ cmp(FieldOperand(reg, HeapObject::kMapOffset),
         instr->hydrogen()->map());
  DeoptimizeIf(not_equal, instr->environment());
}


void LCodeGen::LoadHeapObject(Register result, Handle<HeapObject> object) {
  if (Heap::InNewSpace(*object)) {
    Handle<JSGlobalPropertyCell> cell =
        Factory::NewJSGlobalPropertyCell(object);
    __ mov(result, Operand::Cell(cell));
  } else {
    __ mov(result, object);
  }
}


void LCodeGen::DoCheckPrototypeMaps(LCheckPrototypeMaps* instr) {
  Register reg = ToRegister(instr->temp());

  Handle<JSObject> holder = instr->holder();
  Handle<JSObject> current_prototype = instr->prototype();

  // Load prototype object.
  LoadHeapObject(reg, current_prototype);

  // Check prototype maps up to the holder.
  while (!current_prototype.is_identical_to(holder)) {
    __ cmp(FieldOperand(reg, HeapObject::kMapOffset),
           Handle<Map>(current_prototype->map()));
    DeoptimizeIf(not_equal, instr->environment());
    current_prototype =
        Handle<JSObject>(JSObject::cast(current_prototype->GetPrototype()));
    // Load next prototype object.
    LoadHeapObject(reg, current_prototype);
  }

  // Check the holder map.
  __ cmp(FieldOperand(reg, HeapObject::kMapOffset),
         Handle<Map>(current_prototype->map()));
  DeoptimizeIf(not_equal, instr->environment());
}


void LCodeGen::DoArrayLiteral(LArrayLiteral* instr) {
  // Setup the parameters to the stub/runtime call.
  __ mov(eax, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ push(FieldOperand(eax, JSFunction::kLiteralsOffset));
  __ push(Immediate(Smi::FromInt(instr->hydrogen()->literal_index())));
  __ push(Immediate(instr->hydrogen()->constant_elements()));

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
  // Setup the parameters to the stub/runtime call.
  __ mov(eax, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ push(FieldOperand(eax, JSFunction::kLiteralsOffset));
  __ push(Immediate(Smi::FromInt(instr->hydrogen()->literal_index())));
  __ push(Immediate(instr->hydrogen()->constant_properties()));
  __ push(Immediate(Smi::FromInt(instr->hydrogen()->fast_elements() ? 1 : 0)));

  // Pick the right runtime function to call.
  if (instr->hydrogen()->depth() > 1) {
    CallRuntime(Runtime::kCreateObjectLiteral, 4, instr);
  } else {
    CallRuntime(Runtime::kCreateObjectLiteralShallow, 4, instr);
  }
}


void LCodeGen::DoRegExpLiteral(LRegExpLiteral* instr) {
  NearLabel materialized;
  // Registers will be used as follows:
  // edi = JS function.
  // ecx = literals array.
  // ebx = regexp literal.
  // eax = regexp literal clone.
  __ mov(edi, Operand(ebp, JavaScriptFrameConstants::kFunctionOffset));
  __ mov(ecx, FieldOperand(edi, JSFunction::kLiteralsOffset));
  int literal_offset = FixedArray::kHeaderSize +
      instr->hydrogen()->literal_index() * kPointerSize;
  __ mov(ebx, FieldOperand(ecx, literal_offset));
  __ cmp(ebx, Factory::undefined_value());
  __ j(not_equal, &materialized);

  // Create regexp literal using runtime function
  // Result will be in eax.
  __ push(ecx);
  __ push(Immediate(Smi::FromInt(instr->hydrogen()->literal_index())));
  __ push(Immediate(instr->hydrogen()->pattern()));
  __ push(Immediate(instr->hydrogen()->flags()));
  CallRuntime(Runtime::kMaterializeRegExpLiteral, 4, instr);
  __ mov(ebx, eax);

  __ bind(&materialized);
  int size = JSRegExp::kSize + JSRegExp::kInObjectFieldCount * kPointerSize;
  Label allocated, runtime_allocate;
  __ AllocateInNewSpace(size, eax, ecx, edx, &runtime_allocate, TAG_OBJECT);
  __ jmp(&allocated);

  __ bind(&runtime_allocate);
  __ push(ebx);
  __ push(Immediate(Smi::FromInt(size)));
  CallRuntime(Runtime::kAllocateInNewSpace, 1, instr);
  __ pop(ebx);

  __ bind(&allocated);
  // Copy the content into the newly allocated memory.
  // (Unroll copy loop once for better throughput).
  for (int i = 0; i < size - kPointerSize; i += 2 * kPointerSize) {
    __ mov(edx, FieldOperand(ebx, i));
    __ mov(ecx, FieldOperand(ebx, i + kPointerSize));
    __ mov(FieldOperand(eax, i), edx);
    __ mov(FieldOperand(eax, i + kPointerSize), ecx);
  }
  if ((size % (2 * kPointerSize)) != 0) {
    __ mov(edx, FieldOperand(ebx, size - kPointerSize));
    __ mov(FieldOperand(eax, size - kPointerSize), edx);
  }
}


void LCodeGen::DoFunctionLiteral(LFunctionLiteral* instr) {
  // Use the fast case closure allocation code that allocates in new
  // space for nested functions that don't need literals cloning.
  Handle<SharedFunctionInfo> shared_info = instr->shared_info();
  bool pretenure = !instr->hydrogen()->pretenure();
  if (shared_info->num_literals() == 0 && !pretenure) {
    FastNewClosureStub stub;
    __ push(Immediate(shared_info));
    CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
  } else {
    __ push(esi);
    __ push(Immediate(shared_info));
    __ push(Immediate(pretenure
                      ? Factory::true_value()
                      : Factory::false_value()));
    CallRuntime(Runtime::kNewClosure, 3, instr);
  }
}


void LCodeGen::DoTypeof(LTypeof* instr) {
  LOperand* input = instr->input();
  if (input->IsConstantOperand()) {
    __ push(ToImmediate(input));
  } else {
    __ push(ToOperand(input));
  }
  CallRuntime(Runtime::kTypeof, 1, instr);
}


void LCodeGen::DoTypeofIs(LTypeofIs* instr) {
  Register input = ToRegister(instr->input());
  Register result = ToRegister(instr->result());
  Label true_label;
  Label false_label;
  NearLabel done;

  Condition final_branch_condition = EmitTypeofIs(&true_label,
                                                  &false_label,
                                                  input,
                                                  instr->type_literal());
  __ j(final_branch_condition, &true_label);
  __ bind(&false_label);
  __ mov(result, Handle<Object>(Heap::false_value()));
  __ jmp(&done);

  __ bind(&true_label);
  __ mov(result, Handle<Object>(Heap::true_value()));

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
  if (type_name->Equals(Heap::number_symbol())) {
    __ test(input, Immediate(kSmiTagMask));
    __ j(zero, true_label);
    __ cmp(FieldOperand(input, HeapObject::kMapOffset),
           Factory::heap_number_map());
    final_branch_condition = equal;

  } else if (type_name->Equals(Heap::string_symbol())) {
    __ test(input, Immediate(kSmiTagMask));
    __ j(zero, false_label);
    __ mov(input, FieldOperand(input, HeapObject::kMapOffset));
    __ test_b(FieldOperand(input, Map::kBitFieldOffset),
              1 << Map::kIsUndetectable);
    __ j(not_zero, false_label);
    __ CmpInstanceType(input, FIRST_NONSTRING_TYPE);
    final_branch_condition = below;

  } else if (type_name->Equals(Heap::boolean_symbol())) {
    __ cmp(input, Handle<Object>(Heap::true_value()));
    __ j(equal, true_label);
    __ cmp(input, Handle<Object>(Heap::false_value()));
    final_branch_condition = equal;

  } else if (type_name->Equals(Heap::undefined_symbol())) {
    __ cmp(input, Factory::undefined_value());
    __ j(equal, true_label);
    __ test(input, Immediate(kSmiTagMask));
    __ j(zero, false_label);
    // Check for undetectable objects => true.
    __ mov(input, FieldOperand(input, HeapObject::kMapOffset));
    __ test_b(FieldOperand(input, Map::kBitFieldOffset),
              1 << Map::kIsUndetectable);
    final_branch_condition = not_zero;

  } else if (type_name->Equals(Heap::function_symbol())) {
    __ test(input, Immediate(kSmiTagMask));
    __ j(zero, false_label);
    __ CmpObjectType(input, JS_FUNCTION_TYPE, input);
    __ j(equal, true_label);
    // Regular expressions => 'function' (they are callable).
    __ CmpInstanceType(input, JS_REGEXP_TYPE);
    final_branch_condition = equal;

  } else if (type_name->Equals(Heap::object_symbol())) {
    __ test(input, Immediate(kSmiTagMask));
    __ j(zero, false_label);
    __ cmp(input, Factory::null_value());
    __ j(equal, true_label);
    // Regular expressions => 'function', not 'object'.
    __ CmpObjectType(input, JS_REGEXP_TYPE, input);
    __ j(equal, false_label);
    // Check for undetectable objects => false.
    __ test_b(FieldOperand(input, Map::kBitFieldOffset),
              1 << Map::kIsUndetectable);
    __ j(not_zero, false_label);
    // Check for JS objects => true.
    __ CmpInstanceType(input, FIRST_JS_OBJECT_TYPE);
    __ j(below, false_label);
    __ CmpInstanceType(input, LAST_JS_OBJECT_TYPE);
    final_branch_condition = below_equal;

  } else {
    final_branch_condition = not_equal;
    __ jmp(false_label);
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
  LOperand* obj = instr->object();
  LOperand* key = instr->key();
  __ push(ToOperand(obj));
  if (key->IsConstantOperand()) {
    __ push(ToImmediate(key));
  } else {
    __ push(ToOperand(key));
  }
  RecordPosition(instr->pointer_map()->position());
  SafepointGenerator safepoint_generator(this,
                                         instr->pointer_map(),
                                         Safepoint::kNoDeoptimizationIndex);
  __ InvokeBuiltin(Builtins::DELETE, CALL_FUNCTION, &safepoint_generator);
}


void LCodeGen::DoStackCheck(LStackCheck* instr) {
  // Perform stack overflow check.
  NearLabel done;
  ExternalReference stack_limit = ExternalReference::address_of_stack_limit();
  __ cmp(esp, Operand::StaticVariable(stack_limit));
  __ j(above_equal, &done);

  StackCheckStub stub;
  CallCode(stub.GetCode(), RelocInfo::CODE_TARGET, instr);
  __ bind(&done);
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
  RegisterEnvironmentForDeoptimization(environment);
  ASSERT(osr_pc_offset_ == -1);
  osr_pc_offset_ = masm()->pc_offset();
}


#undef __

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32
