// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-graph-builder.h"

#include <limits>

#include "src/base/logging.h"
#include "src/base/optional.h"
#include "src/base/v8-fallthrough.h"
#include "src/base/vector.h"
#include "src/builtins/builtins-constructor.h"
#include "src/builtins/builtins.h"
#include "src/codegen/cpu-features.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/common/globals.h"
#include "src/compiler/access-info.h"
#include "src/compiler/bytecode-liveness-map.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/feedback-source.h"
#include "src/compiler/heap-refs.h"
#include "src/compiler/js-heap-broker.h"
#include "src/compiler/processed-feedback.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/flags/flags.h"
#include "src/handles/maybe-handles-inl.h"
#include "src/ic/handler-configuration-inl.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/interpreter/bytecode-register.h"
#include "src/interpreter/bytecodes.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"
#include "src/objects/elements-kind.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/literal-objects-inl.h"
#include "src/objects/name-inl.h"
#include "src/objects/property-cell.h"
#include "src/objects/property-details.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/slots-inl.h"
#include "src/objects/type-hints.h"
#include "src/zone/zone-handle-set.h"

namespace v8::internal::maglev {

namespace {

enum class CpuOperation {
  kFloat64Round,
};

// TODO(leszeks): Add a generic mechanism for marking nodes as optionally
// supported.
bool IsSupported(CpuOperation op) {
#ifdef V8_TARGET_ARCH_X64
  switch (op) {
    case CpuOperation::kFloat64Round:
      return CpuFeatures::IsSupported(SSE4_1) || CpuFeatures::IsSupported(AVX);
  }
#elif V8_TARGET_ARCH_ARM64
  return true;
#else
#error "Maglev does not supported this architecture."
#endif
}

ValueNode* TryGetParentContext(ValueNode* node) {
  if (CreateFunctionContext* n = node->TryCast<CreateFunctionContext>()) {
    return n->context().node();
  }

  if (CallRuntime* n = node->TryCast<CallRuntime>()) {
    switch (n->function_id()) {
      case Runtime::kPushBlockContext:
      case Runtime::kPushCatchContext:
      case Runtime::kNewFunctionContext:
        return n->context().node();
      default:
        break;
    }
  }

  return nullptr;
}

// Attempts to walk up the context chain through the graph in order to reduce
// depth and thus the number of runtime loads.
void MinimizeContextChainDepth(ValueNode** context, size_t* depth) {
  while (*depth > 0) {
    ValueNode* parent_context = TryGetParentContext(*context);
    if (parent_context == nullptr) return;
    *context = parent_context;
    (*depth)--;
  }
}

class FunctionContextSpecialization final : public AllStatic {
 public:
  static compiler::OptionalContextRef TryToRef(
      const MaglevCompilationUnit* unit, ValueNode* context, size_t* depth) {
    DCHECK(unit->info()->specialize_to_function_context());
    compiler::OptionalContextRef ref;
    if (InitialValue* n = context->TryCast<InitialValue>()) {
      if (n->source().is_current_context()) {
        ref = unit->function().context(unit->broker());
      }
    } else if (Constant* n = context->TryCast<Constant>()) {
      ref = n->ref().AsContext();
    }
    if (!ref.has_value()) return {};
    return ref->previous(unit->broker(), depth);
  }
};

}  // namespace

class CallArguments {
 public:
  enum Mode {
    kDefault,
    kWithSpread,
    kWithArrayLike,
  };

  CallArguments(ConvertReceiverMode receiver_mode,
                interpreter::RegisterList reglist,
                const InterpreterFrameState& frame, Mode mode = kDefault)
      : receiver_mode_(receiver_mode),
        args_(reglist.register_count()),
        mode_(mode) {
    for (int i = 0; i < reglist.register_count(); i++) {
      args_[i] = frame.get(reglist[i]);
    }
    DCHECK_IMPLIES(args_.size() == 0,
                   receiver_mode == ConvertReceiverMode::kNullOrUndefined);
    DCHECK_IMPLIES(mode != kDefault,
                   receiver_mode == ConvertReceiverMode::kAny);
    DCHECK_IMPLIES(mode == kWithArrayLike, args_.size() == 2);
  }

  explicit CallArguments(ConvertReceiverMode receiver_mode)
      : receiver_mode_(receiver_mode), args_(), mode_(kDefault) {
    DCHECK_EQ(receiver_mode, ConvertReceiverMode::kNullOrUndefined);
  }

  CallArguments(ConvertReceiverMode receiver_mode,
                std::initializer_list<ValueNode*> args, Mode mode = kDefault)
      : receiver_mode_(receiver_mode), args_(args), mode_(mode) {
    DCHECK_IMPLIES(mode != kDefault,
                   receiver_mode == ConvertReceiverMode::kAny);
    DCHECK_IMPLIES(mode == kWithArrayLike, args_.size() == 2);
  }

  ValueNode* receiver() const {
    if (receiver_mode_ == ConvertReceiverMode::kNullOrUndefined) {
      return nullptr;
    }
    return args_[0];
  }

  size_t count() const {
    if (receiver_mode_ == ConvertReceiverMode::kNullOrUndefined) {
      return args_.size();
    }
    return args_.size() - 1;
  }

  size_t count_with_receiver() const { return count() + 1; }

  ValueNode* operator[](size_t i) const {
    if (receiver_mode_ != ConvertReceiverMode::kNullOrUndefined) {
      i++;
    }
    if (i >= args_.size()) return nullptr;
    return args_[i];
  }

  void set_arg(size_t i, ValueNode* node) {
    if (receiver_mode_ != ConvertReceiverMode::kNullOrUndefined) {
      i++;
    }
    DCHECK_LT(i, args_.size());
    args_[i] = node;
  }

  Mode mode() const { return mode_; }

  ConvertReceiverMode receiver_mode() const { return receiver_mode_; }

  void Truncate(size_t new_args_count) {
    if (new_args_count >= count()) return;
    size_t args_to_pop = count() - new_args_count;
    for (size_t i = 0; i < args_to_pop; i++) {
      args_.pop_back();
    }
  }

  void PopReceiver(ConvertReceiverMode new_receiver_mode) {
    DCHECK_NE(receiver_mode_, ConvertReceiverMode::kNullOrUndefined);
    DCHECK_NE(new_receiver_mode, ConvertReceiverMode::kNullOrUndefined);
    DCHECK_GT(args_.size(), 0);  // We have at least a receiver to pop!
    // TODO(victorgomes): Do this better!
    for (size_t i = 0; i < args_.size() - 1; i++) {
      args_[i] = args_[i + 1];
    }
    args_.pop_back();

    // If there is no non-receiver argument to become the new receiver,
    // consider the new receiver to be known undefined.
    receiver_mode_ = args_.size() == 0 ? ConvertReceiverMode::kNullOrUndefined
                                       : new_receiver_mode;
  }

 private:
  ConvertReceiverMode receiver_mode_;
  base::SmallVector<ValueNode*, 8> args_;
  Mode mode_;
};

class V8_NODISCARD MaglevGraphBuilder::CallSpeculationScope {
 public:
  CallSpeculationScope(MaglevGraphBuilder* builder,
                       compiler::FeedbackSource feedback_source)
      : builder_(builder) {
    DCHECK(!builder_->current_speculation_feedback_.IsValid());
    if (feedback_source.IsValid()) {
      DCHECK_EQ(
          FeedbackNexus(feedback_source.vector, feedback_source.slot).kind(),
          FeedbackSlotKind::kCall);
    }
    builder_->current_speculation_feedback_ = feedback_source;
  }
  ~CallSpeculationScope() {
    builder_->current_speculation_feedback_ = compiler::FeedbackSource();
  }

 private:
  MaglevGraphBuilder* builder_;
};

class V8_NODISCARD MaglevGraphBuilder::LazyDeoptContinuationScope {
 public:
  LazyDeoptContinuationScope(MaglevGraphBuilder* builder, Builtin continuation)
      : builder_(builder),
        parent_(builder->current_lazy_deopt_continuation_scope_),
        continuation_(continuation) {
    builder_->current_lazy_deopt_continuation_scope_ = this;
  }
  ~LazyDeoptContinuationScope() {
    builder_->current_lazy_deopt_continuation_scope_ = parent_;
  }

  LazyDeoptContinuationScope* parent() const { return parent_; }
  Builtin continuation() const { return continuation_; }

 private:
  MaglevGraphBuilder* builder_;
  LazyDeoptContinuationScope* parent_;
  Builtin continuation_;
};

MaglevGraphBuilder::MaglevGraphBuilder(LocalIsolate* local_isolate,
                                       MaglevCompilationUnit* compilation_unit,
                                       Graph* graph, float call_frequency,
                                       BytecodeOffset bytecode_offset,
                                       MaglevGraphBuilder* parent)
    : local_isolate_(local_isolate),
      compilation_unit_(compilation_unit),
      parent_(parent),
      graph_(graph),
      bytecode_analysis_(bytecode().object(), zone(), BytecodeOffset::None(),
                         true),
      iterator_(bytecode().object()),
      source_position_iterator_(bytecode().SourcePositionTable(broker())),
      call_frequency_(call_frequency),
      // Add an extra jump_target slot for the inline exit if needed.
      jump_targets_(zone()->NewArray<BasicBlockRef>(bytecode().length() +
                                                    (is_inline() ? 1 : 0))),
      // Overallocate merge_states_ by one to allow always looking up the
      // next offset. This overallocated slot can also be used for the inline
      // exit when needed.
      merge_states_(zone()->NewArray<MergePointInterpreterFrameState*>(
          bytecode().length() + 1)),
      current_interpreter_frame_(
          *compilation_unit_,
          is_inline() ? parent->current_interpreter_frame_.known_node_aspects()
                      : compilation_unit_->zone()->New<KnownNodeAspects>(
                            compilation_unit_->zone())),
      caller_bytecode_offset_(bytecode_offset),
      catch_block_stack_(zone()) {
  memset(merge_states_, 0,
         (bytecode().length() + 1) * sizeof(InterpreterFrameState*));
  // Default construct basic block refs.
  // TODO(leszeks): This could be a memset of nullptr to ..._jump_targets_.
  for (int i = 0; i < bytecode().length(); ++i) {
    new (&jump_targets_[i]) BasicBlockRef();
  }

  if (is_inline()) {
    DCHECK_NOT_NULL(parent_);
    DCHECK_GT(compilation_unit->inlining_depth(), 0);
    // The allocation/initialisation logic here relies on inline_exit_offset
    // being the offset one past the end of the bytecode.
    DCHECK_EQ(inline_exit_offset(), bytecode().length());
    merge_states_[inline_exit_offset()] = nullptr;
    new (&jump_targets_[inline_exit_offset()]) BasicBlockRef();
  }

  CalculatePredecessorCounts();
}

void MaglevGraphBuilder::StartPrologue() {
  current_block_ = zone()->New<BasicBlock>(nullptr);
}

BasicBlock* MaglevGraphBuilder::EndPrologue() {
  BasicBlock* first_block = FinishBlock<Jump>({}, &jump_targets_[0]);
  MergeIntoFrameState(first_block, 0);
  return first_block;
}

void MaglevGraphBuilder::SetArgument(int i, ValueNode* value) {
  interpreter::Register reg = interpreter::Register::FromParameterIndex(i);
  current_interpreter_frame_.set(reg, value);
}

ValueNode* MaglevGraphBuilder::GetTaggedArgument(int i) {
  interpreter::Register reg = interpreter::Register::FromParameterIndex(i);
  return GetTaggedValue(reg);
}

void MaglevGraphBuilder::InitializeRegister(interpreter::Register reg,
                                            ValueNode* value) {
  current_interpreter_frame_.set(
      reg, value ? value : AddNewNode<InitialValue>({}, reg));
}

void MaglevGraphBuilder::BuildRegisterFrameInitialization(ValueNode* context,
                                                          ValueNode* closure) {
  InitializeRegister(interpreter::Register::current_context(), context);
  InitializeRegister(interpreter::Register::function_closure(), closure);

  interpreter::Register new_target_or_generator_register =
      bytecode().incoming_new_target_or_generator_register();

  int register_index = 0;
  // TODO(leszeks): Don't emit if not needed.
  ValueNode* undefined_value = GetRootConstant(RootIndex::kUndefinedValue);
  if (new_target_or_generator_register.is_valid()) {
    int new_target_index = new_target_or_generator_register.index();
    for (; register_index < new_target_index; register_index++) {
      current_interpreter_frame_.set(interpreter::Register(register_index),
                                     undefined_value);
    }
    current_interpreter_frame_.set(
        new_target_or_generator_register,
        GetRegisterInput(kJavaScriptCallNewTargetRegister));
    register_index++;
  }
  for (; register_index < register_count(); register_index++) {
    current_interpreter_frame_.set(interpreter::Register(register_index),
                                   undefined_value);
  }
}

void MaglevGraphBuilder::BuildMergeStates() {
  for (auto& offset_and_info : bytecode_analysis().GetLoopInfos()) {
    int offset = offset_and_info.first;
    const compiler::LoopInfo& loop_info = offset_and_info.second;
    const compiler::BytecodeLivenessState* liveness = GetInLivenessFor(offset);
    DCHECK_NULL(merge_states_[offset]);
    if (v8_flags.trace_maglev_graph_building) {
      std::cout << "- Creating loop merge state at @" << offset << std::endl;
    }
    merge_states_[offset] = MergePointInterpreterFrameState::NewForLoop(
        current_interpreter_frame_, *compilation_unit_, offset,
        NumPredecessors(offset), liveness, &loop_info);
  }

  if (bytecode().handler_table_size() > 0) {
    HandlerTable table(*bytecode().object());
    for (int i = 0; i < table.NumberOfRangeEntries(); i++) {
      const int offset = table.GetRangeHandler(i);
      const interpreter::Register context_reg(table.GetRangeData(i));
      const compiler::BytecodeLivenessState* liveness =
          GetInLivenessFor(offset);
      DCHECK_EQ(NumPredecessors(offset), 0);
      DCHECK_NULL(merge_states_[offset]);
      if (v8_flags.trace_maglev_graph_building) {
        std::cout << "- Creating exception merge state at @" << offset
                  << ", context register r" << context_reg.index() << std::endl;
      }
      merge_states_[offset] = MergePointInterpreterFrameState::NewForCatchBlock(
          *compilation_unit_, liveness, offset, context_reg, graph_,
          is_inline());
    }
  }
}

namespace {

template <int index, interpreter::OperandType... operands>
struct GetResultLocationAndSizeHelper;

// Terminal cases
template <int index>
struct GetResultLocationAndSizeHelper<index> {
  static std::pair<interpreter::Register, int> GetResultLocationAndSize(
      const interpreter::BytecodeArrayIterator& iterator) {
    // TODO(leszeks): This should probably actually be "UNREACHABLE" but we have
    // lazy deopt info for interrupt budget updates at returns, not for actual
    // lazy deopts, but just for stack iteration purposes.
    return {interpreter::Register::invalid_value(), 0};
  }
  static bool HasOutputRegisterOperand() { return false; }
};

template <int index, interpreter::OperandType... operands>
struct GetResultLocationAndSizeHelper<index, interpreter::OperandType::kRegOut,
                                      operands...> {
  static std::pair<interpreter::Register, int> GetResultLocationAndSize(
      const interpreter::BytecodeArrayIterator& iterator) {
    // We shouldn't have any other output operands than this one.
    return {iterator.GetRegisterOperand(index), 1};
  }
  static bool HasOutputRegisterOperand() { return true; }
};

template <int index, interpreter::OperandType... operands>
struct GetResultLocationAndSizeHelper<
    index, interpreter::OperandType::kRegOutPair, operands...> {
  static std::pair<interpreter::Register, int> GetResultLocationAndSize(
      const interpreter::BytecodeArrayIterator& iterator) {
    // We shouldn't have any other output operands than this one.
    return {iterator.GetRegisterOperand(index), 2};
  }
  static bool HasOutputRegisterOperand() { return true; }
};

template <int index, interpreter::OperandType... operands>
struct GetResultLocationAndSizeHelper<
    index, interpreter::OperandType::kRegOutTriple, operands...> {
  static std::pair<interpreter::Register, int> GetResultLocationAndSize(
      const interpreter::BytecodeArrayIterator& iterator) {
    // We shouldn't have any other output operands than this one.
    DCHECK(!(GetResultLocationAndSizeHelper<
             index + 1, operands...>::HasOutputRegisterOperand()));
    return {iterator.GetRegisterOperand(index), 3};
  }
  static bool HasOutputRegisterOperand() { return true; }
};

// We don't support RegOutList for lazy deopts.
template <int index, interpreter::OperandType... operands>
struct GetResultLocationAndSizeHelper<
    index, interpreter::OperandType::kRegOutList, operands...> {
  static std::pair<interpreter::Register, int> GetResultLocationAndSize(
      const interpreter::BytecodeArrayIterator& iterator) {
    interpreter::RegisterList list = iterator.GetRegisterListOperand(index);
    return {list.first_register(), list.register_count()};
  }
  static bool HasOutputRegisterOperand() { return true; }
};

// Induction case.
template <int index, interpreter::OperandType operand,
          interpreter::OperandType... operands>
struct GetResultLocationAndSizeHelper<index, operand, operands...> {
  static std::pair<interpreter::Register, int> GetResultLocationAndSize(
      const interpreter::BytecodeArrayIterator& iterator) {
    return GetResultLocationAndSizeHelper<
        index + 1, operands...>::GetResultLocationAndSize(iterator);
  }
  static bool HasOutputRegisterOperand() {
    return GetResultLocationAndSizeHelper<
        index + 1, operands...>::HasOutputRegisterOperand();
  }
};

template <interpreter::Bytecode bytecode,
          interpreter::ImplicitRegisterUse implicit_use,
          interpreter::OperandType... operands>
std::pair<interpreter::Register, int> GetResultLocationAndSizeForBytecode(
    const interpreter::BytecodeArrayIterator& iterator) {
  // We don't support output registers for implicit registers.
  DCHECK(!interpreter::BytecodeOperands::WritesImplicitRegister(implicit_use));
  if (interpreter::BytecodeOperands::WritesAccumulator(implicit_use)) {
    // If we write the accumulator, we shouldn't also write an output register.
    DCHECK(!(GetResultLocationAndSizeHelper<
             0, operands...>::HasOutputRegisterOperand()));
    return {interpreter::Register::virtual_accumulator(), 1};
  }

  // Use template magic to output a the appropriate GetRegisterOperand call and
  // size for this bytecode.
  return GetResultLocationAndSizeHelper<
      0, operands...>::GetResultLocationAndSize(iterator);
}

}  // namespace

std::pair<interpreter::Register, int>
MaglevGraphBuilder::GetResultLocationAndSize() const {
  using Bytecode = interpreter::Bytecode;
  using OperandType = interpreter::OperandType;
  using ImplicitRegisterUse = interpreter::ImplicitRegisterUse;
  Bytecode bytecode = iterator_.current_bytecode();
  // TODO(leszeks): Only emit these cases for bytecodes we know can lazy deopt.
  switch (bytecode) {
#define CASE(Name, ...)                                           \
  case Bytecode::k##Name:                                         \
    return GetResultLocationAndSizeForBytecode<Bytecode::k##Name, \
                                               __VA_ARGS__>(iterator_);
    BYTECODE_LIST(CASE)
#undef CASE
  }
  UNREACHABLE();
}

#ifdef DEBUG
bool MaglevGraphBuilder::HasOutputRegister(interpreter::Register reg) const {
  interpreter::Bytecode bytecode = iterator_.current_bytecode();
  if (reg == interpreter::Register::virtual_accumulator()) {
    return interpreter::Bytecodes::WritesAccumulator(bytecode);
  }
  for (int i = 0; i < interpreter::Bytecodes::NumberOfOperands(bytecode); ++i) {
    if (interpreter::Bytecodes::IsRegisterOutputOperandType(
            interpreter::Bytecodes::GetOperandType(bytecode, i))) {
      interpreter::Register operand_reg = iterator_.GetRegisterOperand(i);
      int operand_range = iterator_.GetRegisterOperandRange(i);
      if (base::IsInRange(reg.index(), operand_reg.index(),
                          operand_reg.index() + operand_range)) {
        return true;
      }
    }
  }
  return false;
}
#endif

DeoptFrame* MaglevGraphBuilder::GetParentDeoptFrame() {
  if (parent_ == nullptr) return nullptr;
  if (parent_deopt_frame_ == nullptr) {
    // The parent resumes after the call, which is roughly equivalent to a lazy
    // deopt. Use the helper function directly so that we can mark the
    // accumulator as dead (since it'll be overwritten by this function's
    // return value anyway).
    // TODO(leszeks): This is true for our current set of
    // inlinings/continuations, but there might be cases in the future where it
    // isn't. We may need to store the relevant overwritten register in
    // LazyDeoptContinuationScope.
    DCHECK(interpreter::Bytecodes::WritesAccumulator(
        parent_->iterator_.current_bytecode()));

    parent_deopt_frame_ =
        zone()->New<DeoptFrame>(parent_->GetDeoptFrameForLazyDeoptHelper(
            parent_->current_lazy_deopt_continuation_scope_, true));
    if (inlined_arguments_) {
      parent_deopt_frame_ = zone()->New<InlinedArgumentsDeoptFrame>(
          *compilation_unit_, caller_bytecode_offset_, *inlined_arguments_,
          parent_deopt_frame_);
    }
  }
  return parent_deopt_frame_;
}

DeoptFrame MaglevGraphBuilder::GetLatestCheckpointedFrame() {
  if (!latest_checkpointed_frame_) {
    // TODO(leszeks): Figure out a way of handling eager continuations.
    DCHECK_NULL(current_lazy_deopt_continuation_scope_);
    latest_checkpointed_frame_.emplace(
        *compilation_unit_,
        zone()->New<CompactInterpreterFrameState>(
            *compilation_unit_, GetInLiveness(), current_interpreter_frame_),
        BytecodeOffset(iterator_.current_offset()), current_source_position_,
        GetParentDeoptFrame());
  }
  return *latest_checkpointed_frame_;
}

DeoptFrame MaglevGraphBuilder::GetDeoptFrameForLazyDeopt() {
  return GetDeoptFrameForLazyDeoptHelper(current_lazy_deopt_continuation_scope_,
                                         false);
}

DeoptFrame MaglevGraphBuilder::GetDeoptFrameForLazyDeoptHelper(
    LazyDeoptContinuationScope* continuation_scope,
    bool mark_accumulator_dead) {
  if (continuation_scope == nullptr) {
    // Potentially copy the out liveness if we want to explicitly drop the
    // accumulator.
    const compiler::BytecodeLivenessState* liveness = GetOutLiveness();
    if (mark_accumulator_dead && liveness->AccumulatorIsLive()) {
      compiler::BytecodeLivenessState* liveness_copy =
          zone()->New<compiler::BytecodeLivenessState>(*liveness, zone());
      liveness_copy->MarkAccumulatorDead();
      liveness = liveness_copy;
    }
    return InterpretedDeoptFrame(
        *compilation_unit_,
        zone()->New<CompactInterpreterFrameState>(*compilation_unit_, liveness,
                                                  current_interpreter_frame_),
        BytecodeOffset(iterator_.current_offset()), current_source_position_,
        GetParentDeoptFrame());
  }

  // Currently only support builtin continuations for bytecodes that write to
  // the accumulator
  DCHECK(
      interpreter::Bytecodes::WritesAccumulator(iterator_.current_bytecode()));
  return BuiltinContinuationDeoptFrame(
      continuation_scope->continuation(), {}, GetContext(),
      // Mark the accumulator dead in parent frames since we know that the
      // continuation will write it.
      zone()->New<DeoptFrame>(
          GetDeoptFrameForLazyDeoptHelper(continuation_scope->parent(), true)));
}

InterpretedDeoptFrame MaglevGraphBuilder::GetDeoptFrameForEntryStackCheck() {
  DCHECK_EQ(iterator_.current_offset(), 0);
  DCHECK_NULL(parent_);
  return InterpretedDeoptFrame(
      *compilation_unit_,
      zone()->New<CompactInterpreterFrameState>(
          *compilation_unit_, GetInLivenessFor(0), current_interpreter_frame_),
      BytecodeOffset(kFunctionEntryBytecodeOffset), current_source_position_,
      nullptr);
}

ValueNode* MaglevGraphBuilder::GetTaggedValue(ValueNode* value) {
  switch (value->properties().value_representation()) {
    case ValueRepresentation::kWord64:
      UNREACHABLE();
    case ValueRepresentation::kTagged:
      return value;
    case ValueRepresentation::kInt32: {
      NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
      if (node_info->tagged_alternative == nullptr) {
        if (NodeTypeIsSmi(node_info->type)) {
          node_info->tagged_alternative = AddNewNode<UnsafeSmiTag>({value});
        } else {
          node_info->tagged_alternative = AddNewNode<Int32ToNumber>({value});
        }
      }
      return node_info->tagged_alternative;
    }
    case ValueRepresentation::kUint32: {
      NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
      if (node_info->tagged_alternative == nullptr) {
        if (NodeTypeIsSmi(node_info->type)) {
          node_info->tagged_alternative = AddNewNode<UnsafeSmiTag>({value});
        } else {
          node_info->tagged_alternative = AddNewNode<Uint32ToNumber>({value});
        }
      }
      return node_info->tagged_alternative;
    }
    case ValueRepresentation::kFloat64: {
      NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
      if (node_info->tagged_alternative == nullptr) {
        node_info->tagged_alternative = AddNewNode<Float64ToTagged>({value});
      }
      return node_info->tagged_alternative;
    }
  }
  UNREACHABLE();
}

ValueNode* MaglevGraphBuilder::GetInternalizedString(
    interpreter::Register reg) {
  ValueNode* node = GetTaggedValue(reg);
  if (known_node_aspects().GetOrCreateInfoFor(node)->is_internalized_string()) {
    return node;
  }
  if (Constant* constant = node->TryCast<Constant>()) {
    if (constant->object().IsInternalizedString()) {
      SetKnownType(constant, NodeType::kInternalizedString);
      return constant;
    }
  }
  node = AddNewNode<CheckedInternalizedString>({node});
  SetKnownType(node, NodeType::kInternalizedString);
  current_interpreter_frame_.set(reg, node);
  return node;
}

namespace {
NodeType TaggedToFloat64ConversionTypeToNodeType(
    TaggedToFloat64ConversionType conversion_type) {
  switch (conversion_type) {
    case TaggedToFloat64ConversionType::kNumber:
      return NodeType::kNumber;
    case TaggedToFloat64ConversionType::kNumberOrOddball:
      return NodeType::kNumberOrOddball;
  }
}
}  // namespace

ValueNode* MaglevGraphBuilder::GetTruncatedInt32FromNumber(
    ValueNode* value, TaggedToFloat64ConversionType conversion_type) {
  switch (value->properties().value_representation()) {
    case ValueRepresentation::kWord64:
      UNREACHABLE();
    case ValueRepresentation::kTagged: {
      if (SmiConstant* constant = value->TryCast<SmiConstant>()) {
        return GetInt32Constant(constant->value().value());
      }
      NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
      if (node_info->int32_alternative != nullptr) {
        return node_info->int32_alternative;
      }
      if (node_info->truncated_int32_alternative != nullptr) {
        return node_info->truncated_int32_alternative;
      }
      NodeType old_type;
      NodeType desired_type =
          TaggedToFloat64ConversionTypeToNodeType(conversion_type);
      EnsureType(value, desired_type, &old_type);
      if (NodeTypeIsSmi(old_type)) {
        return node_info->int32_alternative =
                   AddNewNode<UnsafeSmiUntag>({value});
      }
      if (NodeTypeIs(old_type, desired_type)) {
        return node_info->truncated_int32_alternative =
                   AddNewNode<TruncateNumberOrOddballToInt32>({value},
                                                              conversion_type);
      }
      return node_info->truncated_int32_alternative =
                 AddNewNode<CheckedTruncateNumberOrOddballToInt32>(
                     {value}, conversion_type);
      UNREACHABLE();
    }
    case ValueRepresentation::kFloat64: {
      NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
      if (node_info->truncated_int32_alternative == nullptr) {
        node_info->truncated_int32_alternative =
            AddNewNode<TruncateFloat64ToInt32>({value});
      }
      return node_info->truncated_int32_alternative;
    }
    case ValueRepresentation::kInt32:
      // Already good.
      return value;
    case ValueRepresentation::kUint32:
      return AddNewNode<TruncateUint32ToInt32>({value});
  }
  UNREACHABLE();
}

ValueNode* MaglevGraphBuilder::GetTruncatedInt32(ValueNode* value) {
  switch (value->properties().value_representation()) {
    case ValueRepresentation::kWord64:
      UNREACHABLE();
    case ValueRepresentation::kTagged:
    case ValueRepresentation::kFloat64:
      return GetInt32(value);
    case ValueRepresentation::kInt32:
      // Already good.
      return value;
    case ValueRepresentation::kUint32:
      return AddNewNode<TruncateUint32ToInt32>({value});
  }
  UNREACHABLE();
}

ValueNode* MaglevGraphBuilder::GetInt32(ValueNode* value) {
  switch (value->properties().value_representation()) {
    case ValueRepresentation::kWord64:
      UNREACHABLE();
    case ValueRepresentation::kTagged: {
      if (SmiConstant* constant = value->TryCast<SmiConstant>()) {
        return GetInt32Constant(constant->value().value());
      }
      NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
      if (node_info->int32_alternative == nullptr) {
        node_info->int32_alternative = BuildSmiUntag(value);
      }
      return node_info->int32_alternative;
    }
    case ValueRepresentation::kInt32:
      return value;
    case ValueRepresentation::kUint32: {
      NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
      if (node_info->int32_alternative == nullptr) {
        if (node_info->is_smi()) {
          node_info->int32_alternative =
              AddNewNode<TruncateUint32ToInt32>({value});
        } else {
          node_info->int32_alternative =
              AddNewNode<CheckedUint32ToInt32>({value});
        }
      }
      return node_info->int32_alternative;
    }
    case ValueRepresentation::kFloat64: {
      NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
      if (node_info->int32_alternative == nullptr) {
        node_info->int32_alternative =
            AddNewNode<CheckedTruncateFloat64ToInt32>({value});
      }
      return node_info->int32_alternative;
    }
  }
  UNREACHABLE();
}

ValueNode* MaglevGraphBuilder::GetFloat64(
    ValueNode* value, TaggedToFloat64ConversionType conversion_type) {
  switch (value->properties().value_representation()) {
    case ValueRepresentation::kWord64:
      UNREACHABLE();
    case ValueRepresentation::kTagged: {
      if (Constant* constant = value->TryCast<Constant>()) {
        if (constant->object().IsHeapNumber()) {
          return GetFloat64Constant(constant->object().AsHeapNumber().value());
        }
      }
      if (SmiConstant* constant = value->TryCast<SmiConstant>()) {
        return GetFloat64Constant(constant->value().value());
      }
      if (conversion_type == TaggedToFloat64ConversionType::kNumberOrOddball) {
        if (RootConstant* constant = value->TryCast<RootConstant>()) {
          Object root_object = local_isolate_->root(constant->index());
          if (root_object.IsOddball(local_isolate_)) {
            return GetFloat64Constant(
                Oddball::cast(root_object).to_number_raw());
          }
        }
      }

      NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
      if (node_info->float64_alternative != nullptr) {
        return node_info->float64_alternative;
      }
      switch (conversion_type) {
        case TaggedToFloat64ConversionType::kNumber:
          // Number->Float64 conversions are lossless, so they can become the
          // canonical float64_alternative.
          return node_info->float64_alternative = BuildNumberOrOddballToFloat64(
                     value, TaggedToFloat64ConversionType::kNumber);
        case TaggedToFloat64ConversionType::kNumberOrOddball: {
          // NumberOrOddball->Float64 conversions are lossy, since they lose the
          // oddball information, so they can only become the canonical
          // float64_alternative if they are known number (and therefore not
          // oddball).
          ValueNode* float64_node = BuildNumberOrOddballToFloat64(
              value, TaggedToFloat64ConversionType::kNumberOrOddball);
          if (NodeTypeIsNumber(node_info->type)) {
            node_info->float64_alternative = float64_node;
          }
          return float64_node;
        }
      }
    }
    case ValueRepresentation::kInt32: {
      NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
      if (node_info->float64_alternative == nullptr) {
        node_info->float64_alternative =
            AddNewNode<ChangeInt32ToFloat64>({value});
      }
      return node_info->float64_alternative;
    }
    case ValueRepresentation::kUint32: {
      NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
      if (node_info->float64_alternative == nullptr) {
        node_info->float64_alternative =
            AddNewNode<ChangeUint32ToFloat64>({value});
      }
      return node_info->float64_alternative;
    }
    case ValueRepresentation::kFloat64:
      return value;
  }
  UNREACHABLE();
}

ValueNode* MaglevGraphBuilder::GetUint32ClampedFromNumber(ValueNode* value) {
  switch (value->properties().value_representation()) {
    case ValueRepresentation::kWord64:
      UNREACHABLE();
    case ValueRepresentation::kTagged: {
      if (SmiConstant* constant = value->TryCast<SmiConstant>()) {
        int32_t value = constant->value().value();
        if (value < 0) return GetInt32Constant(0);
        if (value > 255) return GetInt32Constant(255);
        return GetInt32Constant(constant->value().value());
      }
      NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(value);
      if (node_info->int32_alternative != nullptr) {
        return AddNewNode<Int32ToUint8Clamped>({node_info->int32_alternative});
      }
      return AddNewNode<CheckedNumberToUint8Clamped>({value});
    }
    case ValueRepresentation::kFloat64: {
      return AddNewNode<Float64ToUint8Clamped>({value});
    }
    case ValueRepresentation::kInt32:
      return AddNewNode<Int32ToUint8Clamped>({value});
    case ValueRepresentation::kUint32:
      return AddNewNode<Uint32ToUint8Clamped>({value});
  }
  UNREACHABLE();
}

namespace {
template <Operation kOperation>
struct NodeForOperationHelper;

#define NODE_FOR_OPERATION_HELPER(Name)               \
  template <>                                         \
  struct NodeForOperationHelper<Operation::k##Name> { \
    using generic_type = Generic##Name;               \
  };
OPERATION_LIST(NODE_FOR_OPERATION_HELPER)
#undef NODE_FOR_OPERATION_HELPER

template <Operation kOperation>
using GenericNodeForOperation =
    typename NodeForOperationHelper<kOperation>::generic_type;

// Bitwise operations reinterprets the numeric input as Int32 bits for a
// bitwise operation, which means we want to do slightly different conversions.
template <Operation kOperation>
constexpr bool BinaryOperationIsBitwiseInt32() {
  switch (kOperation) {
    case Operation::kBitwiseNot:
    case Operation::kBitwiseAnd:
    case Operation::kBitwiseOr:
    case Operation::kBitwiseXor:
    case Operation::kShiftLeft:
    case Operation::kShiftRight:
    case Operation::kShiftRightLogical:
      return true;
    default:
      return false;
  }
}
}  // namespace

// MAP_OPERATION_TO_NODES are tuples with the following format:
// - Operation name,
// - Int32 operation node,
// - Identity of int32 operation (e.g, 0 for add/sub and 1 for mul/div), if it
//   exists, or otherwise {}.
#define MAP_BINARY_OPERATION_TO_INT32_NODE(V) \
  V(Add, Int32AddWithOverflow, 0)             \
  V(Subtract, Int32SubtractWithOverflow, 0)   \
  V(Multiply, Int32MultiplyWithOverflow, 1)   \
  V(Divide, Int32DivideWithOverflow, 1)       \
  V(Modulus, Int32ModulusWithOverflow, {})    \
  V(BitwiseAnd, Int32BitwiseAnd, ~0)          \
  V(BitwiseOr, Int32BitwiseOr, 0)             \
  V(BitwiseXor, Int32BitwiseXor, 0)           \
  V(ShiftLeft, Int32ShiftLeft, 0)             \
  V(ShiftRight, Int32ShiftRight, 0)           \
  V(ShiftRightLogical, Int32ShiftRightLogical, {})

#define MAP_UNARY_OPERATION_TO_INT32_NODE(V) \
  V(BitwiseNot, Int32BitwiseNot)             \
  V(Increment, Int32IncrementWithOverflow)   \
  V(Decrement, Int32DecrementWithOverflow)   \
  V(Negate, Int32NegateWithOverflow)

#define MAP_COMPARE_OPERATION_TO_INT32_NODE(V) \
  V(Equal, Int32Equal)                         \
  V(StrictEqual, Int32StrictEqual)             \
  V(LessThan, Int32LessThan)                   \
  V(LessThanOrEqual, Int32LessThanOrEqual)     \
  V(GreaterThan, Int32GreaterThan)             \
  V(GreaterThanOrEqual, Int32GreaterThanOrEqual)

// MAP_OPERATION_TO_FLOAT64_NODE are tuples with the following format:
// (Operation name, Float64 operation node).
#define MAP_OPERATION_TO_FLOAT64_NODE(V) \
  V(Add, Float64Add)                     \
  V(Subtract, Float64Subtract)           \
  V(Multiply, Float64Multiply)           \
  V(Divide, Float64Divide)               \
  V(Modulus, Float64Modulus)             \
  V(Negate, Float64Negate)               \
  V(Exponentiate, Float64Exponentiate)

#define MAP_COMPARE_OPERATION_TO_FLOAT64_NODE(V) \
  V(Equal, Float64Equal)                         \
  V(StrictEqual, Float64StrictEqual)             \
  V(LessThan, Float64LessThan)                   \
  V(LessThanOrEqual, Float64LessThanOrEqual)     \
  V(GreaterThan, Float64GreaterThan)             \
  V(GreaterThanOrEqual, Float64GreaterThanOrEqual)

template <Operation kOperation>
static constexpr base::Optional<int> Int32Identity() {
  switch (kOperation) {
#define CASE(op, _, identity) \
  case Operation::k##op:      \
    return identity;
    MAP_BINARY_OPERATION_TO_INT32_NODE(CASE)
#undef CASE
    default:
      UNREACHABLE();
  }
}

namespace {
template <Operation kOperation>
struct Int32NodeForHelper;
#define SPECIALIZATION(op, OpNode, ...)         \
  template <>                                   \
  struct Int32NodeForHelper<Operation::k##op> { \
    using type = OpNode;                        \
  };
MAP_UNARY_OPERATION_TO_INT32_NODE(SPECIALIZATION)
MAP_BINARY_OPERATION_TO_INT32_NODE(SPECIALIZATION)
MAP_COMPARE_OPERATION_TO_INT32_NODE(SPECIALIZATION)
#undef SPECIALIZATION

template <Operation kOperation>
using Int32NodeFor = typename Int32NodeForHelper<kOperation>::type;

template <Operation kOperation>
struct Float64NodeForHelper;
#define SPECIALIZATION(op, OpNode)                \
  template <>                                     \
  struct Float64NodeForHelper<Operation::k##op> { \
    using type = OpNode;                          \
  };
MAP_OPERATION_TO_FLOAT64_NODE(SPECIALIZATION)
MAP_COMPARE_OPERATION_TO_FLOAT64_NODE(SPECIALIZATION)
#undef SPECIALIZATION

template <Operation kOperation>
using Float64NodeFor = typename Float64NodeForHelper<kOperation>::type;
}  // namespace

template <Operation kOperation>
void MaglevGraphBuilder::BuildGenericUnaryOperationNode() {
  FeedbackSlot slot_index = GetSlotOperand(0);
  ValueNode* value = GetAccumulatorTagged();
  SetAccumulator(AddNewNode<GenericNodeForOperation<kOperation>>(
      {value}, compiler::FeedbackSource{feedback(), slot_index}));
}

template <Operation kOperation>
void MaglevGraphBuilder::BuildGenericBinaryOperationNode() {
  ValueNode* left = LoadRegisterTagged(0);
  ValueNode* right = GetAccumulatorTagged();
  FeedbackSlot slot_index = GetSlotOperand(1);
  SetAccumulator(AddNewNode<GenericNodeForOperation<kOperation>>(
      {left, right}, compiler::FeedbackSource{feedback(), slot_index}));
}

template <Operation kOperation>
void MaglevGraphBuilder::BuildGenericBinarySmiOperationNode() {
  ValueNode* left = GetAccumulatorTagged();
  int constant = iterator_.GetImmediateOperand(0);
  ValueNode* right = GetSmiConstant(constant);
  FeedbackSlot slot_index = GetSlotOperand(1);
  SetAccumulator(AddNewNode<GenericNodeForOperation<kOperation>>(
      {left, right}, compiler::FeedbackSource{feedback(), slot_index}));
}

template <Operation kOperation>
void MaglevGraphBuilder::BuildInt32UnaryOperationNode() {
  static const bool input_is_truncated =
      BinaryOperationIsBitwiseInt32<kOperation>();
  // TODO(v8:7700): Do constant folding.
  ValueNode* value = input_is_truncated ? GetAccumulatorTruncatedInt32()
                                        : GetAccumulatorInt32();
  using OpNodeT = Int32NodeFor<kOperation>;
  SetAccumulator(AddNewNode<OpNodeT>({value}));
}

void MaglevGraphBuilder::BuildTruncatingInt32BitwiseNotForNumber(
    TaggedToFloat64ConversionType conversion_type) {
  // TODO(v8:7700): Do constant folding.
  ValueNode* value = GetTruncatedInt32FromNumber(
      current_interpreter_frame_.accumulator(), conversion_type);
  SetAccumulator(AddNewNode<Int32BitwiseNot>({value}));
}

template <Operation kOperation>
ValueNode* MaglevGraphBuilder::TryFoldInt32BinaryOperation(ValueNode* left,
                                                           ValueNode* right) {
  switch (kOperation) {
    case Operation::kModulus:
      // Note the `x % x = 0` fold is invalid since for negative x values the
      // result is -0.0.
      // TODO(v8:7700): Consider re-enabling this fold if the result is used
      // only in contexts where -0.0 is semantically equivalent to 0.0, or if x
      // is known to be non-negative.
    default:
      // TODO(victorgomes): Implement more folds.
      break;
  }
  return nullptr;
}

template <Operation kOperation>
ValueNode* MaglevGraphBuilder::TryFoldInt32BinaryOperation(ValueNode* left,
                                                           int right) {
  switch (kOperation) {
    case Operation::kModulus:
      // Note the `x % 1 = 0` and `x % -1 = 0` folds are invalid since for
      // negative x values the result is -0.0.
      // TODO(v8:7700): Consider re-enabling this fold if the result is used
      // only in contexts where -0.0 is semantically equivalent to 0.0, or if x
      // is known to be non-negative.
      // TODO(victorgomes): We can emit faster mod operation if {right} is power
      // of 2, unfortunately we need to know if {left} is negative or not.
      // Maybe emit a Int32ModulusRightIsPowerOf2?
    default:
      // TODO(victorgomes): Implement more folds.
      break;
  }
  return nullptr;
}

template <Operation kOperation>
void MaglevGraphBuilder::BuildInt32BinaryOperationNode() {
  // Truncating Int32 nodes treat their input as a signed int32 regardless
  // of whether it's really signed or not, so we allow Uint32 by loading a
  // TruncatedInt32 value.
  static const bool inputs_are_truncated =
      BinaryOperationIsBitwiseInt32<kOperation>();
  // TODO(v8:7700): Do constant folding.
  ValueNode* left = inputs_are_truncated ? LoadRegisterTruncatedInt32(0)
                                         : LoadRegisterInt32(0);
  ValueNode* right = inputs_are_truncated ? GetAccumulatorTruncatedInt32()
                                          : GetAccumulatorInt32();

  if (ValueNode* result =
          TryFoldInt32BinaryOperation<kOperation>(left, right)) {
    SetAccumulator(result);
    return;
  }
  using OpNodeT = Int32NodeFor<kOperation>;

  SetAccumulator(AddNewNode<OpNodeT>({left, right}));
}

template <Operation kOperation>
void MaglevGraphBuilder::BuildTruncatingInt32BinaryOperationNodeForNumber(
    TaggedToFloat64ConversionType conversion_type) {
  DCHECK(BinaryOperationIsBitwiseInt32<kOperation>());
  // TODO(v8:7700): Do constant folding.
  ValueNode* left;
  ValueNode* right;
  if (IsRegisterEqualToAccumulator(0)) {
    left = right = GetTruncatedInt32FromNumber(
        current_interpreter_frame_.get(iterator_.GetRegisterOperand(0)),
        conversion_type);
  } else {
    left = GetTruncatedInt32FromNumber(
        current_interpreter_frame_.get(iterator_.GetRegisterOperand(0)),
        conversion_type);
    right = GetTruncatedInt32FromNumber(
        current_interpreter_frame_.accumulator(), conversion_type);
  }

  if (ValueNode* result =
          TryFoldInt32BinaryOperation<kOperation>(left, right)) {
    SetAccumulator(result);
    return;
  }
  SetAccumulator(AddNewNode<Int32NodeFor<kOperation>>({left, right}));
}

template <Operation kOperation>
void MaglevGraphBuilder::BuildInt32BinarySmiOperationNode() {
  // Truncating Int32 nodes treat their input as a signed int32 regardless
  // of whether it's really signed or not, so we allow Uint32 by loading a
  // TruncatedInt32 value.
  static const bool inputs_are_truncated =
      BinaryOperationIsBitwiseInt32<kOperation>();
  // TODO(v8:7700): Do constant folding.
  ValueNode* left = inputs_are_truncated ? GetAccumulatorTruncatedInt32()
                                         : GetAccumulatorInt32();
  int32_t constant = iterator_.GetImmediateOperand(0);
  if (base::Optional<int>(constant) == Int32Identity<kOperation>()) {
    // If the constant is the unit of the operation, it already has the right
    // value, so use the truncated value if necessary (and if not just a
    // conversion) and return.
    if (inputs_are_truncated && !left->properties().is_conversion()) {
      current_interpreter_frame_.set_accumulator(left);
    }
    return;
  }
  if (ValueNode* result =
          TryFoldInt32BinaryOperation<kOperation>(left, constant)) {
    SetAccumulator(result);
    return;
  }
  ValueNode* right = GetInt32Constant(constant);

  using OpNodeT = Int32NodeFor<kOperation>;

  SetAccumulator(AddNewNode<OpNodeT>({left, right}));
}

template <Operation kOperation>
void MaglevGraphBuilder::BuildTruncatingInt32BinarySmiOperationNodeForNumber(
    TaggedToFloat64ConversionType conversion_type) {
  DCHECK(BinaryOperationIsBitwiseInt32<kOperation>());
  // TODO(v8:7700): Do constant folding.
  ValueNode* left = GetTruncatedInt32FromNumber(
      current_interpreter_frame_.accumulator(), conversion_type);
  int32_t constant = iterator_.GetImmediateOperand(0);
  if (base::Optional<int>(constant) == Int32Identity<kOperation>()) {
    // If the constant is the unit of the operation, it already has the right
    // value, so use the truncated value (if not just a conversion) and return.
    if (!left->properties().is_conversion()) {
      current_interpreter_frame_.set_accumulator(left);
    }
    return;
  }
  if (ValueNode* result =
          TryFoldInt32BinaryOperation<kOperation>(left, constant)) {
    SetAccumulator(result);
    return;
  }
  ValueNode* right = GetInt32Constant(constant);
  SetAccumulator(AddNewNode<Int32NodeFor<kOperation>>({left, right}));
}

template <Operation kOperation>
void MaglevGraphBuilder::BuildFloat64BinarySmiOperationNode(
    TaggedToFloat64ConversionType conversion_type) {
  // TODO(v8:7700): Do constant folding.
  ValueNode* left = GetAccumulatorFloat64(conversion_type);
  double constant = static_cast<double>(iterator_.GetImmediateOperand(0));
  ValueNode* right = GetFloat64Constant(constant);
  SetAccumulator(AddNewNode<Float64NodeFor<kOperation>>({left, right}));
}

template <Operation kOperation>
void MaglevGraphBuilder::BuildFloat64UnaryOperationNode(
    TaggedToFloat64ConversionType conversion_type) {
  // TODO(v8:7700): Do constant folding.
  ValueNode* value = GetAccumulatorFloat64(conversion_type);
  switch (kOperation) {
    case Operation::kNegate:
      SetAccumulator(AddNewNode<Float64Negate>({value}));
      break;
    case Operation::kIncrement:
      SetAccumulator(AddNewNode<Float64Add>({value, GetFloat64Constant(1)}));
      break;
    case Operation::kDecrement:
      SetAccumulator(
          AddNewNode<Float64Subtract>({value, GetFloat64Constant(1)}));
      break;
    default:
      UNREACHABLE();
  }
}

template <Operation kOperation>
void MaglevGraphBuilder::BuildFloat64BinaryOperationNode(
    TaggedToFloat64ConversionType conversion_type) {
  // TODO(v8:7700): Do constant folding.
  ValueNode* left = LoadRegisterFloat64(0, conversion_type);
  ValueNode* right = GetAccumulatorFloat64(conversion_type);
  SetAccumulator(AddNewNode<Float64NodeFor<kOperation>>({left, right}));
}

namespace {
TaggedToFloat64ConversionType HintToTaggedToFloat64ConversionType(
    BinaryOperationHint hint) {
  switch (hint) {
    case BinaryOperationHint::kSignedSmallInputs:
    case BinaryOperationHint::kNumber:
      return TaggedToFloat64ConversionType::kNumber;
    case BinaryOperationHint::kNumberOrOddball:
      return TaggedToFloat64ConversionType::kNumberOrOddball;

    case BinaryOperationHint::kNone:
    case BinaryOperationHint::kSignedSmall:
    case BinaryOperationHint::kString:
    case BinaryOperationHint::kBigInt:
    case BinaryOperationHint::kBigInt64:
    case BinaryOperationHint::kAny:
      UNREACHABLE();
  }
}
}  // namespace

template <Operation kOperation>
void MaglevGraphBuilder::VisitUnaryOperation() {
  FeedbackNexus nexus = FeedbackNexusForOperand(0);
  switch (nexus.GetBinaryOperationFeedback()) {
    case BinaryOperationHint::kNone:
      return EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForBinaryOperation);
    case BinaryOperationHint::kSignedSmall:
      return BuildInt32UnaryOperationNode<kOperation>();
    case BinaryOperationHint::kSignedSmallInputs:
    case BinaryOperationHint::kNumber:
    case BinaryOperationHint::kNumberOrOddball: {
      TaggedToFloat64ConversionType conversion_type =
          HintToTaggedToFloat64ConversionType(
              nexus.GetBinaryOperationFeedback());
      if constexpr (BinaryOperationIsBitwiseInt32<kOperation>()) {
        static_assert(kOperation == Operation::kBitwiseNot);
        return BuildTruncatingInt32BitwiseNotForNumber(conversion_type);
      }
      return BuildFloat64UnaryOperationNode<kOperation>(conversion_type);
      break;
    }
    case BinaryOperationHint::kString:
    case BinaryOperationHint::kBigInt:
    case BinaryOperationHint::kBigInt64:
    case BinaryOperationHint::kAny:
      // Fallback to generic node.
      break;
  }
  BuildGenericUnaryOperationNode<kOperation>();
}

template <Operation kOperation>
void MaglevGraphBuilder::VisitBinaryOperation() {
  FeedbackNexus nexus = FeedbackNexusForOperand(1);
  switch (nexus.GetBinaryOperationFeedback()) {
    case BinaryOperationHint::kNone:
      return EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForBinaryOperation);
    case BinaryOperationHint::kSignedSmall:
      if constexpr (kOperation == Operation::kExponentiate) {
        // Exponentiate never updates the feedback to be a Smi.
        UNREACHABLE();
      } else {
        return BuildInt32BinaryOperationNode<kOperation>();
      }
    case BinaryOperationHint::kSignedSmallInputs:
    case BinaryOperationHint::kNumber:
    case BinaryOperationHint::kNumberOrOddball: {
      TaggedToFloat64ConversionType conversion_type =
          HintToTaggedToFloat64ConversionType(
              nexus.GetBinaryOperationFeedback());
      if constexpr (BinaryOperationIsBitwiseInt32<kOperation>()) {
        return BuildTruncatingInt32BinaryOperationNodeForNumber<kOperation>(
            conversion_type);
      } else {
        return BuildFloat64BinaryOperationNode<kOperation>(conversion_type);
      }
      break;
    }
    case BinaryOperationHint::kString:
    case BinaryOperationHint::kBigInt:
    case BinaryOperationHint::kBigInt64:
    case BinaryOperationHint::kAny:
      // Fallback to generic node.
      break;
  }
  BuildGenericBinaryOperationNode<kOperation>();
}

template <Operation kOperation>
void MaglevGraphBuilder::VisitBinarySmiOperation() {
  FeedbackNexus nexus = FeedbackNexusForOperand(1);
  switch (nexus.GetBinaryOperationFeedback()) {
    case BinaryOperationHint::kNone:
      return EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForBinaryOperation);
    case BinaryOperationHint::kSignedSmall:
      if constexpr (kOperation == Operation::kExponentiate) {
        // Exponentiate never updates the feedback to be a Smi.
        UNREACHABLE();
      } else {
        return BuildInt32BinarySmiOperationNode<kOperation>();
      }
    case BinaryOperationHint::kSignedSmallInputs:
    case BinaryOperationHint::kNumber:
    case BinaryOperationHint::kNumberOrOddball: {
      TaggedToFloat64ConversionType conversion_type =
          HintToTaggedToFloat64ConversionType(
              nexus.GetBinaryOperationFeedback());
      if constexpr (BinaryOperationIsBitwiseInt32<kOperation>()) {
        return BuildTruncatingInt32BinarySmiOperationNodeForNumber<kOperation>(
            conversion_type);
      } else {
        return BuildFloat64BinarySmiOperationNode<kOperation>(conversion_type);
      }
      break;
    }
    case BinaryOperationHint::kString:
    case BinaryOperationHint::kBigInt:
    case BinaryOperationHint::kBigInt64:
    case BinaryOperationHint::kAny:
      // Fallback to generic node.
      break;
  }
  BuildGenericBinarySmiOperationNode<kOperation>();
}

template <typename BranchControlNodeT, typename... Args>
bool MaglevGraphBuilder::TryBuildBranchFor(
    std::initializer_list<ValueNode*> control_inputs, Args&&... args) {
  // Don't emit the shortcut branch if the next bytecode is a merge target.
  if (IsOffsetAMergePoint(next_offset())) return false;

  interpreter::Bytecode next_bytecode = iterator_.next_bytecode();
  int true_offset, false_offset;
  int true_interrupt_correction, false_interrupt_correction;
  switch (next_bytecode) {
    case interpreter::Bytecode::kJumpIfFalse:
    case interpreter::Bytecode::kJumpIfFalseConstant:
    case interpreter::Bytecode::kJumpIfToBooleanFalse:
    case interpreter::Bytecode::kJumpIfToBooleanFalseConstant:
      // This jump must kill the accumulator, otherwise we need to
      // materialize the actual boolean value.
      if (GetOutLivenessFor(next_offset())->AccumulatorIsLive()) return false;
      // Advance the iterator past the test to the jump, skipping
      // emitting the test.
      iterator_.Advance();
      true_offset = next_offset();
      true_interrupt_correction = 0;
      false_offset = iterator_.GetJumpTargetOffset();
      false_interrupt_correction = iterator_.GetRelativeJumpTargetOffset();
      break;
    case interpreter::Bytecode::kJumpIfTrue:
    case interpreter::Bytecode::kJumpIfTrueConstant:
    case interpreter::Bytecode::kJumpIfToBooleanTrue:
    case interpreter::Bytecode::kJumpIfToBooleanTrueConstant:
      // This jump must kill the accumulator, otherwise we need to
      // materialize the actual boolean value.
      if (GetOutLivenessFor(next_offset())->AccumulatorIsLive()) return false;
      // Advance the iterator past the test to the jump, skipping
      // emitting the test.
      iterator_.Advance();
      true_offset = iterator_.GetJumpTargetOffset();
      true_interrupt_correction = iterator_.GetRelativeJumpTargetOffset();
      false_offset = next_offset();
      false_interrupt_correction = 0;
      break;
    default:
      return false;
  }

  BasicBlock* block = FinishBlock<BranchControlNodeT>(
      control_inputs, std::forward<Args>(args)..., &jump_targets_[true_offset],
      true_interrupt_correction, &jump_targets_[false_offset],
      false_interrupt_correction);

  MergeIntoFrameState(block, iterator_.GetJumpTargetOffset());
  StartFallthroughBlock(next_offset(), block);
  return true;
}

template <Operation kOperation>
void MaglevGraphBuilder::VisitCompareOperation() {
  FeedbackNexus nexus = FeedbackNexusForOperand(1);
  switch (nexus.GetCompareOperationFeedback()) {
    case CompareOperationHint::kNone:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForCompareOperation);
      return;
    case CompareOperationHint::kSignedSmall: {
      ValueNode* left = LoadRegisterInt32(0);
      ValueNode* right = GetAccumulatorInt32();
      if (TryBuildBranchFor<BranchIfInt32Compare>({left, right}, kOperation)) {
        return;
      }
      SetAccumulator(AddNewNode<Int32NodeFor<kOperation>>({left, right}));
      return;
    }
    case CompareOperationHint::kNumber: {
      // TODO(leszeks): we could support kNumberOrOddball with
      // BranchIfFloat64Compare, but we'd need to special case comparing
      // oddballs with NaN value (e.g. undefined) against themselves.
      ValueNode* left =
          LoadRegisterFloat64(0, TaggedToFloat64ConversionType::kNumber);
      ValueNode* right =
          GetAccumulatorFloat64(TaggedToFloat64ConversionType::kNumber);
      if (TryBuildBranchFor<BranchIfFloat64Compare>({left, right},
                                                    kOperation)) {
        return;
      }
      SetAccumulator(AddNewNode<Float64NodeFor<kOperation>>({left, right}));
      return;
    }
    case CompareOperationHint::kInternalizedString: {
      DCHECK(kOperation == Operation::kEqual ||
             kOperation == Operation::kStrictEqual);
      ValueNode *left, *right;
      if (IsRegisterEqualToAccumulator(0)) {
        left = right = GetInternalizedString(iterator_.GetRegisterOperand(0));
        current_interpreter_frame_.set(
            interpreter::Register::virtual_accumulator(), left);
      } else {
        left = GetInternalizedString(iterator_.GetRegisterOperand(0));
        right =
            GetInternalizedString(interpreter::Register::virtual_accumulator());
      }
      if (TryBuildBranchFor<BranchIfReferenceCompare>({left, right},
                                                      kOperation)) {
        return;
      }
      SetAccumulator(AddNewNode<TaggedEqual>({left, right}));
      return;
    }
    case CompareOperationHint::kSymbol: {
      DCHECK(kOperation == Operation::kEqual ||
             kOperation == Operation::kStrictEqual);
      ValueNode* left = LoadRegisterTagged(0);
      ValueNode* right = GetAccumulatorTagged();
      BuildCheckSymbol(left);
      BuildCheckSymbol(right);
      if (TryBuildBranchFor<BranchIfReferenceCompare>({left, right},
                                                      kOperation)) {
        return;
      }
      SetAccumulator(AddNewNode<TaggedEqual>({left, right}));
      return;
    }
    case CompareOperationHint::kNumberOrOddball:
    case CompareOperationHint::kNumberOrBoolean:
    case CompareOperationHint::kString:
    case CompareOperationHint::kBigInt:
    case CompareOperationHint::kBigInt64:
    case CompareOperationHint::kReceiver:
    case CompareOperationHint::kReceiverOrNullOrUndefined:
    case CompareOperationHint::kAny:
      // Fallback to generic node.
      break;
  }
  BuildGenericBinaryOperationNode<kOperation>();
}

void MaglevGraphBuilder::VisitLdar() {
  MoveNodeBetweenRegisters(iterator_.GetRegisterOperand(0),
                           interpreter::Register::virtual_accumulator());
}

void MaglevGraphBuilder::VisitLdaZero() { SetAccumulator(GetSmiConstant(0)); }
void MaglevGraphBuilder::VisitLdaSmi() {
  int constant = iterator_.GetImmediateOperand(0);
  SetAccumulator(GetSmiConstant(constant));
}
void MaglevGraphBuilder::VisitLdaUndefined() {
  SetAccumulator(GetRootConstant(RootIndex::kUndefinedValue));
}
void MaglevGraphBuilder::VisitLdaNull() {
  SetAccumulator(GetRootConstant(RootIndex::kNullValue));
}
void MaglevGraphBuilder::VisitLdaTheHole() {
  SetAccumulator(GetRootConstant(RootIndex::kTheHoleValue));
}
void MaglevGraphBuilder::VisitLdaTrue() {
  SetAccumulator(GetRootConstant(RootIndex::kTrueValue));
}
void MaglevGraphBuilder::VisitLdaFalse() {
  SetAccumulator(GetRootConstant(RootIndex::kFalseValue));
}
void MaglevGraphBuilder::VisitLdaConstant() {
  SetAccumulator(GetConstant(GetRefOperand<HeapObject>(0)));
}

bool MaglevGraphBuilder::TrySpecializeLoadContextSlotToFunctionContext(
    ValueNode** context, size_t* depth, int slot_index,
    ContextSlotMutability slot_mutability) {
  DCHECK(compilation_unit_->info()->specialize_to_function_context());

  size_t new_depth = *depth;
  compiler::OptionalContextRef maybe_context_ref =
      FunctionContextSpecialization::TryToRef(compilation_unit_, *context,
                                              &new_depth);
  if (!maybe_context_ref.has_value()) return false;

  compiler::ContextRef context_ref = maybe_context_ref.value();
  if (slot_mutability == kMutable || new_depth != 0) {
    *depth = new_depth;
    *context = GetConstant(context_ref);
    return false;
  }

  compiler::OptionalObjectRef maybe_slot_value =
      context_ref.get(broker(), slot_index);
  if (!maybe_slot_value.has_value()) {
    *depth = new_depth;
    *context = GetConstant(context_ref);
    return false;
  }

  compiler::ObjectRef slot_value = maybe_slot_value.value();
  if (slot_value.IsHeapObject()) {
    // Even though the context slot is immutable, the context might have escaped
    // before the function to which it belongs has initialized the slot.  We
    // must be conservative and check if the value in the slot is currently the
    // hole or undefined. Only if it is neither of these, can we be sure that it
    // won't change anymore.
    //
    // See also: JSContextSpecialization::ReduceJSLoadContext.
    compiler::OddballType oddball_type =
        slot_value.AsHeapObject().map(broker()).oddball_type(broker());
    if (oddball_type == compiler::OddballType::kUndefined ||
        oddball_type == compiler::OddballType::kHole) {
      *depth = new_depth;
      *context = GetConstant(context_ref);
      return false;
    }
  }

  // Fold the load of the immutable slot.

  SetAccumulator(GetConstant(slot_value));
  return true;
}

ValueNode* MaglevGraphBuilder::LoadAndCacheContextSlot(
    ValueNode* context, int offset, ContextSlotMutability slot_mutability) {
  ValueNode*& cached_value =
      slot_mutability == ContextSlotMutability::kMutable
          ? known_node_aspects().loaded_context_slots[{context, offset}]
          : known_node_aspects().loaded_context_constants[{context, offset}];
  if (cached_value) {
    if (v8_flags.trace_maglev_graph_building) {
      std::cout << "  * Reusing cached context slot "
                << PrintNodeLabel(graph_labeller(), context) << "[" << offset
                << "]: " << PrintNode(graph_labeller(), cached_value)
                << std::endl;
    }
    return cached_value;
  }
  return cached_value = AddNewNode<LoadTaggedField>({context}, offset);
}

void MaglevGraphBuilder::StoreAndCacheContextSlot(ValueNode* context,
                                                  int offset,
                                                  ValueNode* value) {
  DCHECK_EQ(
      known_node_aspects().loaded_context_constants.count({context, offset}),
      0);
  BuildStoreTaggedField(context, GetTaggedValue(value), offset);

  if (v8_flags.trace_maglev_graph_building) {
    std::cout << "  * Recording context slot store "
              << PrintNodeLabel(graph_labeller(), context) << "[" << offset
              << "]: " << PrintNode(graph_labeller(), value) << std::endl;
  }
  known_node_aspects().loaded_context_slots[{context, offset}] = value;
}

void MaglevGraphBuilder::BuildLoadContextSlot(
    ValueNode* context, size_t depth, int slot_index,
    ContextSlotMutability slot_mutability) {
  MinimizeContextChainDepth(&context, &depth);

  if (compilation_unit_->info()->specialize_to_function_context() &&
      TrySpecializeLoadContextSlotToFunctionContext(
          &context, &depth, slot_index, slot_mutability)) {
    return;  // Our work here is done.
  }

  for (size_t i = 0; i < depth; ++i) {
    context = LoadAndCacheContextSlot(
        context, Context::OffsetOfElementAt(Context::PREVIOUS_INDEX),
        kImmutable);
  }

  // Always load the slot here as if it were mutable. Immutable slots have a
  // narrow range of mutability if the context escapes before the slot is
  // initialized, so we can't safely assume that the load can be cached in case
  // it's a load before initialization (e.g. var a = a + 42).
  current_interpreter_frame_.set_accumulator(LoadAndCacheContextSlot(
      context, Context::OffsetOfElementAt(slot_index), kMutable));
}

void MaglevGraphBuilder::BuildStoreContextSlot(ValueNode* context, size_t depth,
                                               int slot_index,
                                               ValueNode* value) {
  MinimizeContextChainDepth(&context, &depth);

  if (compilation_unit_->info()->specialize_to_function_context()) {
    compiler::OptionalContextRef maybe_ref =
        FunctionContextSpecialization::TryToRef(compilation_unit_, context,
                                                &depth);
    if (maybe_ref.has_value()) {
      context = GetConstant(maybe_ref.value());
    }
  }

  for (size_t i = 0; i < depth; ++i) {
    context = LoadAndCacheContextSlot(
        context, Context::OffsetOfElementAt(Context::PREVIOUS_INDEX),
        kImmutable);
  }

  StoreAndCacheContextSlot(context, Context::OffsetOfElementAt(slot_index),
                           value);
}

void MaglevGraphBuilder::VisitLdaContextSlot() {
  ValueNode* context = LoadRegisterTagged(0);
  int slot_index = iterator_.GetIndexOperand(1);
  size_t depth = iterator_.GetUnsignedImmediateOperand(2);
  BuildLoadContextSlot(context, depth, slot_index, kMutable);
}
void MaglevGraphBuilder::VisitLdaImmutableContextSlot() {
  ValueNode* context = LoadRegisterTagged(0);
  int slot_index = iterator_.GetIndexOperand(1);
  size_t depth = iterator_.GetUnsignedImmediateOperand(2);
  BuildLoadContextSlot(context, depth, slot_index, kImmutable);
}
void MaglevGraphBuilder::VisitLdaCurrentContextSlot() {
  ValueNode* context = GetContext();
  int slot_index = iterator_.GetIndexOperand(0);
  BuildLoadContextSlot(context, 0, slot_index, kMutable);
}
void MaglevGraphBuilder::VisitLdaImmutableCurrentContextSlot() {
  ValueNode* context = GetContext();
  int slot_index = iterator_.GetIndexOperand(0);
  BuildLoadContextSlot(context, 0, slot_index, kImmutable);
}

void MaglevGraphBuilder::VisitStaContextSlot() {
  ValueNode* context = LoadRegisterTagged(0);
  int slot_index = iterator_.GetIndexOperand(1);
  size_t depth = iterator_.GetUnsignedImmediateOperand(2);
  BuildStoreContextSlot(context, depth, slot_index, GetRawAccumulator());
}
void MaglevGraphBuilder::VisitStaCurrentContextSlot() {
  ValueNode* context = GetContext();
  int slot_index = iterator_.GetIndexOperand(0);
  BuildStoreContextSlot(context, 0, slot_index, GetRawAccumulator());
}

void MaglevGraphBuilder::VisitStar() {
  MoveNodeBetweenRegisters(interpreter::Register::virtual_accumulator(),
                           iterator_.GetRegisterOperand(0));
}
#define SHORT_STAR_VISITOR(Name, ...)                                          \
  void MaglevGraphBuilder::Visit##Name() {                                     \
    MoveNodeBetweenRegisters(                                                  \
        interpreter::Register::virtual_accumulator(),                          \
        interpreter::Register::FromShortStar(interpreter::Bytecode::k##Name)); \
  }
SHORT_STAR_BYTECODE_LIST(SHORT_STAR_VISITOR)
#undef SHORT_STAR_VISITOR

void MaglevGraphBuilder::VisitMov() {
  MoveNodeBetweenRegisters(iterator_.GetRegisterOperand(0),
                           iterator_.GetRegisterOperand(1));
}

void MaglevGraphBuilder::VisitPushContext() {
  MoveNodeBetweenRegisters(interpreter::Register::current_context(),
                           iterator_.GetRegisterOperand(0));
  SetContext(GetAccumulatorTagged());
}

void MaglevGraphBuilder::VisitPopContext() {
  SetContext(LoadRegisterTagged(0));
}

void MaglevGraphBuilder::VisitTestReferenceEqual() {
  ValueNode* lhs = LoadRegisterTagged(0);
  ValueNode* rhs = GetAccumulatorTagged();
  if (lhs == rhs) {
    SetAccumulator(GetRootConstant(RootIndex::kTrueValue));
    return;
  }
  if (TryBuildBranchFor<BranchIfReferenceCompare>({lhs, rhs},
                                                  Operation::kStrictEqual)) {
    return;
  }
  SetAccumulator(AddNewNode<TaggedEqual>({lhs, rhs}));
}

void MaglevGraphBuilder::VisitTestUndetectable() {
  ValueNode* value = GetAccumulatorTagged();
  SetAccumulator(AddNewNode<TestUndetectable>({value}));
}

void MaglevGraphBuilder::VisitTestNull() {
  ValueNode* value = GetAccumulatorTagged();
  if (IsConstantNode(value->opcode())) {
    SetAccumulator(GetBooleanConstant(IsNullValue(value)));
    return;
  }
  ValueNode* null_constant = GetRootConstant(RootIndex::kNullValue);
  SetAccumulator(AddNewNode<TaggedEqual>({value, null_constant}));
}

void MaglevGraphBuilder::VisitTestUndefined() {
  ValueNode* value = GetAccumulatorTagged();
  if (IsConstantNode(value->opcode())) {
    SetAccumulator(GetBooleanConstant(IsUndefinedValue(value)));
    return;
  }
  ValueNode* undefined_constant = GetRootConstant(RootIndex::kUndefinedValue);
  SetAccumulator(AddNewNode<TaggedEqual>({value, undefined_constant}));
}

void MaglevGraphBuilder::VisitTestTypeOf() {
  using LiteralFlag = interpreter::TestTypeOfFlags::LiteralFlag;
  // TODO(v8:7700): Add a branch version of TestTypeOf that does not need to
  // materialise the boolean value.
  LiteralFlag literal =
      interpreter::TestTypeOfFlags::Decode(GetFlag8Operand(0));
  if (literal == LiteralFlag::kOther) {
    SetAccumulator(GetRootConstant(RootIndex::kFalseValue));
    return;
  }
  ValueNode* value = GetAccumulatorTagged();
  if (TryBuildBranchFor<BranchIfTypeOf>({value}, literal)) {
    return;
  }
  SetAccumulator(AddNewNode<TestTypeOf>({value}, literal));
}

bool MaglevGraphBuilder::TryBuildScriptContextConstantAccess(
    const compiler::GlobalAccessFeedback& global_access_feedback) {
  if (!global_access_feedback.immutable()) return false;

  compiler::OptionalObjectRef maybe_slot_value =
      global_access_feedback.script_context().get(
          broker(), global_access_feedback.slot_index());
  if (!maybe_slot_value) return false;

  SetAccumulator(GetConstant(maybe_slot_value.value()));
  return true;
}

bool MaglevGraphBuilder::TryBuildScriptContextAccess(
    const compiler::GlobalAccessFeedback& global_access_feedback) {
  if (!global_access_feedback.IsScriptContextSlot()) return false;
  if (TryBuildScriptContextConstantAccess(global_access_feedback)) return true;

  auto script_context = GetConstant(global_access_feedback.script_context());
  current_interpreter_frame_.set_accumulator(LoadAndCacheContextSlot(
      script_context,
      Context::OffsetOfElementAt(global_access_feedback.slot_index()),
      global_access_feedback.immutable() ? kImmutable : kMutable));
  return true;
}

bool MaglevGraphBuilder::TryBuildPropertyCellAccess(
    const compiler::GlobalAccessFeedback& global_access_feedback) {
  // TODO(leszeks): A bunch of this is copied from
  // js-native-context-specialization.cc -- I wonder if we can unify it
  // somehow.

  if (!global_access_feedback.IsPropertyCell()) return false;
  compiler::PropertyCellRef property_cell =
      global_access_feedback.property_cell();

  if (!property_cell.Cache(broker())) return false;

  compiler::ObjectRef property_cell_value = property_cell.value(broker());
  if (property_cell_value.IsTheHole(broker())) {
    // The property cell is no longer valid.
    EmitUnconditionalDeopt(
        DeoptimizeReason::kInsufficientTypeFeedbackForGenericNamedAccess);
    return true;
  }

  PropertyDetails property_details = property_cell.property_details();
  PropertyCellType property_cell_type = property_details.cell_type();
  DCHECK_EQ(PropertyKind::kData, property_details.kind());

  if (!property_details.IsConfigurable() && property_details.IsReadOnly()) {
    SetAccumulator(GetConstant(property_cell_value));
    return true;
  }

  // Record a code dependency on the cell if we can benefit from the
  // additional feedback, or the global property is configurable (i.e.
  // can be deleted or reconfigured to an accessor property).
  if (property_cell_type != PropertyCellType::kMutable ||
      property_details.IsConfigurable()) {
    broker()->dependencies()->DependOnGlobalProperty(property_cell);
  }

  // Load from constant/undefined global property can be constant-folded.
  if (property_cell_type == PropertyCellType::kConstant ||
      property_cell_type == PropertyCellType::kUndefined) {
    SetAccumulator(GetConstant(property_cell_value));
    return true;
  }

  ValueNode* property_cell_node = GetConstant(property_cell.AsHeapObject());
  SetAccumulator(AddNewNode<LoadTaggedField>({property_cell_node},
                                             PropertyCell::kValueOffset));
  return true;
}

void MaglevGraphBuilder::VisitLdaGlobal() {
  // LdaGlobal <name_index> <slot>

  static const int kNameOperandIndex = 0;
  static const int kSlotOperandIndex = 1;

  compiler::NameRef name = GetRefOperand<Name>(kNameOperandIndex);
  FeedbackSlot slot = GetSlotOperand(kSlotOperandIndex);
  compiler::FeedbackSource feedback_source{feedback(), slot};
  BuildLoadGlobal(name, feedback_source, TypeofMode::kNotInside);
}

void MaglevGraphBuilder::VisitLdaGlobalInsideTypeof() {
  // LdaGlobalInsideTypeof <name_index> <slot>

  static const int kNameOperandIndex = 0;
  static const int kSlotOperandIndex = 1;

  compiler::NameRef name = GetRefOperand<Name>(kNameOperandIndex);
  FeedbackSlot slot = GetSlotOperand(kSlotOperandIndex);
  compiler::FeedbackSource feedback_source{feedback(), slot};
  BuildLoadGlobal(name, feedback_source, TypeofMode::kInside);
}

void MaglevGraphBuilder::VisitStaGlobal() {
  // StaGlobal <name_index> <slot>
  ValueNode* value = GetAccumulatorTagged();
  compiler::NameRef name = GetRefOperand<Name>(0);
  FeedbackSlot slot = GetSlotOperand(1);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  // TODO(v8:7700): Add fast path.

  ValueNode* context = GetContext();
  AddNewNode<StoreGlobal>({context, value}, name, feedback_source);
}

void MaglevGraphBuilder::VisitLdaLookupSlot() {
  // LdaLookupSlot <name_index>
  ValueNode* name = GetConstant(GetRefOperand<Name>(0));
  SetAccumulator(BuildCallRuntime(Runtime::kLoadLookupSlot, {name}));
}

void MaglevGraphBuilder::VisitLdaLookupContextSlot() {
  // LdaLookupContextSlot <name_index> <feedback_slot> <depth>
  ValueNode* name = GetConstant(GetRefOperand<Name>(0));
  ValueNode* slot = GetSmiConstant(iterator_.GetIndexOperand(1));
  ValueNode* depth = GetSmiConstant(iterator_.GetUnsignedImmediateOperand(2));
  SetAccumulator(
      BuildCallBuiltin<Builtin::kLookupContextTrampoline>({name, depth, slot}));
}

void MaglevGraphBuilder::VisitLdaLookupGlobalSlot() {
  // LdaLookupGlobalSlot <name_index> <feedback_slot> <depth>
  ValueNode* name = GetConstant(GetRefOperand<Name>(0));
  ValueNode* slot = GetSmiConstant(iterator_.GetIndexOperand(1));
  ValueNode* depth = GetSmiConstant(iterator_.GetUnsignedImmediateOperand(2));
  ValueNode* result;
  if (parent_) {
    ValueNode* vector = GetConstant(feedback());
    result =
        BuildCallBuiltin<Builtin::kLookupGlobalIC>({name, depth, slot, vector});
  } else {
    result = BuildCallBuiltin<Builtin::kLookupGlobalICTrampoline>(
        {name, depth, slot});
  }
  SetAccumulator(result);
}

void MaglevGraphBuilder::VisitLdaLookupSlotInsideTypeof() {
  // LdaLookupSlotInsideTypeof <name_index>
  ValueNode* name = GetConstant(GetRefOperand<Name>(0));
  SetAccumulator(
      BuildCallRuntime(Runtime::kLoadLookupSlotInsideTypeof, {name}));
}

void MaglevGraphBuilder::VisitLdaLookupContextSlotInsideTypeof() {
  // LdaLookupContextSlotInsideTypeof <name_index> <context_slot> <depth>
  ValueNode* name = GetConstant(GetRefOperand<Name>(0));
  ValueNode* slot = GetSmiConstant(iterator_.GetIndexOperand(1));
  ValueNode* depth = GetSmiConstant(iterator_.GetUnsignedImmediateOperand(2));
  SetAccumulator(
      BuildCallBuiltin<Builtin::kLookupContextInsideTypeofTrampoline>(
          {name, depth, slot}));
}

void MaglevGraphBuilder::VisitLdaLookupGlobalSlotInsideTypeof() {
  // LdaLookupGlobalSlotInsideTypeof <name_index> <context_slot> <depth>
  ValueNode* name = GetConstant(GetRefOperand<Name>(0));
  ValueNode* slot = GetSmiConstant(iterator_.GetIndexOperand(1));
  ValueNode* depth = GetSmiConstant(iterator_.GetUnsignedImmediateOperand(2));
  ValueNode* result;
  if (parent_) {
    ValueNode* vector = GetConstant(feedback());
    result = BuildCallBuiltin<Builtin::kLookupGlobalICInsideTypeof>(
        {name, depth, slot, vector});
  } else {
    result = BuildCallBuiltin<Builtin::kLookupGlobalICInsideTypeofTrampoline>(
        {name, depth, slot});
  }
  SetAccumulator(result);
}

namespace {
Runtime::FunctionId StaLookupSlotFunction(uint8_t sta_lookup_slot_flags) {
  using Flags = interpreter::StoreLookupSlotFlags;
  switch (Flags::GetLanguageMode(sta_lookup_slot_flags)) {
    case LanguageMode::kStrict:
      return Runtime::kStoreLookupSlot_Strict;
    case LanguageMode::kSloppy:
      if (Flags::IsLookupHoistingMode(sta_lookup_slot_flags)) {
        return Runtime::kStoreLookupSlot_SloppyHoisting;
      } else {
        return Runtime::kStoreLookupSlot_Sloppy;
      }
  }
}
}  // namespace

void MaglevGraphBuilder::VisitStaLookupSlot() {
  // StaLookupSlot <name_index> <flags>
  ValueNode* value = GetAccumulatorTagged();
  ValueNode* name = GetConstant(GetRefOperand<Name>(0));
  uint32_t flags = GetFlag8Operand(1);
  SetAccumulator(BuildCallRuntime(StaLookupSlotFunction(flags), {name, value}));
}

namespace {
NodeType StaticTypeForNode(ValueNode* node) {
  if (!node->is_tagged()) {
    return NodeType::kNumber;
  }
  switch (node->opcode()) {
    case Opcode::kCheckedSmiTagInt32:
    case Opcode::kCheckedSmiTagUint32:
    case Opcode::kSmiConstant:
      return NodeType::kSmi;
    case Opcode::kConstant: {
      compiler::HeapObjectRef ref = node->Cast<Constant>()->object();
      if (ref.IsString()) {
        return NodeType::kString;
      } else if (ref.IsSymbol()) {
        return NodeType::kSymbol;
      } else if (ref.IsHeapNumber()) {
        return NodeType::kHeapNumber;
      } else if (ref.IsJSReceiver()) {
        return NodeType::kJSReceiverWithKnownMap;
      }
      return NodeType::kHeapObjectWithKnownMap;
    }
    case Opcode::kLoadPolymorphicTaggedField: {
      Representation field_representation =
          node->Cast<LoadPolymorphicTaggedField>()->field_representation();
      switch (field_representation.kind()) {
        case Representation::kSmi:
          return NodeType::kSmi;
        case Representation::kHeapObject:
          return NodeType::kAnyHeapObject;
        default:
          return NodeType::kUnknown;
      }
    }
    case Opcode::kToNumberOrNumeric:
      if (node->Cast<ToNumberOrNumeric>()->mode() ==
          Object::Conversion::kToNumber) {
        return NodeType::kNumber;
      }
      // TODO(verwaest): Check what we need here.
      return NodeType::kUnknown;
    case Opcode::kToString:
      return NodeType::kString;
    case Opcode::kCheckedInternalizedString:
      return NodeType::kInternalizedString;
    case Opcode::kToObject:
      return NodeType::kJSReceiver;
    case Opcode::kToName:
      return NodeType::kName;
    default:
      return NodeType::kUnknown;
  }
}
}  // namespace

bool MaglevGraphBuilder::EnsureType(ValueNode* node, NodeType type,
                                    NodeType* old_type) {
  NodeType static_type = StaticTypeForNode(node);
  if (NodeTypeIs(static_type, type)) {
    if (old_type) *old_type = static_type;
    return true;
  }
  NodeInfo* known_info = known_node_aspects().GetOrCreateInfoFor(node);
  if (old_type) *old_type = known_info->type;
  if (NodeTypeIs(known_info->type, type)) return true;
  known_info->type = CombineType(known_info->type, type);
  return false;
}

bool MaglevGraphBuilder::CheckType(ValueNode* node, NodeType type) {
  if (NodeTypeIs(StaticTypeForNode(node), type)) return true;
  auto it = known_node_aspects().FindInfo(node);
  if (!known_node_aspects().IsValid(it)) return false;
  return NodeTypeIs(it->second.type, type);
}

ValueNode* MaglevGraphBuilder::BuildSmiUntag(ValueNode* node) {
  if (EnsureType(node, NodeType::kSmi)) {
    return AddNewNode<UnsafeSmiUntag>({node});
  } else {
    return AddNewNode<CheckedSmiUntag>({node});
  }
}

ValueNode* MaglevGraphBuilder::BuildNumberOrOddballToFloat64(
    ValueNode* node, TaggedToFloat64ConversionType conversion_type) {
  NodeType old_type;
  if (EnsureType(node, TaggedToFloat64ConversionTypeToNodeType(conversion_type),
                 &old_type)) {
    if (old_type == NodeType::kSmi) {
      ValueNode* untagged_smi = AddNewNode<UnsafeSmiUntag>({node});
      return AddNewNode<ChangeInt32ToFloat64>({untagged_smi});
    }
    return AddNewNode<UncheckedNumberOrOddballToFloat64>({node},
                                                         conversion_type);
  } else {
    return AddNewNode<CheckedNumberOrOddballToFloat64>({node}, conversion_type);
  }
}

void MaglevGraphBuilder::BuildCheckSmi(ValueNode* object) {
  if (EnsureType(object, NodeType::kSmi)) return;
  AddNewNode<CheckSmi>({object});
}

void MaglevGraphBuilder::BuildCheckHeapObject(ValueNode* object) {
  if (EnsureType(object, NodeType::kAnyHeapObject)) return;
  AddNewNode<CheckHeapObject>({object});
}

namespace {
CheckType GetCheckType(NodeType type) {
  return NodeTypeIs(type, NodeType::kAnyHeapObject)
             ? CheckType::kOmitHeapObjectCheck
             : CheckType::kCheckHeapObject;
}
}  // namespace

void MaglevGraphBuilder::BuildCheckString(ValueNode* object) {
  NodeType known_type;
  if (EnsureType(object, NodeType::kString, &known_type)) return;
  AddNewNode<CheckString>({object}, GetCheckType(known_type));
}

void MaglevGraphBuilder::BuildCheckNumber(ValueNode* object) {
  if (EnsureType(object, NodeType::kNumber)) return;
  AddNewNode<CheckNumber>({object}, Object::Conversion::kToNumber);
}

void MaglevGraphBuilder::BuildCheckSymbol(ValueNode* object) {
  NodeType known_type;
  if (EnsureType(object, NodeType::kSymbol, &known_type)) return;
  AddNewNode<CheckSymbol>({object}, GetCheckType(known_type));
}

namespace {

class KnownMapsMerger {
 public:
  explicit KnownMapsMerger(compiler::JSHeapBroker* broker,
                           base::Vector<const compiler::MapRef> maps)
      : broker_(broker),
        maps_(maps),
        known_maps_are_subset_of_maps_(true),
        emit_check_with_migration_(false) {}

  void IntersectWithKnownNodeAspects(
      ValueNode* object, const KnownNodeAspects& known_node_aspects) {
    // A non-value value here means the universal set, i.e., we don't know
    // anything about the possible maps of the object.
    base::Optional<ZoneHandleSet<Map>> known_stable_map_set =
        GetKnownMapSet(object, known_node_aspects.stable_maps);
    base::Optional<ZoneHandleSet<Map>> known_unstable_map_set =
        GetKnownMapSet(object, known_node_aspects.unstable_maps);

    IntersectKnownMaps(known_stable_map_set, true);
    IntersectKnownMaps(known_unstable_map_set, false);
  }

  void UpdateKnownNodeAspects(ValueNode* object,
                              KnownNodeAspects& known_node_aspects) {
    // Update known maps.
    known_node_aspects.stable_maps[object] = stable_map_set_;
    known_node_aspects.unstable_maps[object] = unstable_map_set_;
  }

  bool known_maps_are_subset_of_maps() const {
    return known_maps_are_subset_of_maps_;
  }
  bool emit_check_with_migration() const { return emit_check_with_migration_; }

  ZoneHandleSet<Map> intersect_set() const {
    ZoneHandleSet<Map> map_set;
    map_set.Union(stable_map_set_, zone());
    map_set.Union(unstable_map_set_, zone());
    return map_set;
  }

  NodeType node_type() const { return node_type_; }

 private:
  compiler::JSHeapBroker* broker_;
  base::Vector<const compiler::MapRef> maps_;
  bool known_maps_are_subset_of_maps_;
  bool emit_check_with_migration_;
  ZoneHandleSet<Map> stable_map_set_;
  ZoneHandleSet<Map> unstable_map_set_;
  NodeType node_type_ = NodeType::kJSReceiverWithKnownMap;

  Zone* zone() const { return broker_->zone(); }

  base::Optional<ZoneHandleSet<Map>> GetKnownMapSet(
      ValueNode* object,
      const ZoneMap<ValueNode*, ZoneHandleSet<Map>>& map_of_map_set) {
    auto it = map_of_map_set.find(object);
    if (it == map_of_map_set.end()) {
      return {};
    }
    return it->second;
  }

  compiler::OptionalMapRef GetMapRefFromMaps(Handle<Map> handle) {
    auto it =
        std::find_if(maps_.begin(), maps_.end(), [&](compiler::MapRef map_ref) {
          return map_ref.object().is_identical_to(handle);
        });
    if (it == maps_.end()) return {};
    return *it;
  }

  void InsertMap(compiler::MapRef map, bool add_dependency) {
    if (map.is_migration_target()) {
      emit_check_with_migration_ = true;
    }
    if (map.IsHeapNumberMap()) {
      // If this is a heap number map, the object may be a Smi, so mask away
      // the known HeapObject bit.
      node_type_ = IntersectType(node_type_, NodeType::kObjectWithKnownMap);
    } else if (!map.IsJSReceiverMap()) {
      node_type_ = IntersectType(node_type_, NodeType::kHeapObjectWithKnownMap);
    }
    if (map.is_stable()) {
      // TODO(victorgomes): Add a DCHECK_SLOW that checks if the map already
      // exists in the CompilationDependencySet for the else branch.
      if (add_dependency) {
        broker_->dependencies()->DependOnStableMap(map);
      }
      stable_map_set_.insert(map.object(), zone());
    } else {
      unstable_map_set_.insert(map.object(), zone());
    }
  }

  void IntersectKnownMaps(base::Optional<ZoneHandleSet<Map>>& known_maps,
                          bool is_set_with_stable_maps) {
    if (known_maps.has_value()) {
      // TODO(v8:7700): Make intersection non-quadratic.
      for (Handle<Map> handle : *known_maps) {
        auto map = GetMapRefFromMaps(handle);
        if (map.has_value()) {
          InsertMap(*map, false);
        } else {
          known_maps_are_subset_of_maps_ = false;
        }
      }
    } else {
      // Intersect with the universal set.
      known_maps_are_subset_of_maps_ = false;
      for (compiler::MapRef map : maps_) {
        if (map.is_stable() == is_set_with_stable_maps) {
          InsertMap(map, true);
        }
      }
    }
  }
};

}  // namespace

void MaglevGraphBuilder::BuildCheckMaps(
    ValueNode* object, base::Vector<const compiler::MapRef> maps) {
  // TODO(verwaest): Support other objects with possible known stable maps as
  // well.
  if (object->Is<Constant>()) {
    // For constants with stable maps that match one of the desired maps, we
    // don't need to emit a map check, and can use the dependency -- we
    // can't do this for unstable maps because the constant could migrate
    // during compilation.
    compiler::MapRef constant_map =
        object->Cast<Constant>()->object().map(broker());
    if (std::find(maps.begin(), maps.end(), constant_map) != maps.end()) {
      if (constant_map.is_stable()) {
        broker()->dependencies()->DependOnStableMap(constant_map);
        return;
      }
      // TODO(verwaest): Reduce maps to the constant map.
    } else {
      // TODO(leszeks): Insert an unconditional deopt if the constant map
      // doesn't match the required map.
    }
  }

  NodeInfo* known_info = known_node_aspects().GetOrCreateInfoFor(object);
  known_info->type = CombineType(known_info->type, StaticTypeForNode(object));

  // Calculates if known maps are a subset of maps, their map intersection and
  // whether we should emit check with migration.
  KnownMapsMerger merger(broker(), maps);
  merger.IntersectWithKnownNodeAspects(object, known_node_aspects());
  merger.UpdateKnownNodeAspects(object, known_node_aspects());

  // If the known maps are the subset of the maps to check, we are done.
  if (merger.known_maps_are_subset_of_maps()) {
    DCHECK(NodeTypeIs(known_info->type, merger.node_type()));
    return;
  }

  // TODO(v8:7700): Insert an unconditional deopt here if intersect map sets are
  // empty.

  // TODO(v8:7700): Check if the {maps} - {known_maps} size is smaller than
  // {maps} \intersect {known_maps}, we can emit CheckNotMaps instead.

  // Emit checks.
  if (merger.emit_check_with_migration()) {
    AddNewNode<CheckMapsWithMigration>({object}, merger.intersect_set(),
                                       GetCheckType(known_info->type));
  } else {
    AddNewNode<CheckMaps>({object}, merger.intersect_set(),
                          GetCheckType(known_info->type));
  }
  known_info->type = merger.node_type();
}

namespace {
AllocateRaw* GetAllocation(ValueNode* object) {
  if (object->Is<FoldedAllocation>()) {
    object = object->Cast<FoldedAllocation>()->input(0).node();
  }
  if (object->Is<AllocateRaw>()) {
    return object->Cast<AllocateRaw>();
  }
  return nullptr;
}
}  // namespace

bool MaglevGraphBuilder::CanElideWriteBarrier(ValueNode* object,
                                              ValueNode* value) {
  if (value->Is<RootConstant>()) return true;
  if (value->Is<SmiConstant>()) return true;
  if (CheckType(value, NodeType::kSmi)) return true;

  // No need for a write barrier if both object and value are part of the same
  // folded young allocation.
  AllocateRaw* allocation = GetAllocation(object);
  if (allocation != nullptr &&
      allocation->allocation_type() == AllocationType::kYoung &&
      allocation == GetAllocation(value)) {
    return true;
  }

  return false;
}

void MaglevGraphBuilder::BuildStoreTaggedField(ValueNode* object,
                                               ValueNode* value, int offset) {
  if (CanElideWriteBarrier(object, value)) {
    AddNewNode<StoreTaggedFieldNoWriteBarrier>({object, value}, offset);
  } else {
    AddNewNode<StoreTaggedFieldWithWriteBarrier>({object, value}, offset);
  }
}

void MaglevGraphBuilder::BuildStoreTaggedFieldNoWriteBarrier(ValueNode* object,
                                                             ValueNode* value,
                                                             int offset) {
  DCHECK(CanElideWriteBarrier(object, value));
  AddNewNode<StoreTaggedFieldNoWriteBarrier>({object, value}, offset);
}

void MaglevGraphBuilder::BuildStoreFixedArrayElement(ValueNode* elements,
                                                     ValueNode* index,
                                                     ValueNode* value) {
  if (CanElideWriteBarrier(elements, value)) {
    AddNewNode<StoreFixedArrayElementNoWriteBarrier>({elements, index, value});
  } else {
    AddNewNode<StoreFixedArrayElementWithWriteBarrier>(
        {elements, index, value});
  }
}

bool MaglevGraphBuilder::CanTreatHoleAsUndefined(
    base::Vector<const compiler::MapRef> const& receiver_maps) {
  // Check if all {receiver_maps} have one of the initial Array.prototype
  // or Object.prototype objects as their prototype (in any of the current
  // native contexts, as the global Array protector works isolate-wide).
  for (compiler::MapRef receiver_map : receiver_maps) {
    compiler::ObjectRef receiver_prototype = receiver_map.prototype(broker());
    if (!receiver_prototype.IsJSObject() ||
        !broker()->IsArrayOrObjectPrototype(receiver_prototype.AsJSObject())) {
      return false;
    }
  }

  // Check if the array prototype chain is intact.
  return broker()->dependencies()->DependOnNoElementsProtector();
}

compiler::OptionalObjectRef
MaglevGraphBuilder::TryFoldLoadDictPrototypeConstant(
    compiler::PropertyAccessInfo access_info) {
  DCHECK(V8_DICT_PROPERTY_CONST_TRACKING_BOOL);
  DCHECK(access_info.IsDictionaryProtoDataConstant());
  DCHECK(access_info.holder().has_value());

  compiler::OptionalObjectRef constant =
      access_info.holder()->GetOwnDictionaryProperty(
          broker(), access_info.dictionary_index(), broker()->dependencies());
  if (!constant.has_value()) return {};

  for (compiler::MapRef map : access_info.lookup_start_object_maps()) {
    Handle<Map> map_handle = map.object();
    // Non-JSReceivers that passed AccessInfoFactory::ComputePropertyAccessInfo
    // must have different lookup start map.
    if (!map_handle->IsJSReceiverMap()) {
      // Perform the implicit ToObject for primitives here.
      // Implemented according to ES6 section 7.3.2 GetV (V, P).
      JSFunction constructor =
          Map::GetConstructorFunction(
              *map_handle, *broker()->target_native_context().object())
              .value();
      // {constructor.initial_map()} is loaded/stored with acquire-release
      // semantics for constructors.
      map = MakeRefAssumeMemoryFence(broker(), constructor.initial_map());
      DCHECK(map.object()->IsJSObjectMap());
    }
    broker()->dependencies()->DependOnConstantInDictionaryPrototypeChain(
        map, access_info.name(), constant.value(), PropertyKind::kData);
  }

  return constant;
}

compiler::OptionalObjectRef MaglevGraphBuilder::TryFoldLoadConstantDataField(
    compiler::PropertyAccessInfo access_info, ValueNode* lookup_start_object) {
  if (!access_info.IsFastDataConstant()) return {};
  compiler::OptionalJSObjectRef source;
  if (access_info.holder().has_value()) {
    source = access_info.holder();
  } else if (Constant* n = lookup_start_object->TryCast<Constant>()) {
    if (!n->ref().IsJSObject()) return {};
    source = n->ref().AsJSObject();
  } else {
    return {};
  }
  return source.value().GetOwnFastDataProperty(
      broker(), access_info.field_representation(), access_info.field_index(),
      broker()->dependencies());
}

ReduceResult MaglevGraphBuilder::TryBuildPropertyGetterCall(
    compiler::PropertyAccessInfo access_info, ValueNode* receiver,
    ValueNode* lookup_start_object) {
  compiler::ObjectRef constant = access_info.constant().value();

  if (access_info.IsDictionaryProtoAccessorConstant()) {
    // For fast mode holders we recorded dependencies in BuildPropertyLoad.
    for (const compiler::MapRef map : access_info.lookup_start_object_maps()) {
      broker()->dependencies()->DependOnConstantInDictionaryPrototypeChain(
          map, access_info.name(), constant, PropertyKind::kAccessor);
    }
  }

  // Introduce the call to the getter function.
  if (constant.IsJSFunction()) {
    ConvertReceiverMode receiver_mode =
        receiver == lookup_start_object
            ? ConvertReceiverMode::kNotNullOrUndefined
            : ConvertReceiverMode::kAny;
    CallArguments args(receiver_mode, {receiver});
    return ReduceCall(constant.AsJSFunction(), args);
  } else if (receiver != lookup_start_object) {
    return ReduceResult::Fail();
  } else {
    ValueNode* api_holder = access_info.api_holder().has_value()
                                ? GetConstant(access_info.api_holder().value())
                                : receiver;
    compiler::FunctionTemplateInfoRef templ = constant.AsFunctionTemplateInfo();
    if (!templ.call_code(broker()).has_value()) return ReduceResult::Fail();

    compiler::CallHandlerInfoRef call_handler_info =
        templ.call_code(broker()).value();
    ApiFunction function(call_handler_info.callback());
    ExternalReference reference = ExternalReference::Create(
        &function, ExternalReference::DIRECT_API_CALL);
    return BuildCallBuiltin<Builtin::kCallApiCallback>(
        {GetExternalConstant(reference), GetInt32Constant(0),
         GetConstant(call_handler_info.data(broker())), api_holder, receiver});
  }
}

ReduceResult MaglevGraphBuilder::TryBuildPropertySetterCall(
    compiler::PropertyAccessInfo access_info, ValueNode* receiver,
    ValueNode* value) {
  compiler::ObjectRef constant = access_info.constant().value();
  if (constant.IsJSFunction()) {
    CallArguments args(ConvertReceiverMode::kNotNullOrUndefined,
                       {receiver, value});
    return ReduceCall(constant.AsJSFunction(), args);
  } else {
    // TODO(victorgomes): API calls.
    return ReduceResult::Fail();
  }
}

ValueNode* MaglevGraphBuilder::BuildLoadField(
    compiler::PropertyAccessInfo access_info, ValueNode* lookup_start_object) {
  compiler::OptionalObjectRef constant =
      TryFoldLoadConstantDataField(access_info, lookup_start_object);
  if (constant.has_value()) {
    return GetConstant(constant.value());
  }

  // Resolve property holder.
  ValueNode* load_source;
  if (access_info.holder().has_value()) {
    load_source = GetConstant(access_info.holder().value());
  } else {
    load_source = lookup_start_object;
  }

  FieldIndex field_index = access_info.field_index();
  if (!field_index.is_inobject()) {
    // The field is in the property array, first load it from there.
    load_source = AddNewNode<LoadTaggedField>(
        {load_source}, JSReceiver::kPropertiesOrHashOffset);
  }

  // Do the load.
  if (field_index.is_double()) {
    return AddNewNode<LoadDoubleField>({load_source}, field_index.offset());
  }
  ValueNode* value =
      AddNewNode<LoadTaggedField>({load_source}, field_index.offset());
  // Insert stable field information if present.
  if (access_info.field_representation().IsSmi()) {
    NodeInfo* known_info = known_node_aspects().GetOrCreateInfoFor(value);
    known_info->type = NodeType::kSmi;
  } else if (access_info.field_representation().IsHeapObject()) {
    NodeInfo* known_info = known_node_aspects().GetOrCreateInfoFor(value);
    if (access_info.field_map().has_value() &&
        access_info.field_map().value().is_stable()) {
      DCHECK(access_info.field_map().value().IsJSReceiverMap());
      known_info->type = NodeType::kJSReceiverWithKnownMap;
      auto map = access_info.field_map().value();
      ZoneHandleSet<Map> stable_maps(map.object());
      ZoneHandleSet<Map> unstable_maps;
      known_node_aspects().stable_maps.emplace(value, stable_maps);
      known_node_aspects().unstable_maps.emplace(value, unstable_maps);
      broker()->dependencies()->DependOnStableMap(map);
    } else {
      known_info->type = NodeType::kAnyHeapObject;
    }
  }
  return value;
}

ReduceResult MaglevGraphBuilder::TryBuildStoreField(
    compiler::PropertyAccessInfo access_info, ValueNode* receiver,
    compiler::AccessMode access_mode) {
  FieldIndex field_index = access_info.field_index();
  Representation field_representation = access_info.field_representation();

  if (access_info.HasTransitionMap()) {
    compiler::MapRef transition = access_info.transition_map().value();
    compiler::MapRef original_map = transition.GetBackPointer(broker()).AsMap();
    // TODO(verwaest): Support growing backing stores.
    if (original_map.UnusedPropertyFields() == 0) {
      return ReduceResult::Fail();
    }
  } else if (access_info.IsFastDataConstant() &&
             access_mode == compiler::AccessMode::kStore) {
    return ReduceResult::DoneWithAbort();
  }

  ValueNode* store_target;
  if (field_index.is_inobject()) {
    store_target = receiver;
  } else {
    // The field is in the property array, first load it from there.
    store_target = AddNewNode<LoadTaggedField>(
        {receiver}, JSReceiver::kPropertiesOrHashOffset);
  }

  ValueNode* value;
  if (field_representation.IsDouble()) {
    value = GetAccumulatorFloat64(TaggedToFloat64ConversionType::kNumber);
    if (access_info.HasTransitionMap()) {
      // Allocate the mutable double box owned by the field.
      value = AddNewNode<Float64ToTagged>(
          {value}, Float64ToTagged::ConversionMode::kForceHeapNumber);
    }
  } else {
    value = GetAccumulatorTagged();
    if (field_representation.IsHeapObject()) {
      // Emit a map check for the field type, if needed, otherwise just a
      // HeapObject check.
      if (access_info.field_map().has_value()) {
        BuildCheckMaps(value,
                       base::VectorOf({access_info.field_map().value()}));
      } else {
        BuildCheckHeapObject(value);
      }
    }
  }

  if (field_representation.IsSmi()) {
    AddNewNode<CheckedStoreSmiField>({store_target, value},
                                     field_index.offset());
  } else if (value->use_double_register()) {
    DCHECK(field_representation.IsDouble());
    DCHECK(!access_info.HasTransitionMap());
    AddNewNode<StoreDoubleField>({store_target, value}, field_index.offset());
  } else {
    DCHECK(field_representation.IsHeapObject() ||
           field_representation.IsTagged() ||
           (field_representation.IsDouble() && access_info.HasTransitionMap()));
    BuildStoreTaggedField(store_target, value, field_index.offset());
  }

  if (access_info.HasTransitionMap()) {
    compiler::MapRef transition = access_info.transition_map().value();
    AddNewNode<StoreMap>({receiver}, transition);
    NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(receiver);
    DCHECK(transition.IsJSReceiverMap());
    node_info->type = NodeType::kJSReceiverWithKnownMap;
    if (transition.is_stable()) {
      ZoneHandleSet<Map> stable_maps(transition.object());
      ZoneHandleSet<Map> unstable_maps;
      known_node_aspects().stable_maps.emplace(receiver, stable_maps);
      known_node_aspects().unstable_maps.emplace(receiver, unstable_maps);
      broker()->dependencies()->DependOnStableMap(transition);
    } else {
      ZoneHandleSet<Map> stable_maps;
      ZoneHandleSet<Map> unstable_maps(transition.object());
      known_node_aspects().stable_maps.emplace(receiver, stable_maps);
      known_node_aspects().unstable_maps.emplace(receiver, unstable_maps);
    }
  }

  return ReduceResult::Done();
}

ReduceResult MaglevGraphBuilder::TryBuildPropertyLoad(
    ValueNode* receiver, ValueNode* lookup_start_object, compiler::NameRef name,
    compiler::PropertyAccessInfo const& access_info) {
  if (access_info.holder().has_value() && !access_info.HasDictionaryHolder()) {
    broker()->dependencies()->DependOnStablePrototypeChains(
        access_info.lookup_start_object_maps(), kStartAtPrototype,
        access_info.holder().value());
  }

  switch (access_info.kind()) {
    case compiler::PropertyAccessInfo::kInvalid:
      UNREACHABLE();
    case compiler::PropertyAccessInfo::kNotFound:
      return GetRootConstant(RootIndex::kUndefinedValue);
    case compiler::PropertyAccessInfo::kDataField:
    case compiler::PropertyAccessInfo::kFastDataConstant: {
      ValueNode* result = BuildLoadField(access_info, lookup_start_object);
      RecordKnownProperty(lookup_start_object, name, result, access_info);
      return result;
    }
    case compiler::PropertyAccessInfo::kDictionaryProtoDataConstant: {
      compiler::OptionalObjectRef constant =
          TryFoldLoadDictPrototypeConstant(access_info);
      if (!constant.has_value()) return ReduceResult::Fail();
      return GetConstant(constant.value());
    }
    case compiler::PropertyAccessInfo::kFastAccessorConstant:
    case compiler::PropertyAccessInfo::kDictionaryProtoAccessorConstant:
      return TryBuildPropertyGetterCall(access_info, receiver,
                                        lookup_start_object);
    case compiler::PropertyAccessInfo::kModuleExport: {
      ValueNode* cell = GetConstant(access_info.constant().value().AsCell());
      return AddNewNode<LoadTaggedField>({cell}, Cell::kValueOffset);
    }
    case compiler::PropertyAccessInfo::kStringLength: {
      DCHECK_EQ(receiver, lookup_start_object);
      ValueNode* result = AddNewNode<StringLength>({receiver});
      RecordKnownProperty(lookup_start_object, name, result, access_info);
      return result;
    }
  }
}

ReduceResult MaglevGraphBuilder::TryBuildPropertyStore(
    ValueNode* receiver, compiler::NameRef name,
    compiler::PropertyAccessInfo const& access_info,
    compiler::AccessMode access_mode) {
  if (access_info.holder().has_value()) {
    broker()->dependencies()->DependOnStablePrototypeChains(
        access_info.lookup_start_object_maps(), kStartAtPrototype,
        access_info.holder().value());
  }

  if (access_info.IsFastAccessorConstant()) {
    return TryBuildPropertySetterCall(access_info, receiver,
                                      GetAccumulatorTagged());
  } else {
    DCHECK(access_info.IsDataField() || access_info.IsFastDataConstant());
    if (TryBuildStoreField(access_info, receiver, access_mode).IsDone()) {
      RecordKnownProperty(receiver, name,
                          current_interpreter_frame_.accumulator(),
                          access_info);
      return ReduceResult::Done();
    }
    return ReduceResult::Fail();
  }
}

ReduceResult MaglevGraphBuilder::TryBuildPropertyAccess(
    ValueNode* receiver, ValueNode* lookup_start_object, compiler::NameRef name,
    compiler::PropertyAccessInfo const& access_info,
    compiler::AccessMode access_mode) {
  switch (access_mode) {
    case compiler::AccessMode::kLoad:
      return TryBuildPropertyLoad(receiver, lookup_start_object, name,
                                  access_info);
    case compiler::AccessMode::kStore:
    case compiler::AccessMode::kStoreInLiteral:
    case compiler::AccessMode::kDefine:
      DCHECK_EQ(receiver, lookup_start_object);
      return TryBuildPropertyStore(receiver, name, access_info, access_mode);
    case compiler::AccessMode::kHas:
      // TODO(victorgomes): BuildPropertyTest.
      return ReduceResult::Fail();
  }
}

ReduceResult MaglevGraphBuilder::TryBuildNamedAccess(
    ValueNode* receiver, ValueNode* lookup_start_object,
    compiler::NamedAccessFeedback const& feedback,
    compiler::FeedbackSource const& feedback_source,
    compiler::AccessMode access_mode) {
  // Check for the megamorphic case.
  if (feedback.maps().empty()) {
    // We don't have a builtin to fast path megamorphic stores.
    // TODO(leszeks): Maybe we should?
    if (access_mode != compiler::AccessMode::kLoad) return ReduceResult::Fail();
    // We can't do megamorphic loads for lookups where the lookup start isn't
    // the receiver (e.g. load from super).
    if (receiver != lookup_start_object) return ReduceResult::Fail();

    return BuildCallBuiltin<Builtin::kLoadIC_Megamorphic>(
        {receiver, GetConstant(feedback.name())}, feedback_source);
  }

  ZoneVector<compiler::PropertyAccessInfo> access_infos(zone());
  {
    ZoneVector<compiler::PropertyAccessInfo> access_infos_for_feedback(zone());
    ZoneHandleSet<Map> inferred_maps;

    if (Constant* n = lookup_start_object->TryCast<Constant>()) {
      compiler::MapRef constant_map = n->object().map(broker());
      inferred_maps = ZoneHandleSet<Map>(constant_map.object());
    } else {
      // TODO(leszeks): This is doing duplicate work with BuildCheckMaps,
      // consider passing the merger into there.
      KnownMapsMerger merger(broker(), base::VectorOf(feedback.maps()));
      merger.IntersectWithKnownNodeAspects(lookup_start_object,
                                           known_node_aspects());
      inferred_maps = merger.intersect_set();
    }

    for (Handle<Map> map : inferred_maps) {
      if (map->is_deprecated()) continue;

      // TODO(v8:12547): Support writing to objects in shared space, which
      // need a write barrier that calls Object::Share to ensure the RHS is
      // shared.
      if (InstanceTypeChecker::IsAlwaysSharedSpaceJSObject(
              map->instance_type()) &&
          access_mode == compiler::AccessMode::kStore) {
        return ReduceResult::Fail();
      }

      compiler::MapRef map_ref = MakeRefAssumeMemoryFence(broker(), map);
      compiler::PropertyAccessInfo access_info =
          broker()->GetPropertyAccessInfo(map_ref, feedback.name(),
                                          access_mode);
      access_infos_for_feedback.push_back(access_info);
    }

    compiler::AccessInfoFactory access_info_factory(broker(), zone());
    if (!access_info_factory.FinalizePropertyAccessInfos(
            access_infos_for_feedback, access_mode, &access_infos)) {
      return ReduceResult::Fail();
    }
  }

  // Check for monomorphic case.
  if (access_infos.size() == 1) {
    compiler::PropertyAccessInfo access_info = access_infos.front();
    base::Vector<const compiler::MapRef> maps =
        base::VectorOf(access_info.lookup_start_object_maps());
    if (HasOnlyStringMaps(maps)) {
      // Check for string maps before checking if we need to do an access
      // check. Primitive strings always get the prototype from the native
      // context they're operated on, so they don't need the access check.
      BuildCheckString(lookup_start_object);
    } else if (HasOnlyNumberMaps(maps)) {
      BuildCheckNumber(lookup_start_object);
    } else {
      BuildCheckMaps(lookup_start_object, maps);
    }

    // Generate the actual property
    return TryBuildPropertyAccess(receiver, lookup_start_object,
                                  feedback.name(), access_info, access_mode);
  } else {
    // TODO(victorgomes): Support more generic polymorphic case.

    // Only support polymorphic load at the moment.
    if (access_mode != compiler::AccessMode::kLoad) {
      return ReduceResult::Fail();
    }

    // Check if we support the polymorphic load.
    for (compiler::PropertyAccessInfo access_info : access_infos) {
      DCHECK(!access_info.IsInvalid());
      if (access_info.IsDictionaryProtoDataConstant()) {
        compiler::OptionalObjectRef constant =
            access_info.holder()->GetOwnDictionaryProperty(
                broker(), access_info.dictionary_index(),
                broker()->dependencies());
        if (!constant.has_value()) {
          return ReduceResult::Fail();
        }
      } else if (access_info.IsDictionaryProtoAccessorConstant() ||
                 access_info.IsFastAccessorConstant()) {
        return ReduceResult::Fail();
      }

      // TODO(victorgomes): Support map migration.
      for (compiler::MapRef map : access_info.lookup_start_object_maps()) {
        if (map.is_migration_target()) {
          return ReduceResult::Fail();
        }
      }
    }

    // Add compilation dependencies if needed, get constants and fill
    // polymorphic access info.
    Representation field_repr = Representation::Smi();
    ZoneVector<PolymorphicAccessInfo> poly_access_infos(zone());
    poly_access_infos.reserve(access_infos.size());

    for (compiler::PropertyAccessInfo access_info : access_infos) {
      if (access_info.holder().has_value() &&
          !access_info.HasDictionaryHolder()) {
        broker()->dependencies()->DependOnStablePrototypeChains(
            access_info.lookup_start_object_maps(), kStartAtPrototype,
            access_info.holder().value());
      }

      const auto& maps = access_info.lookup_start_object_maps();
      switch (access_info.kind()) {
        case compiler::PropertyAccessInfo::kNotFound:
          field_repr = Representation::Tagged();
          poly_access_infos.push_back(PolymorphicAccessInfo::NotFound(maps));
          break;
        case compiler::PropertyAccessInfo::kDataField:
        case compiler::PropertyAccessInfo::kFastDataConstant: {
          field_repr =
              field_repr.generalize(access_info.field_representation());
          compiler::OptionalObjectRef constant =
              TryFoldLoadConstantDataField(access_info, lookup_start_object);
          if (constant.has_value()) {
            poly_access_infos.push_back(
                PolymorphicAccessInfo::Constant(maps, constant.value()));
          } else {
            poly_access_infos.push_back(PolymorphicAccessInfo::DataLoad(
                maps, access_info.field_representation(), access_info.holder(),
                access_info.field_index()));
          }
          break;
        }
        case compiler::PropertyAccessInfo::kDictionaryProtoDataConstant: {
          field_repr =
              field_repr.generalize(access_info.field_representation());
          compiler::OptionalObjectRef constant =
              TryFoldLoadDictPrototypeConstant(access_info);
          DCHECK(constant.has_value());
          poly_access_infos.push_back(
              PolymorphicAccessInfo::Constant(maps, constant.value()));
          break;
        }
        case compiler::PropertyAccessInfo::kModuleExport:
          field_repr = Representation::Tagged();
          break;
        case compiler::PropertyAccessInfo::kStringLength:
          poly_access_infos.push_back(
              PolymorphicAccessInfo::StringLength(maps));
          break;
        default:
          UNREACHABLE();
      }
    }

    if (field_repr.kind() == Representation::kDouble) {
      return AddNewNode<LoadPolymorphicDoubleField>(
          {lookup_start_object}, std::move(poly_access_infos));
    }

    return AddNewNode<LoadPolymorphicTaggedField>(
        {lookup_start_object}, field_repr, std::move(poly_access_infos));
  }
}

ValueNode* MaglevGraphBuilder::GetInt32ElementIndex(ValueNode* object) {
  switch (object->properties().value_representation()) {
    case ValueRepresentation::kWord64:
      UNREACHABLE();
    case ValueRepresentation::kTagged:
      if (SmiConstant* constant = object->TryCast<SmiConstant>()) {
        return GetInt32Constant(constant->value().value());
      } else if (CheckType(object, NodeType::kSmi)) {
        NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(object);
        if (!node_info->int32_alternative) {
          node_info->int32_alternative = AddNewNode<UnsafeSmiUntag>({object});
        }
        return node_info->int32_alternative;
      } else {
        // TODO(leszeks): Cache this knowledge/converted value somehow on
        // the node info.
        return AddNewNode<CheckedObjectToIndex>({object});
      }
    case ValueRepresentation::kInt32:
      // Already good.
      return object;
    case ValueRepresentation::kUint32:
    case ValueRepresentation::kFloat64:
      return GetInt32(object);
  }
}

// TODO(victorgomes): Consider caching the values and adding an
// uint32_alternative in node_info.
ValueNode* MaglevGraphBuilder::GetUint32ElementIndex(ValueNode* object) {
  switch (object->properties().value_representation()) {
    case ValueRepresentation::kWord64:
      UNREACHABLE();
    case ValueRepresentation::kTagged:
      // TODO(victorgomes): Consider create Uint32Constants and
      // CheckedObjectToUnsignedIndex.
      return AddNewNode<CheckedInt32ToUint32>({GetInt32ElementIndex(object)});
    case ValueRepresentation::kInt32:
      return AddNewNode<CheckedInt32ToUint32>({object});
    case ValueRepresentation::kUint32:
      return object;
    case ValueRepresentation::kFloat64:
      return AddNewNode<CheckedTruncateFloat64ToUint32>({object});
  }
}

ReduceResult MaglevGraphBuilder::TryBuildElementAccessOnString(
    ValueNode* object, ValueNode* index_object,
    compiler::KeyedAccessMode const& keyed_mode) {
  // Strings are immutable and `in` cannot be used on strings
  if (keyed_mode.access_mode() != compiler::AccessMode::kLoad) {
    return ReduceResult::Fail();
  }

  // TODO(victorgomes): Deal with LOAD_IGNORE_OUT_OF_BOUNDS.
  if (keyed_mode.load_mode() == LOAD_IGNORE_OUT_OF_BOUNDS) {
    return ReduceResult::Fail();
  }

  DCHECK_EQ(keyed_mode.load_mode(), STANDARD_LOAD);

  // Ensure that {object} is actually a String.
  BuildCheckString(object);

  ValueNode* length = AddNewNode<StringLength>({object});
  ValueNode* index = GetInt32ElementIndex(index_object);
  AddNewNode<CheckInt32Condition>({index, length},
                                  AssertCondition::kUnsignedLessThan,
                                  DeoptimizeReason::kOutOfBounds);

  return AddNewNode<StringAt>({object, index});
}

ValueNode* MaglevGraphBuilder::BuildLoadTypedArrayElement(
    ValueNode* object, ValueNode* index, ElementsKind elements_kind) {
#define BUILD_AND_RETURN_LOAD_TYPED_ARRAY(Type, ...)                       \
  if (broker()->dependencies()->DependOnArrayBufferDetachingProtector()) { \
    return AddNewNode<Load##Type##TypedArrayElementNoDeopt>(__VA_ARGS__);  \
  }                                                                        \
  return AddNewNode<Load##Type##TypedArrayElement>(__VA_ARGS__);
  switch (elements_kind) {
    case INT8_ELEMENTS:
    case INT16_ELEMENTS:
    case INT32_ELEMENTS:
      BUILD_AND_RETURN_LOAD_TYPED_ARRAY(SignedInt, {object, index},
                                        elements_kind);
    case UINT8_CLAMPED_ELEMENTS:
    case UINT8_ELEMENTS:
    case UINT16_ELEMENTS:
    case UINT32_ELEMENTS:
      BUILD_AND_RETURN_LOAD_TYPED_ARRAY(UnsignedInt, {object, index},
                                        elements_kind);
    case FLOAT32_ELEMENTS:
    case FLOAT64_ELEMENTS:
      BUILD_AND_RETURN_LOAD_TYPED_ARRAY(Double, {object, index}, elements_kind);
    default:
      UNREACHABLE();
  }
#undef BUILD_AND_RETURN_LOAD_TYPED_ARRAY
}

void MaglevGraphBuilder::BuildStoreTypedArrayElement(
    ValueNode* object, ValueNode* index, ElementsKind elements_kind) {
#define BUILD_STORE_TYPED_ARRAY(Type, ...)                                 \
  if (broker()->dependencies()->DependOnArrayBufferDetachingProtector()) { \
    AddNewNode<Store##Type##TypedArrayElementNoDeopt>(__VA_ARGS__);        \
  } else {                                                                 \
    AddNewNode<Store##Type##TypedArrayElement>(__VA_ARGS__);               \
  }
  switch (elements_kind) {
    case UINT8_CLAMPED_ELEMENTS: {
      BUILD_STORE_TYPED_ARRAY(
          Int, {object, index, GetAccumulatorUint32ClampedFromNumber()},
          elements_kind)
      break;
    }
    case INT8_ELEMENTS:
    case INT16_ELEMENTS:
    case INT32_ELEMENTS:
    case UINT8_ELEMENTS:
    case UINT16_ELEMENTS:
    case UINT32_ELEMENTS:
      BUILD_STORE_TYPED_ARRAY(Int,
                              {object, index,
                               GetAccumulatorTruncatedInt32FromNumber(
                                   TaggedToFloat64ConversionType::kNumber)},
                              elements_kind)
      break;
    case FLOAT32_ELEMENTS:
    case FLOAT64_ELEMENTS:
      BUILD_STORE_TYPED_ARRAY(
          Double,
          {object, index,
           GetAccumulatorFloat64(TaggedToFloat64ConversionType::kNumber)},
          elements_kind)
      break;
    default:
      UNREACHABLE();
  }
#undef BUILD_STORE_TYPED_ARRAY
}

ReduceResult MaglevGraphBuilder::TryBuildElementAccessOnTypedArray(
    ValueNode* object, ValueNode* index_object,
    const compiler::ElementAccessInfo& access_info,
    compiler::KeyedAccessMode const& keyed_mode) {
  DCHECK(HasOnlyJSTypedArrayMaps(
      base::VectorOf(access_info.lookup_start_object_maps())));
  ElementsKind elements_kind = access_info.elements_kind();
  if (elements_kind == BIGUINT64_ELEMENTS ||
      elements_kind == BIGINT64_ELEMENTS) {
    return ReduceResult::Fail();
  }
  if (keyed_mode.access_mode() == compiler::AccessMode::kStore &&
      keyed_mode.store_mode() == STORE_IGNORE_OUT_OF_BOUNDS) {
    // TODO(victorgomes): Handle STORE_IGNORE_OUT_OF_BOUNDS mode.
    return ReduceResult::Fail();
  }
  if (keyed_mode.access_mode() == compiler::AccessMode::kStore &&
      elements_kind == UINT8_CLAMPED_ELEMENTS &&
      !IsSupported(CpuOperation::kFloat64Round)) {
    // TODO(victorgomes): Technically we still support if value (in the
    // accumulator) is of type int32. It would be nice to have a roll back
    // mechanism instead, so that we do not need to check this early.
    return ReduceResult::Fail();
  }
  ValueNode* index = GetUint32ElementIndex(index_object);
  AddNewNode<CheckJSTypedArrayBounds>({object, index}, elements_kind);
  switch (keyed_mode.access_mode()) {
    case compiler::AccessMode::kLoad:
      DCHECK_EQ(keyed_mode.load_mode(), STANDARD_LOAD);
      return BuildLoadTypedArrayElement(object, index, elements_kind);
    case compiler::AccessMode::kStore:
      DCHECK_EQ(keyed_mode.store_mode(), STANDARD_STORE);
      BuildStoreTypedArrayElement(object, index, elements_kind);
      return ReduceResult::Done();
    case compiler::AccessMode::kHas:
      // TODO(victorgomes): Implement has element access.
      return ReduceResult::Fail();
    case compiler::AccessMode::kStoreInLiteral:
    case compiler::AccessMode::kDefine:
      UNREACHABLE();
  }
}

ReduceResult MaglevGraphBuilder::TryBuildElementAccessOnJSArrayOrJSObject(
    ValueNode* object, ValueNode* index_object,
    const compiler::ElementAccessInfo& access_info,
    compiler::KeyedAccessMode const& keyed_mode) {
  ElementsKind elements_kind = access_info.elements_kind();
  if (!IsFastElementsKind(elements_kind)) {
    return ReduceResult::Fail();
  }
  base::Vector<const compiler::MapRef> maps =
      base::VectorOf(access_info.lookup_start_object_maps());
  if ((keyed_mode.access_mode() == compiler::AccessMode::kLoad) &&
      IsHoleyElementsKind(elements_kind) && !CanTreatHoleAsUndefined(maps)) {
    return ReduceResult::Fail();
  }
  if (keyed_mode.access_mode() == compiler::AccessMode::kStore &&
      keyed_mode.store_mode() == STORE_AND_GROW_HANDLE_COW) {
    // TODO(victorgomes): Handle growable elements.
    return ReduceResult::Fail();
  }
  ValueNode* index;
  if (HasOnlyJSArrayMaps(maps)) {
    index = GetInt32ElementIndex(index_object);
    AddNewNode<CheckJSArrayBounds>({object, index});
  } else {
    DCHECK(HasOnlyJSObjectMaps(maps));
    index = GetInt32ElementIndex(index_object);
    AddNewNode<CheckJSObjectElementsBounds>({object, index});
  }
  switch (keyed_mode.access_mode()) {
    case compiler::AccessMode::kLoad: {
      DCHECK_EQ(keyed_mode.load_mode(), STANDARD_LOAD);
      ValueNode* elements_array =
          AddNewNode<LoadTaggedField>({object}, JSObject::kElementsOffset);
      ValueNode* result;
      if (IsDoubleElementsKind(elements_kind)) {
        result =
            AddNewNode<LoadFixedDoubleArrayElement>({elements_array, index});
        if (IsHoleyElementsKind(elements_kind)) {
          // TODO(v8:7700): Add a representation for "Float64OrHole" and emit
          // this boxing lazily.
          result = AddNewNode<HoleyFloat64Box>({result});
        }
      } else {
        result = AddNewNode<LoadFixedArrayElement>({elements_array, index});
        if (IsHoleyElementsKind(elements_kind)) {
          result = AddNewNode<ConvertHoleToUndefined>({result});
        }
      }
      return result;
    }
    case compiler::AccessMode::kStore: {
      ValueNode* elements_array =
          AddNewNode<LoadTaggedField>({object}, JSObject::kElementsOffset);
      if (IsDoubleElementsKind(elements_kind)) {
        ValueNode* value =
            GetAccumulatorFloat64(TaggedToFloat64ConversionType::kNumber);
        // Make sure we do not store signalling NaNs into double arrays.
        // TODO(leszeks): Consider making this a bit on
        // StoreFixedDoubleArrayElement rather than a separate node.
        value = GetSilencedNaN(value);
        AddNewNode<StoreFixedDoubleArrayElement>(
            {elements_array, index, value});
      } else {
        if (keyed_mode.store_mode() == STORE_HANDLE_COW) {
          elements_array =
              AddNewNode<EnsureWritableFastElements>({elements_array, object});
        } else {
          // Ensure that this is not a COW FixedArray.
          BuildCheckMaps(elements_array,
                         base::VectorOf({broker()->fixed_array_map()}));
        }
        ValueNode* value = GetAccumulatorTagged();
        if (IsSmiElementsKind(elements_kind)) {
          AddNewNode<CheckedStoreFixedArraySmiElement>(
              {elements_array, index, value});
        } else {
          BuildStoreFixedArrayElement(elements_array, index, value);
        }
      }
      return ReduceResult::Done();
    }
    default:
      // TODO(victorgomes): Implement more access types.
      return ReduceResult::Fail();
  }
}

ReduceResult MaglevGraphBuilder::TryBuildElementAccess(
    ValueNode* object, ValueNode* index_object,
    compiler::ElementAccessFeedback const& feedback,
    compiler::FeedbackSource const& feedback_source) {
  const compiler::KeyedAccessMode& keyed_mode = feedback.keyed_mode();
  // Check for the megamorphic case.
  if (feedback.transition_groups().empty()) {
    if (keyed_mode.access_mode() != compiler::AccessMode::kLoad) {
      return ReduceResult::Fail();
    }
    return BuildCallBuiltin<Builtin::kKeyedLoadIC_Megamorphic>(
        {object, GetTaggedValue(index_object)}, feedback_source);
  }

  // TODO(leszeks): Add non-deopting bounds check (has to support undefined
  // values).
  if (keyed_mode.access_mode() == compiler::AccessMode::kLoad &&
      keyed_mode.load_mode() != STANDARD_LOAD) {
    return ReduceResult::Fail();
  }

  // TODO(victorgomes): Add fast path for loading from HeapConstant.

  if (feedback.HasOnlyStringMaps(broker())) {
    return TryBuildElementAccessOnString(object, index_object, keyed_mode);
  }

  compiler::AccessInfoFactory access_info_factory(broker(), zone());
  ZoneVector<compiler::ElementAccessInfo> access_infos(zone());
  if (!access_info_factory.ComputeElementAccessInfos(feedback, &access_infos) ||
      access_infos.empty()) {
    return ReduceResult::Fail();
  }

  // TODO(leszeks): This is copied without changes from TurboFan's native
  // context specialization. We should figure out a way to share this code.
  //
  // For holey stores or growing stores, we need to check that the prototype
  // chain contains no setters for elements, and we need to guard those checks
  // via code dependencies on the relevant prototype maps.
  if (keyed_mode.access_mode() == compiler::AccessMode::kStore) {
    // TODO(v8:7700): We could have a fast path here, that checks for the
    // common case of Array or Object prototype only and therefore avoids
    // the zone allocation of this vector.
    ZoneVector<compiler::MapRef> prototype_maps(zone());
    for (compiler::ElementAccessInfo const& access_info : access_infos) {
      for (compiler::MapRef receiver_map :
           access_info.lookup_start_object_maps()) {
        // If the {receiver_map} has a prototype and its elements backing
        // store is either holey, or we have a potentially growing store,
        // then we need to check that all prototypes have stable maps with
        // fast elements (and we need to guard against changes to that below).
        if ((IsHoleyOrDictionaryElementsKind(receiver_map.elements_kind()) ||
             IsGrowStoreMode(feedback.keyed_mode().store_mode())) &&
            !receiver_map.HasOnlyStablePrototypesWithFastElements(
                broker(), &prototype_maps)) {
          return ReduceResult::Fail();
        }

        // TODO(v8:12547): Support writing to objects in shared space, which
        // need a write barrier that calls Object::Share to ensure the RHS is
        // shared.
        if (InstanceTypeChecker::IsAlwaysSharedSpaceJSObject(
                receiver_map.instance_type())) {
          return ReduceResult::Fail();
        }
      }
    }
    for (compiler::MapRef const& prototype_map : prototype_maps) {
      broker()->dependencies()->DependOnStableMap(prototype_map);
    }
  }

  // Check for monomorphic case.
  if (access_infos.size() == 1) {
    compiler::ElementAccessInfo access_info = access_infos.front();
    // TODO(victorgomes): Support RAB/GSAB backed typed arrays.
    if (IsRabGsabTypedArrayElementsKind(access_info.elements_kind())) {
      return ReduceResult::Fail();
    }

    compiler::MapRef transition_target =
        access_info.lookup_start_object_maps().front();
    if (!access_info.transition_sources().empty()) {
      base::Vector<compiler::MapRef> transition_sources =
          zone()->CloneVector(base::VectorOf(access_info.transition_sources()));
      AddNewNode<TransitionElementsKind>({object}, transition_sources,
                                         transition_target);
    }

    BuildCheckMaps(object,
                   base::VectorOf(access_info.lookup_start_object_maps()));
    if (IsTypedArrayElementsKind(access_info.elements_kind())) {
      return TryBuildElementAccessOnTypedArray(object, index_object,
                                               access_info, keyed_mode);
    }
    return TryBuildElementAccessOnJSArrayOrJSObject(object, index_object,
                                                    access_info, keyed_mode);
  } else {
    // TODO(victorgomes): polymorphic case.
    return ReduceResult::Fail();
  }
}

void MaglevGraphBuilder::RecordKnownProperty(
    ValueNode* lookup_start_object, compiler::NameRef name, ValueNode* value,
    compiler::PropertyAccessInfo const& access_info) {
  bool is_const;
  if (access_info.IsFastDataConstant() || access_info.IsStringLength()) {
    is_const = true;
    // Even if we have a constant load, if the map is not stable, we cannot
    // guarantee that the load is preserved across side-effecting calls.
    // TODO(v8:7700): It might be possible to track it as const if we know that
    // we're still on the main transition tree; and if we add a dependency on
    // the stable end-maps of the entire tree.
    for (auto& map : access_info.lookup_start_object_maps()) {
      if (!map.is_stable()) {
        is_const = false;
        break;
      }
    }
  } else {
    is_const = false;
  }

  if (v8_flags.trace_maglev_graph_building) {
    std::cout << "  * Recording " << (is_const ? "constant" : "non-constant")
              << " known property "
              << PrintNodeLabel(graph_labeller(), lookup_start_object) << ": "
              << PrintNode(graph_labeller(), lookup_start_object) << " ["
              << *name.object()
              << "] = " << PrintNodeLabel(graph_labeller(), value) << ": "
              << PrintNode(graph_labeller(), value) << std::endl;
  }
  auto& loaded_properties =
      is_const ? known_node_aspects().loaded_constant_properties
               : known_node_aspects().loaded_properties;
  loaded_properties[std::make_pair(lookup_start_object, name)] = value;
}

bool MaglevGraphBuilder::TryReuseKnownPropertyLoad(
    ValueNode* lookup_start_object, compiler::NameRef name) {
  if (auto it = known_node_aspects().loaded_properties.find(
          {lookup_start_object, name});
      it != known_node_aspects().loaded_properties.end()) {
    current_interpreter_frame_.set_accumulator(it->second);
    if (v8_flags.trace_maglev_graph_building) {
      std::cout << "  * Reusing non-constant loaded property "
                << PrintNodeLabel(graph_labeller(), it->second) << ": "
                << PrintNode(graph_labeller(), it->second) << std::endl;
    }
    return true;
  }
  if (auto it = known_node_aspects().loaded_constant_properties.find(
          {lookup_start_object, name});
      it != known_node_aspects().loaded_constant_properties.end()) {
    current_interpreter_frame_.set_accumulator(it->second);
    if (v8_flags.trace_maglev_graph_building) {
      std::cout << "  * Reusing constant loaded property "
                << PrintNodeLabel(graph_labeller(), it->second) << ": "
                << PrintNode(graph_labeller(), it->second) << std::endl;
    }
    return true;
  }
  return false;
}

void MaglevGraphBuilder::VisitGetNamedProperty() {
  // GetNamedProperty <object> <name_index> <slot>
  ValueNode* object = LoadRegisterTagged(0);
  compiler::NameRef name = GetRefOperand<Name>(1);
  FeedbackSlot slot = GetSlotOperand(2);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  const compiler::ProcessedFeedback& processed_feedback =
      broker()->GetFeedbackForPropertyAccess(feedback_source,
                                             compiler::AccessMode::kLoad, name);

  switch (processed_feedback.kind()) {
    case compiler::ProcessedFeedback::kInsufficient:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericNamedAccess);
      return;

    case compiler::ProcessedFeedback::kNamedAccess: {
      if (TryReuseKnownPropertyLoad(object, name)) return;
      ReduceResult result = TryBuildNamedAccess(
          object, object, processed_feedback.AsNamedAccess(), feedback_source,
          compiler::AccessMode::kLoad);
      PROCESS_AND_RETURN_IF_DONE(
          result, [&](ValueNode* value) { SetAccumulator(value); });
      break;
    }
    default:
      break;
  }

  // Create a generic load in the fallthrough.
  ValueNode* context = GetContext();
  SetAccumulator(
      AddNewNode<LoadNamedGeneric>({context, object}, name, feedback_source));
}

void MaglevGraphBuilder::VisitGetNamedPropertyFromSuper() {
  // GetNamedPropertyFromSuper <receiver> <name_index> <slot>
  ValueNode* receiver = LoadRegisterTagged(0);
  ValueNode* home_object = GetAccumulatorTagged();
  compiler::NameRef name = GetRefOperand<Name>(1);
  FeedbackSlot slot = GetSlotOperand(2);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  // {home_object} is guaranteed to be a HeapObject.
  ValueNode* home_object_map =
      AddNewNode<LoadTaggedField>({home_object}, HeapObject::kMapOffset);
  ValueNode* lookup_start_object =
      AddNewNode<LoadTaggedField>({home_object_map}, Map::kPrototypeOffset);

  const compiler::ProcessedFeedback& processed_feedback =
      broker()->GetFeedbackForPropertyAccess(feedback_source,
                                             compiler::AccessMode::kLoad, name);

  switch (processed_feedback.kind()) {
    case compiler::ProcessedFeedback::kInsufficient:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericNamedAccess);
      return;

    case compiler::ProcessedFeedback::kNamedAccess: {
      if (TryReuseKnownPropertyLoad(lookup_start_object, name)) return;
      ReduceResult result = TryBuildNamedAccess(
          receiver, lookup_start_object, processed_feedback.AsNamedAccess(),
          feedback_source, compiler::AccessMode::kLoad);
      PROCESS_AND_RETURN_IF_DONE(
          result, [&](ValueNode* value) { SetAccumulator(value); });
      break;
    }
    default:
      break;
  }

  // Create a generic load.
  ValueNode* context = GetContext();
  SetAccumulator(AddNewNode<LoadNamedFromSuperGeneric>(
      {context, receiver, lookup_start_object}, name, feedback_source));
}

void MaglevGraphBuilder::VisitGetKeyedProperty() {
  // GetKeyedProperty <object> <slot>
  ValueNode* object = LoadRegisterTagged(0);
  // TODO(leszeks): We don't need to tag the key if it's an Int32 and a simple
  // monomorphic element load.
  FeedbackSlot slot = GetSlotOperand(1);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  const compiler::ProcessedFeedback& processed_feedback =
      broker()->GetFeedbackForPropertyAccess(
          feedback_source, compiler::AccessMode::kLoad, base::nullopt);

  if (current_for_in_state.index != nullptr &&
      current_for_in_state.receiver == object &&
      current_for_in_state.key == current_interpreter_frame_.accumulator()) {
    if (current_for_in_state.receiver_needs_map_check) {
      auto* receiver_map =
          AddNewNode<LoadTaggedField>({object}, HeapObject::kMapOffset);
      AddNewNode<CheckDynamicValue>(
          {receiver_map, current_for_in_state.cache_type});
      current_for_in_state.receiver_needs_map_check = false;
    }
    // TODO(leszeks): Cache the indices across the loop.
    auto* cache_array = AddNewNode<LoadTaggedField>(
        {current_for_in_state.enum_cache}, EnumCache::kIndicesOffset);
    AddNewNode<CheckFixedArrayNonEmpty>({cache_array});
    // TODO(leszeks): Cache the field index per iteration.
    auto* field_index = AddNewNode<LoadFixedArrayElement>(
        {cache_array, current_for_in_state.index});
    SetAccumulator(
        AddNewNode<LoadTaggedFieldByFieldIndex>({object, field_index}));
    return;
  }

  switch (processed_feedback.kind()) {
    case compiler::ProcessedFeedback::kInsufficient:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericKeyedAccess);
      return;

    case compiler::ProcessedFeedback::kElementAccess: {
      // Get the accumulator without conversion. TryBuildElementAccess
      // will try to pick the best representation.
      ValueNode* index = current_interpreter_frame_.accumulator();
      ReduceResult result = TryBuildElementAccess(
          object, index, processed_feedback.AsElementAccess(), feedback_source);
      PROCESS_AND_RETURN_IF_DONE(
          result, [&](ValueNode* value) { SetAccumulator(value); });
      break;
    }

    case compiler::ProcessedFeedback::kNamedAccess: {
      ValueNode* key = GetAccumulatorTagged();
      compiler::NameRef name = processed_feedback.AsNamedAccess().name();
      if (BuildCheckValue(key, name).IsDoneWithAbort()) return;
      if (TryReuseKnownPropertyLoad(object, name)) return;
      ReduceResult result = TryBuildNamedAccess(
          object, object, processed_feedback.AsNamedAccess(), feedback_source,
          compiler::AccessMode::kLoad);
      PROCESS_AND_RETURN_IF_DONE(
          result, [&](ValueNode* value) { SetAccumulator(value); });
      break;
    }

    default:
      break;
  }

  // Create a generic store in the fallthrough.
  ValueNode* context = GetContext();
  ValueNode* key = GetAccumulatorTagged();
  SetAccumulator(
      AddNewNode<GetKeyedGeneric>({context, object, key}, feedback_source));
}

void MaglevGraphBuilder::VisitLdaModuleVariable() {
  // LdaModuleVariable <cell_index> <depth>
  int cell_index = iterator_.GetImmediateOperand(0);
  size_t depth = iterator_.GetUnsignedImmediateOperand(1);

  ValueNode* context = GetContext();
  MinimizeContextChainDepth(&context, &depth);

  if (compilation_unit_->info()->specialize_to_function_context()) {
    compiler::OptionalContextRef maybe_ref =
        FunctionContextSpecialization::TryToRef(compilation_unit_, context,
                                                &depth);
    if (maybe_ref.has_value()) {
      context = GetConstant(maybe_ref.value());
    }
  }

  for (size_t i = 0; i < depth; i++) {
    context = LoadAndCacheContextSlot(
        context, Context::OffsetOfElementAt(Context::PREVIOUS_INDEX),
        kImmutable);
  }
  ValueNode* module = LoadAndCacheContextSlot(
      context, Context::OffsetOfElementAt(Context::EXTENSION_INDEX),
      kImmutable);
  ValueNode* exports_or_imports;
  if (cell_index > 0) {
    exports_or_imports = AddNewNode<LoadTaggedField>(
        {module}, SourceTextModule::kRegularExportsOffset);
    // The actual array index is (cell_index - 1).
    cell_index -= 1;
  } else {
    exports_or_imports = AddNewNode<LoadTaggedField>(
        {module}, SourceTextModule::kRegularImportsOffset);
    // The actual array index is (-cell_index - 1).
    cell_index = -cell_index - 1;
  }
  ValueNode* cell = BuildLoadFixedArrayElement(exports_or_imports, cell_index);
  SetAccumulator(AddNewNode<LoadTaggedField>({cell}, Cell::kValueOffset));
}

void MaglevGraphBuilder::VisitStaModuleVariable() {
  // StaModuleVariable <cell_index> <depth>
  int cell_index = iterator_.GetImmediateOperand(0);
  if (V8_UNLIKELY(cell_index < 0)) {
    BuildCallRuntime(Runtime::kAbort,
                     {GetSmiConstant(static_cast<int>(
                         AbortReason::kUnsupportedModuleOperation))});
    return;
  }

  ValueNode* context = GetContext();
  size_t depth = iterator_.GetUnsignedImmediateOperand(1);
  MinimizeContextChainDepth(&context, &depth);

  if (compilation_unit_->info()->specialize_to_function_context()) {
    compiler::OptionalContextRef maybe_ref =
        FunctionContextSpecialization::TryToRef(compilation_unit_, context,
                                                &depth);
    if (maybe_ref.has_value()) {
      context = GetConstant(maybe_ref.value());
    }
  }

  for (size_t i = 0; i < depth; i++) {
    context = LoadAndCacheContextSlot(
        context, Context::OffsetOfElementAt(Context::PREVIOUS_INDEX),
        kImmutable);
  }
  ValueNode* module = LoadAndCacheContextSlot(
      context, Context::OffsetOfElementAt(Context::EXTENSION_INDEX),
      kImmutable);
  ValueNode* exports = AddNewNode<LoadTaggedField>(
      {module}, SourceTextModule::kRegularExportsOffset);
  // The actual array index is (cell_index - 1).
  cell_index -= 1;
  ValueNode* cell = BuildLoadFixedArrayElement(exports, cell_index);
  BuildStoreTaggedField(cell, GetAccumulatorTagged(), Cell::kValueOffset);
}

void MaglevGraphBuilder::BuildLoadGlobal(
    compiler::NameRef name, compiler::FeedbackSource& feedback_source,
    TypeofMode typeof_mode) {
  const compiler::ProcessedFeedback& access_feedback =
      broker()->GetFeedbackForGlobalAccess(feedback_source);

  if (!access_feedback.IsInsufficient()) {
    const compiler::GlobalAccessFeedback& global_access_feedback =
        access_feedback.AsGlobalAccess();

    if (TryBuildScriptContextAccess(global_access_feedback)) return;
    if (TryBuildPropertyCellAccess(global_access_feedback)) return;
  }

  ValueNode* context = GetContext();
  SetAccumulator(
      AddNewNode<LoadGlobal>({context}, name, feedback_source, typeof_mode));
}

void MaglevGraphBuilder::VisitSetNamedProperty() {
  // SetNamedProperty <object> <name_index> <slot>
  ValueNode* object = LoadRegisterTagged(0);
  compiler::NameRef name = GetRefOperand<Name>(1);
  FeedbackSlot slot = GetSlotOperand(2);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  const compiler::ProcessedFeedback& processed_feedback =
      broker()->GetFeedbackForPropertyAccess(
          feedback_source, compiler::AccessMode::kStore, name);

  switch (processed_feedback.kind()) {
    case compiler::ProcessedFeedback::kInsufficient:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericNamedAccess);
      return;

    case compiler::ProcessedFeedback::kNamedAccess:
      if (TryBuildNamedAccess(object, object,
                              processed_feedback.AsNamedAccess(),
                              feedback_source, compiler::AccessMode::kStore)
              .IsDone()) {
        return;
      }
      break;
    default:
      break;
  }

  // Create a generic store in the fallthrough.
  ValueNode* context = GetContext();
  ValueNode* value = GetAccumulatorTagged();
  AddNewNode<SetNamedGeneric>({context, object, value}, name, feedback_source);
}

void MaglevGraphBuilder::VisitDefineNamedOwnProperty() {
  // DefineNamedOwnProperty <object> <name_index> <slot>
  ValueNode* object = LoadRegisterTagged(0);
  compiler::NameRef name = GetRefOperand<Name>(1);
  FeedbackSlot slot = GetSlotOperand(2);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  const compiler::ProcessedFeedback& processed_feedback =
      broker()->GetFeedbackForPropertyAccess(
          feedback_source, compiler::AccessMode::kStore, name);

  switch (processed_feedback.kind()) {
    case compiler::ProcessedFeedback::kInsufficient:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericNamedAccess);
      return;

    case compiler::ProcessedFeedback::kNamedAccess:
      if (TryBuildNamedAccess(object, object,
                              processed_feedback.AsNamedAccess(),
                              feedback_source, compiler::AccessMode::kDefine)
              .IsDone()) {
        return;
      }
      break;

    default:
      break;
  }

  // Create a generic store in the fallthrough.
  ValueNode* context = GetContext();
  ValueNode* value = GetAccumulatorTagged();
  AddNewNode<DefineNamedOwnGeneric>({context, object, value}, name,
                                    feedback_source);
}

void MaglevGraphBuilder::VisitSetKeyedProperty() {
  // SetKeyedProperty <object> <key> <slot>
  ValueNode* object = LoadRegisterTagged(0);
  FeedbackSlot slot = GetSlotOperand(2);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  const compiler::ProcessedFeedback& processed_feedback =
      broker()->GetFeedbackForPropertyAccess(
          feedback_source, compiler::AccessMode::kLoad, base::nullopt);

  switch (processed_feedback.kind()) {
    case compiler::ProcessedFeedback::kInsufficient:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericKeyedAccess);
      return;

    case compiler::ProcessedFeedback::kElementAccess: {
      // Get the key without conversion. TryBuildElementAccess will try to pick
      // the best representation.
      ValueNode* index =
          current_interpreter_frame_.get(iterator_.GetRegisterOperand(1));
      if (TryBuildElementAccess(object, index,
                                processed_feedback.AsElementAccess(),
                                feedback_source)
              .IsDone()) {
        return;
      }
    } break;

    default:
      break;
  }

  // Create a generic store in the fallthrough.
  ValueNode* key = LoadRegisterTagged(1);
  ValueNode* context = GetContext();
  ValueNode* value = GetAccumulatorTagged();
  AddNewNode<SetKeyedGeneric>({context, object, key, value}, feedback_source);
}

void MaglevGraphBuilder::VisitDefineKeyedOwnProperty() {
  // DefineKeyedOwnProperty <object> <key> <flags> <slot>
  ValueNode* object = LoadRegisterTagged(0);
  ValueNode* key = LoadRegisterTagged(1);
  ValueNode* flags = GetSmiConstant(GetFlag8Operand(2));
  FeedbackSlot slot = GetSlotOperand(3);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  // TODO(victorgomes): Add monomorphic fast path.

  // Create a generic store in the fallthrough.
  ValueNode* context = GetContext();
  ValueNode* value = GetAccumulatorTagged();
  AddNewNode<DefineKeyedOwnGeneric>({context, object, key, value, flags},
                                    feedback_source);
}

void MaglevGraphBuilder::VisitStaInArrayLiteral() {
  // StaInArrayLiteral <object> <name_reg> <slot>
  ValueNode* object = LoadRegisterTagged(0);
  ValueNode* name = LoadRegisterTagged(1);
  FeedbackSlot slot = GetSlotOperand(2);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  const compiler::ProcessedFeedback& processed_feedback =
      broker()->GetFeedbackForPropertyAccess(
          feedback_source, compiler::AccessMode::kStoreInLiteral,
          base::nullopt);

  switch (processed_feedback.kind()) {
    case compiler::ProcessedFeedback::kInsufficient:
      EmitUnconditionalDeopt(
          DeoptimizeReason::kInsufficientTypeFeedbackForGenericKeyedAccess);
      return;

    default:
      break;
  }

  // Create a generic store in the fallthrough.
  ValueNode* context = GetContext();
  ValueNode* value = GetAccumulatorTagged();
  AddNewNode<StoreInArrayLiteralGeneric>({context, object, name, value},
                                         feedback_source);
}

void MaglevGraphBuilder::VisitDefineKeyedOwnPropertyInLiteral() {
  ValueNode* object = LoadRegisterTagged(0);
  ValueNode* name = LoadRegisterTagged(1);
  ValueNode* value = GetAccumulatorTagged();
  ValueNode* flags = GetSmiConstant(GetFlag8Operand(2));
  ValueNode* slot = GetSmiConstant(GetSlotOperand(3).ToInt());
  ValueNode* feedback_vector = GetConstant(feedback());
  BuildCallRuntime(Runtime::kDefineKeyedOwnPropertyInLiteral,
                   {object, name, value, flags, feedback_vector, slot});
}

void MaglevGraphBuilder::VisitAdd() { VisitBinaryOperation<Operation::kAdd>(); }
void MaglevGraphBuilder::VisitSub() {
  VisitBinaryOperation<Operation::kSubtract>();
}
void MaglevGraphBuilder::VisitMul() {
  VisitBinaryOperation<Operation::kMultiply>();
}
void MaglevGraphBuilder::VisitDiv() {
  VisitBinaryOperation<Operation::kDivide>();
}
void MaglevGraphBuilder::VisitMod() {
  VisitBinaryOperation<Operation::kModulus>();
}
void MaglevGraphBuilder::VisitExp() {
  VisitBinaryOperation<Operation::kExponentiate>();
}
void MaglevGraphBuilder::VisitBitwiseOr() {
  VisitBinaryOperation<Operation::kBitwiseOr>();
}
void MaglevGraphBuilder::VisitBitwiseXor() {
  VisitBinaryOperation<Operation::kBitwiseXor>();
}
void MaglevGraphBuilder::VisitBitwiseAnd() {
  VisitBinaryOperation<Operation::kBitwiseAnd>();
}
void MaglevGraphBuilder::VisitShiftLeft() {
  VisitBinaryOperation<Operation::kShiftLeft>();
}
void MaglevGraphBuilder::VisitShiftRight() {
  VisitBinaryOperation<Operation::kShiftRight>();
}
void MaglevGraphBuilder::VisitShiftRightLogical() {
  VisitBinaryOperation<Operation::kShiftRightLogical>();
}

void MaglevGraphBuilder::VisitAddSmi() {
  VisitBinarySmiOperation<Operation::kAdd>();
}
void MaglevGraphBuilder::VisitSubSmi() {
  VisitBinarySmiOperation<Operation::kSubtract>();
}
void MaglevGraphBuilder::VisitMulSmi() {
  VisitBinarySmiOperation<Operation::kMultiply>();
}
void MaglevGraphBuilder::VisitDivSmi() {
  VisitBinarySmiOperation<Operation::kDivide>();
}
void MaglevGraphBuilder::VisitModSmi() {
  VisitBinarySmiOperation<Operation::kModulus>();
}
void MaglevGraphBuilder::VisitExpSmi() {
  VisitBinarySmiOperation<Operation::kExponentiate>();
}
void MaglevGraphBuilder::VisitBitwiseOrSmi() {
  VisitBinarySmiOperation<Operation::kBitwiseOr>();
}
void MaglevGraphBuilder::VisitBitwiseXorSmi() {
  VisitBinarySmiOperation<Operation::kBitwiseXor>();
}
void MaglevGraphBuilder::VisitBitwiseAndSmi() {
  VisitBinarySmiOperation<Operation::kBitwiseAnd>();
}
void MaglevGraphBuilder::VisitShiftLeftSmi() {
  VisitBinarySmiOperation<Operation::kShiftLeft>();
}
void MaglevGraphBuilder::VisitShiftRightSmi() {
  VisitBinarySmiOperation<Operation::kShiftRight>();
}
void MaglevGraphBuilder::VisitShiftRightLogicalSmi() {
  VisitBinarySmiOperation<Operation::kShiftRightLogical>();
}

void MaglevGraphBuilder::VisitInc() {
  VisitUnaryOperation<Operation::kIncrement>();
}
void MaglevGraphBuilder::VisitDec() {
  VisitUnaryOperation<Operation::kDecrement>();
}
void MaglevGraphBuilder::VisitNegate() {
  VisitUnaryOperation<Operation::kNegate>();
}
void MaglevGraphBuilder::VisitBitwiseNot() {
  VisitUnaryOperation<Operation::kBitwiseNot>();
}

void MaglevGraphBuilder::VisitToBooleanLogicalNot() {
  ValueNode* value = GetAccumulatorTagged();
  switch (value->opcode()) {
#define CASE(Name)                                                             \
  case Opcode::k##Name: {                                                      \
    SetAccumulator(                                                            \
        GetBooleanConstant(!value->Cast<Name>()->ToBoolean(local_isolate()))); \
    break;                                                                     \
  }
    CONSTANT_VALUE_NODE_LIST(CASE)
#undef CASE
    default:
      SetAccumulator(AddNewNode<ToBooleanLogicalNot>({value}));
      break;
  }
}

void MaglevGraphBuilder::VisitLogicalNot() {
  // Invariant: accumulator must already be a boolean value.
  ValueNode* value = GetAccumulatorTagged();
  switch (value->opcode()) {
#define CASE(Name)                                                             \
  case Opcode::k##Name: {                                                      \
    SetAccumulator(                                                            \
        GetBooleanConstant(!value->Cast<Name>()->ToBoolean(local_isolate()))); \
    break;                                                                     \
  }
    CONSTANT_VALUE_NODE_LIST(CASE)
#undef CASE
    default:
      SetAccumulator(AddNewNode<LogicalNot>({value}));
      break;
  }
}

void MaglevGraphBuilder::VisitTypeOf() {
  ValueNode* value = GetAccumulatorTagged();
  SetAccumulator(BuildCallBuiltin<Builtin::kTypeof>({value}));
}

void MaglevGraphBuilder::VisitDeletePropertyStrict() {
  ValueNode* object = LoadRegisterTagged(0);
  ValueNode* key = GetAccumulatorTagged();
  ValueNode* context = GetContext();
  SetAccumulator(AddNewNode<DeleteProperty>({context, object, key},
                                            LanguageMode::kStrict));
}

void MaglevGraphBuilder::VisitDeletePropertySloppy() {
  ValueNode* object = LoadRegisterTagged(0);
  ValueNode* key = GetAccumulatorTagged();
  ValueNode* context = GetContext();
  SetAccumulator(AddNewNode<DeleteProperty>({context, object, key},
                                            LanguageMode::kSloppy));
}

void MaglevGraphBuilder::VisitGetSuperConstructor() {
  ValueNode* active_function = GetAccumulatorTagged();
  ValueNode* map =
      AddNewNode<LoadTaggedField>({active_function}, HeapObject::kMapOffset);
  ValueNode* map_proto =
      AddNewNode<LoadTaggedField>({map}, Map::kPrototypeOffset);
  StoreRegister(iterator_.GetRegisterOperand(0), map_proto);
}

void MaglevGraphBuilder::VisitFindNonDefaultConstructorOrConstruct() {
  ValueNode* this_function = LoadRegisterTagged(0);
  ValueNode* new_target = LoadRegisterTagged(1);

  CallBuiltin* call_builtin =
      BuildCallBuiltin<Builtin::kFindNonDefaultConstructorOrConstruct>(
          {this_function, new_target});
  auto result = iterator_.GetRegisterPairOperand(2);
  StoreRegisterPair(result, call_builtin);
}

ReduceResult MaglevGraphBuilder::BuildInlined(const CallArguments& args,
                                              BasicBlockRef* start_ref,
                                              BasicBlockRef* end_ref) {
  DCHECK(is_inline());

  // Manually create the prologue of the inner function graph, so that we
  // can manually set up the arguments.
  StartPrologue();

  // Set receiver.
  ValueNode* receiver = GetConvertReceiver(function(), args);
  SetArgument(0, receiver);
  // Set remaining arguments.
  RootConstant* undefined_constant =
      GetRootConstant(RootIndex::kUndefinedValue);
  for (int i = 1; i < parameter_count(); i++) {
    ValueNode* arg_value = args[i - 1];
    if (arg_value == nullptr) arg_value = undefined_constant;
    SetArgument(i, arg_value);
  }

  int arg_count = static_cast<int>(args.count());
  int formal_parameter_count =
      compilation_unit_->shared_function_info()
          .internal_formal_parameter_count_without_receiver();
  if (arg_count != formal_parameter_count) {
    inlined_arguments_.emplace(zone()->NewVector<ValueNode*>(arg_count + 1));
    (*inlined_arguments_)[0] = receiver;
    for (int i = 0; i < arg_count; i++) {
      (*inlined_arguments_)[i + 1] = args[i];
    }
  }

  BuildRegisterFrameInitialization(GetConstant(function().context(broker())),
                                   GetConstant(function()));
  BuildMergeStates();
  BasicBlock* inlined_prologue = EndPrologue();

  // Set the entry JumpToInlined to jump to the prologue block.
  // TODO(leszeks): Passing start_ref to JumpToInlined creates a two-element
  // linked list of refs. Consider adding a helper to explicitly set the target
  // instead.
  start_ref->SetToBlockAndReturnNext(inlined_prologue)
      ->SetToBlockAndReturnNext(inlined_prologue);

  // Build the inlined function body.
  BuildBody();

  // All returns in the inlined body jump to a merge point one past the
  // bytecode length (i.e. at offset bytecode.length()). Create a block at
  // this fake offset and have it jump out of the inlined function, into a new
  // block that we create which resumes execution of the outer function.
  // TODO(leszeks): Wrap this up in a helper.
  DCHECK_NULL(current_block_);

  // If we don't have a merge state at the inline_exit_offset, then there is no
  // control flow that reaches the end of the inlined function, either because
  // of infinite loops or deopts
  if (merge_states_[inline_exit_offset()] == nullptr) {
    return ReduceResult::DoneWithAbort();
  }

  ProcessMergePoint(inline_exit_offset());
  StartNewBlock(inline_exit_offset());
  FinishBlock<JumpFromInlined>({}, end_ref);

  // Pull the returned accumulator value out of the inlined function's final
  // merged return state.
  return current_interpreter_frame_.accumulator();
}

#define TRACE_INLINING(...)                       \
  do {                                            \
    if (v8_flags.trace_maglev_inlining)           \
      StdoutStream{} << __VA_ARGS__ << std::endl; \
  } while (false)

#define TRACE_CANNOT_INLINE(...) \
  TRACE_INLINING("  cannot inline " << shared << ": " << __VA_ARGS__)

bool MaglevGraphBuilder::ShouldInlineCall(compiler::JSFunctionRef function,
                                          float call_frequency) {
  compiler::OptionalCodeRef code = function.code(broker());
  compiler::SharedFunctionInfoRef shared = function.shared(broker());
  if (graph()->total_inlined_bytecode_size() >
      v8_flags.max_maglev_inlined_bytecode_size_cumulative) {
    TRACE_CANNOT_INLINE("maximum inlined bytecode size");
    return false;
  }
  if (!code) {
    // TODO(verwaest): Soft deopt instead?
    TRACE_CANNOT_INLINE("it has not been compiled yet");
    return false;
  }
  if (code->object()->kind() == CodeKind::TURBOFAN) {
    TRACE_CANNOT_INLINE("already turbofanned");
    return false;
  }
  if (!function.feedback_vector(broker()).has_value()) {
    TRACE_CANNOT_INLINE("no feedback vector");
    return false;
  }
  SharedFunctionInfo::Inlineability inlineability =
      shared.GetInlineability(broker());
  if (inlineability != SharedFunctionInfo::Inlineability::kIsInlineable) {
    TRACE_CANNOT_INLINE(inlineability);
    return false;
  }
  // TODO(victorgomes): Support NewTarget/RegisterInput in inlined functions.
  compiler::BytecodeArrayRef bytecode = shared.GetBytecodeArray(broker());
  if (bytecode.incoming_new_target_or_generator_register().is_valid()) {
    TRACE_CANNOT_INLINE("use unsupported NewTargetOrGenerator register");
    return false;
  }
  // TODO(victorgomes): Support exception handler inside inlined functions.
  if (bytecode.handler_table_size() > 0) {
    TRACE_CANNOT_INLINE("use unsupported expection handlers");
    return false;
  }
  // TODO(victorgomes): Support inlined allocation of the arguments object.
  interpreter::BytecodeArrayIterator iterator(bytecode.object());
  for (; !iterator.done(); iterator.Advance()) {
    switch (iterator.current_bytecode()) {
      case interpreter::Bytecode::kCreateMappedArguments:
      case interpreter::Bytecode::kCreateUnmappedArguments:
      case interpreter::Bytecode::kCreateRestParameter:
        TRACE_CANNOT_INLINE("not supported inlined arguments object");
        return false;
      default:
        break;
    }
  }
  if (call_frequency < v8_flags.min_maglev_inlining_frequency) {
    TRACE_CANNOT_INLINE("call frequency ("
                        << call_frequency << ") < minimum threshold ("
                        << v8_flags.min_maglev_inlining_frequency << ")");
    return false;
  }
  if (bytecode.length() < v8_flags.max_maglev_inlined_bytecode_size_small) {
    TRACE_INLINING("  inlining " << shared << ": small function");
    return true;
  }
  if (bytecode.length() > v8_flags.max_maglev_inlined_bytecode_size) {
    TRACE_CANNOT_INLINE("big function, size ("
                        << bytecode.length() << ") >= max-size ("
                        << v8_flags.max_maglev_inlined_bytecode_size << ")");
    return false;
  }
  if (inlining_depth() > v8_flags.max_maglev_inline_depth) {
    TRACE_CANNOT_INLINE("inlining depth ("
                        << inlining_depth() << ") >= max-depth ("
                        << v8_flags.max_maglev_inline_depth << ")");
    return false;
  }
  TRACE_INLINING("  inlining " << shared);
  if (v8_flags.trace_maglev_inlining_verbose) {
    BytecodeArray::Disassemble(bytecode.object(), std::cout);
    function.feedback_vector(broker())->object()->Print(std::cout);
  }
  graph()->add_inlined_bytecode_size(bytecode.length());
  return true;
}

ReduceResult MaglevGraphBuilder::TryBuildInlinedCall(
    compiler::JSFunctionRef function, CallArguments& args,
    const compiler::FeedbackSource& feedback_source) {
  DCHECK_EQ(args.mode(), CallArguments::kDefault);
  float feedback_frequency = 0.0f;
  if (feedback_source.IsValid()) {
    compiler::ProcessedFeedback const& feedback =
        broker()->GetFeedbackForCall(feedback_source);
    feedback_frequency =
        feedback.IsInsufficient() ? 0.0f : feedback.AsCall().frequency();
  }
  float call_frequency = feedback_frequency * call_frequency_;
  if (!ShouldInlineCall(function, call_frequency)) {
    return ReduceResult::Fail();
  }

  compiler::BytecodeArrayRef bytecode =
      function.shared(broker()).GetBytecodeArray(broker());
  graph()->inlined_functions().push_back(
      OptimizedCompilationInfo::InlinedFunctionHolder(
          function.shared(broker()).object(), bytecode.object(),
          current_source_position_));

  // Create a new compilation unit and graph builder for the inlined
  // function.
  MaglevCompilationUnit* inner_unit =
      MaglevCompilationUnit::NewInner(zone(), compilation_unit_, function);
  MaglevGraphBuilder inner_graph_builder(
      local_isolate_, inner_unit, graph_, call_frequency,
      BytecodeOffset(iterator_.current_offset()), this);

  // Propagate catch block.
  inner_graph_builder.parent_catch_block_ = GetCurrentTryCatchBlockOffset();

  // Finish the current block with a jump to the inlined function.
  BasicBlockRef start_ref, end_ref;
  BasicBlock* block = FinishBlock<JumpToInlined>({}, &start_ref, inner_unit);

  ReduceResult result =
      inner_graph_builder.BuildInlined(args, &start_ref, &end_ref);
  if (result.IsDoneWithAbort()) {
    MarkBytecodeDead();
    return ReduceResult::DoneWithAbort();
  }
  DCHECK(result.IsDoneWithValue());

  // Propagate KnownNodeAspects back to the caller.
  current_interpreter_frame_.set_known_node_aspects(
      inner_graph_builder.current_interpreter_frame_.known_node_aspects());

  // Create a new block at our current offset, and resume execution. Do this
  // manually to avoid trying to resolve any merges to this offset, which will
  // have already been processed on entry to this visitor.
  current_block_ = zone()->New<BasicBlock>(MergePointInterpreterFrameState::New(
      *compilation_unit_, current_interpreter_frame_,
      iterator_.current_offset(), 1, block, GetInLiveness()));
  // Set the exit JumpFromInlined to jump to this resume block.
  // TODO(leszeks): Passing start_ref to JumpFromInlined creates a two-element
  // linked list of refs. Consider adding a helper to explicitly set the
  // target instead.
  end_ref.SetToBlockAndReturnNext(current_block_)
      ->SetToBlockAndReturnNext(current_block_);

  return result;
}

ReduceResult MaglevGraphBuilder::TryReduceStringFromCharCode(
    compiler::JSFunctionRef target, CallArguments& args) {
  if (args.count() != 1) return ReduceResult::Fail();
  return AddNewNode<BuiltinStringFromCharCode>({GetTruncatedInt32FromNumber(
      args[0], TaggedToFloat64ConversionType::kNumber)});
}

ReduceResult MaglevGraphBuilder::TryReduceStringPrototypeCharCodeAt(
    compiler::JSFunctionRef target, CallArguments& args) {
  ValueNode* receiver = GetTaggedOrUndefined(args.receiver());
  ValueNode* index;
  if (args.count() == 0) {
    // Index is the undefined object. ToIntegerOrInfinity(undefined) = 0.
    index = GetInt32Constant(0);
  } else {
    index = GetInt32ElementIndex(args[0]);
  }
  // Any other argument is ignored.
  // Ensure that {receiver} is actually a String.
  BuildCheckString(receiver);
  // And index is below length.
  ValueNode* length = AddNewNode<StringLength>({receiver});
  AddNewNode<CheckInt32Condition>({index, length},
                                  AssertCondition::kUnsignedLessThan,
                                  DeoptimizeReason::kOutOfBounds);
  return AddNewNode<BuiltinStringPrototypeCharCodeOrCodePointAt>(
      {receiver, index},
      BuiltinStringPrototypeCharCodeOrCodePointAt::kCharCodeAt);
}

ReduceResult MaglevGraphBuilder::TryReduceStringPrototypeCodePointAt(
    compiler::JSFunctionRef target, CallArguments& args) {
  ValueNode* receiver = GetTaggedOrUndefined(args.receiver());
  ValueNode* index;
  if (args.count() == 0) {
    // Index is the undefined object. ToIntegerOrInfinity(undefined) = 0.
    index = GetInt32Constant(0);
  } else {
    index = GetInt32ElementIndex(args[0]);
  }
  // Any other argument is ignored.
  // Ensure that {receiver} is actually a String.
  BuildCheckString(receiver);
  // And index is below length.
  ValueNode* length = AddNewNode<StringLength>({receiver});
  AddNewNode<CheckInt32Condition>({index, length},
                                  AssertCondition::kUnsignedLessThan,
                                  DeoptimizeReason::kOutOfBounds);
  return AddNewNode<BuiltinStringPrototypeCharCodeOrCodePointAt>(
      {receiver, index},
      BuiltinStringPrototypeCharCodeOrCodePointAt::kCodePointAt);
}

template <typename LoadNode>
ReduceResult MaglevGraphBuilder::TryBuildLoadDataView(const CallArguments& args,
                                                      ExternalArrayType type) {
  if (!broker()->dependencies()->DependOnArrayBufferDetachingProtector()) {
    // TODO(victorgomes): Add checks whether the array has been detached.
    return ReduceResult::Fail();
  }
  // TODO(victorgomes): Add data view to known types.
  ValueNode* receiver = GetTaggedOrUndefined(args.receiver());
  AddNewNode<CheckInstanceType>({receiver}, CheckType::kCheckHeapObject,
                                JS_DATA_VIEW_TYPE);
  // TODO(v8:11111): Optimize for JS_RAB_GSAB_DATA_VIEW_TYPE too.
  ValueNode* offset =
      args[0] ? GetInt32ElementIndex(args[0]) : GetInt32Constant(0);
  AddNewNode<CheckJSDataViewBounds>({receiver, offset}, type);
  ValueNode* is_little_endian =
      args[1] ? GetTaggedValue(args[1]) : GetBooleanConstant(false);
  return AddNewNode<LoadNode>({receiver, offset, is_little_endian}, type);
}

template <typename StoreNode, typename Function>
ReduceResult MaglevGraphBuilder::TryBuildStoreDataView(
    const CallArguments& args, ExternalArrayType type, Function&& getValue) {
  if (!broker()->dependencies()->DependOnArrayBufferDetachingProtector()) {
    // TODO(victorgomes): Add checks whether the array has been detached.
    return ReduceResult::Fail();
  }
  // TODO(victorgomes): Add data view to known types.
  ValueNode* receiver = GetTaggedOrUndefined(args.receiver());
  AddNewNode<CheckInstanceType>({receiver}, CheckType::kCheckHeapObject,
                                JS_DATA_VIEW_TYPE);
  // TODO(v8:11111): Optimize for JS_RAB_GSAB_DATA_VIEW_TYPE too.
  ValueNode* offset =
      args[0] ? GetInt32ElementIndex(args[0]) : GetInt32Constant(0);
  AddNewNode<CheckJSDataViewBounds>({receiver, offset},
                                    ExternalArrayType::kExternalFloat64Array);
  ValueNode* value = getValue(args[1]);
  ValueNode* is_little_endian =
      args[2] ? GetTaggedValue(args[2]) : GetBooleanConstant(false);
  AddNewNode<StoreNode>({receiver, offset, value, is_little_endian}, type);
  return GetRootConstant(RootIndex::kUndefinedValue);
}

ReduceResult MaglevGraphBuilder::TryReduceDataViewPrototypeGetInt8(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryBuildLoadDataView<LoadSignedIntDataViewElement>(
      args, ExternalArrayType::kExternalInt8Array);
}
ReduceResult MaglevGraphBuilder::TryReduceDataViewPrototypeSetInt8(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryBuildStoreDataView<StoreSignedIntDataViewElement>(
      args, ExternalArrayType::kExternalInt8Array, [&](ValueNode* value) {
        return value ? GetInt32(value) : GetInt32Constant(0);
      });
}
ReduceResult MaglevGraphBuilder::TryReduceDataViewPrototypeGetInt16(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryBuildLoadDataView<LoadSignedIntDataViewElement>(
      args, ExternalArrayType::kExternalInt16Array);
}
ReduceResult MaglevGraphBuilder::TryReduceDataViewPrototypeSetInt16(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryBuildStoreDataView<StoreSignedIntDataViewElement>(
      args, ExternalArrayType::kExternalInt16Array, [&](ValueNode* value) {
        return value ? GetInt32(value) : GetInt32Constant(0);
      });
}
ReduceResult MaglevGraphBuilder::TryReduceDataViewPrototypeGetInt32(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryBuildLoadDataView<LoadSignedIntDataViewElement>(
      args, ExternalArrayType::kExternalInt32Array);
}
ReduceResult MaglevGraphBuilder::TryReduceDataViewPrototypeSetInt32(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryBuildStoreDataView<StoreSignedIntDataViewElement>(
      args, ExternalArrayType::kExternalInt32Array, [&](ValueNode* value) {
        return value ? GetInt32(value) : GetInt32Constant(0);
      });
}
ReduceResult MaglevGraphBuilder::TryReduceDataViewPrototypeGetFloat64(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryBuildLoadDataView<LoadDoubleDataViewElement>(
      args, ExternalArrayType::kExternalFloat64Array);
}
ReduceResult MaglevGraphBuilder::TryReduceDataViewPrototypeSetFloat64(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryBuildStoreDataView<StoreDoubleDataViewElement>(
      args, ExternalArrayType::kExternalFloat64Array, [&](ValueNode* value) {
        return value ? GetFloat64(value, TaggedToFloat64ConversionType::kNumber)
                     : GetFloat64Constant(
                           std::numeric_limits<double>::quiet_NaN());
      });
}

ReduceResult MaglevGraphBuilder::TryReduceFunctionPrototypeCall(
    compiler::JSFunctionRef target, CallArguments& args) {
  // We can't reduce Function#call when there is no receiver function.
  if (args.receiver_mode() == ConvertReceiverMode::kNullOrUndefined) {
    return ReduceResult::Fail();
  }
  // Use Function.prototype.call context, to ensure any exception is thrown in
  // the correct context.
  ValueNode* context = GetConstant(target.context(broker()));
  ValueNode* receiver = GetTaggedOrUndefined(args.receiver());
  args.PopReceiver(ConvertReceiverMode::kAny);
  return BuildGenericCall(receiver, context, Call::TargetType::kAny, args);
}

ReduceResult MaglevGraphBuilder::TryReduceObjectPrototypeHasOwnProperty(
    compiler::JSFunctionRef target, CallArguments& args) {
  // We can't reduce Function#call when there is no receiver function.
  if (args.receiver_mode() == ConvertReceiverMode::kNullOrUndefined) {
    return ReduceResult::Fail();
  }
  if (args.receiver() != current_for_in_state.receiver) {
    return ReduceResult::Fail();
  }
  if (args.count() != 1 || args[0] != current_for_in_state.key) {
    return ReduceResult::Fail();
  }
  return GetRootConstant(RootIndex::kTrueValue);
}

ReduceResult MaglevGraphBuilder::TryReduceMathRound(
    compiler::JSFunctionRef target, CallArguments& args) {
  return DoTryReduceMathRound(target, args, Float64Round::Kind::kNearest);
}

ReduceResult MaglevGraphBuilder::TryReduceMathFloor(
    compiler::JSFunctionRef target, CallArguments& args) {
  return DoTryReduceMathRound(target, args, Float64Round::Kind::kFloor);
}

ReduceResult MaglevGraphBuilder::TryReduceMathCeil(
    compiler::JSFunctionRef target, CallArguments& args) {
  return DoTryReduceMathRound(target, args, Float64Round::Kind::kCeil);
}

ReduceResult MaglevGraphBuilder::DoTryReduceMathRound(
    compiler::JSFunctionRef target, CallArguments& args,
    Float64Round::Kind kind) {
  if (args.count() == 0) {
    return GetFloat64Constant(std::numeric_limits<double>::quiet_NaN());
  }
  ValueNode* arg = args[0];
  switch (arg->value_representation()) {
    case ValueRepresentation::kInt32:
    case ValueRepresentation::kUint32:
      return arg;
    case ValueRepresentation::kTagged:
      if (CheckType(arg, NodeType::kSmi)) return arg;
      if (CheckType(arg, NodeType::kNumber)) {
        arg = GetFloat64(arg, TaggedToFloat64ConversionType::kNumber);
      } else {
        LazyDeoptContinuationScope continuation_scope(
            this, Float64Round::continuation(kind));
        ToNumberOrNumeric* conversion = AddNewNode<ToNumberOrNumeric>(
            {GetContext(), arg}, Object::Conversion::kToNumber);
        arg = AddNewNode<UncheckedNumberOrOddballToFloat64>(
            {conversion}, TaggedToFloat64ConversionType::kNumber);
      }
      break;
    case ValueRepresentation::kWord64:
      UNREACHABLE();
    case ValueRepresentation::kFloat64:
      break;
  }
  if (IsSupported(CpuOperation::kFloat64Round)) {
    return AddNewNode<Float64Round>({arg}, kind);
  }

  // Update the first argument, in case there was a side-effecting ToNumber
  // conversion.
  args.set_arg(0, arg);
  return ReduceResult::Fail();
}

ReduceResult MaglevGraphBuilder::TryReduceMathPow(
    compiler::JSFunctionRef target, CallArguments& args) {
  if (args.count() < 2) {
    return GetRootConstant(RootIndex::kNanValue);
  }
  // If both arguments are tagged, it is cheaper to call Math.Pow builtin,
  // instead of Float64Exponentiate, since we are still making a call and we
  // don't need to unbox both inputs. See https://crbug.com/1393643.
  if (args[0]->properties().is_tagged() && args[1]->properties().is_tagged()) {
    // The Math.pow call will be created in CallKnownJSFunction reduction.
    return ReduceResult::Fail();
  }
  ValueNode* left = GetFloat64(args[0], TaggedToFloat64ConversionType::kNumber);
  ValueNode* right =
      GetFloat64(args[1], TaggedToFloat64ConversionType::kNumber);
  return AddNewNode<Float64Exponentiate>({left, right});
}

#define MAP_MATH_UNARY_TO_IEEE_754(V) \
  V(MathAcos, acos)                   \
  V(MathAcosh, acosh)                 \
  V(MathAsin, asin)                   \
  V(MathAsinh, asinh)                 \
  V(MathAtan, atan)                   \
  V(MathAtanh, atanh)                 \
  V(MathCbrt, cbrt)                   \
  V(MathCos, cos)                     \
  V(MathCosh, cosh)                   \
  V(MathExp, exp)                     \
  V(MathExpm1, expm1)                 \
  V(MathLog, log)                     \
  V(MathLog1p, log1p)                 \
  V(MathLog10, log10)                 \
  V(MathLog2, log2)                   \
  V(MathSin, sin)                     \
  V(MathSinh, sinh)                   \
  V(MathTan, tan)                     \
  V(MathTanh, tanh)

#define MATH_UNARY_IEEE_BUILTIN_REDUCER(Name, IeeeOp)                \
  ReduceResult MaglevGraphBuilder::TryReduce##Name(                  \
      compiler::JSFunctionRef target, CallArguments& args) {         \
    if (args.count() < 1) {                                          \
      return GetRootConstant(RootIndex::kNanValue);                  \
    }                                                                \
    ValueNode* value =                                               \
        GetFloat64(args[0], TaggedToFloat64ConversionType::kNumber); \
    return AddNewNode<Float64Ieee754Unary>(                          \
        {value}, ExternalReference::ieee754_##IeeeOp##_function());  \
  }

MAP_MATH_UNARY_TO_IEEE_754(MATH_UNARY_IEEE_BUILTIN_REDUCER)

#undef MATH_UNARY_IEEE_BUILTIN_REDUCER
#undef MAP_MATH_UNARY_TO_IEEE_754

ReduceResult MaglevGraphBuilder::TryReduceBuiltin(
    compiler::JSFunctionRef target, CallArguments& args,
    const compiler::FeedbackSource& feedback_source,
    SpeculationMode speculation_mode) {
  if (args.mode() != CallArguments::kDefault) {
    // TODO(victorgomes): Maybe inline the spread stub? Or call known function
    // directly if arguments list is an array.
    return ReduceResult::Fail();
  }
  if (feedback_source.IsValid() &&
      speculation_mode == SpeculationMode::kDisallowSpeculation) {
    // TODO(leszeks): Some builtins might be inlinable without speculation.
    return ReduceResult::Fail();
  }
  CallSpeculationScope speculate(this, feedback_source);
  if (!target.shared(broker()).HasBuiltinId()) {
    return ReduceResult::Fail();
  }
  switch (target.shared(broker()).builtin_id()) {
#define CASE(Name)       \
  case Builtin::k##Name: \
    return TryReduce##Name(target, args);
    MAGLEV_REDUCED_BUILTIN(CASE)
#undef CASE
    default:
      // TODO(v8:7700): Inline more builtins.
      return ReduceResult::Fail();
  }
}

ValueNode* MaglevGraphBuilder::GetConvertReceiver(
    compiler::JSFunctionRef function, const CallArguments& args) {
  compiler::SharedFunctionInfoRef shared = function.shared(broker());
  if (shared.native() || shared.language_mode() == LanguageMode::kStrict) {
    if (args.receiver_mode() == ConvertReceiverMode::kNullOrUndefined) {
      return GetRootConstant(RootIndex::kUndefinedValue);
    } else {
      return GetTaggedValue(args.receiver());
    }
  }
  if (args.receiver_mode() == ConvertReceiverMode::kNullOrUndefined) {
    return GetConstant(
        function.native_context(broker()).global_proxy_object(broker()));
  }
  ValueNode* receiver = GetTaggedValue(args.receiver());
  if (CheckType(receiver, NodeType::kJSReceiver)) return receiver;
  if (Constant* constant = receiver->TryCast<Constant>()) {
    const Handle<HeapObject> object = constant->object().object();
    if (object->IsUndefined() || object->IsNull()) {
      return GetConstant(
          function.native_context(broker()).global_proxy_object(broker()));
    } else if (object->IsJSReceiver()) {
      return constant;
    }
  }
  return AddNewNode<ConvertReceiver>({receiver}, function,
                                     args.receiver_mode());
}

template <typename CallNode, typename... Args>
CallNode* MaglevGraphBuilder::AddNewCallNode(const CallArguments& args,
                                             Args&&... extra_args) {
  size_t input_count = args.count_with_receiver() + CallNode::kFixedInputCount;
  return AddNewNode<CallNode>(
      input_count,
      [&](CallNode* call) {
        int arg_index = 0;
        call->set_arg(arg_index++, GetTaggedOrUndefined(args.receiver()));
        for (size_t i = 0; i < args.count(); ++i) {
          call->set_arg(arg_index++, GetTaggedValue(args[i]));
        }
      },
      std::forward<Args>(extra_args)...);
}

ValueNode* MaglevGraphBuilder::BuildGenericCall(
    ValueNode* target, ValueNode* context, Call::TargetType target_type,
    const CallArguments& args,
    const compiler::FeedbackSource& feedback_source) {
  // TODO(victorgomes): We do not collect call feedback from optimized/inlined
  // calls. In order to be consistent, we don't pass the feedback_source to the
  // IR, so that we avoid collecting for generic calls as well. We might want to
  // revisit this in the future.
  switch (args.mode()) {
    case CallArguments::kDefault:
      return AddNewCallNode<Call>(args, args.receiver_mode(), target_type,
                                  compiler::FeedbackSource(), target, context);
    case CallArguments::kWithSpread:
      DCHECK_EQ(args.receiver_mode(), ConvertReceiverMode::kAny);
      return AddNewCallNode<CallWithSpread>(args, compiler::FeedbackSource(),
                                            target, context);
    case CallArguments::kWithArrayLike:
      DCHECK_EQ(args.receiver_mode(), ConvertReceiverMode::kAny);
      return AddNewNode<CallWithArrayLike>(
          {target, args.receiver(), args[0], context});
  }
}

ValueNode* MaglevGraphBuilder::BuildCallSelf(compiler::JSFunctionRef function,
                                             CallArguments& args) {
  ValueNode* receiver = GetConvertReceiver(function, args);
  size_t input_count = args.count() + CallSelf::kFixedInputCount;
  graph()->set_has_recursive_calls(true);
  return AddNewNode<CallSelf>(
      input_count,
      [&](CallSelf* call) {
        for (int i = 0; i < static_cast<int>(args.count()); i++) {
          call->set_arg(i, GetTaggedValue(args[i]));
        }
      },
      broker(), function, receiver);
}

bool MaglevGraphBuilder::TargetIsCurrentCompilingUnit(
    compiler::JSFunctionRef target) {
  if (compilation_unit_->info()->specialize_to_function_context()) {
    return target.equals(compilation_unit_->function());
  }
  return compilation_unit_->shared_function_info().equals(
      target.shared(broker()));
}

ReduceResult MaglevGraphBuilder::TryBuildCallKnownJSFunction(
    compiler::JSFunctionRef function, CallArguments& args,
    const compiler::FeedbackSource& feedback_source) {
  // Don't inline CallFunction stub across native contexts.
  if (function.native_context(broker()) != broker()->target_native_context()) {
    return ReduceResult::Fail();
  }
  if (args.mode() != CallArguments::kDefault) {
    // TODO(victorgomes): Maybe inline the spread stub? Or call known function
    // directly if arguments list is an array.
    return ReduceResult::Fail();
  }
  if (!is_inline() && TargetIsCurrentCompilingUnit(function)) {
    return BuildCallSelf(function, args);
  }
  if (v8_flags.maglev_inlining) {
    RETURN_IF_DONE(TryBuildInlinedCall(function, args, feedback_source));
  }
  ValueNode* receiver = GetConvertReceiver(function, args);
  size_t input_count = args.count() + CallKnownJSFunction::kFixedInputCount;
  return AddNewNode<CallKnownJSFunction>(
      input_count,
      [&](CallKnownJSFunction* call) {
        for (int i = 0; i < static_cast<int>(args.count()); i++) {
          call->set_arg(i, GetTaggedValue(args[i]));
        }
      },
      broker(), function, receiver);
}

ReduceResult MaglevGraphBuilder::BuildCheckValue(
    ValueNode* node, const compiler::HeapObjectRef& ref) {
  if (node->Is<Constant>()) {
    if (node->Cast<Constant>()->object().equals(ref))
      return ReduceResult::Done();
    EmitUnconditionalDeopt(DeoptimizeReason::kUnknown);
    return ReduceResult::DoneWithAbort();
  }
  // TODO(v8:7700): Add CheckValue support for numbers (incl. conversion between
  // Smi and HeapNumber).
  DCHECK(!ref.IsSmi());
  DCHECK(!ref.IsHeapNumber());
  if (ref.IsString()) {
    AddNewNode<CheckValueEqualsString>({node}, ref.AsInternalizedString());
  } else {
    AddNewNode<CheckValue>({node}, ref);
  }
  return ReduceResult::Done();
}

ReduceResult MaglevGraphBuilder::ReduceCall(
    compiler::ObjectRef object, CallArguments& args,
    const compiler::FeedbackSource& feedback_source,
    SpeculationMode speculation_mode) {
  if (!object.IsJSFunction()) {
    return BuildGenericCall(GetConstant(object), GetContext(),
                            Call::TargetType::kAny, args);
  }
  compiler::JSFunctionRef target = object.AsJSFunction();
  // Do not reduce calls to functions with break points.
  if (!target.shared(broker()).HasBreakInfo()) {
    if (target.object()->IsJSClassConstructor()) {
      // If we have a class constructor, we should raise an exception.
      return BuildCallRuntime(Runtime::kThrowConstructorNonCallableError,
                              {GetConstant(target)});
    }

    DCHECK(target.object()->IsCallable());
    RETURN_IF_DONE(
        TryReduceBuiltin(target, args, feedback_source, speculation_mode));
    RETURN_IF_DONE(TryBuildCallKnownJSFunction(target, args, feedback_source));
  }
  return BuildGenericCall(GetConstant(target), GetContext(),
                          Call::TargetType::kJSFunction, args);
}

ReduceResult MaglevGraphBuilder::ReduceCallForTarget(
    ValueNode* target_node, compiler::JSFunctionRef target, CallArguments& args,
    const compiler::FeedbackSource& feedback_source,
    SpeculationMode speculation_mode) {
  if (BuildCheckValue(target_node, target).IsDoneWithAbort())
    return ReduceResult::DoneWithAbort();
  return ReduceCall(target, args, feedback_source, speculation_mode);
}

ReduceResult MaglevGraphBuilder::ReduceFunctionPrototypeApplyCallWithReceiver(
    ValueNode* target_node, compiler::JSFunctionRef receiver,
    CallArguments& args, const compiler::FeedbackSource& feedback_source,
    SpeculationMode speculation_mode) {
  // TODO(victorgomes): This has currently pretty limited usage and does not
  // work with inlining. We need to implement arguments forwarding like TF
  // ReduceCallOrConstructWithArrayLikeOrSpreadOfCreateArguments.
  return ReduceResult::Fail();
  // compiler::NativeContextRef native_context =
  // broker()->target_native_context(); RETURN_IF_ABORT(BuildCheckValue(
  //     target_node, native_context.function_prototype_apply(broker())));
  // ValueNode* receiver_node = GetTaggedOrUndefined(args.receiver());
  // RETURN_IF_ABORT(BuildCheckValue(receiver_node, receiver));
  // if (args.mode() == CallArguments::kDefault) {
  //   if (args.count() == 0) {
  //     // No need for spread.
  //     CallArguments empty_args(ConvertReceiverMode::kNullOrUndefined);
  //     return ReduceCall(receiver, empty_args, feedback_source,
  //                       speculation_mode);
  //   }
  //   if (args.count() == 1 || IsNullValue(args[1]) ||
  //       IsUndefinedValue(args[1])) {
  //     // No need for spread. We have only the new receiver.
  //     CallArguments new_args(ConvertReceiverMode::kAny,
  //                            {GetTaggedValue(args[0])});
  //     return ReduceCall(receiver, new_args, feedback_source,
  //     speculation_mode);
  //   }
  //   if (args.count() > 2) {
  //     // FunctionPrototypeApply only consider two arguments: the new receiver
  //     // and an array-like arguments_list. All others shall be ignored.
  //     args.Truncate(2);
  //   }
  // }
  // return ReduceCall(native_context.function_prototype_apply(broker()), args,
  //                   feedback_source, speculation_mode);
}

void MaglevGraphBuilder::BuildCall(ValueNode* target_node, CallArguments& args,
                                   compiler::FeedbackSource& feedback_source) {
  const compiler::ProcessedFeedback& processed_feedback =
      broker()->GetFeedbackForCall(feedback_source);
  if (processed_feedback.IsInsufficient()) {
    EmitUnconditionalDeopt(DeoptimizeReason::kInsufficientTypeFeedbackForCall);
    return;
  }

  DCHECK_EQ(processed_feedback.kind(), compiler::ProcessedFeedback::kCall);
  const compiler::CallFeedback& call_feedback = processed_feedback.AsCall();
  if (call_feedback.target().has_value() &&
      call_feedback.target()->IsJSFunction()) {
    CallFeedbackContent content = call_feedback.call_feedback_content();
    compiler::JSFunctionRef function = call_feedback.target()->AsJSFunction();
    ReduceResult result;
    if (content == CallFeedbackContent::kTarget) {
      result = ReduceCallForTarget(target_node, function, args, feedback_source,
                                   call_feedback.speculation_mode());
    } else {
      DCHECK_EQ(content, CallFeedbackContent::kReceiver);
      // We only collect receiver feedback for FunctionPrototypeApply.
      // See CollectCallFeedback in ic-callable.tq
      result = ReduceFunctionPrototypeApplyCallWithReceiver(
          target_node, function, args, feedback_source,
          call_feedback.speculation_mode());
    }
    PROCESS_AND_RETURN_IF_DONE(
        result, [&](ValueNode* value) { SetAccumulator(value); });
  }

  // On fallthrough, create a generic call.
  ValueNode* context = GetContext();
  SetAccumulator(BuildGenericCall(target_node, context, Call::TargetType::kAny,
                                  args, feedback_source));
}

void MaglevGraphBuilder::BuildCallFromRegisterList(
    ConvertReceiverMode receiver_mode) {
  ValueNode* target = LoadRegisterTagged(0);
  interpreter::RegisterList reg_list = iterator_.GetRegisterListOperand(1);
  FeedbackSlot slot = GetSlotOperand(3);
  compiler::FeedbackSource feedback_source(feedback(), slot);
  CallArguments args(receiver_mode, reg_list, current_interpreter_frame_);
  BuildCall(target, args, feedback_source);
}

void MaglevGraphBuilder::BuildCallFromRegisters(
    int arg_count, ConvertReceiverMode receiver_mode) {
  ValueNode* target = LoadRegisterTagged(0);
  const int receiver_count =
      (receiver_mode == ConvertReceiverMode::kNullOrUndefined) ? 0 : 1;
  const int reg_count = arg_count + receiver_count;
  FeedbackSlot slot = GetSlotOperand(reg_count + 1);
  compiler::FeedbackSource feedback_source(feedback(), slot);
  switch (reg_count) {
    case 0: {
      DCHECK_EQ(receiver_mode, ConvertReceiverMode::kNullOrUndefined);
      CallArguments args(receiver_mode);
      BuildCall(target, args, feedback_source);
      break;
    }
    case 1: {
      CallArguments args(receiver_mode, {LoadRegisterRaw(1)});
      BuildCall(target, args, feedback_source);
      break;
    }
    case 2: {
      CallArguments args(receiver_mode,
                         {LoadRegisterRaw(1), LoadRegisterRaw(2)});
      BuildCall(target, args, feedback_source);
      break;
    }
    case 3: {
      CallArguments args(receiver_mode, {LoadRegisterRaw(1), LoadRegisterRaw(2),
                                         LoadRegisterRaw(3)});
      BuildCall(target, args, feedback_source);
      break;
    }
    default:
      UNREACHABLE();
  }
}

void MaglevGraphBuilder::VisitCallAnyReceiver() {
  BuildCallFromRegisterList(ConvertReceiverMode::kAny);
}
void MaglevGraphBuilder::VisitCallProperty() {
  BuildCallFromRegisterList(ConvertReceiverMode::kNotNullOrUndefined);
}
void MaglevGraphBuilder::VisitCallProperty0() {
  BuildCallFromRegisters(0, ConvertReceiverMode::kNotNullOrUndefined);
}
void MaglevGraphBuilder::VisitCallProperty1() {
  BuildCallFromRegisters(1, ConvertReceiverMode::kNotNullOrUndefined);
}
void MaglevGraphBuilder::VisitCallProperty2() {
  BuildCallFromRegisters(2, ConvertReceiverMode::kNotNullOrUndefined);
}
void MaglevGraphBuilder::VisitCallUndefinedReceiver() {
  BuildCallFromRegisterList(ConvertReceiverMode::kNullOrUndefined);
}
void MaglevGraphBuilder::VisitCallUndefinedReceiver0() {
  BuildCallFromRegisters(0, ConvertReceiverMode::kNullOrUndefined);
}
void MaglevGraphBuilder::VisitCallUndefinedReceiver1() {
  BuildCallFromRegisters(1, ConvertReceiverMode::kNullOrUndefined);
}
void MaglevGraphBuilder::VisitCallUndefinedReceiver2() {
  BuildCallFromRegisters(2, ConvertReceiverMode::kNullOrUndefined);
}

void MaglevGraphBuilder::VisitCallWithSpread() {
  ValueNode* function = LoadRegisterTagged(0);
  interpreter::RegisterList reglist = iterator_.GetRegisterListOperand(1);
  FeedbackSlot slot = GetSlotOperand(3);
  compiler::FeedbackSource feedback_source(feedback(), slot);
  CallArguments args(ConvertReceiverMode::kAny, reglist,
                     current_interpreter_frame_, CallArguments::kWithSpread);
  BuildCall(function, args, feedback_source);
}

void MaglevGraphBuilder::VisitCallRuntime() {
  Runtime::FunctionId function_id = iterator_.GetRuntimeIdOperand(0);
  interpreter::RegisterList args = iterator_.GetRegisterListOperand(1);
  ValueNode* context = GetContext();
  size_t input_count = args.register_count() + CallRuntime::kFixedInputCount;
  CallRuntime* call_runtime = AddNewNode<CallRuntime>(
      input_count,
      [&](CallRuntime* call_runtime) {
        for (int i = 0; i < args.register_count(); ++i) {
          call_runtime->set_arg(i, GetTaggedValue(args[i]));
        }
      },
      function_id, context);
  SetAccumulator(call_runtime);
}

void MaglevGraphBuilder::VisitCallJSRuntime() {
  // Get the function to call from the native context.
  compiler::NativeContextRef native_context = broker()->target_native_context();
  ValueNode* context = GetConstant(native_context);
  uint32_t slot = iterator_.GetNativeContextIndexOperand(0);
  ValueNode* callee = LoadAndCacheContextSlot(
      context, NativeContext::OffsetOfElementAt(slot), kMutable);
  // Call the function.
  interpreter::RegisterList reglist = iterator_.GetRegisterListOperand(1);
  CallArguments args(ConvertReceiverMode::kNullOrUndefined, reglist,
                     current_interpreter_frame_);
  SetAccumulator(BuildGenericCall(callee, GetContext(),
                                  Call::TargetType::kJSFunction, args));
}

void MaglevGraphBuilder::VisitCallRuntimeForPair() {
  Runtime::FunctionId function_id = iterator_.GetRuntimeIdOperand(0);
  interpreter::RegisterList args = iterator_.GetRegisterListOperand(1);
  ValueNode* context = GetContext();

  size_t input_count = args.register_count() + CallRuntime::kFixedInputCount;
  CallRuntime* call_runtime = AddNewNode<CallRuntime>(
      input_count,
      [&](CallRuntime* call_runtime) {
        for (int i = 0; i < args.register_count(); ++i) {
          call_runtime->set_arg(i, GetTaggedValue(args[i]));
        }
      },
      function_id, context);
  auto result = iterator_.GetRegisterPairOperand(3);
  StoreRegisterPair(result, call_runtime);
}

void MaglevGraphBuilder::VisitInvokeIntrinsic() {
  // InvokeIntrinsic <function_id> <first_arg> <arg_count>
  Runtime::FunctionId intrinsic_id = iterator_.GetIntrinsicIdOperand(0);
  interpreter::RegisterList args = iterator_.GetRegisterListOperand(1);
  switch (intrinsic_id) {
#define CASE(Name, _, arg_count)                                         \
  case Runtime::kInline##Name:                                           \
    DCHECK_IMPLIES(arg_count != -1, arg_count == args.register_count()); \
    VisitIntrinsic##Name(args);                                          \
    break;
    INTRINSICS_LIST(CASE)
#undef CASE
    default:
      UNREACHABLE();
  }
}

void MaglevGraphBuilder::VisitIntrinsicCopyDataProperties(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kCopyDataProperties>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::
    VisitIntrinsicCopyDataPropertiesWithExcludedPropertiesOnStack(
        interpreter::RegisterList args) {
  SmiConstant* excluded_property_count =
      GetSmiConstant(args.register_count() - 1);
  int kContext = 1;
  int kExcludedPropertyCount = 1;
  CallBuiltin* call_builtin = AddNewNode<CallBuiltin>(
      args.register_count() + kContext + kExcludedPropertyCount,
      [&](CallBuiltin* call_builtin) {
        int arg_index = 0;
        call_builtin->set_arg(arg_index++, GetTaggedValue(args[0]));
        call_builtin->set_arg(arg_index++, excluded_property_count);
        for (int i = 1; i < args.register_count(); i++) {
          call_builtin->set_arg(arg_index++, GetTaggedValue(args[i]));
        }
      },
      Builtin::kCopyDataPropertiesWithExcludedProperties, GetContext());
  SetAccumulator(call_builtin);
}

void MaglevGraphBuilder::VisitIntrinsicCreateIterResultObject(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kCreateIterResultObject>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicCreateAsyncFromSyncIterator(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 1);
  SetAccumulator(
      BuildCallBuiltin<Builtin::kCreateAsyncFromSyncIteratorBaseline>(
          {GetTaggedValue(args[0])}));
}

void MaglevGraphBuilder::VisitIntrinsicCreateJSGeneratorObject(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kCreateGeneratorObject>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicGeneratorGetResumeMode(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 1);
  ValueNode* generator = GetTaggedValue(args[0]);
  SetAccumulator(AddNewNode<LoadTaggedField>(
      {generator}, JSGeneratorObject::kResumeModeOffset));
}

void MaglevGraphBuilder::VisitIntrinsicGeneratorClose(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 1);
  ValueNode* generator = GetTaggedValue(args[0]);
  ValueNode* value = GetSmiConstant(JSGeneratorObject::kGeneratorClosed);
  BuildStoreTaggedFieldNoWriteBarrier(generator, value,
                                      JSGeneratorObject::kContinuationOffset);
  SetAccumulator(GetRootConstant(RootIndex::kUndefinedValue));
}

void MaglevGraphBuilder::VisitIntrinsicGetImportMetaObject(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 0);
  SetAccumulator(BuildCallRuntime(Runtime::kGetImportMetaObject, {}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncFunctionAwaitCaught(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncFunctionAwaitCaught>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncFunctionAwaitUncaught(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncFunctionAwaitUncaught>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncFunctionEnter(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncFunctionEnter>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncFunctionReject(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncFunctionReject>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncFunctionResolve(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncFunctionResolve>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncGeneratorAwaitCaught(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncGeneratorAwaitCaught>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncGeneratorAwaitUncaught(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncGeneratorAwaitUncaught>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncGeneratorReject(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 2);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncGeneratorReject>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1])}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncGeneratorResolve(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 3);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncGeneratorResolve>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1]),
       GetTaggedValue(args[2])}));
}

void MaglevGraphBuilder::VisitIntrinsicAsyncGeneratorYieldWithAwait(
    interpreter::RegisterList args) {
  DCHECK_EQ(args.register_count(), 3);
  SetAccumulator(BuildCallBuiltin<Builtin::kAsyncGeneratorYieldWithAwait>(
      {GetTaggedValue(args[0]), GetTaggedValue(args[1]),
       GetTaggedValue(args[2])}));
}

void MaglevGraphBuilder::VisitConstruct() {
  ValueNode* new_target = GetAccumulatorTagged();
  ValueNode* constructor = LoadRegisterTagged(0);
  interpreter::RegisterList args = iterator_.GetRegisterListOperand(1);
  ValueNode* context = GetContext();
  FeedbackSlot slot = GetSlotOperand(3);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  size_t input_count = args.register_count() + 1 + Construct::kFixedInputCount;
  Construct* construct = AddNewNode<Construct>(
      input_count,
      [&](Construct* construct) {
        int arg_index = 0;
        // Add undefined receiver.
        construct->set_arg(arg_index++,
                           GetRootConstant(RootIndex::kUndefinedValue));
        for (int i = 0; i < args.register_count(); i++) {
          construct->set_arg(arg_index++, GetTaggedValue(args[i]));
        }
      },
      feedback_source, constructor, new_target, context);
  SetAccumulator(construct);
}

void MaglevGraphBuilder::VisitConstructWithSpread() {
  ValueNode* new_target = GetAccumulatorTagged();
  ValueNode* constructor = LoadRegisterTagged(0);
  interpreter::RegisterList args = iterator_.GetRegisterListOperand(1);
  ValueNode* context = GetContext();
  FeedbackSlot slot = GetSlotOperand(3);
  compiler::FeedbackSource feedback_source(feedback(), slot);

  int kReceiver = 1;
  size_t input_count =
      args.register_count() + kReceiver + ConstructWithSpread::kFixedInputCount;
  ConstructWithSpread* construct = AddNewNode<ConstructWithSpread>(
      input_count,
      [&](ConstructWithSpread* construct) {
        int arg_index = 0;
        // Add undefined receiver.
        construct->set_arg(arg_index++,
                           GetRootConstant(RootIndex::kUndefinedValue));
        for (int i = 0; i < args.register_count(); i++) {
          construct->set_arg(arg_index++, GetTaggedValue(args[i]));
        }
      },
      feedback_source, constructor, new_target, context);
  SetAccumulator(construct);
}

void MaglevGraphBuilder::VisitTestEqual() {
  VisitCompareOperation<Operation::kEqual>();
}
void MaglevGraphBuilder::VisitTestEqualStrict() {
  VisitCompareOperation<Operation::kStrictEqual>();
}
void MaglevGraphBuilder::VisitTestLessThan() {
  VisitCompareOperation<Operation::kLessThan>();
}
void MaglevGraphBuilder::VisitTestLessThanOrEqual() {
  VisitCompareOperation<Operation::kLessThanOrEqual>();
}
void MaglevGraphBuilder::VisitTestGreaterThan() {
  VisitCompareOperation<Operation::kGreaterThan>();
}
void MaglevGraphBuilder::VisitTestGreaterThanOrEqual() {
  VisitCompareOperation<Operation::kGreaterThanOrEqual>();
}

MaglevGraphBuilder::InferHasInPrototypeChainResult
MaglevGraphBuilder::InferHasInPrototypeChain(
    ValueNode* receiver, compiler::HeapObjectRef prototype) {
  auto stable_it = known_node_aspects().stable_maps.find(receiver);
  auto unstable_it = known_node_aspects().unstable_maps.find(receiver);
  auto stable_end = known_node_aspects().stable_maps.end();
  auto unstable_end = known_node_aspects().unstable_maps.end();
  // If either of the map sets is not found, then we don't know anything about
  // the map of the receiver, so bail.
  if (stable_it == stable_end || unstable_it == unstable_end) {
    return kMayBeInPrototypeChain;
  }

  ZoneVector<compiler::MapRef> receiver_map_refs(zone());

  // Try to determine either that all of the {receiver_maps} have the given
  // {prototype} in their chain, or that none do. If we can't tell, return
  // kMayBeInPrototypeChain.
  bool all = true;
  bool none = true;
  for (const ZoneHandleSet<Map>& map_set :
       {stable_it->second, unstable_it->second}) {
    for (Handle<Map> map_handle : map_set) {
      compiler::MapRef map = MakeRefAssumeMemoryFence(broker(), map_handle);
      receiver_map_refs.push_back(map);
      while (true) {
        if (IsSpecialReceiverInstanceType(map.instance_type())) {
          return kMayBeInPrototypeChain;
        }
        if (!map.IsJSObjectMap()) {
          all = false;
          break;
        }
        compiler::HeapObjectRef map_prototype = map.prototype(broker());
        if (map_prototype.equals(prototype)) {
          none = false;
          break;
        }
        map = map_prototype.map(broker());
        // TODO(v8:11457) Support dictionary mode protoypes here.
        if (!map.is_stable() || map.is_dictionary_map()) {
          return kMayBeInPrototypeChain;
        }
        if (map.oddball_type(broker()) == compiler::OddballType::kNull) {
          all = false;
          break;
        }
      }
    }
  }
  DCHECK_IMPLIES(all, !none);
  if (!all && !none) return kMayBeInPrototypeChain;

  {
    compiler::OptionalJSObjectRef last_prototype;
    if (all) {
      // We don't need to protect the full chain if we found the prototype, we
      // can stop at {prototype}.  In fact we could stop at the one before
      // {prototype} but since we're dealing with multiple receiver maps this
      // might be a different object each time, so it's much simpler to include
      // {prototype}. That does, however, mean that we must check {prototype}'s
      // map stability.
      if (!prototype.map(broker()).is_stable()) return kMayBeInPrototypeChain;
      last_prototype = prototype.AsJSObject();
    }
    broker()->dependencies()->DependOnStablePrototypeChains(
        receiver_map_refs, kStartAtPrototype, last_prototype);
  }

  DCHECK_EQ(all, !none);
  return all ? kIsInPrototypeChain : kIsNotInPrototypeChain;
}

bool MaglevGraphBuilder::TryBuildFastHasInPrototypeChain(
    ValueNode* object, compiler::ObjectRef prototype) {
  if (!prototype.IsHeapObject()) return false;
  auto in_prototype_chain =
      InferHasInPrototypeChain(object, prototype.AsHeapObject());
  if (in_prototype_chain == kMayBeInPrototypeChain) return false;

  SetAccumulator(GetBooleanConstant(in_prototype_chain == kIsInPrototypeChain));
  return true;
}

void MaglevGraphBuilder::BuildHasInPrototypeChain(
    ValueNode* object, compiler::ObjectRef prototype) {
  if (TryBuildFastHasInPrototypeChain(object, prototype)) return;

  SetAccumulator(BuildCallRuntime(Runtime::kHasInPrototypeChain,
                                  {object, GetConstant(prototype)}));
}

bool MaglevGraphBuilder::TryBuildFastOrdinaryHasInstance(
    ValueNode* object, compiler::JSObjectRef callable,
    ValueNode* callable_node_if_not_constant) {
  const bool is_constant = callable_node_if_not_constant == nullptr;
  if (!is_constant) return false;

  if (callable.IsJSBoundFunction()) {
    // OrdinaryHasInstance on bound functions turns into a recursive
    // invocation of the instanceof operator again.
    compiler::JSBoundFunctionRef function = callable.AsJSBoundFunction();
    compiler::JSReceiverRef bound_target_function =
        function.bound_target_function(broker());

    if (!bound_target_function.IsJSObject() ||
        !TryBuildFastInstanceOf(object, bound_target_function.AsJSObject(),
                                nullptr)) {
      // If we can't build a fast instance-of, build a slow one with the
      // partial optimisation of using the bound target function constant.
      SetAccumulator(BuildCallBuiltin<Builtin::kInstanceOf>(
          {object, GetConstant(bound_target_function)}));
    }
    return true;
  }

  if (callable.IsJSFunction()) {
    // Optimize if we currently know the "prototype" property.
    compiler::JSFunctionRef function = callable.AsJSFunction();

    // TODO(v8:7700): Remove the has_prototype_slot condition once the broker
    // is always enabled.
    if (!function.map(broker()).has_prototype_slot() ||
        !function.has_instance_prototype(broker()) ||
        function.PrototypeRequiresRuntimeLookup(broker())) {
      return false;
    }

    compiler::ObjectRef prototype =
        broker()->dependencies()->DependOnPrototypeProperty(function);
    BuildHasInPrototypeChain(object, prototype);
    return true;
  }

  return false;
}

void MaglevGraphBuilder::BuildOrdinaryHasInstance(
    ValueNode* object, compiler::JSObjectRef callable,
    ValueNode* callable_node_if_not_constant) {
  if (TryBuildFastOrdinaryHasInstance(object, callable,
                                      callable_node_if_not_constant))
    return;

  SetAccumulator(BuildCallBuiltin<Builtin::kOrdinaryHasInstance>(
      {object, callable_node_if_not_constant ? callable_node_if_not_constant
                                             : GetConstant(callable)}));
}

bool MaglevGraphBuilder::TryBuildFastInstanceOf(
    ValueNode* object, compiler::JSObjectRef callable,
    ValueNode* callable_node_if_not_constant) {
  compiler::MapRef receiver_map = callable.map(broker());
  compiler::NameRef name = broker()->has_instance_symbol();
  compiler::PropertyAccessInfo access_info = broker()->GetPropertyAccessInfo(
      receiver_map, name, compiler::AccessMode::kLoad);

  // TODO(v8:11457) Support dictionary mode holders here.
  if (access_info.IsInvalid() || access_info.HasDictionaryHolder()) {
    return false;
  }
  access_info.RecordDependencies(broker()->dependencies());

  if (access_info.IsNotFound()) {
    // If there's no @@hasInstance handler, the OrdinaryHasInstance operation
    // takes over, but that requires the constructor to be callable.
    if (!receiver_map.is_callable()) {
      return false;
    }

    broker()->dependencies()->DependOnStablePrototypeChains(
        access_info.lookup_start_object_maps(), kStartAtPrototype);

    // Monomorphic property access.
    if (callable_node_if_not_constant) {
      BuildCheckMaps(callable_node_if_not_constant,
                     base::VectorOf(access_info.lookup_start_object_maps()));
    }

    BuildOrdinaryHasInstance(object, callable, callable_node_if_not_constant);
    return true;
  }

  if (access_info.IsFastDataConstant()) {
    compiler::OptionalJSObjectRef holder = access_info.holder();
    bool found_on_proto = holder.has_value();
    compiler::JSObjectRef holder_ref =
        found_on_proto ? holder.value() : callable;
    compiler::OptionalObjectRef has_instance_field =
        holder_ref.GetOwnFastDataProperty(
            broker(), access_info.field_representation(),
            access_info.field_index(), broker()->dependencies());
    if (!has_instance_field.has_value() ||
        !has_instance_field->IsHeapObject() ||
        !has_instance_field->AsHeapObject().map(broker()).is_callable()) {
      return false;
    }

    if (found_on_proto) {
      broker()->dependencies()->DependOnStablePrototypeChains(
          access_info.lookup_start_object_maps(), kStartAtPrototype,
          holder.value());
    }

    ValueNode* callable_node;
    if (callable_node_if_not_constant) {
      // Check that {callable_node_if_not_constant} is actually {callable}.
      BuildCheckValue(callable_node_if_not_constant, callable);
      callable_node = callable_node_if_not_constant;
    } else {
      callable_node = GetConstant(callable);
    }
    BuildCheckMaps(callable_node,
                   base::VectorOf(access_info.lookup_start_object_maps()));

    // Call @@hasInstance
    CallArguments args(ConvertReceiverMode::kNotNullOrUndefined,
                       {callable_node, object});
    ValueNode* call_result;
    {
      // Make sure that a lazy deopt after the @@hasInstance call also performs
      // ToBoolean before returning to the interpreter.
      LazyDeoptContinuationScope continuation_scope(
          this, Builtin::kToBooleanLazyDeoptContinuation);
      ReduceResult result = ReduceCall(*has_instance_field, args);
      // TODO(victorgomes): Propagate the case if we need to soft deopt.
      DCHECK(!result.IsDoneWithAbort());
      call_result = result.value();
    }

    // TODO(v8:7700): Do we need to call ToBoolean here? If we have reduce the
    // call further, we might already have a boolean constant as result.
    // TODO(leszeks): Avoid forcing a conversion to tagged here.
    SetAccumulator(AddNewNode<ToBoolean>({GetTaggedValue(call_result)}));
    return true;
  }

  return false;
}

bool MaglevGraphBuilder::TryBuildFastInstanceOfWithFeedback(
    ValueNode* object, ValueNode* callable,
    compiler::FeedbackSource feedback_source) {
  compiler::ProcessedFeedback const& feedback =
      broker()->GetFeedbackForInstanceOf(feedback_source);

  // TurboFan emits generic code when there's no feedback, rather than
  // deopting.
  if (feedback.IsInsufficient()) return false;

  // Check if the right hand side is a known receiver, or
  // we have feedback from the InstanceOfIC.
  if (callable->Is<Constant>() &&
      callable->Cast<Constant>()->object().IsJSObject()) {
    compiler::JSObjectRef callable_ref =
        callable->Cast<Constant>()->object().AsJSObject();
    return TryBuildFastInstanceOf(object, callable_ref, nullptr);
  }
  if (feedback_source.IsValid()) {
    compiler::OptionalJSObjectRef callable_from_feedback =
        feedback.AsInstanceOf().value();
    if (callable_from_feedback) {
      return TryBuildFastInstanceOf(object, *callable_from_feedback, callable);
    }
  }
  return false;
}

void MaglevGraphBuilder::VisitTestInstanceOf() {
  // TestInstanceOf <src> <feedback_slot>
  ValueNode* object = LoadRegisterTagged(0);
  ValueNode* callable = GetAccumulatorTagged();
  FeedbackSlot slot = GetSlotOperand(1);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  if (TryBuildFastInstanceOfWithFeedback(object, callable, feedback_source)) {
    return;
  }

  ValueNode* context = GetContext();
  SetAccumulator(
      AddNewNode<TestInstanceOf>({context, object, callable}, feedback_source));
}

void MaglevGraphBuilder::VisitTestIn() {
  // TestIn <src> <feedback_slot>
  ValueNode* object = GetAccumulatorTagged();
  ValueNode* name = LoadRegisterTagged(0);
  FeedbackSlot slot = GetSlotOperand(1);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  // TODO(victorgomes): Create fast path using feedback.
  USE(feedback_source);

  SetAccumulator(
      BuildCallBuiltin<Builtin::kKeyedHasIC>({object, name}, feedback_source));
}

void MaglevGraphBuilder::VisitToName() {
  // ToObject <dst>
  ValueNode* value = GetAccumulatorTagged();
  interpreter::Register destination = iterator_.GetRegisterOperand(0);
  if (CheckType(value, NodeType::kName)) {
    MoveNodeBetweenRegisters(interpreter::Register::virtual_accumulator(),
                             destination);
  } else {
    StoreRegister(destination, AddNewNode<ToName>({GetContext(), value}));
  }
}

void MaglevGraphBuilder::BuildToNumberOrToNumeric(Object::Conversion mode) {
  ValueNode* value = GetAccumulatorTagged();
  FeedbackSlot slot = GetSlotOperand(0);
  switch (broker()->GetFeedbackForBinaryOperation(
      compiler::FeedbackSource(feedback(), slot))) {
    case BinaryOperationHint::kSignedSmall:
      BuildCheckSmi(value);
      break;
    case BinaryOperationHint::kSignedSmallInputs:
      UNREACHABLE();
    case BinaryOperationHint::kNumber:
    case BinaryOperationHint::kBigInt:
    case BinaryOperationHint::kBigInt64:
      if (mode == Object::Conversion::kToNumber &&
          EnsureType(value, NodeType::kNumber)) {
        return;
      }
      AddNewNode<CheckNumber>({value}, mode);
      break;
    case BinaryOperationHint::kNone:
    // TODO(leszeks): Faster ToNumber for kNumberOrOddball
    case BinaryOperationHint::kNumberOrOddball:
    case BinaryOperationHint::kString:
    case BinaryOperationHint::kAny:
      if (CheckType(value, NodeType::kNumber)) return;
      SetAccumulator(
          AddNewNode<ToNumberOrNumeric>({GetContext(), value}, mode));
      break;
  }
}

void MaglevGraphBuilder::VisitToNumber() {
  BuildToNumberOrToNumeric(Object::Conversion::kToNumber);
}
void MaglevGraphBuilder::VisitToNumeric() {
  BuildToNumberOrToNumeric(Object::Conversion::kToNumeric);
}

void MaglevGraphBuilder::VisitToObject() {
  // ToObject <dst>
  ValueNode* value = GetAccumulatorTagged();
  interpreter::Register destination = iterator_.GetRegisterOperand(0);
  if (CheckType(value, NodeType::kJSReceiver)) {
    MoveNodeBetweenRegisters(interpreter::Register::virtual_accumulator(),
                             destination);
  } else {
    StoreRegister(destination, AddNewNode<ToObject>({GetContext(), value}));
  }
}

void MaglevGraphBuilder::VisitToString() {
  // ToString
  ValueNode* value = GetAccumulatorTagged();
  if (CheckType(value, NodeType::kString)) return;
  // TODO(victorgomes): Add fast path for constant primitives.
  SetAccumulator(AddNewNode<ToString>({GetContext(), value}));
}

void MaglevGraphBuilder::VisitCreateRegExpLiteral() {
  // CreateRegExpLiteral <pattern_idx> <literal_idx> <flags>
  compiler::StringRef pattern = GetRefOperand<String>(0);
  FeedbackSlot slot = GetSlotOperand(1);
  uint32_t flags = GetFlag16Operand(2);
  compiler::FeedbackSource feedback_source{feedback(), slot};
  // TODO(victorgomes): Inline allocation if feedback has a RegExpLiteral.
  SetAccumulator(
      AddNewNode<CreateRegExpLiteral>({}, pattern, feedback_source, flags));
}

void MaglevGraphBuilder::VisitCreateArrayLiteral() {
  compiler::HeapObjectRef constant_elements = GetRefOperand<HeapObject>(0);
  FeedbackSlot slot_index = GetSlotOperand(1);
  int bytecode_flags = GetFlag8Operand(2);
  int literal_flags =
      interpreter::CreateArrayLiteralFlags::FlagsBits::decode(bytecode_flags);
  compiler::FeedbackSource feedback_source(feedback(), slot_index);

  compiler::ProcessedFeedback const& processed_feedback =
      broker()->GetFeedbackForArrayOrObjectLiteral(feedback_source);
  if (!processed_feedback.IsInsufficient()) {
    ReduceResult result =
        TryBuildFastCreateObjectOrArrayLiteral(processed_feedback.AsLiteral());
    PROCESS_AND_RETURN_IF_DONE(
        result, [&](ValueNode* value) { SetAccumulator(value); });
  }

  if (interpreter::CreateArrayLiteralFlags::FastCloneSupportedBit::decode(
          bytecode_flags)) {
    // TODO(victorgomes): CreateShallowArrayLiteral should not need the
    // boilerplate descriptor. However the current builtin checks that the
    // feedback exists and fallsback to CreateArrayLiteral if it doesn't.
    SetAccumulator(AddNewNode<CreateShallowArrayLiteral>(
        {}, constant_elements, feedback_source, literal_flags));
  } else {
    SetAccumulator(AddNewNode<CreateArrayLiteral>(
        {}, constant_elements, feedback_source, literal_flags));
  }
}

void MaglevGraphBuilder::VisitCreateArrayFromIterable() {
  ValueNode* iterable = GetAccumulatorTagged();
  SetAccumulator(
      BuildCallBuiltin<Builtin::kIterableToListWithSymbolLookup>({iterable}));
}

void MaglevGraphBuilder::VisitCreateEmptyArrayLiteral() {
  // TODO(v8:7700): Consider inlining the allocation.
  FeedbackSlot slot_index = GetSlotOperand(0);
  SetAccumulator(AddNewNode<CreateEmptyArrayLiteral>(
      {}, compiler::FeedbackSource{feedback(), slot_index}));
}

base::Optional<FastLiteralObject>
MaglevGraphBuilder::TryReadBoilerplateForFastLiteral(
    compiler::JSObjectRef boilerplate, AllocationType allocation, int max_depth,
    int* max_properties) {
  DCHECK_GE(max_depth, 0);
  DCHECK_GE(*max_properties, 0);

  if (max_depth == 0) return {};

  // Prevent concurrent migrations of boilerplate objects.
  compiler::JSHeapBroker::BoilerplateMigrationGuardIfNeeded
      boilerplate_access_guard(broker());

  // Now that we hold the migration lock, get the current map.
  compiler::MapRef boilerplate_map = boilerplate.map(broker());
  // Protect against concurrent changes to the boilerplate object by checking
  // for an identical value at the end of the compilation.
  broker()->dependencies()->DependOnObjectSlotValue(
      boilerplate, HeapObject::kMapOffset, boilerplate_map);
  {
    compiler::OptionalMapRef current_boilerplate_map =
        boilerplate.map_direct_read(broker());
    if (!current_boilerplate_map.has_value() ||
        !current_boilerplate_map->equals(boilerplate_map)) {
      // TODO(leszeks): Emit an eager deopt for this case, so that we can
      // re-learn the boilerplate. This will be easier once we get rid of the
      // two-pass approach, since we'll be able to create the eager deopt here
      // and return a ReduceResult::DoneWithAbort().
      return {};
    }
  }

  // Bail out if the boilerplate map has been deprecated.  The map could of
  // course be deprecated at some point after the line below, but it's not a
  // correctness issue -- it only means the literal won't be created with the
  // most up to date map(s).
  if (boilerplate_map.is_deprecated()) return {};

  // We currently only support in-object properties.
  if (boilerplate.map(broker()).elements_kind() == DICTIONARY_ELEMENTS ||
      boilerplate.map(broker()).is_dictionary_map() ||
      !boilerplate.raw_properties_or_hash(broker()).has_value()) {
    return {};
  }
  {
    compiler::ObjectRef properties =
        *boilerplate.raw_properties_or_hash(broker());
    bool const empty =
        properties.IsSmi() ||
        properties.equals(MakeRef(
            broker(), local_isolate()->factory()->empty_fixed_array())) ||
        properties.equals(MakeRef(
            broker(), Handle<Object>::cast(
                          local_isolate()->factory()->empty_property_array())));
    if (!empty) return {};
  }

  compiler::OptionalFixedArrayBaseRef maybe_elements =
      boilerplate.elements(broker(), kRelaxedLoad);
  if (!maybe_elements.has_value()) return {};
  compiler::FixedArrayBaseRef boilerplate_elements = maybe_elements.value();
  broker()->dependencies()->DependOnObjectSlotValue(
      boilerplate, JSObject::kElementsOffset, boilerplate_elements);
  int const elements_length = boilerplate_elements.length();

  FastLiteralObject fast_literal(boilerplate_map, zone(), {});

  // Compute the in-object properties to store first.
  int index = 0;
  for (InternalIndex i :
       InternalIndex::Range(boilerplate_map.NumberOfOwnDescriptors())) {
    PropertyDetails const property_details =
        boilerplate_map.GetPropertyDetails(broker(), i);
    if (property_details.location() != PropertyLocation::kField) continue;
    DCHECK_EQ(PropertyKind::kData, property_details.kind());
    if ((*max_properties)-- == 0) return {};

    int offset = boilerplate_map.GetInObjectPropertyOffset(index);
#ifdef DEBUG
    FieldIndex field_index =
        FieldIndex::ForDetails(*boilerplate_map.object(), property_details);
    DCHECK(field_index.is_inobject());
    DCHECK_EQ(index, field_index.property_index());
    DCHECK_EQ(field_index.offset(), offset);
#endif

    // Note: the use of RawInobjectPropertyAt (vs. the higher-level
    // GetOwnFastDataProperty) here is necessary, since the underlying value
    // may be `uninitialized`, which the latter explicitly does not support.
    compiler::OptionalObjectRef maybe_boilerplate_value =
        boilerplate.RawInobjectPropertyAt(
            broker(),
            FieldIndex::ForInObjectOffset(offset, FieldIndex::kTagged));
    if (!maybe_boilerplate_value.has_value()) return {};

    // Note: We don't need to take a compilation dependency verifying the value
    // of `boilerplate_value`, since boilerplate properties are constant after
    // initialization modulo map migration. We protect against concurrent map
    // migrations (other than elements kind transition, which don't affect us)
    // via the boilerplate_migration_access lock.
    compiler::ObjectRef boilerplate_value = maybe_boilerplate_value.value();

    FastLiteralField value;
    if (boilerplate_value.IsJSObject()) {
      compiler::JSObjectRef boilerplate_object = boilerplate_value.AsJSObject();
      base::Optional<FastLiteralObject> maybe_object_value =
          TryReadBoilerplateForFastLiteral(boilerplate_object, allocation,
                                           max_depth - 1, max_properties);
      if (!maybe_object_value.has_value()) return {};
      value = FastLiteralField(maybe_object_value.value());
    } else if (property_details.representation().IsDouble()) {
      Float64 number =
          Float64::FromBits(boilerplate_value.AsHeapNumber().value_as_bits());
      value = FastLiteralField(number);
    } else {
      // It's fine to store the 'uninitialized' Oddball into a Smi field since
      // it will get overwritten anyway.
      DCHECK_IMPLIES(property_details.representation().IsSmi() &&
                         !boilerplate_value.IsSmi(),
                     boilerplate_value.object()->IsUninitialized());
      value = FastLiteralField(boilerplate_value);
    }

    DCHECK_LT(index, boilerplate_map.GetInObjectProperties());
    fast_literal.fields[index] = value;
    index++;
  }

  // Fill slack at the end of the boilerplate object with filler maps.
  int const boilerplate_length = boilerplate_map.GetInObjectProperties();
  for (; index < boilerplate_length; ++index) {
    DCHECK(!V8_MAP_PACKING_BOOL);
    // TODO(wenyuzhao): Fix incorrect MachineType when V8_MAP_PACKING is
    // enabled.
    DCHECK_LT(index, boilerplate_map.GetInObjectProperties());
    fast_literal.fields[index] = FastLiteralField(MakeRef(
        broker(), local_isolate()->factory()->one_pointer_filler_map()));
  }

  // Empty or copy-on-write elements just store a constant.
  compiler::MapRef elements_map = boilerplate_elements.map(broker());
  // Protect against concurrent changes to the boilerplate object by checking
  // for an identical value at the end of the compilation.
  broker()->dependencies()->DependOnObjectSlotValue(
      boilerplate_elements, HeapObject::kMapOffset, elements_map);
  if (boilerplate_elements.length() == 0 ||
      elements_map.IsFixedCowArrayMap(broker())) {
    if (allocation == AllocationType::kOld &&
        !boilerplate.IsElementsTenured(boilerplate_elements)) {
      return {};
    }
    fast_literal.elements = FastLiteralFixedArray(boilerplate_elements);
  } else {
    // Compute the elements to store first (might have effects).
    if (boilerplate_elements.IsFixedDoubleArray()) {
      int const size = FixedDoubleArray::SizeFor(elements_length);
      if (size > kMaxRegularHeapObjectSize) return {};
      fast_literal.elements =
          FastLiteralFixedArray(elements_length, zone(), double{});

      compiler::FixedDoubleArrayRef elements =
          boilerplate_elements.AsFixedDoubleArray();
      for (int i = 0; i < elements_length; ++i) {
        Float64 value = elements.GetFromImmutableFixedDoubleArray(i);
        fast_literal.elements.double_values[i] = value;
      }
    } else {
      int const size = FixedArray::SizeFor(elements_length);
      if (size > kMaxRegularHeapObjectSize) return {};
      fast_literal.elements = FastLiteralFixedArray(elements_length, zone());

      compiler::FixedArrayRef elements = boilerplate_elements.AsFixedArray();
      for (int i = 0; i < elements_length; ++i) {
        if ((*max_properties)-- == 0) return {};
        compiler::OptionalObjectRef element_value =
            elements.TryGet(broker(), i);
        if (!element_value.has_value()) return {};
        if (element_value->IsJSObject()) {
          base::Optional<FastLiteralObject> object =
              TryReadBoilerplateForFastLiteral(element_value->AsJSObject(),
                                               allocation, max_depth - 1,
                                               max_properties);
          if (!object.has_value()) return {};
          fast_literal.elements.values[i] = FastLiteralField(*object);
        } else {
          fast_literal.elements.values[i] = FastLiteralField(*element_value);
        }
      }
    }
  }

  if (boilerplate.IsJSArray()) {
    fast_literal.js_array_length =
        boilerplate.AsJSArray().GetBoilerplateLength(broker());
  }

  return fast_literal;
}

ValueNode* MaglevGraphBuilder::ExtendOrReallocateCurrentRawAllocation(
    int size, AllocationType allocation_type) {
  if (!current_raw_allocation_ ||
      current_raw_allocation_->allocation_type() != allocation_type) {
    current_raw_allocation_ =
        AddNewNode<AllocateRaw>({}, allocation_type, size);
    return current_raw_allocation_;
  }

  int current_size = current_raw_allocation_->size();
  if (current_size + size > kMaxRegularHeapObjectSize) {
    return current_raw_allocation_ =
               AddNewNode<AllocateRaw>({}, allocation_type, size);
  }

  DCHECK_GT(current_size, 0);
  int previous_end = current_size;
  current_raw_allocation_->extend(size);
  return AddNewNode<FoldedAllocation>({current_raw_allocation_}, previous_end);
}

void MaglevGraphBuilder::ClearCurrentRawAllocation() {
  current_raw_allocation_ = nullptr;
}

ValueNode* MaglevGraphBuilder::BuildAllocateFastLiteral(
    FastLiteralObject object, AllocationType allocation_type) {
  base::SmallVector<ValueNode*, 8, ZoneAllocator<ValueNode*>> properties(
      object.map.GetInObjectProperties(), ZoneAllocator<ValueNode*>(zone()));
  for (int i = 0; i < object.map.GetInObjectProperties(); ++i) {
    properties[i] = BuildAllocateFastLiteral(object.fields[i], allocation_type);
  }
  ValueNode* elements =
      BuildAllocateFastLiteral(object.elements, allocation_type);

  DCHECK(object.map.IsJSObjectMap());
  // TODO(leszeks): Fold allocations.
  ValueNode* allocation = ExtendOrReallocateCurrentRawAllocation(
      object.map.instance_size(), allocation_type);
  AddNewNode<StoreMap>({allocation}, object.map);
  AddNewNode<StoreTaggedFieldNoWriteBarrier>(
      {allocation,
       GetConstant(MakeRefAssumeMemoryFence(
           broker(), local_isolate()->factory()->empty_fixed_array()))},
      JSObject::kPropertiesOrHashOffset);
  if (object.js_array_length.has_value()) {
    BuildStoreTaggedField(allocation, GetConstant(*object.js_array_length),
                          JSArray::kLengthOffset);
  }

  BuildStoreTaggedField(allocation, elements, JSObject::kElementsOffset);
  for (int i = 0; i < object.map.GetInObjectProperties(); ++i) {
    BuildStoreTaggedField(allocation, properties[i],
                          object.map.GetInObjectPropertyOffset(i));
  }
  return allocation;
}

ValueNode* MaglevGraphBuilder::BuildAllocateFastLiteral(
    FastLiteralField value, AllocationType allocation_type) {
  switch (value.type) {
    case FastLiteralField::kObject:
      return BuildAllocateFastLiteral(value.object, allocation_type);
    case FastLiteralField::kMutableDouble: {
      ValueNode* new_alloc = ExtendOrReallocateCurrentRawAllocation(
          HeapNumber::kSize, allocation_type);
      AddNewNode<StoreMap>(
          {new_alloc},
          MakeRefAssumeMemoryFence(
              broker(), local_isolate()->factory()->heap_number_map()));
      // TODO(leszeks): Fix hole storage, in case this should be a custom NaN.
      AddNewNode<StoreFloat64>(
          {new_alloc, GetFloat64Constant(value.mutable_double_value)},
          HeapNumber::kValueOffset);
      return new_alloc;
    }

    case FastLiteralField::kConstant:
      return GetConstant(value.constant_value);
    case FastLiteralField::kUninitialized:
      UNREACHABLE();
  }
}

ValueNode* MaglevGraphBuilder::BuildAllocateFastLiteral(
    FastLiteralFixedArray value, AllocationType allocation_type) {
  switch (value.type) {
    case FastLiteralFixedArray::kTagged: {
      base::SmallVector<ValueNode*, 8, ZoneAllocator<ValueNode*>> elements(
          value.length, ZoneAllocator<ValueNode*>(zone()));
      for (int i = 0; i < value.length; ++i) {
        elements[i] =
            BuildAllocateFastLiteral(value.values[i], allocation_type);
      }
      ValueNode* allocation = ExtendOrReallocateCurrentRawAllocation(
          FixedArray::SizeFor(value.length), allocation_type);
      AddNewNode<StoreMap>(
          {allocation},
          MakeRefAssumeMemoryFence(
              broker(), local_isolate()->factory()->fixed_array_map()));
      AddNewNode<StoreTaggedFieldNoWriteBarrier>(
          {allocation, GetSmiConstant(value.length)},
          FixedArray::kLengthOffset);
      for (int i = 0; i < value.length; ++i) {
        // TODO(leszeks): Elide the write barrier where possible.
        BuildStoreTaggedField(allocation, elements[i],
                              FixedArray::OffsetOfElementAt(i));
      }
      return allocation;
    }
    case FastLiteralFixedArray::kDouble: {
      ValueNode* allocation = ExtendOrReallocateCurrentRawAllocation(
          FixedDoubleArray::SizeFor(value.length), allocation_type);
      AddNewNode<StoreMap>(
          {allocation},
          MakeRefAssumeMemoryFence(
              broker(), local_isolate()->factory()->fixed_double_array_map()));
      AddNewNode<StoreTaggedFieldNoWriteBarrier>(
          {allocation, GetSmiConstant(value.length)},
          FixedDoubleArray::kLengthOffset);
      for (int i = 0; i < value.length; ++i) {
        // TODO(leszeks): Fix hole storage, in case Float64::get_scalar doesn't
        // preserve custom NaNs.
        AddNewNode<StoreFloat64>(
            {allocation, GetFloat64Constant(value.double_values[i])},
            FixedDoubleArray::OffsetOfElementAt(i));
      }
      return allocation;
    }
    case FastLiteralFixedArray::kCoW:
      return GetConstant(value.cow_value);
    case FastLiteralFixedArray::kUninitialized:
      UNREACHABLE();
  }
}

ReduceResult MaglevGraphBuilder::TryBuildFastCreateObjectOrArrayLiteral(
    const compiler::LiteralFeedback& feedback) {
  compiler::AllocationSiteRef site = feedback.value();
  if (!site.boilerplate(broker()).has_value()) return ReduceResult::Fail();
  AllocationType allocation_type =
      broker()->dependencies()->DependOnPretenureMode(site);

  // First try to extract out the shape and values of the boilerplate, bailing
  // out on complex boilerplates.
  int max_properties = compiler::kMaxFastLiteralProperties;
  base::Optional<FastLiteralObject> maybe_value =
      TryReadBoilerplateForFastLiteral(
          *site.boilerplate(broker()), allocation_type,
          compiler::kMaxFastLiteralDepth, &max_properties);
  if (!maybe_value.has_value()) return ReduceResult::Fail();

  // Then, use the collected information to actually create nodes in the graph.
  // TODO(leszeks): Add support for unwinding graph modifications, so that we
  // can get rid of this two pass approach.
  broker()->dependencies()->DependOnElementsKinds(site);
  ReduceResult result = BuildAllocateFastLiteral(*maybe_value, allocation_type);
  // TODO(leszeks): Don't eagerly clear the raw allocation, have the next side
  // effect clear it.
  ClearCurrentRawAllocation();
  return result;
}

void MaglevGraphBuilder::VisitCreateObjectLiteral() {
  compiler::ObjectBoilerplateDescriptionRef boilerplate_desc =
      GetRefOperand<ObjectBoilerplateDescription>(0);
  FeedbackSlot slot_index = GetSlotOperand(1);
  int bytecode_flags = GetFlag8Operand(2);
  int literal_flags =
      interpreter::CreateObjectLiteralFlags::FlagsBits::decode(bytecode_flags);
  compiler::FeedbackSource feedback_source(feedback(), slot_index);

  compiler::ProcessedFeedback const& processed_feedback =
      broker()->GetFeedbackForArrayOrObjectLiteral(feedback_source);
  if (!processed_feedback.IsInsufficient()) {
    ReduceResult result =
        TryBuildFastCreateObjectOrArrayLiteral(processed_feedback.AsLiteral());
    PROCESS_AND_RETURN_IF_DONE(
        result, [&](ValueNode* value) { SetAccumulator(value); });
  }

  if (interpreter::CreateObjectLiteralFlags::FastCloneSupportedBit::decode(
          bytecode_flags)) {
    // TODO(victorgomes): CreateShallowObjectLiteral should not need the
    // boilerplate descriptor. However the current builtin checks that the
    // feedback exists and fallsback to CreateObjectLiteral if it doesn't.
    SetAccumulator(AddNewNode<CreateShallowObjectLiteral>(
        {}, boilerplate_desc, feedback_source, literal_flags));
  } else {
    SetAccumulator(AddNewNode<CreateObjectLiteral>(
        {}, boilerplate_desc, feedback_source, literal_flags));
  }
}

void MaglevGraphBuilder::VisitCreateEmptyObjectLiteral() {
  compiler::NativeContextRef native_context = broker()->target_native_context();
  compiler::MapRef map =
      native_context.object_function(broker()).initial_map(broker());
  DCHECK(!map.is_dictionary_map());
  DCHECK(!map.IsInobjectSlackTrackingInProgress());
  SetAccumulator(AddNewNode<CreateEmptyObjectLiteral>({}, map));
}

void MaglevGraphBuilder::VisitCloneObject() {
  // CloneObject <source_idx> <flags> <feedback_slot>
  ValueNode* source = LoadRegisterTagged(0);
  ValueNode* flags =
      GetSmiConstant(interpreter::CreateObjectLiteralFlags::FlagsBits::decode(
          GetFlag8Operand(1)));
  FeedbackSlot slot = GetSlotOperand(2);
  compiler::FeedbackSource feedback_source{feedback(), slot};
  SetAccumulator(BuildCallBuiltin<Builtin::kCloneObjectIC>({source, flags},
                                                           feedback_source));
}

void MaglevGraphBuilder::VisitGetTemplateObject() {
  // GetTemplateObject <descriptor_idx> <literal_idx>
  compiler::SharedFunctionInfoRef shared_function_info =
      compilation_unit_->shared_function_info();
  ValueNode* description = GetConstant(GetRefOperand<HeapObject>(0));
  FeedbackSlot slot = GetSlotOperand(1);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  const compiler::ProcessedFeedback& feedback =
      broker()->GetFeedbackForTemplateObject(feedback_source);
  if (feedback.IsInsufficient()) {
    return SetAccumulator(AddNewNode<GetTemplateObject>(
        {description}, shared_function_info, feedback_source));
  }
  compiler::JSArrayRef template_object = feedback.AsTemplateObject().value();
  SetAccumulator(GetConstant(template_object));
}

void MaglevGraphBuilder::VisitCreateClosure() {
  compiler::SharedFunctionInfoRef shared_function_info =
      GetRefOperand<SharedFunctionInfo>(0);
  compiler::FeedbackCellRef feedback_cell =
      feedback().GetClosureFeedbackCell(broker(), iterator_.GetIndexOperand(1));
  uint32_t flags = GetFlag8Operand(2);

  if (interpreter::CreateClosureFlags::FastNewClosureBit::decode(flags)) {
    SetAccumulator(AddNewNode<FastCreateClosure>(
        {GetContext()}, shared_function_info, feedback_cell));
  } else {
    bool pretenured =
        interpreter::CreateClosureFlags::PretenuredBit::decode(flags);
    SetAccumulator(AddNewNode<CreateClosure>(
        {GetContext()}, shared_function_info, feedback_cell, pretenured));
  }
}

void MaglevGraphBuilder::VisitCreateBlockContext() {
  // TODO(v8:7700): Inline allocation when context is small.
  // TODO(v8:7700): Update TryGetParentContext if this ever emits its own Node
  // type.
  // CreateBlockContext <scope_info_idx>
  ValueNode* scope_info = GetConstant(GetRefOperand<ScopeInfo>(0));
  SetAccumulator(BuildCallRuntime(Runtime::kPushBlockContext, {scope_info}));
}

void MaglevGraphBuilder::VisitCreateCatchContext() {
  // TODO(v8:7700): Inline allocation when context is small.
  // TODO(v8:7700): Update TryGetParentContext if this ever emits its own Node
  // type.
  // CreateCatchContext <exception> <scope_info_idx>
  ValueNode* exception = LoadRegisterTagged(0);
  ValueNode* scope_info = GetConstant(GetRefOperand<ScopeInfo>(1));
  SetAccumulator(
      BuildCallRuntime(Runtime::kPushCatchContext, {exception, scope_info}));
}

void MaglevGraphBuilder::VisitCreateFunctionContext() {
  compiler::ScopeInfoRef info = GetRefOperand<ScopeInfo>(0);
  uint32_t slot_count = iterator_.GetUnsignedImmediateOperand(1);
  SetAccumulator(AddNewNode<CreateFunctionContext>(
      {GetContext()}, info, slot_count, ScopeType::FUNCTION_SCOPE));
}

void MaglevGraphBuilder::VisitCreateEvalContext() {
  // TODO(v8:7700): Update TryGetParentContext if this ever emits its own Node
  // type.
  compiler::ScopeInfoRef info = GetRefOperand<ScopeInfo>(0);
  uint32_t slot_count = iterator_.GetUnsignedImmediateOperand(1);
  if (slot_count <= static_cast<uint32_t>(
                        ConstructorBuiltins::MaximumFunctionContextSlots())) {
    SetAccumulator(AddNewNode<CreateFunctionContext>(
        {GetContext()}, info, slot_count, ScopeType::EVAL_SCOPE));
  } else {
    SetAccumulator(
        BuildCallRuntime(Runtime::kNewFunctionContext, {GetConstant(info)}));
  }
}

void MaglevGraphBuilder::VisitCreateWithContext() {
  // TODO(v8:7700): Inline allocation when context is small.
  // CreateWithContext <register> <scope_info_idx>
  ValueNode* object = LoadRegisterTagged(0);
  ValueNode* scope_info = GetConstant(GetRefOperand<ScopeInfo>(1));
  SetAccumulator(
      BuildCallRuntime(Runtime::kPushWithContext, {object, scope_info}));
}

void MaglevGraphBuilder::VisitCreateMappedArguments() {
  compiler::SharedFunctionInfoRef shared =
      compilation_unit_->shared_function_info();
  if (is_inline() || shared.object()->has_duplicate_parameters()) {
    SetAccumulator(
        BuildCallRuntime(Runtime::kNewSloppyArguments, {GetClosure()}));
  } else {
    SetAccumulator(
        BuildCallBuiltin<Builtin::kFastNewSloppyArguments>({GetClosure()}));
  }
}

void MaglevGraphBuilder::VisitCreateUnmappedArguments() {
  if (is_inline()) {
    SetAccumulator(
        BuildCallRuntime(Runtime::kNewStrictArguments, {GetClosure()}));
  } else {
    SetAccumulator(
        BuildCallBuiltin<Builtin::kFastNewStrictArguments>({GetClosure()}));
  }
}

void MaglevGraphBuilder::VisitCreateRestParameter() {
  if (is_inline()) {
    SetAccumulator(
        BuildCallRuntime(Runtime::kNewRestParameter, {GetClosure()}));
  } else {
    SetAccumulator(
        BuildCallBuiltin<Builtin::kFastNewRestArguments>({GetClosure()}));
  }
}

void MaglevGraphBuilder::VisitJumpLoop() {
  const uint32_t relative_jump_bytecode_offset =
      iterator_.GetUnsignedImmediateOperand(0);
  const int32_t loop_offset = iterator_.GetImmediateOperand(1);
  const FeedbackSlot feedback_slot = iterator_.GetSlotOperand(2);
  int target = iterator_.GetJumpTargetOffset();

  if (ShouldEmitInterruptBudgetChecks()) {
    if (relative_jump_bytecode_offset > 0) {
      AddNewNode<ReduceInterruptBudgetForLoop>({},
                                               relative_jump_bytecode_offset);
    }
    AddNewNode<JumpLoopPrologue>({}, loop_offset, feedback_slot,
                                 BytecodeOffset(iterator_.current_offset()),
                                 compilation_unit_);
  }
  BasicBlock* block =
      FinishBlock<JumpLoop>({}, jump_targets_[target].block_ptr());

  merge_states_[target]->MergeLoop(*compilation_unit_, graph_->smi(),
                                   current_interpreter_frame_, block);
  block->set_predecessor_id(merge_states_[target]->predecessor_count() - 1);
}
void MaglevGraphBuilder::VisitJump() {
  const uint32_t relative_jump_bytecode_offset =
      iterator_.GetRelativeJumpTargetOffset();
  if (v8_flags.maglev_increase_budget_forward_jump &&
      ShouldEmitInterruptBudgetChecks() && relative_jump_bytecode_offset > 0) {
    AddNewNode<IncreaseInterruptBudget>({}, relative_jump_bytecode_offset);
  }
  BasicBlock* block =
      FinishBlock<Jump>({}, &jump_targets_[iterator_.GetJumpTargetOffset()]);
  MergeIntoFrameState(block, iterator_.GetJumpTargetOffset());
  DCHECK_LT(next_offset(), bytecode().length());
}
void MaglevGraphBuilder::VisitJumpConstant() { VisitJump(); }
void MaglevGraphBuilder::VisitJumpIfNullConstant() { VisitJumpIfNull(); }
void MaglevGraphBuilder::VisitJumpIfNotNullConstant() { VisitJumpIfNotNull(); }
void MaglevGraphBuilder::VisitJumpIfUndefinedConstant() {
  VisitJumpIfUndefined();
}
void MaglevGraphBuilder::VisitJumpIfNotUndefinedConstant() {
  VisitJumpIfNotUndefined();
}
void MaglevGraphBuilder::VisitJumpIfUndefinedOrNullConstant() {
  VisitJumpIfUndefinedOrNull();
}
void MaglevGraphBuilder::VisitJumpIfTrueConstant() { VisitJumpIfTrue(); }
void MaglevGraphBuilder::VisitJumpIfFalseConstant() { VisitJumpIfFalse(); }
void MaglevGraphBuilder::VisitJumpIfJSReceiverConstant() {
  VisitJumpIfJSReceiver();
}
void MaglevGraphBuilder::VisitJumpIfToBooleanTrueConstant() {
  VisitJumpIfToBooleanTrue();
}
void MaglevGraphBuilder::VisitJumpIfToBooleanFalseConstant() {
  VisitJumpIfToBooleanFalse();
}

void MaglevGraphBuilder::MergeIntoFrameState(BasicBlock* predecessor,
                                             int target) {
  if (merge_states_[target] == nullptr) {
    DCHECK(!bytecode_analysis().IsLoopHeader(target));
    const compiler::BytecodeLivenessState* liveness = GetInLivenessFor(target);
    // If there's no target frame state, allocate a new one.
    merge_states_[target] = MergePointInterpreterFrameState::New(
        *compilation_unit_, current_interpreter_frame_, target,
        NumPredecessors(target), predecessor, liveness);
  } else {
    // If there already is a frame state, merge.
    merge_states_[target]->Merge(*compilation_unit_, graph_->smi(),
                                 current_interpreter_frame_, predecessor);
  }
}

void MaglevGraphBuilder::MergeDeadIntoFrameState(int target) {
  // If there is no merge state yet, don't create one, but just reduce the
  // number of possible predecessors to zero.
  predecessors_[target]--;
  if (merge_states_[target]) {
    // If there already is a frame state, merge.
    merge_states_[target]->MergeDead(*compilation_unit_);
    // If this merge is the last one which kills a loop merge, remove that
    // merge state.
    if (merge_states_[target]->is_unreachable_loop()) {
      if (v8_flags.trace_maglev_graph_building) {
        std::cout << "! Killing loop merge state at @" << target << std::endl;
      }
      merge_states_[target] = nullptr;
    }
  }
}

void MaglevGraphBuilder::MergeDeadLoopIntoFrameState(int target) {
  // If there is no merge state yet, don't create one, but just reduce the
  // number of possible predecessors to zero.
  predecessors_[target]--;
  if (merge_states_[target]) {
    // If there already is a frame state, merge.
    merge_states_[target]->MergeDeadLoop(*compilation_unit_);
  }
}

void MaglevGraphBuilder::MergeIntoInlinedReturnFrameState(
    BasicBlock* predecessor) {
  int target = inline_exit_offset();
  if (merge_states_[target] == nullptr) {
    // All returns should have the same liveness, which is that only the
    // accumulator is live.
    const compiler::BytecodeLivenessState* liveness = GetInLiveness();
    DCHECK(liveness->AccumulatorIsLive());
    DCHECK_EQ(liveness->live_value_count(), 1);

    // If there's no target frame state, allocate a new one.
    merge_states_[target] = MergePointInterpreterFrameState::New(
        *compilation_unit_, current_interpreter_frame_, target,
        NumPredecessors(target), predecessor, liveness);
  } else {
    // Again, all returns should have the same liveness, so double check this.
    DCHECK(GetInLiveness()->Equals(
        *merge_states_[target]->frame_state().liveness()));
    merge_states_[target]->Merge(*compilation_unit_, graph_->smi(),
                                 current_interpreter_frame_, predecessor);
  }
}

void MaglevGraphBuilder::BuildBranchIfRootConstant(ValueNode* node,
                                                   JumpType jump_type,
                                                   RootIndex root_index) {
  int fallthrough_offset = next_offset();
  int jump_offset = iterator_.GetJumpTargetOffset();
  if (RootConstant* c = node->TryCast<RootConstant>()) {
    bool constant_is_match = c->index() == root_index;
    bool is_jump_taken = constant_is_match == (jump_type == kJumpIfTrue);
    if (is_jump_taken) {
      BasicBlock* block = FinishBlock<Jump>({}, &jump_targets_[jump_offset]);
      MergeDeadIntoFrameState(fallthrough_offset);
      MergeIntoFrameState(block, jump_offset);
    } else {
      MergeDeadIntoFrameState(jump_offset);
    }
    return;
  }

  BasicBlockRef *true_target, *false_target;
  int true_interrupt_correction, false_interrupt_correction;
  if (jump_type == kJumpIfTrue) {
    true_target = &jump_targets_[jump_offset];
    true_interrupt_correction = iterator_.GetRelativeJumpTargetOffset();
    false_target = &jump_targets_[fallthrough_offset];
    false_interrupt_correction = 0;
  } else {
    true_target = &jump_targets_[fallthrough_offset];
    true_interrupt_correction = 0;
    false_target = &jump_targets_[jump_offset];
    false_interrupt_correction = iterator_.GetRelativeJumpTargetOffset();
  }

  BasicBlock* block = FinishBlock<BranchIfRootConstant>(
      {node}, true_target, true_interrupt_correction, false_target,
      false_interrupt_correction, root_index);

  MergeIntoFrameState(block, jump_offset);
  StartFallthroughBlock(fallthrough_offset, block);
}
void MaglevGraphBuilder::BuildBranchIfTrue(ValueNode* node,
                                           JumpType jump_type) {
  BuildBranchIfRootConstant(node, jump_type, RootIndex::kTrueValue);
}
void MaglevGraphBuilder::BuildBranchIfNull(ValueNode* node,
                                           JumpType jump_type) {
  BuildBranchIfRootConstant(node, jump_type, RootIndex::kNullValue);
}
void MaglevGraphBuilder::BuildBranchIfUndefined(ValueNode* node,
                                                JumpType jump_type) {
  BuildBranchIfRootConstant(node, jump_type, RootIndex::kUndefinedValue);
}
void MaglevGraphBuilder::BuildBranchIfToBooleanTrue(ValueNode* node,
                                                    JumpType jump_type) {
  int fallthrough_offset = next_offset();
  int jump_offset = iterator_.GetJumpTargetOffset();

  if (IsConstantNode(node->opcode())) {
    bool constant_is_true = FromConstantToBool(local_isolate(), node);
    bool is_jump_taken = constant_is_true == (jump_type == kJumpIfTrue);
    if (is_jump_taken) {
      BasicBlock* block = FinishBlock<Jump>({}, &jump_targets_[jump_offset]);
      MergeDeadIntoFrameState(fallthrough_offset);
      MergeIntoFrameState(block, jump_offset);
    } else {
      MergeDeadIntoFrameState(jump_offset);
    }
    return;
  }

  BasicBlockRef *true_target, *false_target;
  int true_interrupt_correction, false_interrupt_correction;
  if (jump_type == kJumpIfTrue) {
    true_target = &jump_targets_[jump_offset];
    true_interrupt_correction = iterator_.GetRelativeJumpTargetOffset();
    false_target = &jump_targets_[fallthrough_offset];
    false_interrupt_correction = 0;
  } else {
    true_target = &jump_targets_[fallthrough_offset];
    true_interrupt_correction = 0;
    false_target = &jump_targets_[jump_offset];
    false_interrupt_correction = iterator_.GetRelativeJumpTargetOffset();
  }

  auto make_specialized_branch_if_compare = [&](ValueRepresentation repr,
                                                ValueNode* cond) {
    // Note that this function (BuildBranchIfToBooleanTrue) generates either a
    // BranchIfToBooleanTrue, or a BranchIfFloat64Compare/BranchIfInt32Compare
    // comparing the {node} with 0. In the former case, the jump is taken if
    // {node} is non-zero, while in the latter cases, the jump is taken if
    // {node} is zero. The {true_target} and {false_target} are thus swapped
    // when we generate a BranchIfFloat64Compare or a BranchIfInt32Compare
    // below.
    DCHECK_EQ(repr, cond->value_representation());
    switch (repr) {
      case ValueRepresentation::kFloat64:
        return FinishBlock<BranchIfFloat64Compare>(
            {cond, GetFloat64Constant(0)}, Operation::kEqual, false_target,
            false_interrupt_correction, true_target, true_interrupt_correction,
            BranchIfFloat64Compare::JumpModeIfNaN::kJumpToTrue);
      case ValueRepresentation::kInt32:
        return FinishBlock<BranchIfInt32Compare>(
            {cond, GetInt32Constant(0)}, Operation::kEqual, false_target,
            false_interrupt_correction, true_target, true_interrupt_correction);
      default:
        UNREACHABLE();
    }
  };

  BasicBlock* block;
  if (node->value_representation() == ValueRepresentation::kInt32) {
    block =
        make_specialized_branch_if_compare(ValueRepresentation::kInt32, node);
  } else if (node->value_representation() == ValueRepresentation::kFloat64) {
    block =
        make_specialized_branch_if_compare(ValueRepresentation::kFloat64, node);
  } else {
    NodeInfo* node_info = known_node_aspects().GetOrCreateInfoFor(node);
    if (ValueNode* as_int32 = node_info->int32_alternative) {
      block = make_specialized_branch_if_compare(ValueRepresentation::kInt32,
                                                 as_int32);
    } else if (ValueNode* as_float64 = node_info->float64_alternative) {
      block = make_specialized_branch_if_compare(ValueRepresentation::kFloat64,
                                                 as_float64);
    } else {
      DCHECK(node->value_representation() == ValueRepresentation::kTagged ||
             node->value_representation() == ValueRepresentation::kUint32);
      // Uint32 should be rare enough that tagging them shouldn't be too
      // expensive (we don't have a `BranchIfUint32Compare` node, and adding one
      // doesn't seem worth it at this point).
      node = GetTaggedValue(node);
      block = FinishBlock<BranchIfToBooleanTrue>(
          {node}, true_target, true_interrupt_correction, false_target,
          false_interrupt_correction);
    }
  }

  MergeIntoFrameState(block, jump_offset);
  StartFallthroughBlock(fallthrough_offset, block);
}
void MaglevGraphBuilder::VisitJumpIfToBooleanTrue() {
  BuildBranchIfToBooleanTrue(GetRawAccumulator(), kJumpIfTrue);
}
void MaglevGraphBuilder::VisitJumpIfToBooleanFalse() {
  BuildBranchIfToBooleanTrue(GetRawAccumulator(), kJumpIfFalse);
}
void MaglevGraphBuilder::VisitJumpIfTrue() {
  BuildBranchIfTrue(GetAccumulatorTagged(), kJumpIfTrue);
}
void MaglevGraphBuilder::VisitJumpIfFalse() {
  BuildBranchIfTrue(GetAccumulatorTagged(), kJumpIfFalse);
}
void MaglevGraphBuilder::VisitJumpIfNull() {
  BuildBranchIfNull(GetAccumulatorTagged(), kJumpIfTrue);
}
void MaglevGraphBuilder::VisitJumpIfNotNull() {
  BuildBranchIfNull(GetAccumulatorTagged(), kJumpIfFalse);
}
void MaglevGraphBuilder::VisitJumpIfUndefined() {
  BuildBranchIfUndefined(GetAccumulatorTagged(), kJumpIfTrue);
}
void MaglevGraphBuilder::VisitJumpIfNotUndefined() {
  BuildBranchIfUndefined(GetAccumulatorTagged(), kJumpIfFalse);
}
void MaglevGraphBuilder::VisitJumpIfUndefinedOrNull() {
  int true_interrupt_correction = iterator_.GetRelativeJumpTargetOffset();
  int false_interrupt_correction = 0;
  BasicBlock* block = FinishBlock<BranchIfUndefinedOrNull>(
      {GetAccumulatorTagged()}, &jump_targets_[iterator_.GetJumpTargetOffset()],
      true_interrupt_correction, &jump_targets_[next_offset()],
      false_interrupt_correction);
  MergeIntoFrameState(block, iterator_.GetJumpTargetOffset());
  StartFallthroughBlock(next_offset(), block);
}
void MaglevGraphBuilder::VisitJumpIfJSReceiver() {
  int true_interrupt_correction = iterator_.GetRelativeJumpTargetOffset();
  int false_interrupt_correction = 0;
  BasicBlock* block = FinishBlock<BranchIfJSReceiver>(
      {GetAccumulatorTagged()}, &jump_targets_[iterator_.GetJumpTargetOffset()],
      true_interrupt_correction, &jump_targets_[next_offset()],
      false_interrupt_correction);
  MergeIntoFrameState(block, iterator_.GetJumpTargetOffset());
  StartFallthroughBlock(next_offset(), block);
}

void MaglevGraphBuilder::VisitSwitchOnSmiNoFeedback() {
  // SwitchOnSmiNoFeedback <table_start> <table_length> <case_value_base>
  interpreter::JumpTableTargetOffsets offsets =
      iterator_.GetJumpTableTargetOffsets();

  if (offsets.size() == 0) return;

  int case_value_base = (*offsets.begin()).case_value;
  BasicBlockRef* targets = zone()->NewArray<BasicBlockRef>(offsets.size());
  for (interpreter::JumpTableTargetOffset offset : offsets) {
    BasicBlockRef* ref = &targets[offset.case_value - case_value_base];
    new (ref) BasicBlockRef(&jump_targets_[offset.target_offset]);
  }

  ValueNode* case_value = GetAccumulatorInt32();
  BasicBlock* block =
      FinishBlock<Switch>({case_value}, case_value_base, targets,
                          offsets.size(), &jump_targets_[next_offset()]);
  for (interpreter::JumpTableTargetOffset offset : offsets) {
    MergeIntoFrameState(block, offset.target_offset);
  }
  StartFallthroughBlock(next_offset(), block);
}

void MaglevGraphBuilder::VisitForInEnumerate() {
  // ForInEnumerate <receiver>
  ValueNode* receiver = LoadRegisterTagged(0);
  SetAccumulator(BuildCallBuiltin<Builtin::kForInEnumerate>({receiver}));
}

void MaglevGraphBuilder::VisitForInPrepare() {
  // ForInPrepare <cache_info_triple>
  ValueNode* enumerator = GetAccumulatorTagged();
  FeedbackSlot slot = GetSlotOperand(1);
  compiler::FeedbackSource feedback_source{feedback(), slot};
  // TODO(v8:7700): Use feedback and create fast path.
  ValueNode* context = GetContext();
  interpreter::Register cache_type_reg = iterator_.GetRegisterOperand(0);
  interpreter::Register cache_array_reg{cache_type_reg.index() + 1};
  interpreter::Register cache_length_reg{cache_type_reg.index() + 2};

  ForInHint hint = broker()->GetFeedbackForForIn(feedback_source);

  current_for_in_state = ForInState();
  switch (hint) {
    case ForInHint::kNone:
    case ForInHint::kEnumCacheKeysAndIndices:
    case ForInHint::kEnumCacheKeys: {
      BuildCheckMaps(enumerator, base::VectorOf({broker()->meta_map()}));

      auto* descriptor_array = AddNewNode<LoadTaggedField>(
          {enumerator}, Map::kInstanceDescriptorsOffset);
      auto* enum_cache = AddNewNode<LoadTaggedField>(
          {descriptor_array}, DescriptorArray::kEnumCacheOffset);
      auto* cache_array =
          AddNewNode<LoadTaggedField>({enum_cache}, EnumCache::kKeysOffset);
      current_for_in_state.enum_cache = enum_cache;

      auto* cache_length = AddNewNode<LoadEnumCacheLength>({enumerator});

      MoveNodeBetweenRegisters(interpreter::Register::virtual_accumulator(),
                               cache_type_reg);
      StoreRegister(cache_array_reg, cache_array);
      StoreRegister(cache_length_reg, cache_length);
      break;
    }
    case ForInHint::kAny: {
      // The result of the  bytecode is output in registers |cache_info_triple|
      // to |cache_info_triple + 2|, with the registers holding cache_type,
      // cache_array, and cache_length respectively.
      //
      // We set the cache type first (to the accumulator value), and write
      // the other two with a ForInPrepare builtin call. This can lazy deopt,
      // which will write to cache_array and cache_length, with cache_type
      // already set on the translation frame.

      // This move needs to happen before ForInPrepare to avoid lazy deopt
      // extending the lifetime of the {cache_type} register.
      MoveNodeBetweenRegisters(interpreter::Register::virtual_accumulator(),
                               cache_type_reg);
      ForInPrepare* result =
          AddNewNode<ForInPrepare>({context, enumerator}, feedback_source);
      // The add will set up a lazy deopt info writing to all three output
      // registers. Update this to only write to the latter two.
      result->lazy_deopt_info()->UpdateResultLocation(cache_array_reg, 2);
      StoreRegisterPair({cache_array_reg, cache_length_reg}, result);
      // Force a conversion to Int32 for the cache length value.
      GetInt32(cache_length_reg);
      break;
    }
  }
}

void MaglevGraphBuilder::VisitForInContinue() {
  // ForInContinue <index> <cache_length>
  ValueNode* index = LoadRegisterInt32(0);
  ValueNode* cache_length = LoadRegisterInt32(1);
  if (TryBuildBranchFor<BranchIfInt32Compare>({index, cache_length},
                                              Operation::kLessThan)) {
    return;
  }
  SetAccumulator(
      AddNewNode<Int32NodeFor<Operation::kLessThan>>({index, cache_length}));
}

void MaglevGraphBuilder::VisitForInNext() {
  // ForInNext <receiver> <index> <cache_info_pair>
  ValueNode* receiver = LoadRegisterTagged(0);
  interpreter::Register cache_type_reg, cache_array_reg;
  std::tie(cache_type_reg, cache_array_reg) =
      iterator_.GetRegisterPairOperand(2);
  ValueNode* cache_type = GetTaggedValue(cache_type_reg);
  ValueNode* cache_array = GetTaggedValue(cache_array_reg);
  FeedbackSlot slot = GetSlotOperand(3);
  compiler::FeedbackSource feedback_source{feedback(), slot};

  ForInHint hint = broker()->GetFeedbackForForIn(feedback_source);

  switch (hint) {
    case ForInHint::kNone:
    case ForInHint::kEnumCacheKeysAndIndices:
    case ForInHint::kEnumCacheKeys: {
      ValueNode* index = LoadRegisterInt32(1);
      // Ensure that the expected map still matches that of the {receiver}.
      auto* receiver_map =
          AddNewNode<LoadTaggedField>({receiver}, HeapObject::kMapOffset);
      AddNewNode<CheckDynamicValue>({receiver_map, cache_type});
      auto* key = AddNewNode<LoadFixedArrayElement>({cache_array, index});
      SetAccumulator(key);

      current_for_in_state.receiver = receiver;
      if (ToObject* to_object =
              current_for_in_state.receiver->TryCast<ToObject>()) {
        current_for_in_state.receiver = to_object->value_input().node();
      }
      current_for_in_state.receiver_needs_map_check = false;
      current_for_in_state.cache_type = cache_type;
      current_for_in_state.key = key;
      if (hint == ForInHint::kEnumCacheKeysAndIndices) {
        current_for_in_state.index = index;
      }
      // We know that the enum cache entry is not undefined, so skip over the
      // next JumpIfUndefined.
      DCHECK(iterator_.next_bytecode() ==
                 interpreter::Bytecode::kJumpIfUndefined ||
             iterator_.next_bytecode() ==
                 interpreter::Bytecode::kJumpIfUndefinedConstant);
      iterator_.Advance();
      MergeDeadIntoFrameState(iterator_.GetJumpTargetOffset());
      break;
    }
    case ForInHint::kAny: {
      ValueNode* index = LoadRegisterTagged(1);
      ValueNode* context = GetContext();
      SetAccumulator(AddNewNode<ForInNext>(
          {context, receiver, cache_array, cache_type, index},
          feedback_source));
      break;
    };
  }
}

void MaglevGraphBuilder::VisitForInStep() {
  ValueNode* index = LoadRegisterInt32(0);
  SetAccumulator(AddNewNode<Int32NodeFor<Operation::kIncrement>>({index}));
  current_for_in_state = ForInState();
}

void MaglevGraphBuilder::VisitSetPendingMessage() {
  ValueNode* message = GetAccumulatorTagged();
  SetAccumulator(AddNewNode<SetPendingMessage>({message}));
}

void MaglevGraphBuilder::VisitThrow() {
  ValueNode* exception = GetAccumulatorTagged();
  BuildCallRuntime(Runtime::kThrow, {exception});
  BuildAbort(AbortReason::kUnexpectedReturnFromThrow);
}
void MaglevGraphBuilder::VisitReThrow() {
  ValueNode* exception = GetAccumulatorTagged();
  BuildCallRuntime(Runtime::kReThrow, {exception});
  BuildAbort(AbortReason::kUnexpectedReturnFromThrow);
}

void MaglevGraphBuilder::VisitReturn() {
  // See also: InterpreterAssembler::UpdateInterruptBudgetOnReturn.
  const uint32_t relative_jump_bytecode_offset = iterator_.current_offset();
  if (ShouldEmitInterruptBudgetChecks() && relative_jump_bytecode_offset > 0) {
    AddNewNode<ReduceInterruptBudgetForReturn>({},
                                               relative_jump_bytecode_offset);
  }

  if (!is_inline()) {
    FinishBlock<Return>({GetAccumulatorTagged()});
    return;
  }

  // All inlined function returns instead jump to one past the end of the
  // bytecode, where we'll later create a final basic block which resumes
  // execution of the caller.
  // TODO(leszeks): Consider shortcutting this Jump for cases where there is
  // only one return and no need to merge return states.
  BasicBlock* block =
      FinishBlock<Jump>({}, &jump_targets_[inline_exit_offset()]);
  MergeIntoInlinedReturnFrameState(block);
}

void MaglevGraphBuilder::VisitThrowReferenceErrorIfHole() {
  // ThrowReferenceErrorIfHole <variable_name>
  compiler::NameRef name = GetRefOperand<Name>(0);
  ValueNode* value = GetAccumulatorTagged();
  // Avoid the check if we know it is not the hole.
  if (IsConstantNode(value->opcode())) {
    if (IsTheHoleValue(value)) {
      ValueNode* constant = GetConstant(name);
      BuildCallRuntime(Runtime::kThrowAccessedUninitializedVariable,
                       {constant});
      BuildAbort(AbortReason::kUnexpectedReturnFromThrow);
    }
    return;
  }
  AddNewNode<ThrowReferenceErrorIfHole>({value}, name);
}
void MaglevGraphBuilder::VisitThrowSuperNotCalledIfHole() {
  // ThrowSuperNotCalledIfHole
  ValueNode* value = GetAccumulatorTagged();
  // Avoid the check if we know it is not the hole.
  if (IsConstantNode(value->opcode())) {
    if (IsTheHoleValue(value)) {
      BuildCallRuntime(Runtime::kThrowSuperNotCalled, {});
      BuildAbort(AbortReason::kUnexpectedReturnFromThrow);
    }
    return;
  }
  AddNewNode<ThrowSuperNotCalledIfHole>({value});
}
void MaglevGraphBuilder::VisitThrowSuperAlreadyCalledIfNotHole() {
  // ThrowSuperAlreadyCalledIfNotHole
  ValueNode* value = GetAccumulatorTagged();
  // Avoid the check if we know it is the hole.
  if (IsConstantNode(value->opcode())) {
    if (!IsTheHoleValue(value)) {
      BuildCallRuntime(Runtime::kThrowSuperAlreadyCalledError, {});
      BuildAbort(AbortReason::kUnexpectedReturnFromThrow);
    }
    return;
  }
  AddNewNode<ThrowSuperAlreadyCalledIfNotHole>({value});
}
void MaglevGraphBuilder::VisitThrowIfNotSuperConstructor() {
  // ThrowIfNotSuperConstructor <constructor>
  ValueNode* constructor = LoadRegisterTagged(0);
  ValueNode* function = GetClosure();
  AddNewNode<ThrowIfNotSuperConstructor>({constructor, function});
}

void MaglevGraphBuilder::VisitSwitchOnGeneratorState() {
  // SwitchOnGeneratorState <generator> <table_start> <table_length>
  // It should be the first bytecode in the bytecode array.
  DCHECK_EQ(iterator_.current_offset(), 0);
  int generator_prologue_block_offset = 1;
  DCHECK_LT(generator_prologue_block_offset, next_offset());

  interpreter::JumpTableTargetOffsets offsets =
      iterator_.GetJumpTableTargetOffsets();
  // If there are no jump offsets, then this generator is not resumable, which
  // means we can skip checking for it and switching on its state.
  if (offsets.size() == 0) return;

  // We create an initial block that checks if the generator is undefined.
  ValueNode* maybe_generator = LoadRegisterTagged(0);
  // Neither the true nor the false path jump over any bytecode
  int true_interrupt_correction = 0, false_interrupt_correction = 0;
  BasicBlock* block_is_generator_undefined = FinishBlock<BranchIfRootConstant>(
      {maybe_generator}, &jump_targets_[next_offset()],
      true_interrupt_correction,
      &jump_targets_[generator_prologue_block_offset],
      false_interrupt_correction, RootIndex::kUndefinedValue);
  MergeIntoFrameState(block_is_generator_undefined, next_offset());

  // We create the generator prologue block.
  StartNewBlock(generator_prologue_block_offset);

  // Generator prologue.
  ValueNode* generator = maybe_generator;
  ValueNode* state = AddNewNode<LoadTaggedField>(
      {generator}, JSGeneratorObject::kContinuationOffset);
  ValueNode* new_state = GetSmiConstant(JSGeneratorObject::kGeneratorExecuting);
  BuildStoreTaggedFieldNoWriteBarrier(generator, new_state,
                                      JSGeneratorObject::kContinuationOffset);
  ValueNode* context = AddNewNode<LoadTaggedField>(
      {generator}, JSGeneratorObject::kContextOffset);
  SetContext(context);

  // Guarantee that we have something in the accumulator.
  MoveNodeBetweenRegisters(iterator_.GetRegisterOperand(0),
                           interpreter::Register::virtual_accumulator());

  // Switch on generator state.
  int case_value_base = (*offsets.begin()).case_value;
  BasicBlockRef* targets = zone()->NewArray<BasicBlockRef>(offsets.size());
  for (interpreter::JumpTableTargetOffset offset : offsets) {
    BasicBlockRef* ref = &targets[offset.case_value - case_value_base];
    new (ref) BasicBlockRef(&jump_targets_[offset.target_offset]);
  }
  ValueNode* case_value = AddNewNode<UnsafeSmiUntag>({state});
  BasicBlock* generator_prologue_block = FinishBlock<Switch>(
      {case_value}, case_value_base, targets, offsets.size());
  for (interpreter::JumpTableTargetOffset offset : offsets) {
    MergeIntoFrameState(generator_prologue_block, offset.target_offset);
  }
}

void MaglevGraphBuilder::VisitSuspendGenerator() {
  // SuspendGenerator <generator> <first input register> <register count>
  // <suspend_id>
  ValueNode* generator = LoadRegisterTagged(0);
  ValueNode* context = GetContext();
  interpreter::RegisterList args = iterator_.GetRegisterListOperand(1);
  uint32_t suspend_id = iterator_.GetUnsignedImmediateOperand(3);

  int input_count = parameter_count_without_receiver() + args.register_count() +
                    GeneratorStore::kFixedInputCount;
  int debug_pos_offset = iterator_.current_offset() +
                         (BytecodeArray::kHeaderSize - kHeapObjectTag);
  AddNewNode<GeneratorStore>(
      input_count,
      [&](GeneratorStore* node) {
        int arg_index = 0;
        for (int i = 1 /* skip receiver */; i < parameter_count(); ++i) {
          node->set_parameters_and_registers(arg_index++, GetTaggedArgument(i));
        }
        const compiler::BytecodeLivenessState* liveness = GetOutLiveness();
        for (int i = 0; i < args.register_count(); ++i) {
          ValueNode* value = liveness->RegisterIsLive(args[i].index())
                                 ? GetTaggedValue(args[i])
                                 : GetRootConstant(RootIndex::kOptimizedOut);
          node->set_parameters_and_registers(arg_index++, value);
        }
      },

      context, generator, suspend_id, debug_pos_offset);

  const uint32_t relative_jump_bytecode_offset = iterator_.current_offset();
  if (ShouldEmitInterruptBudgetChecks() && relative_jump_bytecode_offset > 0) {
    AddNewNode<ReduceInterruptBudgetForReturn>({},
                                               relative_jump_bytecode_offset);
  }
  FinishBlock<Return>({GetAccumulatorTagged()});
}

void MaglevGraphBuilder::VisitResumeGenerator() {
  // ResumeGenerator <generator> <first output register> <register count>
  ValueNode* generator = LoadRegisterTagged(0);
  ValueNode* array = AddNewNode<LoadTaggedField>(
      {generator}, JSGeneratorObject::kParametersAndRegistersOffset);
  interpreter::RegisterList registers = iterator_.GetRegisterListOperand(1);

  if (v8_flags.maglev_assert) {
    // Check if register count is invalid, that is, larger than the
    // register file length.
    ValueNode* array_length_smi =
        AddNewNode<LoadTaggedField>({array}, FixedArrayBase::kLengthOffset);
    ValueNode* array_length = AddNewNode<UnsafeSmiUntag>({array_length_smi});
    ValueNode* register_size = GetInt32Constant(
        parameter_count_without_receiver() + registers.register_count());
    AddNewNode<AssertInt32>(
        {register_size, array_length}, AssertCondition::kLessThanEqual,
        AbortReason::kInvalidParametersAndRegistersInGenerator);
  }

  const compiler::BytecodeLivenessState* liveness =
      GetOutLivenessFor(next_offset());
  RootConstant* stale = GetRootConstant(RootIndex::kStaleRegister);
  for (int i = 0; i < registers.register_count(); ++i) {
    if (liveness->RegisterIsLive(registers[i].index())) {
      int array_index = parameter_count_without_receiver() + i;
      StoreRegister(registers[i], AddNewNode<GeneratorRestoreRegister>(
                                      {array, stale}, array_index));
    }
  }
  SetAccumulator(AddNewNode<LoadTaggedField>(
      {generator}, JSGeneratorObject::kInputOrDebugPosOffset));
}

void MaglevGraphBuilder::VisitGetIterator() {
  // GetIterator <object>
  ValueNode* receiver = LoadRegisterTagged(0);
  ValueNode* context = GetContext();
  int load_slot = iterator_.GetIndexOperand(1);
  int call_slot = iterator_.GetIndexOperand(2);
  SetAccumulator(AddNewNode<GetIterator>({context, receiver}, load_slot,
                                         call_slot, feedback()));
}

void MaglevGraphBuilder::VisitDebugger() {
  BuildCallRuntime(Runtime::kHandleDebuggerStatement, {});
}

void MaglevGraphBuilder::VisitIncBlockCounter() {
  ValueNode* closure = GetClosure();
  ValueNode* coverage_array_slot = GetSmiConstant(iterator_.GetIndexOperand(0));
  BuildCallBuiltin<Builtin::kIncBlockCounter>({closure, coverage_array_slot});
}

void MaglevGraphBuilder::VisitAbort() {
  AbortReason reason = static_cast<AbortReason>(GetFlag8Operand(0));
  BuildAbort(reason);
}

void MaglevGraphBuilder::VisitWide() { UNREACHABLE(); }
void MaglevGraphBuilder::VisitExtraWide() { UNREACHABLE(); }
#define DEBUG_BREAK(Name, ...) \
  void MaglevGraphBuilder::Visit##Name() { UNREACHABLE(); }
DEBUG_BREAK_BYTECODE_LIST(DEBUG_BREAK)
#undef DEBUG_BREAK
void MaglevGraphBuilder::VisitIllegal() { UNREACHABLE(); }

}  // namespace v8::internal::maglev
