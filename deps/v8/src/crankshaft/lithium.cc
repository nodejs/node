// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/lithium.h"

#include "src/ast/scopes.h"
#include "src/codegen.h"
#include "src/objects-inl.h"

#if V8_TARGET_ARCH_IA32
#include "src/crankshaft/ia32/lithium-ia32.h"  // NOLINT
#include "src/crankshaft/ia32/lithium-codegen-ia32.h"  // NOLINT
#elif V8_TARGET_ARCH_X64
#include "src/crankshaft/x64/lithium-x64.h"  // NOLINT
#include "src/crankshaft/x64/lithium-codegen-x64.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM
#include "src/crankshaft/arm/lithium-arm.h"  // NOLINT
#include "src/crankshaft/arm/lithium-codegen-arm.h"  // NOLINT
#elif V8_TARGET_ARCH_PPC
#include "src/crankshaft/ppc/lithium-ppc.h"          // NOLINT
#include "src/crankshaft/ppc/lithium-codegen-ppc.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS
#include "src/crankshaft/mips/lithium-mips.h"  // NOLINT
#include "src/crankshaft/mips/lithium-codegen-mips.h"  // NOLINT
#elif V8_TARGET_ARCH_ARM64
#include "src/crankshaft/arm64/lithium-arm64.h"  // NOLINT
#include "src/crankshaft/arm64/lithium-codegen-arm64.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS64
#include "src/crankshaft/mips64/lithium-mips64.h"  // NOLINT
#include "src/crankshaft/mips64/lithium-codegen-mips64.h"  // NOLINT
#elif V8_TARGET_ARCH_X87
#include "src/crankshaft/x87/lithium-x87.h"  // NOLINT
#include "src/crankshaft/x87/lithium-codegen-x87.h"  // NOLINT
#elif V8_TARGET_ARCH_S390
#include "src/crankshaft/s390/lithium-s390.h"          // NOLINT
#include "src/crankshaft/s390/lithium-codegen-s390.h"  // NOLINT
#else
#error "Unknown architecture."
#endif

namespace v8 {
namespace internal {

const auto GetRegConfig = RegisterConfiguration::Crankshaft;

void LOperand::PrintTo(StringStream* stream) {
  LUnallocated* unalloc = NULL;
  switch (kind()) {
    case INVALID:
      stream->Add("(0)");
      break;
    case UNALLOCATED:
      unalloc = LUnallocated::cast(this);
      stream->Add("v%d", unalloc->virtual_register());
      if (unalloc->basic_policy() == LUnallocated::FIXED_SLOT) {
        stream->Add("(=%dS)", unalloc->fixed_slot_index());
        break;
      }
      switch (unalloc->extended_policy()) {
        case LUnallocated::NONE:
          break;
        case LUnallocated::FIXED_REGISTER: {
          int reg_index = unalloc->fixed_register_index();
          if (reg_index < 0 || reg_index >= Register::kNumRegisters) {
            stream->Add("(=invalid_reg#%d)", reg_index);
          } else {
            const char* register_name =
                GetRegConfig()->GetGeneralRegisterName(reg_index);
            stream->Add("(=%s)", register_name);
          }
          break;
        }
        case LUnallocated::FIXED_DOUBLE_REGISTER: {
          int reg_index = unalloc->fixed_register_index();
          if (reg_index < 0 || reg_index >= DoubleRegister::kMaxNumRegisters) {
            stream->Add("(=invalid_double_reg#%d)", reg_index);
          } else {
            const char* double_register_name =
                GetRegConfig()->GetDoubleRegisterName(reg_index);
            stream->Add("(=%s)", double_register_name);
          }
          break;
        }
        case LUnallocated::MUST_HAVE_REGISTER:
          stream->Add("(R)");
          break;
        case LUnallocated::MUST_HAVE_DOUBLE_REGISTER:
          stream->Add("(D)");
          break;
        case LUnallocated::WRITABLE_REGISTER:
          stream->Add("(WR)");
          break;
        case LUnallocated::SAME_AS_FIRST_INPUT:
          stream->Add("(1)");
          break;
        case LUnallocated::ANY:
          stream->Add("(-)");
          break;
      }
      break;
    case CONSTANT_OPERAND:
      stream->Add("[constant:%d]", index());
      break;
    case STACK_SLOT:
      stream->Add("[stack:%d]", index());
      break;
    case DOUBLE_STACK_SLOT:
      stream->Add("[double_stack:%d]", index());
      break;
    case REGISTER: {
      int reg_index = index();
      if (reg_index < 0 || reg_index >= Register::kNumRegisters) {
        stream->Add("(=invalid_reg#%d|R)", reg_index);
      } else {
        stream->Add("[%s|R]",
                    GetRegConfig()->GetGeneralRegisterName(reg_index));
      }
      break;
    }
    case DOUBLE_REGISTER: {
      int reg_index = index();
      if (reg_index < 0 || reg_index >= DoubleRegister::kMaxNumRegisters) {
        stream->Add("(=invalid_double_reg#%d|R)", reg_index);
      } else {
        stream->Add("[%s|R]", GetRegConfig()->GetDoubleRegisterName(reg_index));
      }
      break;
    }
  }
}


template<LOperand::Kind kOperandKind, int kNumCachedOperands>
LSubKindOperand<kOperandKind, kNumCachedOperands>*
LSubKindOperand<kOperandKind, kNumCachedOperands>::cache = NULL;


template<LOperand::Kind kOperandKind, int kNumCachedOperands>
void LSubKindOperand<kOperandKind, kNumCachedOperands>::SetUpCache() {
  if (cache) return;
  cache = new LSubKindOperand[kNumCachedOperands];
  for (int i = 0; i < kNumCachedOperands; i++) {
    cache[i].ConvertTo(kOperandKind, i);
  }
}


template<LOperand::Kind kOperandKind, int kNumCachedOperands>
void LSubKindOperand<kOperandKind, kNumCachedOperands>::TearDownCache() {
  delete[] cache;
  cache = NULL;
}


void LOperand::SetUpCaches() {
#define LITHIUM_OPERAND_SETUP(name, type, number) L##name::SetUpCache();
  LITHIUM_OPERAND_LIST(LITHIUM_OPERAND_SETUP)
#undef LITHIUM_OPERAND_SETUP
}


void LOperand::TearDownCaches() {
#define LITHIUM_OPERAND_TEARDOWN(name, type, number) L##name::TearDownCache();
  LITHIUM_OPERAND_LIST(LITHIUM_OPERAND_TEARDOWN)
#undef LITHIUM_OPERAND_TEARDOWN
}


bool LParallelMove::IsRedundant() const {
  for (int i = 0; i < move_operands_.length(); ++i) {
    if (!move_operands_[i].IsRedundant()) return false;
  }
  return true;
}


void LParallelMove::PrintDataTo(StringStream* stream) const {
  bool first = true;
  for (int i = 0; i < move_operands_.length(); ++i) {
    if (!move_operands_[i].IsEliminated()) {
      LOperand* source = move_operands_[i].source();
      LOperand* destination = move_operands_[i].destination();
      if (!first) stream->Add(" ");
      first = false;
      if (source->Equals(destination)) {
        destination->PrintTo(stream);
      } else {
        destination->PrintTo(stream);
        stream->Add(" = ");
        source->PrintTo(stream);
      }
      stream->Add(";");
    }
  }
}


void LEnvironment::PrintTo(StringStream* stream) {
  stream->Add("[id=%d|", ast_id().ToInt());
  if (deoptimization_index() != Safepoint::kNoDeoptimizationIndex) {
    stream->Add("deopt_id=%d|", deoptimization_index());
  }
  stream->Add("parameters=%d|", parameter_count());
  stream->Add("arguments_stack_height=%d|", arguments_stack_height());
  for (int i = 0; i < values_.length(); ++i) {
    if (i != 0) stream->Add(";");
    if (values_[i] == NULL) {
      stream->Add("[hole]");
    } else {
      values_[i]->PrintTo(stream);
    }
  }
  stream->Add("]");
}


void LPointerMap::RecordPointer(LOperand* op, Zone* zone) {
  // Do not record arguments as pointers.
  if (op->IsStackSlot() && op->index() < 0) return;
  DCHECK(!op->IsDoubleRegister() && !op->IsDoubleStackSlot());
  pointer_operands_.Add(op, zone);
}


void LPointerMap::RemovePointer(LOperand* op) {
  // Do not record arguments as pointers.
  if (op->IsStackSlot() && op->index() < 0) return;
  DCHECK(!op->IsDoubleRegister() && !op->IsDoubleStackSlot());
  for (int i = 0; i < pointer_operands_.length(); ++i) {
    if (pointer_operands_[i]->Equals(op)) {
      pointer_operands_.Remove(i);
      --i;
    }
  }
}


void LPointerMap::RecordUntagged(LOperand* op, Zone* zone) {
  // Do not record arguments as pointers.
  if (op->IsStackSlot() && op->index() < 0) return;
  DCHECK(!op->IsDoubleRegister() && !op->IsDoubleStackSlot());
  untagged_operands_.Add(op, zone);
}


void LPointerMap::PrintTo(StringStream* stream) {
  stream->Add("{");
  for (int i = 0; i < pointer_operands_.length(); ++i) {
    if (i != 0) stream->Add(";");
    pointer_operands_[i]->PrintTo(stream);
  }
  stream->Add("}");
}

LChunk::LChunk(CompilationInfo* info, HGraph* graph)
    : base_frame_slots_(info->IsStub()
                            ? TypedFrameConstants::kFixedSlotCount
                            : StandardFrameConstants::kFixedSlotCount),
      current_frame_slots_(base_frame_slots_),
      info_(info),
      graph_(graph),
      instructions_(32, info->zone()),
      pointer_maps_(8, info->zone()),
      deprecation_dependencies_(32, info->zone()),
      stability_dependencies_(8, info->zone()) {}

LLabel* LChunk::GetLabel(int block_id) const {
  HBasicBlock* block = graph_->blocks()->at(block_id);
  int first_instruction = block->first_instruction_index();
  return LLabel::cast(instructions_[first_instruction]);
}


int LChunk::LookupDestination(int block_id) const {
  LLabel* cur = GetLabel(block_id);
  while (cur->replacement() != NULL) {
    cur = cur->replacement();
  }
  return cur->block_id();
}

Label* LChunk::GetAssemblyLabel(int block_id) const {
  LLabel* label = GetLabel(block_id);
  DCHECK(!label->HasReplacement());
  return label->label();
}


void LChunk::MarkEmptyBlocks() {
  LPhase phase("L_Mark empty blocks", this);
  for (int i = 0; i < graph()->blocks()->length(); ++i) {
    HBasicBlock* block = graph()->blocks()->at(i);
    int first = block->first_instruction_index();
    int last = block->last_instruction_index();
    LInstruction* first_instr = instructions()->at(first);
    LInstruction* last_instr = instructions()->at(last);

    LLabel* label = LLabel::cast(first_instr);
    if (last_instr->IsGoto()) {
      LGoto* goto_instr = LGoto::cast(last_instr);
      if (label->IsRedundant() &&
          !label->is_loop_header()) {
        bool can_eliminate = true;
        for (int i = first + 1; i < last && can_eliminate; ++i) {
          LInstruction* cur = instructions()->at(i);
          if (cur->IsGap()) {
            LGap* gap = LGap::cast(cur);
            if (!gap->IsRedundant()) {
              can_eliminate = false;
            }
          } else {
            can_eliminate = false;
          }
        }
        if (can_eliminate) {
          label->set_replacement(GetLabel(goto_instr->block_id()));
        }
      }
    }
  }
}


void LChunk::AddInstruction(LInstruction* instr, HBasicBlock* block) {
  LInstructionGap* gap = new (zone()) LInstructionGap(block);
  gap->set_hydrogen_value(instr->hydrogen_value());
  int index = -1;
  if (instr->IsControl()) {
    instructions_.Add(gap, zone());
    index = instructions_.length();
    instructions_.Add(instr, zone());
  } else {
    index = instructions_.length();
    instructions_.Add(instr, zone());
    instructions_.Add(gap, zone());
  }
  if (instr->HasPointerMap()) {
    pointer_maps_.Add(instr->pointer_map(), zone());
    instr->pointer_map()->set_lithium_position(index);
  }
}

LConstantOperand* LChunk::DefineConstantOperand(HConstant* constant) {
  return LConstantOperand::Create(constant->id(), zone());
}


int LChunk::GetParameterStackSlot(int index) const {
  // The receiver is at index 0, the first parameter at index 1, so we
  // shift all parameter indexes down by the number of parameters, and
  // make sure they end up negative so they are distinguishable from
  // spill slots.
  int result = index - info()->num_parameters() - 1;

  DCHECK(result < 0);
  return result;
}


// A parameter relative to ebp in the arguments stub.
int LChunk::ParameterAt(int index) {
  DCHECK(-1 <= index);  // -1 is the receiver.
  return (1 + info()->scope()->num_parameters() - index) *
      kPointerSize;
}


LGap* LChunk::GetGapAt(int index) const {
  return LGap::cast(instructions_[index]);
}


bool LChunk::IsGapAt(int index) const {
  return instructions_[index]->IsGap();
}


int LChunk::NearestGapPos(int index) const {
  while (!IsGapAt(index)) index--;
  return index;
}


void LChunk::AddGapMove(int index, LOperand* from, LOperand* to) {
  GetGapAt(index)->GetOrCreateParallelMove(
      LGap::START, zone())->AddMove(from, to, zone());
}


HConstant* LChunk::LookupConstant(LConstantOperand* operand) const {
  return HConstant::cast(graph_->LookupValue(operand->index()));
}


Representation LChunk::LookupLiteralRepresentation(
    LConstantOperand* operand) const {
  return graph_->LookupValue(operand->index())->representation();
}


void LChunk::CommitDependencies(Handle<Code> code) const {
  if (!code->is_optimized_code()) return;
  HandleScope scope(isolate());

  for (Handle<Map> map : deprecation_dependencies_) {
    DCHECK(!map->is_deprecated());
    DCHECK(map->CanBeDeprecated());
    Map::AddDependentCode(map, DependentCode::kTransitionGroup, code);
  }

  for (Handle<Map> map : stability_dependencies_) {
    DCHECK(map->is_stable());
    DCHECK(map->CanTransition());
    Map::AddDependentCode(map, DependentCode::kPrototypeCheckGroup, code);
  }

  info_->dependencies()->Commit(code);
}


LChunk* LChunk::NewChunk(HGraph* graph) {
  DisallowHandleAllocation no_handles;
  DisallowHeapAllocation no_gc;
  graph->DisallowAddingNewValues();
  int values = graph->GetMaximumValueID();
  CompilationInfo* info = graph->info();
  if (values > LUnallocated::kMaxVirtualRegisters) {
    info->AbortOptimization(kNotEnoughVirtualRegistersForValues);
    return NULL;
  }
  LAllocator allocator(values, graph);
  LChunkBuilder builder(info, graph, &allocator);
  LChunk* chunk = builder.Build();
  if (chunk == NULL) return NULL;

  if (!allocator.Allocate(chunk)) {
    info->AbortOptimization(kNotEnoughVirtualRegistersRegalloc);
    return NULL;
  }

  chunk->set_allocated_double_registers(
      allocator.assigned_double_registers());

  return chunk;
}


Handle<Code> LChunk::Codegen() {
  MacroAssembler assembler(info()->isolate(), NULL, 0,
                           CodeObjectRequired::kYes);
  // Code serializer only takes unoptimized code.
  DCHECK(!info()->will_serialize());
  LCodeGen generator(this, &assembler, info());

  MarkEmptyBlocks();

  if (generator.GenerateCode()) {
    generator.CheckEnvironmentUsage();
    CodeGenerator::MakeCodePrologue(info(), "optimized");
    Handle<Code> code = CodeGenerator::MakeCodeEpilogue(
        &assembler, nullptr, info(), assembler.CodeObject());
    generator.FinishCode(code);
    CommitDependencies(code);
    Handle<ByteArray> source_positions =
        generator.source_position_table_builder()->ToSourcePositionTable(
            info()->isolate(), Handle<AbstractCode>::cast(code));
    code->set_source_position_table(*source_positions);
    code->set_is_crankshafted(true);

    CodeGenerator::PrintCode(code, info());
    return code;
  }
  assembler.AbortedCodeGeneration();
  return Handle<Code>::null();
}


void LChunk::set_allocated_double_registers(BitVector* allocated_registers) {
  allocated_double_registers_ = allocated_registers;
  BitVector* doubles = allocated_double_registers();
  BitVector::Iterator iterator(doubles);
  while (!iterator.Done()) {
    if (info()->saves_caller_doubles()) {
      if (kDoubleSize == kPointerSize * 2) {
        current_frame_slots_ += 2;
      } else {
        current_frame_slots_++;
      }
    }
    iterator.Advance();
  }
}


void LChunkBuilderBase::Abort(BailoutReason reason) {
  info()->AbortOptimization(reason);
  status_ = ABORTED;
}


void LChunkBuilderBase::Retry(BailoutReason reason) {
  info()->RetryOptimization(reason);
  status_ = ABORTED;
}

void LChunkBuilderBase::CreateLazyBailoutForCall(HBasicBlock* current_block,
                                                 LInstruction* instr,
                                                 HInstruction* hydrogen_val) {
  if (!instr->IsCall()) return;

  HEnvironment* hydrogen_env = current_block->last_environment();
  HValue* hydrogen_value_for_lazy_bailout = hydrogen_val;
  DCHECK_NOT_NULL(hydrogen_env);
  if (instr->IsSyntacticTailCall()) {
    // If it was a syntactic tail call we need to drop the current frame and
    // all the frames on top of it that are either an arguments adaptor frame
    // or a tail caller frame.
    hydrogen_env = hydrogen_env->outer();
    while (hydrogen_env != nullptr &&
           (hydrogen_env->frame_type() == ARGUMENTS_ADAPTOR ||
            hydrogen_env->frame_type() == TAIL_CALLER_FUNCTION)) {
      hydrogen_env = hydrogen_env->outer();
    }
    if (hydrogen_env != nullptr) {
      if (hydrogen_env->frame_type() == JS_FUNCTION) {
        // In case an outer frame is a function frame we have to replay
        // environment manually because
        // 1) it does not contain a result of inlined function yet,
        // 2) we can't find the proper simulate that corresponds to the point
        //    after inlined call to do a ReplayEnvironment() on.
        // So we push return value on top of outer environment.
        // As for JS_GETTER/JS_SETTER/JS_CONSTRUCT nothing has to be done here,
        // the deoptimizer ensures that the result of the callee is correctly
        // propagated to result register during deoptimization.
        hydrogen_env = hydrogen_env->Copy();
        hydrogen_env->Push(hydrogen_val);
      }
    } else {
      // Although we don't need this lazy bailout for normal execution
      // (because when we tail call from the outermost function we should pop
      // its frame) we still need it when debugger is on.
      hydrogen_env = current_block->last_environment();
    }
  } else {
    if (hydrogen_val->HasObservableSideEffects()) {
      HSimulate* sim = HSimulate::cast(hydrogen_val->next());
      sim->ReplayEnvironment(hydrogen_env);
      hydrogen_value_for_lazy_bailout = sim;
    }
  }
  LInstruction* bailout = LChunkBuilderBase::AssignEnvironment(
      new (zone()) LLazyBailout(), hydrogen_env);
  bailout->set_hydrogen_value(hydrogen_value_for_lazy_bailout);
  chunk_->AddInstruction(bailout, current_block);
}

LInstruction* LChunkBuilderBase::AssignEnvironment(LInstruction* instr,
                                                   HEnvironment* hydrogen_env) {
  int argument_index_accumulator = 0;
  ZoneList<HValue*> objects_to_materialize(0, zone());
  DCHECK_NE(TAIL_CALLER_FUNCTION, hydrogen_env->frame_type());
  instr->set_environment(CreateEnvironment(
      hydrogen_env, &argument_index_accumulator, &objects_to_materialize));
  return instr;
}

LEnvironment* LChunkBuilderBase::CreateEnvironment(
    HEnvironment* hydrogen_env, int* argument_index_accumulator,
    ZoneList<HValue*>* objects_to_materialize) {
  if (hydrogen_env == NULL) return NULL;

  BailoutId ast_id = hydrogen_env->ast_id();
  DCHECK(!ast_id.IsNone() ||
         (hydrogen_env->frame_type() != JS_FUNCTION &&
          hydrogen_env->frame_type() != TAIL_CALLER_FUNCTION));

  if (hydrogen_env->frame_type() == TAIL_CALLER_FUNCTION) {
    // Skip potential outer arguments adaptor frame.
    HEnvironment* outer_hydrogen_env = hydrogen_env->outer();
    if (outer_hydrogen_env != nullptr &&
        outer_hydrogen_env->frame_type() == ARGUMENTS_ADAPTOR) {
      outer_hydrogen_env = outer_hydrogen_env->outer();
    }
    LEnvironment* outer = CreateEnvironment(
        outer_hydrogen_env, argument_index_accumulator, objects_to_materialize);
    return new (zone())
        LEnvironment(hydrogen_env->closure(), hydrogen_env->frame_type(),
                     ast_id, 0, 0, 0, outer, hydrogen_env->entry(), zone());
  }

  LEnvironment* outer =
      CreateEnvironment(hydrogen_env->outer(), argument_index_accumulator,
                        objects_to_materialize);

  int omitted_count = (hydrogen_env->frame_type() == JS_FUNCTION)
                          ? 0
                          : hydrogen_env->specials_count();

  int value_count = hydrogen_env->length() - omitted_count;
  LEnvironment* result =
      new(zone()) LEnvironment(hydrogen_env->closure(),
                               hydrogen_env->frame_type(),
                               ast_id,
                               hydrogen_env->parameter_count(),
                               argument_count_,
                               value_count,
                               outer,
                               hydrogen_env->entry(),
                               zone());
  int argument_index = *argument_index_accumulator;

  // Store the environment description into the environment
  // (with holes for nested objects)
  for (int i = 0; i < hydrogen_env->length(); ++i) {
    if (hydrogen_env->is_special_index(i) &&
        hydrogen_env->frame_type() != JS_FUNCTION) {
      continue;
    }
    LOperand* op;
    HValue* value = hydrogen_env->values()->at(i);
    CHECK(!value->IsPushArguments());  // Do not deopt outgoing arguments
    if (value->IsArgumentsObject() || value->IsCapturedObject()) {
      op = LEnvironment::materialization_marker();
    } else {
      op = UseAny(value);
    }
    result->AddValue(op,
                     value->representation(),
                     value->CheckFlag(HInstruction::kUint32));
  }

  // Recursively store the nested objects into the environment
  for (int i = 0; i < hydrogen_env->length(); ++i) {
    if (hydrogen_env->is_special_index(i)) continue;

    HValue* value = hydrogen_env->values()->at(i);
    if (value->IsArgumentsObject() || value->IsCapturedObject()) {
      AddObjectToMaterialize(value, objects_to_materialize, result);
    }
  }

  if (hydrogen_env->frame_type() == JS_FUNCTION) {
    *argument_index_accumulator = argument_index;
  }

  return result;
}


// Add an object to the supplied environment and object materialization list.
//
// Notes:
//
// We are building three lists here:
//
// 1. In the result->object_mapping_ list (added to by the
//    LEnvironment::Add*Object methods), we store the lengths (number
//    of fields) of the captured objects in depth-first traversal order, or
//    in case of duplicated objects, we store the index to the duplicate object
//    (with a tag to differentiate between captured and duplicated objects).
//
// 2. The object fields are stored in the result->values_ list
//    (added to by the LEnvironment.AddValue method) sequentially as lists
//    of fields with holes for nested objects (the holes will be expanded
//    later by LCodegen::AddToTranslation according to the
//    LEnvironment.object_mapping_ list).
//
// 3. The auxiliary objects_to_materialize array stores the hydrogen values
//    in the same order as result->object_mapping_ list. This is used
//    to detect duplicate values and calculate the corresponding object index.
void LChunkBuilderBase::AddObjectToMaterialize(HValue* value,
    ZoneList<HValue*>* objects_to_materialize, LEnvironment* result) {
  int object_index = objects_to_materialize->length();
  // Store the hydrogen value into the de-duplication array
  objects_to_materialize->Add(value, zone());
  // Find out whether we are storing a duplicated value
  int previously_materialized_object = -1;
  for (int prev = 0; prev < object_index; ++prev) {
    if (objects_to_materialize->at(prev) == value) {
      previously_materialized_object = prev;
      break;
    }
  }
  // Store the captured object length (or duplicated object index)
  // into the environment. For duplicated objects, we stop here.
  int length = value->OperandCount();
  bool is_arguments = value->IsArgumentsObject();
  if (previously_materialized_object >= 0) {
    result->AddDuplicateObject(previously_materialized_object);
    return;
  } else {
    result->AddNewObject(is_arguments ? length - 1 : length, is_arguments);
  }
  // Store the captured object's fields into the environment
  for (int i = is_arguments ? 1 : 0; i < length; ++i) {
    LOperand* op;
    HValue* arg_value = value->OperandAt(i);
    if (arg_value->IsArgumentsObject() || arg_value->IsCapturedObject()) {
      // Insert a hole for nested objects
      op = LEnvironment::materialization_marker();
    } else {
      DCHECK(!arg_value->IsPushArguments());
      // For ordinary values, tell the register allocator we need the value
      // to be alive here
      op = UseAny(arg_value);
    }
    result->AddValue(op,
                     arg_value->representation(),
                     arg_value->CheckFlag(HInstruction::kUint32));
  }
  // Recursively store all the nested captured objects into the environment
  for (int i = is_arguments ? 1 : 0; i < length; ++i) {
    HValue* arg_value = value->OperandAt(i);
    if (arg_value->IsArgumentsObject() || arg_value->IsCapturedObject()) {
      AddObjectToMaterialize(arg_value, objects_to_materialize, result);
    }
  }
}


LPhase::~LPhase() {
  if (ShouldProduceTraceOutput()) {
    isolate()->GetHTracer()->TraceLithium(name(), chunk_);
  }
}


}  // namespace internal
}  // namespace v8
