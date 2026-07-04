// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-graph-optimizer.h"

#include <optional>

#include "src/base/logging.h"
#include "src/codegen/bailout-reason.h"
#include "src/codegen/interface-descriptors-inl.h"
#include "src/common/operation.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-graph-builder.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-known-node-aspects.h"
#include "src/maglev/maglev-node-type.h"
#include "src/maglev/maglev-reducer-inl.h"
#include "src/maglev/maglev-reducer.h"
#include "src/maglev/maglev-tracer.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {
namespace maglev {

#define RETURN_IF_SUCCESS(res) \
  do {                         \
    auto _res = (res);         \
    if (_res) return *_res;    \
  } while (false)

namespace {
bool IsDoneWithValue(MaybeReduceResult result) {
  return result.IsDoneWithValue();
}
bool IsDoneWithValue(ValueNode* node) { return node != nullptr; }
bool IsDoneWithAbort(MaybeReduceResult result) {
  return result.IsDoneWithAbort();
}
bool IsDoneWithAbort(ValueNode* node) { return false; }
#ifdef DEBUG
bool IsFail(MaybeReduceResult result) { return result.IsFail(); }
bool IsFail(ValueNode* node) { return node == nullptr; }
#endif
ValueNode* Value(MaybeReduceResult result) { return result.value(); }
ValueNode* Value(ValueNode* node) { return node; }
}  // namespace

#define REPLACE_AND_RETURN_IF_DONE(result)  \
  do {                                      \
    auto res = (result);                    \
    if (IsDoneWithValue(res)) {             \
      return ReplaceWith(Value(res));       \
    } else if (IsDoneWithAbort(res)) {      \
      return ProcessResult::kTruncateBlock; \
    }                                       \
    DCHECK(IsFail(result));                 \
  } while (false)

#define REMOVE_AND_RETURN_IF_DONE(result)   \
  do {                                      \
    auto res = (result);                    \
    if (res.IsDoneWithAbort()) {            \
      return ProcessResult::kTruncateBlock; \
    } else if (res.IsDone()) {              \
      return RemoveCurrentNode();           \
    }                                       \
  } while (false)

#define TRACE(...)                                 \
  if (V8_UNLIKELY(is_tracing())) {                 \
    TraceLogger(reducer_.tracer()) << __VA_ARGS__; \
  }

namespace {
constexpr ValueRepresentation ValueRepresentationFromUse(
    UseRepresentation repr) {
  switch (repr) {
    case UseRepresentation::kTagged:
    case UseRepresentation::kTaggedForNumberToString:
      return ValueRepresentation::kTagged;
    case UseRepresentation::kInt32:
    case UseRepresentation::kTruncatedInt32:
      return ValueRepresentation::kInt32;
    case UseRepresentation::kUint32:
      return ValueRepresentation::kUint32;
    case UseRepresentation::kFloat64:
      return ValueRepresentation::kFloat64;
    case UseRepresentation::kHoleyFloat64:
      return ValueRepresentation::kHoleyFloat64;
  }
  UNREACHABLE();
}

}  // namespace

Subgraph<MaglevGraphOptimizer>::Subgraph(
    MaglevReducer<MaglevGraphOptimizer>* reducer, int variable_count)
    : Base(
          reducer,
          MaglevCompilationUnit::NewDummy(
              reducer->zone(),
              reducer->graph()->compilation_info()->toplevel_compilation_unit(),
              variable_count, /*parameter_count=*/0, /*max_arguments=*/0)),
      zone_(reducer->zone()),
      blocks_(zone_),
      saved_block_(reducer->current_block()),
      saved_position_(reducer->current_block_position()),
      saved_kna_(&reducer->known_node_aspects()),
      parent_(reducer->active_subgraph_) {
  reducer_->active_subgraph_ = this;
  reducer_->FlushNodesToBlock();

  // Create entry block.
  BasicBlock* entry =
      reducer_->zone()->New<BasicBlock>(/*state=*/nullptr, reducer_->zone());
  blocks_.push_back(entry);

  reducer_->set_current_block(entry);
  reducer_->SetNewNodePosition(BasicBlockPosition::End());

  // Active KNA = clone of parent, so refinements inside the subgraph don't
  // leak back. Restored to parent_kna_ in the destructor.
  reducer_->set_known_node_aspects(saved_kna_->Clone(zone_));

  // We need to set a context, since this is unconditioned in the frame state.
  // It should never be used, since this will never be executed. Use a DeadValue
  // placeholder: frame-state bookkeeping can inspect it, but any attempt to
  // materialize or use it as a real operand hits UNREACHABLE.
  variable_frame_.set(interpreter::Register::current_context(),
                      reducer_->graph()->GetDeadValue());
}

Subgraph<MaglevGraphOptimizer>::~Subgraph() {
  // Pop self off the active-subgraph stack; parent_ is the enclosing subgraph.
  reducer_->active_subgraph_ = parent_;

  // A null exit block means every path through the subgraph ended abruptly
  // (deopt/throw), so there is no live exit to continue building from. Such a
  // subgraph is spliced in with a null exit and consumed as a truncating
  // splice.
  BasicBlock* exit_block = reducer_->current_block();
  DCHECK_GT(blocks_.size(), 0);
  BasicBlock* entry_block = blocks_[0];
  // A live exit block may still hold new nodes the reducer buffered (e.g. a
  // constant-folded Select that emitted its taken arm inline); flush them into
  // the block before we inspect or splice it. An abrupt (null) exit already
  // flushed on its way out.
  if (exit_block != nullptr) reducer_->FlushNodesToBlock();
  bool is_empty = entry_block == exit_block && entry_block->nodes().empty();

  // Jumps `from` (which must be live and control-less) into this subgraph's
  // entry block.
  auto connect = [&](BasicBlock* from) {
    DCHECK_NOT_NULL(from);
    BasicBlockRef* jump_ref = zone_->New<BasicBlockRef>();
    Jump* jump = NodeBase::New<Jump>(zone_, /*input_count=*/0, jump_ref);
    jump_ref->Bind(entry_block);
    jump->set_owner(from);
    from->set_control_node(jump);
    entry_block->set_predecessor(from);
  };

  if (!is_empty) {
    if (parent_ != nullptr) {
      // Nested subgraph: splice directly into the enclosing subgraph, which
      // continues building from our exit.
      connect(saved_block_);
      parent_->blocks_.insert(parent_->blocks_.end(), blocks_.begin(),
                              blocks_.end());
      reducer_->set_current_block(exit_block);
      reducer_->SetNewNodePosition(BasicBlockPosition::End());
      // KNA stays as exit_block's merged aspects (set during Bind).
      return;
    }
    if (reducer_->pending_splice_.has_value()) {
      // Sequential sibling within the same reduced node: chain onto the pending
      // splice so it remains a single splice with one insertion point.
      auto& pending = *reducer_->pending_splice_;
      connect(pending.exit);
      pending.all_blocks.insert(pending.all_blocks.end(), blocks_.begin(),
                                blocks_.end());
      pending.exit = exit_block;
    } else {
      reducer_->RecordPendingSplice(entry_block, exit_block, blocks_);
    }
  }
  reducer_->set_current_block(saved_block_);
  reducer_->SetNewNodePosition(saved_position_);
  reducer_->set_known_node_aspects(saved_kna_);
}

void Subgraph<MaglevGraphOptimizer>::MergeIntoLabel(Label* label,
                                                    BasicBlock* predecessor) {
  // Borrow active KNA into variable_frame_ so the merge sees it.
  variable_frame_.set_known_node_aspects(&reducer_->known_node_aspects());
  if (label->variable_merge_state_ == nullptr) {
    label->variable_merge_state_ = MergePointInterpreterFrameState::New(
        *dummy_unit_, variable_frame_, /*merge_offset=*/0,
        label->predecessor_count(), predecessor, label->variable_liveness_,
        /*context_scope_info=*/std::nullopt);
  } else {
    label->variable_merge_state_->Merge(
        reducer_->graph(), reducer_->is_tracing(), *dummy_unit_,
        variable_frame_, predecessor, std::nullopt);
  }
  variable_frame_.clear_known_node_aspects();
}

void Subgraph<MaglevGraphOptimizer>::Goto(Label* label) {
  DCHECK_NOT_NULL(reducer_->current_block());
  BasicBlock* prev = reducer_->current_block();
  CHECK(!reducer_->AddNewControlNode<Jump>(/*inputs=*/{}, label->ref())
             .IsDoneWithAbort());
  reducer_->FlushNodesToBlock();
  MergeIntoLabel(label, prev);
  reducer_->set_current_block(nullptr);
}

void Subgraph<MaglevGraphOptimizer>::Bind(Label* label) {
  DCHECK_NULL(reducer_->current_block());
  DCHECK_NOT_NULL(label->variable_merge_state_);
  MergePointInterpreterFrameState* merge_state = label->variable_merge_state_;
  DCHECK_EQ(merge_state->predecessors_so_far(), label->predecessor_count_);
  DCHECK_GT(label->predecessor_count_, 0);

  // Materialize the merge block and stitch up Phis.
  BasicBlock* block = zone_->New<BasicBlock>(merge_state, zone_);
  label->ref_.Bind(block);
  blocks_.push_back(block);
  merge_state->InitializeWithBasicBlock(block);

  // Pull merged frame state + KNA back into variable_frame_, then move KNA
  // onto the reducer as the active aspects.
  variable_frame_.CopyFrom(*dummy_unit_, *merge_state);
  reducer_->set_known_node_aspects(variable_frame_.known_node_aspects());
  variable_frame_.clear_known_node_aspects();

  // Set predecessor IDs on the predecessor blocks' control nodes.
  if (label->predecessor_count_ > 1) {
    for (int p = 0; p < label->predecessor_count_; ++p) {
      merge_state->predecessor_at(p)->set_predecessor_id(p);
    }
  }

  reducer_->set_current_block(block);
  reducer_->SetNewNodePosition(BasicBlockPosition::End());
}

template <typename ControlNodeT, typename... Args>
ReduceResult Subgraph<MaglevGraphOptimizer>::GotoIfImpl(
    Label* jump_target, bool jump_on_true,
    std::initializer_list<ValueNode*> control_inputs, Args&&... args) {
  static_assert(IsConditionalControlNode(Node::opcode_of<ControlNodeT>));
  DCHECK_NOT_NULL(reducer_->current_block());

  BasicBlock* prev = reducer_->current_block();
  BasicBlock* fallthrough = zone_->New<BasicBlock>(/*state=*/nullptr, zone_);
  BasicBlockRef* fallthrough_ref = zone_->New<BasicBlockRef>();

  BasicBlockRef* true_ref = jump_on_true ? jump_target->ref() : fallthrough_ref;
  BasicBlockRef* false_ref =
      jump_on_true ? fallthrough_ref : jump_target->ref();

  ReduceResult r = reducer_->AddNewControlNode<ControlNodeT>(
      control_inputs, std::forward<Args>(args)..., true_ref, false_ref);
  if (r.IsDoneWithAbort()) return r;
  reducer_->FlushNodesToBlock();
  fallthrough_ref->Bind(fallthrough);
  fallthrough->set_predecessor(prev);
  blocks_.push_back(fallthrough);

  MergeIntoLabel(jump_target, prev);

  reducer_->set_current_block(fallthrough);
  reducer_->SetNewNodePosition(BasicBlockPosition::End());
  return ReduceResult::Done();
}

template <typename ControlNodeT, typename... Args>
ReduceResult Subgraph<MaglevGraphOptimizer>::GotoIfTrue(
    Label* true_target, std::initializer_list<ValueNode*> control_inputs,
    Args&&... args) {
  return GotoIfImpl<ControlNodeT>(true_target, /*jump_on_true=*/true,
                                  control_inputs, std::forward<Args>(args)...);
}

template <typename ControlNodeT, typename... Args>
ReduceResult Subgraph<MaglevGraphOptimizer>::GotoIfFalse(
    Label* false_target, std::initializer_list<ValueNode*> control_inputs,
    Args&&... args) {
  return GotoIfImpl<ControlNodeT>(false_target, /*jump_on_true=*/false,
                                  control_inputs, std::forward<Args>(args)...);
}

static_assert(ReducerBaseWithEagerDeopt<MaglevGraphOptimizer>);
static_assert(ReducerBaseWithLazyDeopt<MaglevGraphOptimizer>);

MaglevGraphOptimizer::MaglevGraphOptimizer(
    Graph* graph, RecomputeKnownNodeAspectsProcessor& kna_processor,
    NodeRanges* ranges)
    : reducer_(this, graph), kna_processor_(kna_processor), ranges_(ranges) {}

BlockProcessResult MaglevGraphOptimizer::PreProcessBasicBlock(
    BasicBlock* block) {
  // TODO(olivf): Support allocation folding across control flow.
  reducer_.ClearCurrentAllocationBlock();
  reducer_.set_current_block(block);
  TRACE(TraceColor::kYellow << "Entering block b" << block->id());
  if (block->is_loop()) {
    loop_depth_++;
  }
  return BlockProcessResult::kContinue;
}

BlockProcessResult MaglevGraphOptimizer::PostProcessBasicBlock(
    BasicBlock* block) {
  reducer_.FlushNodesToBlock();
  return BlockProcessResult::kContinue;
}

void MaglevGraphOptimizer::PreProcessNode(Node* node,
                                          const ProcessingState& state) {
  TRACE(TraceColor::kDarkCyan << "Processing " << PrintNodeLabel(node) << ": "
                              << PrintNode(node));
  reducer_.ResetPeriodThrowingNode();
#ifdef DEBUG
  reducer_.StartNewPeriod();
#endif  // DEBUG
  if (reducer_.has_graph_labeller()) {
    reducer_.SetCurrentProvenance(
        reducer_.graph_labeller()->GetNodeProvenance(current_node()));
  }
  reducer_.SetNewNodePosition(BasicBlockPosition::At(state.node_index()));
}

void MaglevGraphOptimizer::PostProcessNode(Node* node) {
  if (node->opcode() != Opcode::kAllocationBlock &&
      (node->properties().can_allocate() || node->properties().can_deopt() ||
       node->properties().can_throw())) {
    reducer_.ClearCurrentAllocationBlock();
  }
#ifdef DEBUG
  reducer_.SetCurrentProvenance(MaglevGraphLabeller::Provenance{});
  reducer_.SetNewNodePosition(BasicBlockPosition::End());
#endif  // DEBUG
}

void MaglevGraphOptimizer::PreProcessNode(Phi* phi,
                                          const ProcessingState& state) {
  TRACE(TraceColor::kDarkCyan << "Processing " << PrintNodeLabel(phi) << ": "
                              << PrintNode(phi));
}
void MaglevGraphOptimizer::PostProcessNode(Phi*) {}

void MaglevGraphOptimizer::PreProcessNode(ControlNode* node,
                                          const ProcessingState& state) {
  reducer_.SetNewNodePosition(BasicBlockPosition::End());
  TRACE(TraceColor::kDarkCyan << "Processing " << PrintNodeLabel(node) << ": "
                              << PrintNode(node));
}
void MaglevGraphOptimizer::PostProcessNode(ControlNode*) {}

compiler::JSHeapBroker* MaglevGraphOptimizer::broker() const {
  return reducer_.broker();
}

std::optional<Range> MaglevGraphOptimizer::GetRange(ValueNode* node) {
  if (!ranges_) return {};
  return ranges_->Get(reducer_.current_block(), node);
}

bool MaglevGraphOptimizer::IsRangeLessEqual(ValueNode* lhs, ValueNode* rhs) {
  if (!ranges_) return false;
  return ranges_->IsLessEqualConstraint(reducer_.current_block(), lhs, rhs);
}

ProcessResult MaglevGraphOptimizer::ReplaceWith(ValueNode* node) {
  // If current node is not a value node, we shouldn't try to replace it.
  CHECK(current_node()->Cast<ValueNode>());
  DCHECK(!node->Is<Identity>());
  if (reducer_.HasPendingSplice()) {
    TRACE(TraceColor::kDarkGreen << "Splicing subgraph into "
                                 << PrintNodeLabel(current_node()));
  }
  if (current_node()->properties().can_throw()) {
    // Merge the throwing-edge known-node-aspects into the catch handler, which
    // may still be reachable via other throwing nodes. But only keep it
    // reachable through this reduction if a node emitted while lowering can
    // still throw to it (e.g. lowering to `if (c) { throw } return Bar`).
    // Otherwise it is now dead and must not be force-kept reachable.
    kna_processor_.ProcessThrowingNode(
        current_node(),
        /*mark_handler_reachable=*/reducer_.period_added_throwing_node());
  }
  ValueNode* current_value = current_node()->Cast<ValueNode>();
  TRACE(TraceColor::kDarkGreen << "Replacing " << PrintNodeLabel(current_value)
                               << " with " << PrintNodeLabel(node) << ": "
                               << PrintNode(node));
  // Automatically convert node to the same representation of current_node.
  ReduceResult result = reducer_.ConvertInputTo(
      node, current_value->properties().value_representation());
  if (result.IsDoneWithAbort()) {
    return ProcessResult::kTruncateBlock;
  }
  current_value->OverwriteWithIdentityTo(result.value());
  return ProcessResult::kRemove;
}

template <typename NodeT, typename... Args>
ProcessResult MaglevGraphOptimizer::ReplaceWith(
    std::initializer_list<ValueNode*> inputs, Args&&... args) {
  // If current node is not a value node, we shouldn't try to replace it.
  CHECK(current_node()->Cast<ValueNode>());
  ValueNode* current_value = current_node()->Cast<ValueNode>();
  TRACE(TraceColor::kDarkGreen << "Replacing " << PrintNodeLabel(current_value)
                               << " with a new node");
  current_value->ClearInputs();
  NodeT* new_node =
      current_value->OverwriteWith<NodeT>(std::forward<Args>(args)...);
  ReduceResult result = reducer_.SetNodeInputs(new_node, inputs);
  DCHECK(result.IsDone());
  if (result.IsDoneWithAbort()) {
    return DeoptAndTruncate(DeoptimizeReason::kUnknown);
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::RemoveCurrentNode() {
  current_node()->ClearInputs();
  return ProcessResult::kRemove;
}

void MaglevGraphOptimizer::UnwrapInputs() {
  for (int i = 0; i < current_node()->input_count(); i++) {
    ValueNode* input = current_node()->input(i).node();
    if (!input) continue;
    current_node()->change_input(i, input->UnwrapIdentitiesAndPhis());
  }
}

ValueNode* MaglevGraphOptimizer::GetConstantWithRepresentation(
    ValueNode* node, UseRepresentation use_repr,
    std::optional<TaggedToFloat64ConversionType> conversion_type) {
  switch (use_repr) {
    case UseRepresentation::kInt32:
    case UseRepresentation::kTruncatedInt32: {
      auto cst = reducer_.TryGetInt32Constant(node);
      if (cst.has_value()) {
        return reducer_.GetInt32Constant(cst.value());
      }
      return nullptr;
    }
    case UseRepresentation::kFloat64:
    case UseRepresentation::kHoleyFloat64: {
      DCHECK(conversion_type.has_value());
      auto cst = reducer_.TryGetFloat64OrHoleyFloat64Constant(use_repr, node,
                                                              *conversion_type);
      if (cst.has_value()) {
        if (use_repr == UseRepresentation::kHoleyFloat64) {
          return reducer_.GetHoleyFloat64Constant(cst.value());
        }
        return reducer_.GetFloat64Constant(cst.value());
      }
      return nullptr;
    }
    default:
      return nullptr;
  }
}

MaybeReduceResult MaglevGraphOptimizer::GetUntaggedValueWithRepresentation(
    ValueNode* node, UseRepresentation use_repr,
    std::optional<TaggedToFloat64ConversionType> conversion_type) {
  DCHECK_NE(use_repr, UseRepresentation::kTagged);
  if (node->value_representation() == ValueRepresentationFromUse(use_repr)) {
    return node;
  }
  if (node->Is<ReturnedValue>()) {
    ValueNode* input = node->input_node(0);
    return GetUntaggedValueWithRepresentation(input, use_repr, conversion_type);
  }
  // We try getting constant before bailing out and/or calling the reducer,
  // since it does not emit a conversion node.
  if (ValueNode* cst =
          GetConstantWithRepresentation(node, use_repr, conversion_type)) {
    return cst;
  }
  if (node->is_tagged()) {
    // Check if we already have a canonical conversion.
    NodeInfo* node_info =
        known_node_aspects().GetOrCreateInfoFor(broker(), node);
    auto& alternative = node_info->alternative();
    if (ValueNode* alt = alternative.get(use_repr)) return alt;
    return {};
  }
  // TODO(victorgomes): The GetXXX functions may emit a conversion node that
  // might eager deopt. We need to find a correct eager deopt frame for them if
  // current_node_ does not have a deopt info.
  if (!current_node_->properties().has_eager_deopt_info()) {
    return {};
  }
  switch (use_repr) {
    case UseRepresentation::kInt32:
      return reducer_.GetInt32(node);
    case UseRepresentation::kTruncatedInt32:
      DCHECK(conversion_type.has_value());
      return reducer_.GetTruncatedInt32ForToNumber(
          node, GetAllowedTypeFromConversionType(*conversion_type));
    case UseRepresentation::kFloat64:
      DCHECK(conversion_type.has_value());
      return reducer_.GetFloat64ForToNumber(
          node, GetAllowedTypeFromConversionType(*conversion_type));
    case UseRepresentation::kHoleyFloat64:
      DCHECK(conversion_type.has_value());
      return reducer_.GetHoleyFloat64ForToNumber(
          node, GetAllowedTypeFromConversionType(*conversion_type));
    default:
      return {};
  }
  UNREACHABLE();
}

template <typename NodeT>
ValueNode* MaglevGraphOptimizer::TrySmiTag(Input input) {
  auto cst = reducer_.TryGetInt32Constant(input.node());
  if (cst.has_value() && Smi::IsValid(cst.value())) {
    return reducer_.GetSmiConstant(cst.value());
  }
  if (auto range = GetRange(input.node())) {
    if (range->IsSmi()) {
      return reducer_.AddNewNodeNoInputConversion<NodeT>({input.node()});
    }
  }
  return nullptr;
}

template <Operation kOperation>
std::optional<ProcessResult> MaglevGraphOptimizer::TryFoldInt32Operation(
    ValueNode* node) {
  MaybeReduceResult result;
  if constexpr (IsUnaryOperation(kOperation)) {
    result =
        reducer_.TryFoldInt32UnaryOperation<kOperation>(node->input_node(0));
  } else {
    static_assert(IsBinaryOperation(kOperation));
    result = reducer_.TryFoldInt32BinaryOperation<kOperation>(
        node->input_node(0), node->input_node(1));
  }
  if (!result.IsDone()) return {};
  DCHECK(result.IsDoneWithValue());
  if constexpr (kOperation == Operation::kShiftRightLogical) {
    // ShiftRightLogical returns an Uint32 instead of an Int32. We don't have
    // any peephole optimizations for ShiftRightLogical, if we managed to fold
    // it, it must have been because both inputs were constants and we got a
    // Uint32Constant.
    CHECK(result.value()->Is<Uint32Constant>());
    return ReplaceWith(result.value());
  }
  ReduceResult int32_result = reducer_.GetInt32(result.value());
  if (int32_result.IsDoneWithAbort()) {
    return ProcessResult::kTruncateBlock;
  }
  DCHECK(int32_result.IsDoneWithValue());
  return ReplaceWith(int32_result.value());
}

template <Operation kOperation>
std::optional<ProcessResult> MaglevGraphOptimizer::TryFoldFloat64Operation(
    ValueNode* node) {
  MaybeReduceResult result;
  if constexpr (IsUnaryOperation(kOperation)) {
    result = reducer_.TryFoldFloat64UnaryOperationForToNumber<kOperation>(
        TaggedToFloat64ConversionType::kOnlyNumber, node->input_node(0));
  } else {
    static_assert(IsBinaryOperation(kOperation));
    result = reducer_.TryFoldFloat64BinaryOperationForToNumber<kOperation>(
        TaggedToFloat64ConversionType::kOnlyNumber, node->input_node(0),
        node->input_node(1));
  }
  if (!result.IsDone()) return {};
  if (result.IsDoneWithAbort()) {
    return ProcessResult::kTruncateBlock;
  }
  DCHECK(result.IsDoneWithValue());
  ReduceResult float64_result = reducer_.GetFloat64(result.value());
  if (float64_result.IsDoneWithAbort()) {
    return ProcessResult::kTruncateBlock;
  }
  DCHECK(float64_result.IsDoneWithValue());
  return ReplaceWith(float64_result.value());
}

Jump* MaglevGraphOptimizer::FoldBranch(BasicBlock* current,
                                       BranchControlNode* branch_node,
                                       bool if_true) {
  BasicBlock* target =
      if_true ? branch_node->if_true() : branch_node->if_false();
  BasicBlock* unreachable_block =
      if_true ? branch_node->if_false() : branch_node->if_true();

  TRACE(TraceColor::kGreen << "Folding branch " << PrintNodeLabel(branch_node)
                           << " to b" << target->id() << " (unreachable b"
                           << unreachable_block->id() << ")");

  // Remove predecessor from unreachable block.
  if (!unreachable_block->has_state()) {
    unreachable_block->set_predecessor(nullptr);
  } else {
    // Split-edge form guarantees that {unreachable_block} has a single
    // predecessor, which is {current}.
    DCHECK_EQ(unreachable_block->predecessor_count(), 1);
    DCHECK_EQ(unreachable_block->predecessor_at(0), current);
    unreachable_block->state()->RemovePredecessorAt(0);
  }

  // Update control node.
  Jump* new_control_node = branch_node->OverwriteWith<Jump>();
  new_control_node->set_target(target);

  // Cache predecessor id from the target in the unconditional jump.
  constexpr int kPredecessorId = 0;
#ifdef DEBUG
  if (target->has_state()) {
    // Split-edge form guarantees that {target} has a single
    // predecessor, which is {current}.
    DCHECK_EQ(target->predecessor_count(), 1);
    DCHECK_EQ(target->predecessor_at(kPredecessorId), current);
  }
#endif
  new_control_node->set_predecessor_id(kPredecessorId);

  reducer_.graph()->set_may_have_unreachable_blocks(true);
  return new_control_node;
}

void MaglevGraphOptimizer::AttachExceptionHandlerInfo(NodeBase* node) {
  DCHECK(node->properties().can_throw());
  DCHECK(current_node()->properties().can_throw());
  ExceptionHandlerInfo* info = current_node()->exception_handler_info();
  if (info->ShouldLazyDeopt()) {
    new (node->exception_handler_info())
        ExceptionHandlerInfo(ExceptionHandlerInfo::kLazyDeopt);
  } else if (!info->HasExceptionHandler()) {
    new (node->exception_handler_info()) ExceptionHandlerInfo();
  } else {
    new (node->exception_handler_info())
        ExceptionHandlerInfo(info->catch_block(), info->depth());
  }
  if (node->Is<CallKnownJSFunction>()) {
    // Ensure that we always have the handler of inline call
    // candidates.
    reducer_.current_block()->AddExceptionHandler(
        node->exception_handler_info());
  }
}

template <typename NodeT>
ProcessResult MaglevGraphOptimizer::ProcessLoadContextSlot(NodeT* node) {
  REPLACE_AND_RETURN_IF_DONE(known_node_aspects().TryGetContextCachedValue(
      node->input_node(0), node->offset(), node->maybe_assigned()));
  return ProcessResult::kContinue;
}

MaybeReduceResult MaglevGraphOptimizer::EnsureType(ValueNode* node,
                                                   NodeType type,
                                                   DeoptimizeReason reason) {
  if (IsEmptyNodeType(IntersectType(reducer_.GetType(node), type))) {
    return reducer_.EmitUnconditionalDeopt(reason);
  }
  if (!reducer_.EnsureType(node, type)) {
    return {};
  }

  return ReduceResult::Done();
}

ProcessResult MaglevGraphOptimizer::VisitAssertInt32(
    AssertInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckDynamicValue(
    CheckDynamicValue* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckInt32IsSmi(
    CheckInt32IsSmi* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetInt32Constant(node->input_node(0))) {
    if (Smi::IsValid(cst.value())) {
      return RemoveCurrentNode();
    }
    return DeoptAndTruncate(DeoptimizeReason::kNotASmi);
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckUint32IsSmi(
    CheckUint32IsSmi* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetUint32Constant(node->input_node(0))) {
    if (Smi::IsValid(cst.value())) {
      return RemoveCurrentNode();
    }
    return DeoptAndTruncate(DeoptimizeReason::kNotASmi);
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckIntPtrIsSmi(
    CheckIntPtrIsSmi* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetIntPtrConstant(node->input_node(0))) {
    if (Smi::IsValid(cst.value())) {
      return RemoveCurrentNode();
    }
    return DeoptAndTruncate(DeoptimizeReason::kNotASmi);
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckFloat64IsSmi(
    CheckFloat64IsSmi* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kFloat64, node->input_node(0),
          TaggedToFloat64ConversionType::kNumberOrOddball)) {
    if (IsSmiDouble(cst.value().get_scalar())) {
      return RemoveCurrentNode();
    }
    return DeoptAndTruncate(DeoptimizeReason::kNotASmi);
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckHoleyFloat64IsSmi(
    CheckHoleyFloat64IsSmi* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kHoleyFloat64, node->input_node(0),
          TaggedToFloat64ConversionType::kNumberOrOddball)) {
    if (IsSmiDouble(cst.value().get_scalar())) {
      return RemoveCurrentNode();
    }
    return DeoptAndTruncate(DeoptimizeReason::kNotASmi);
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckHeapObject(
    CheckHeapObject* node, const ProcessingState& state) {
  REMOVE_AND_RETURN_IF_DONE(EnsureType(
      node->input_node(0), NodeType::kAnyHeapObject, DeoptimizeReason::kSmi));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckInt32Condition(
    CheckInt32Condition* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  if (node->condition() == AssertCondition::kUnsignedLessThan) {
    ValueNode* lhs = node->input_node(0);
    ValueNode* rhs = node->input_node(1);
    auto r1 = GetRange(lhs);
    auto r2 = GetRange(rhs);
    if (r1 && r2 && r1->IsUint32()) {
      if (*r1 < *r2 || IsRangeLessEqual(lhs, rhs)) {
        return RemoveCurrentNode();
      }
    }
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckCacheIndicesNotCleared(
    CheckCacheIndicesNotCleared* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckJSDataViewBounds(
    CheckJSDataViewBounds* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckTypedArrayBounds(
    CheckTypedArrayBounds* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckTypedArrayValid(
    CheckTypedArrayValid* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

template <typename NodeT>
ProcessResult MaglevGraphOptimizer::ProcessCheckMaps(NodeT* node,
                                                     ValueNode* object_map) {
  ValueNode* object = node->input_node(0);
  KnownMapsMerger<compiler::ZoneRefSet<Map>> merger(broker(), reducer_.zone(),
                                                    node->maps());
  MaybeReduceResult result =
      reducer_.TryFoldCheckMaps(object, object_map, node->maps(), merger);
  if (result.IsDoneWithAbort()) {
    return ProcessResult::kTruncateBlock;
  }
  if (result.IsDone()) {
    if (merger.emit_check_with_migration()) {
      return ProcessResult::kContinue;
    }
    return RemoveCurrentNode();
  }
  if constexpr (std::is_same_v<NodeT, CheckMaps> ||
                std::is_same_v<NodeT, CheckMapsWithMigration> ||
                std::is_same_v<NodeT, CheckMapsWithMigrationAndDeopt>) {
    NodeInfo* known_info =
        known_node_aspects().GetOrCreateInfoFor(broker(), object);
    node->set_check_type(reducer_.GetCheckType(known_info->type()));
  }

  merger.UpdateKnownNodeAspects(object, known_node_aspects());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckMaps(
    CheckMaps* node, const ProcessingState& state) {
  return ProcessCheckMaps(node);
}

ProcessResult MaglevGraphOptimizer::VisitCheckHomomorphicMap(
    CheckHomomorphicMap* node, const ProcessingState& state) {
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckMapsWithMigrationAndDeopt(
    CheckMapsWithMigrationAndDeopt* node, const ProcessingState& state) {
  return ProcessCheckMaps(node);
}

ProcessResult MaglevGraphOptimizer::VisitCheckMapsWithMigration(
    CheckMapsWithMigration* node, const ProcessingState& state) {
  return ProcessCheckMaps(node);
}

ProcessResult MaglevGraphOptimizer::VisitCheckMapsWithAlreadyLoadedMap(
    CheckMapsWithAlreadyLoadedMap* node, const ProcessingState& state) {
  return ProcessCheckMaps(node, node->MapInput().node());
}

ProcessResult MaglevGraphOptimizer::VisitCheckDetectableCallable(
    CheckDetectableCallable* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckJSReceiverOrNullOrUndefined(
    CheckJSReceiverOrNullOrUndefined* node, const ProcessingState& state) {
  ValueNode* input = node->input_node(0);
  REMOVE_AND_RETURN_IF_DONE(
      EnsureType(input, NodeType::kJSReceiverOrNullOrUndefined));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckNotHole(
    CheckNotHole* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckNotUndefined(
    CheckNotUndefined* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckHoleyFloat64NotHoleOrUndefined(
    CheckHoleyFloat64NotHoleOrUndefined* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}
#define VISIT_CHECK(Type)                                 \
  ProcessResult MaglevGraphOptimizer::VisitCheck##Type(   \
      Check##Type* node, const ProcessingState& state) {  \
    if (NodeTypeIs(reducer_.GetType(node->input_node(0)), \
                   NodeType::k##Type)) {                  \
      return RemoveCurrentNode();                         \
    }                                                     \
    return ProcessResult::kContinue;                      \
  }
VISIT_CHECK(Smi)
VISIT_CHECK(Number)
VISIT_CHECK(String)
VISIT_CHECK(SeqOneByteString)
VISIT_CHECK(StringOrStringWrapper)
VISIT_CHECK(StringOrOddball)
VISIT_CHECK(Symbol)
#undef VISIT_CHECK

ProcessResult MaglevGraphOptimizer::VisitCheckValue(
    CheckValue* node, const ProcessingState& state) {
  ValueNode* input = node->input_node(0);
  if (HeapConstant* constant = input->TryCast<HeapConstant>()) {
    if (constant->object() == node->value()) {
      return RemoveCurrentNode();
    }
    // TODO(victorgomes): Support soft deopting and killing the rest of the
    // block.
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckValueEqualsInt32(
    CheckValueEqualsInt32* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetInt32Constant(node->input_node(0))) {
    if (cst.value() == node->value()) {
      return RemoveCurrentNode();
    }
    return DeoptAndTruncate(node->deoptimize_reason());
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckFloat64SameValue(
    CheckFloat64SameValue* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kFloat64, node->input_node(0),
          TaggedToFloat64ConversionType::kNumberOrOddball)) {
    double left = cst.value().get_scalar();
    double right = node->value().get_scalar();
    bool same_value = false;
    if (std::isnan(left) && std::isnan(right)) {
      same_value = true;
    } else {
      same_value =
          base::bit_cast<uint64_t>(left) == base::bit_cast<uint64_t>(right);
    }

    if (same_value) {
      return RemoveCurrentNode();
    }
    return DeoptAndTruncate(node->deoptimize_reason());
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckValueEqualsString(
    CheckValueEqualsString* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckInstanceType(
    CheckInstanceType* node, const ProcessingState& state) {
  ValueNode* input = node->input_node(0);
  if (input->Is<FastCreateClosure>()) {
    if (node->first_instance_type() == FIRST_JS_FUNCTION_TYPE &&
        node->last_instance_type() == LAST_JS_FUNCTION_TYPE) {
      // Don't need to emit this check, we know it is a closure.
      return RemoveCurrentNode();
    }
  }

  if (node->first_instance_type() == FIRST_JS_RECEIVER_TYPE &&
      node->last_instance_type() == LAST_JS_RECEIVER_TYPE) {
    REMOVE_AND_RETURN_IF_DONE(EnsureType(input, NodeType::kJSReceiver,
                                         DeoptimizeReason::kWrongInstanceType));
  }

  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckMaglevType(
    CheckMaglevType* node, const ProcessingState& state) {
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitDead(Dead* node,
                                              const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitAssumeMap(AssumeMap*,
                                                   const ProcessingState&) {
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitAssumeTaggedType(
    AssumeTaggedType*, const ProcessingState&) {
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitAssumeInt32Type(
    AssumeInt32Type*, const ProcessingState&) {
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitAssumeUint32Type(
    AssumeUint32Type*, const ProcessingState&) {
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitAssumeFloat64Type(
    AssumeFloat64Type*, const ProcessingState&) {
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTurbofanStaticAssert(
    TurbofanStaticAssert*, const ProcessingState&) {
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitAssertPeeled(AssertPeeled*,
                                                      const ProcessingState&) {
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitAssertEscapeAnalysisElided(
    AssertEscapeAnalysisElided*, const ProcessingState&) {
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitMajorGCForCompilerTesting(
    MajorGCForCompilerTesting*, const ProcessingState&) {
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitDebugBreak(
    DebugBreak* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFunctionEntryStackCheck(
    FunctionEntryStackCheck* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGeneratorStore(
    GeneratorStore* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTryOnStackReplacement(
    TryOnStackReplacement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreMap(
    StoreMap* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreFixedArrayElementWithWriteBarrier(
    StoreFixedArrayElementWithWriteBarrier* node,
    const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreFixedArrayElementNoWriteBarrier(
    StoreFixedArrayElementNoWriteBarrier* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreFixedHoleyDoubleArrayElement(
    StoreFixedHoleyDoubleArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreFixedDoubleArrayElement(
    StoreFixedDoubleArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreInt32(
    StoreInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreFloat64(
    StoreFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreIntTypedArrayElement(
    StoreIntTypedArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreDoubleTypedArrayElement(
    StoreDoubleTypedArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreIntConstantTypedArrayElement(
    StoreIntConstantTypedArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreDoubleConstantTypedArrayElement(
    StoreDoubleConstantTypedArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreSignedIntDataViewElement(
    StoreSignedIntDataViewElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreDoubleDataViewElement(
    StoreDoubleDataViewElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreTaggedFieldNoWriteBarrier(
    StoreTaggedFieldNoWriteBarrier* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreTaggedFieldWithWriteBarrier(
    StoreTaggedFieldWithWriteBarrier* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreContextSlotWithWriteBarrier(
    StoreContextSlotWithWriteBarrier* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreSmiContextCell(
    StoreSmiContextCell* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreInt32ContextCell(
    StoreInt32ContextCell* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreFloat64ContextCell(
    StoreFloat64ContextCell* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitStoreTrustedPointerFieldWithWriteBarrier(
    StoreTrustedPointerFieldWithWriteBarrier* node,
    const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitHandleNoHeapWritesInterrupt(
    HandleNoHeapWritesInterrupt* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitReduceInterruptBudgetForLoop(
    ReduceInterruptBudgetForLoop* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitReduceInterruptBudgetForReturn(
    ReduceInterruptBudgetForReturn* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitThrowReferenceErrorIfHole(
    ThrowReferenceErrorIfHole* node, const ProcessingState& state) {
  switch (node->ValueInput().node()->IsTheHole()) {
    case Tribool::kTrue: {
      return ThrowAndTruncate(Throw::kThrowAccessedUninitializedVariable,
                              node->ValueInput().node());
    }
    case Tribool::kFalse:
      // Not the hole; removing.
      return RemoveCurrentNode();
    case Tribool::kMaybe:
      return ProcessResult::kContinue;
  }
  UNREACHABLE();
}

ProcessResult MaglevGraphOptimizer::VisitThrowSuperNotCalledIfHole(
    ThrowSuperNotCalledIfHole* node, const ProcessingState& state) {
  switch (node->ValueInput().node()->IsTheHole()) {
    case Tribool::kTrue: {
      return ThrowAndTruncate(Throw::kThrowSuperNotCalled);
    }
    case Tribool::kFalse:
      // Not the hole; removing.
      return RemoveCurrentNode();
    case Tribool::kMaybe:
      return ProcessResult::kContinue;
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitThrowSuperAlreadyCalledIfNotHole(
    ThrowSuperAlreadyCalledIfNotHole* node, const ProcessingState& state) {
  switch (node->ValueInput().node()->IsTheHole()) {
    case Tribool::kTrue:
      // It is the hole; removing.
      return RemoveCurrentNode();
    case Tribool::kFalse: {
      return ThrowAndTruncate(Throw::kThrowSuperAlreadyCalledError);
    }
    case Tribool::kMaybe:
      return ProcessResult::kContinue;
  }
  UNREACHABLE();
}

ProcessResult MaglevGraphOptimizer::VisitThrowIfNotCallable(
    ThrowIfNotCallable* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitThrowIfNotSuperConstructor(
    ThrowIfNotSuperConstructor* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTransitionElementsKindOrCheckMap(
    TransitionElementsKindOrCheckMap* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSetContinuationPreservedEmbedderData(
    SetContinuationPreservedEmbedderData* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFulfillPromise(
    FulfillPromise* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTransitionAndStoreArrayElement(
    TransitionAndStoreArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitIdentity(
    Identity* node, const ProcessingState& state) {
  // If a non-eager inlined function returns a tagged value, we substitute the
  // call with an Identity. The node is then removed from the graph here. All
  // references to it will be removed in this graph optimizer pass.
  //
  // Unlike RemoveCurrentNode(), we must not clear the input here: the Identity
  // stays a valid forwarding stub (UnwrapIdentities walks input(0)) until every
  // remaining user has been revisited, so we only drop our use of the input.
  node->input_node(0)->remove_use();
  return ProcessResult::kRemove;
}

ProcessResult MaglevGraphOptimizer::VisitAllocationBlock(
    AllocationBlock* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitArgumentsElements(
    ArgumentsElements* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitArgumentsLength(
    ArgumentsLength* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitRestLength(
    RestLength* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCall(Call* node,
                                              const ProcessingState& state) {
  ValueNode* target = node->TargetInput().node();
  auto target_function = reducer_.TryGetConstant<JSFunction>(target);
  if (!target_function.has_value()) return ProcessResult::kContinue;

  // Don't inline CallFunction stub across native contexts.
  if (target_function->native_context(broker()) !=
      broker()->target_native_context()) {
    return ProcessResult::kContinue;
  }

  compiler::SharedFunctionInfoRef shared = target_function->shared(broker());
  // Do not reduce calls to functions with break points.
  if (shared.HasBreakInfo(broker())) return ProcessResult::kContinue;
  if (IsClassConstructor(shared.kind())) {
    // If we have a class constructor, we should raise an exception.
    return ThrowAndTruncate(Throw::kThrowConstructorNonCallableError, target);
  }

  // Call's varargs slots are [receiver, arg0, arg1, ...]; CallKnownBuiltin
  // pulls receiver into a fixed input slot.
  if (node->num_args() < 1) return ProcessResult::kContinue;
  ValueNode* receiver = node->arg(0).node();
  int positional_arg_count = node->num_args() - 1;

  // Receiver conversion: CallKnownBuiltin expects a pre-converted receiver.
  // For native or strict callees, no conversion is needed. For sloppy
  // callees, kNullOrUndefined receivers are replaced by the global proxy;
  // otherwise we emit a runtime ConvertReceiver. Already-known JSReceivers
  // pass through unchanged.
  ConvertReceiverMode receiver_mode = node->receiver_mode();
  ReduceResult converted_receiver_result =
      reducer_.GetConvertReceiver(shared, receiver, receiver_mode);
  if (!converted_receiver_result.IsDoneWithValue()) {
    return ProcessResult::kContinue;
  }
  ValueNode* converted_receiver = converted_receiver_result.value();

  ValueNode* context = reducer_.GetConstant(target_function->context(broker()));
  ValueNode* new_target = reducer_.GetRootConstant(RootIndex::kUndefinedValue);
  if (shared.HasBuiltinId()) {
    Builtin builtin_id = shared.builtin_id();
    if (MaglevGraphBuilder::IsReducibleBuiltin(builtin_id)) {
      CallArguments args(node);
      MaybeReduceResult reduction = reducer_.TryReduceBuiltin(
          builtin_id, *target_function, args, node->feedback());
      if (reduction.IsDoneWithAbort()) {
        return ProcessResult::kTruncateBlock;
      }
      if (reduction.IsDoneWithValue()) {
        TRACE(TraceColor::kDarkGreen << "Reducing " << PrintNodeLabel(node)
                                     << " to " << Builtins::name(builtin_id));
        return ReplaceWith(reduction.value());
      }

      size_t input_count =
          positional_arg_count + CallKnownBuiltin::kFixedInputCount;
      ReduceResult new_call = reducer_.AddNewNode<CallKnownBuiltin>(
          input_count,
          [&](CallKnownBuiltin* call) {
            for (int i = 0; i < positional_arg_count; i++) {
              call->set_arg(i, node->arg(i + 1).node());
            }
            return ReduceResult::Done();
          },
          builtin_id, target_function->dispatch_handle(), shared, target,
          context, converted_receiver, new_target, node->feedback());
      DCHECK(new_call.IsDoneWithValue());
      TRACE(TraceColor::kDarkGreen << "Promoting " << PrintNodeLabel(node)
                                   << " to CallKnownBuiltin("
                                   << Builtins::name(builtin_id) << ")");
      return ReplaceWith(new_call.value());
    }
  }

  // Promote to CallKnownJSFunction.
  ReduceResult new_call = reducer_.BuildCallKnownJSFunction(
      target_function->dispatch_handle(), shared, target, context,
      converted_receiver, new_target, positional_arg_count,
      [&](int i) { return ReduceResult::Done(node->arg(i + 1).node()); },
      node->feedback());
  DCHECK(new_call.IsDoneWithValue());
  CallKnownJSFunction* new_call_node =
      new_call.value()->Cast<CallKnownJSFunction>();

  compiler::FeedbackCellRef feedback_cell =
      target_function->raw_feedback_cell(broker());
  if (!feedback_cell.feedback_vector(broker())) {
    return ReplaceWith(new_call_node);
  }

  // TODO(victorgomes): Should we propagate call frequency from feedback?
  float call_frequency = 1.0f;
  const MaglevCompilationUnit* current_unit = nullptr;
  if (new_call_node->properties().can_lazy_deopt()) {
    current_unit =
        &new_call_node->lazy_deopt_info()->top_frame().GetCompilationUnit();
  }
  if (!reducer_.CanInlineCall(current_unit, shared, call_frequency)) {
    return ReplaceWith(new_call_node);
  }

  // Create and push MaglevCallSiteInfo
  // TODO(victorgomes): Investigate if we can avoid this copy.
  auto arguments =
      reducer_.zone()->AllocateVector<ValueNode*>(node->num_args());
  arguments[0] = converted_receiver;
  for (int i = 1; i < node->num_args(); ++i) {
    arguments[i] = node->arg(i).node();
  }

  CatchBlockDetails catch_details;
  if (node->exception_handler_info()->HasExceptionHandler()) {
    int depth = node->exception_handler_info()->ShouldLazyDeopt()
                    ? 0
                    : node->exception_handler_info()->depth() + 1;
    catch_details = {node->exception_handler_info()->catch_block_ref_address(),
                     !node->exception_handler_info()->ShouldLazyDeopt(), true,
                     depth};
  }

  int bytecode_length = shared.GetBytecodeArray(broker()).length();
  float score =
      (call_frequency / bytecode_length) * (loop_depth_ > 0 ? 1.5 : 1.0);

  bool is_small_function =
      bytecode_length <
      reducer_.graph()->compilation_info()->flags().max_eager_inlined_bytecode;
  KnownNodeAspects* call_aspects = known_node_aspects().Clone(reducer_.zone());
  call_aspects->virtual_objects() =
      new_call_node->lazy_deopt_info()->top_frame().GetVirtualObjects();
  MaglevCallSiteInfo* call_site = reducer_.zone()->New<MaglevCallSiteInfo>(
      MaglevCallerDetails{
          base::VectorOf(arguments),
          &new_call_node->lazy_deopt_info()->top_frame(), call_aspects,
          nullptr,  // loop_effects
          ZoneUnorderedMap<KnownNodeAspects::LoadedContextSlotsKey, Node*>(
              reducer_.zone()),  // unobserved_context_slot_stores
          catch_details, loop_depth_,
          0,      // peeled_iteration_count
          false,  // is_eager_inline
          is_small_function, call_frequency,
          reducer_.graph()->inlining_tree_debug_info()},
      new_call_node, feedback_cell, score, bytecode_length);

  reducer_.PushInlineCandidate(call_site);

  TRACE(TraceColor::kDarkGreen << "Promoting " << PrintNodeLabel(node)
                               << " to CallKnownJSFunction");
  return ReplaceWith(new_call_node);
}

ProcessResult MaglevGraphOptimizer::VisitCallBuiltin(
    CallBuiltin* node, const ProcessingState& state) {
  switch (node->builtin()) {
    case Builtin::kInstanceOf: {
      DCHECK_EQ(node->input_count(), 3);
      ValueNode* object = node->input_node(0);
      ValueNode* callable = node->input_node(1);
      ValueNode* context = node->input_node(2);
      if (HeapConstant* callable_cst = callable->TryCast<HeapConstant>()) {
        if (callable_cst->object().IsJSObject()) {
          REPLACE_AND_RETURN_IF_DONE(reducer_.TryBuildFastInstanceOf(
              context, object, callable_cst->object().AsJSObject(), nullptr));
        }
      }
      break;
    }
    case Builtin::kOrdinaryHasInstance: {
      DCHECK_EQ(node->input_count(), 3);
      ValueNode* callable = node->input_node(0);
      ValueNode* object = node->input_node(1);
      ValueNode* context = node->input_node(2);
      if (HeapConstant* callable_cst = callable->TryCast<HeapConstant>()) {
        if (callable_cst->object().IsJSObject()) {
          REPLACE_AND_RETURN_IF_DONE(reducer_.TryBuildFastOrdinaryHasInstance(
              context, object, callable_cst->object().AsJSObject(), nullptr));
        }
      }
      break;
    }
    default:
      break;
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallForwardVarargs(
    CallForwardVarargs* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConstructForwardVarargs(
    ConstructForwardVarargs* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallRuntime(
    CallRuntime* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallWithArrayLike(
    CallWithArrayLike* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallWithSpread(
    CallWithSpread* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallKnownApiFunction(
    CallKnownApiFunction* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallKnownJSFunction(
    CallKnownJSFunction* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  if (!node->NewTargetInput().node()->IsUndefinedValue() &&
      node->shared_function_info().object()->construct_as_builtin()) {
    // The invariant of such builtin targets is that the return value is a
    // JSReceiver. Set the type accordingly here.
    known_node_aspects().EnsureType(broker(), node, NodeType::kJSReceiver);
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallKnownBuiltin(
    CallKnownBuiltin* node, const ProcessingState& state) {
  ValueNode* target = node->TargetInput().node();
  auto target_function = reducer_.TryGetConstant<JSFunction>(target);
  if (!target_function.has_value()) return ProcessResult::kContinue;

  CallArguments args(node);
  MaybeReduceResult reduction = reducer_.TryReduceBuiltin(
      node->builtin_id(), *target_function, args, node->feedback_source());
  if (!reduction.IsDone()) return ProcessResult::kContinue;
  if (reduction.IsDoneWithAbort()) {
    return ProcessResult::kTruncateBlock;
  }
  DCHECK(reduction.IsDoneWithValue());
  TRACE(TraceColor::kDarkGreen << "Reducing " << PrintNodeLabel(node)
                               << " builtin "
                               << Builtins::name(node->builtin_id()));
  return ReplaceWith(reduction.value());
}

ProcessResult MaglevGraphOptimizer::VisitReturnedValue(
    ReturnedValue* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCallSelf(
    CallSelf* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConstruct(
    Construct* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckConstructResult(
    CheckConstructResult* node, const ProcessingState& state) {
  // TODO(b/424157317): Consider optimizing other cases, too.
  if (node->ConstructResultInput().node()->IsUndefinedValue()) {
    return ReplaceWith(node->ImplicitReceiverInput().node());
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckDerivedConstructResult(
    CheckDerivedConstructResult* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConstructWithSpread(
    ConstructWithSpread* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConvertReceiver(
    ConvertReceiver* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConvertHoleToUndefined(
    ConvertHoleToUndefined* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateArrayLiteral(
    CreateArrayLiteral* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateShallowArrayLiteral(
    CreateShallowArrayLiteral* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateObjectLiteral(
    CreateObjectLiteral* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateShallowObjectLiteral(
    CreateShallowObjectLiteral* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateFunctionContext(
    CreateFunctionContext* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateClosure(
    CreateClosure* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFastCreateClosure(
    FastCreateClosure* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateRegExpLiteral(
    CreateRegExpLiteral* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitDeleteProperty(
    DeleteProperty* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitEnsureWritableFastElements(
    EnsureWritableFastElements* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitExtendPropertiesBackingStore(
    ExtendPropertiesBackingStore* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInlinedAllocation(
    InlinedAllocation* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitForInPrepare(
    ForInPrepare* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitForInNext(
    ForInNext* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGeneratorRestoreRegister(
    GeneratorRestoreRegister* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGetIterator(
    GetIterator* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGetSecondReturnedValue(
    GetSecondReturnedValue* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGetTemplateObject(
    GetTemplateObject* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitHasInPrototypeChain(
    HasInPrototypeChain* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(reducer_.TryBuildFastHasInPrototypeChain(
      node->input_node(0), node->prototype()));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInitialValue(
    InitialValue* node, const ProcessingState& state) {
  // Set the type for the `this` register in sloppy mode.
  const MaglevCompilationUnit* unit =
      reducer_.graph()->compilation_info()->toplevel_compilation_unit();
  if (node->source() == interpreter::Register::FromParameterIndex(0) &&
      is_sloppy(unit->shared_function_info().language_mode())) {
    DCHECK(unit->shared_function_info().IsUserJavaScript());
    NodeInfo* node_info =
        known_node_aspects().GetOrCreateInfoFor(broker(), node);
    node_info->IntersectType(NodeType::kJSReceiver);
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadTaggedField(
    LoadTaggedField* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  if (node->offset() == offsetof(HeapObject, map_)) {
    if (auto constant =
            reducer_.TryGetConstant<HeapObject>(node->input_node(0))) {
      compiler::MapRef map = constant->map(broker());
      if (map.is_stable()) {
        broker()->dependencies()->DependOnStableMap(map);
        return ReplaceWith(reducer_.GetConstant(map));
      }
    }
  }
  if (node->offset() == offsetof(JSFunction, feedback_cell_)) {
    if (auto input = node->input_node(0)->TryCast<FastCreateClosure>()) {
      return ReplaceWith(reducer_.GetConstant(input->feedback_cell()));
    }
    if (auto input = node->input_node(0)->TryCast<CreateClosure>()) {
      return ReplaceWith(reducer_.GetConstant(input->feedback_cell()));
    }
  }
  if (node->offset() == offsetof(JSFunction, context_)) {
    if (auto input = node->input_node(0)->TryCast<FastCreateClosure>()) {
      return ReplaceWith(input->ContextInput().node());
    }
    if (auto input = node->input_node(0)->TryCast<CreateClosure>()) {
      return ReplaceWith(input->ContextInput().node());
    }
  }
  if (!node->property_key().is_none()) {
    REPLACE_AND_RETURN_IF_DONE(known_node_aspects().TryFindLoadedProperty(
        node->ValueInput().node(), node->property_key(), node->is_const()));
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadContextSlotNoCells(
    LoadContextSlotNoCells* node, const ProcessingState& state) {
  return ProcessLoadContextSlot(node);
}

ProcessResult MaglevGraphOptimizer::VisitLoadContextSlot(
    LoadContextSlot* node, const ProcessingState& state) {
  return ProcessLoadContextSlot(node);
}

ProcessResult MaglevGraphOptimizer::VisitLoadFloat64(
    LoadFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadInt32(
    LoadInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadTaggedFieldByFieldIndex(
    LoadTaggedFieldByFieldIndex* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadFixedArrayElement(
    LoadFixedArrayElement* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetInt32Constant(node->IndexInput().node())) {
    REPLACE_AND_RETURN_IF_DONE(
        reducer_.TryBuildLoadFixedArrayElementConstantIndex(
            node->ElementsInput().node(), cst.value(), node->load_type()));
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadFixedDoubleArrayElement(
    LoadFixedDoubleArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadHoleyFixedDoubleArrayElement(
    LoadHoleyFixedDoubleArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitLoadHoleyFixedDoubleArrayElementCheckedNotHole(
    LoadHoleyFixedDoubleArrayElementCheckedNotHole* node,
    const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadSignedIntDataViewElement(
    LoadSignedIntDataViewElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadDoubleDataViewElement(
    LoadDoubleDataViewElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadTypedArrayLength(
    LoadTypedArrayLength* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadDataViewByteLength(
    LoadDataViewByteLength* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(known_node_aspects().TryFindLoadedConstantProperty(
      node->ValueInput().node(), PropertyKey::ArrayBufferViewByteLength()));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadDataViewDataPointer(
    LoadDataViewDataPointer* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(known_node_aspects().TryFindLoadedConstantProperty(
      node->ValueInput().node(), PropertyKey::ArrayBufferViewDataPointer()));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadSignedIntTypedArrayElement(
    LoadSignedIntTypedArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadUnsignedIntTypedArrayElement(
    LoadUnsignedIntTypedArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadDoubleTypedArrayElement(
    LoadDoubleTypedArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadSignedIntConstantTypedArrayElement(
    LoadSignedIntConstantTypedArrayElement* node,
    const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitLoadUnsignedIntConstantTypedArrayElement(
    LoadUnsignedIntConstantTypedArrayElement* node,
    const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadDoubleConstantTypedArrayElement(
    LoadDoubleConstantTypedArrayElement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadEnumCacheLength(
    LoadEnumCacheLength* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadGlobal(
    LoadGlobal* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadNamedGeneric(
    LoadNamedGeneric* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadDictionaryField(
    LoadDictionaryField* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitLoadNamedFromSuperGeneric(
    LoadNamedFromSuperGeneric* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitMaybeGrowFastElements(
    MaybeGrowFastElements* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitMigrateMapIfNeeded(
    MigrateMapIfNeeded* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSetNamedGeneric(
    SetNamedGeneric* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitDefineNamedOwnGeneric(
    DefineNamedOwnGeneric* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreInArrayLiteralGeneric(
    StoreInArrayLiteralGeneric* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStoreGlobal(
    StoreGlobal* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGetKeyedGeneric(
    GetKeyedGeneric* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSetKeyedGeneric(
    SetKeyedGeneric* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitDefineKeyedOwnGeneric(
    DefineKeyedOwnGeneric* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitPhi(Phi* node,
                                             const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitRegisterInput(
    RegisterInput* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiSizedInt32(
    CheckedSmiSizedInt32* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetInt32Constant(node->input_node(0))) {
    if (Smi::IsValid(cst.value())) {
      return ReplaceWith(reducer_.GetInt32Constant(cst.value()));
    }
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiTagInt32(
    CheckedSmiTagInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  if (auto range = GetRange(node->input_node(0))) {
    if (Range::Smi().contains(*range)) {
      return ReplaceWith<UnsafeSmiTagInt32>({node->input_node(0)});
    }
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiTagUint32(
    CheckedSmiTagUint32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiTagIntPtr(
    CheckedSmiTagIntPtr* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeSmiTagInt32(
    UnsafeSmiTagInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeSmiTagFloat64(
    UnsafeSmiTagFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeSmiTagHoleyFloat64(
    UnsafeSmiTagHoleyFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeSmiTagUint32(
    UnsafeSmiTagUint32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeSmiTagIntPtr(
    UnsafeSmiTagIntPtr* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedInternalizedString(
    CheckedInternalizedString* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedObjectToIndex(
    CheckedObjectToIndex* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(GetUntaggedValueWithRepresentation(
      node->input_node(0), UseRepresentation::kInt32, {}));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedInt32ToUint32(
    CheckedInt32ToUint32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeInt32ToUint32(
    UnsafeInt32ToUint32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedUint32ToInt32(
    CheckedUint32ToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedIntPtrToInt32(
    CheckedIntPtrToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedIntPtrToUint32(
    CheckedIntPtrToUint32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitChangeInt32ToFloat64(
    ChangeInt32ToFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitChangeInt32ToHoleyFloat64(
    ChangeInt32ToHoleyFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitChangeUint32ToFloat64(
    ChangeUint32ToFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitChangeUint32ToHoleyFloat64(
    ChangeUint32ToHoleyFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitChangeIntPtrToFloat64(
    ChangeIntPtrToFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedFloat64ToInt32(
    CheckedFloat64ToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedFloat64ToUint32(
    CheckedFloat64ToUint32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedHoleyFloat64ToInt32(
    CheckedHoleyFloat64ToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedHoleyFloat64ToUint32(
    CheckedHoleyFloat64ToUint32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedFloat64ToSmiSizedInt32(
    CheckedFloat64ToSmiSizedInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedHoleyFloat64ToSmiSizedInt32(
    CheckedHoleyFloat64ToSmiSizedInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTruncateUint32ToInt32(
    TruncateUint32ToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTruncateFloat64ToInt32(
    TruncateFloat64ToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTruncateHoleyFloat64ToInt32(
    TruncateHoleyFloat64ToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeFloat64ToInt32(
    UnsafeFloat64ToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeHoleyFloat64ToInt32(
    UnsafeHoleyFloat64ToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ToUint8Clamped(
    Int32ToUint8Clamped* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUint32ToUint8Clamped(
    Uint32ToUint8Clamped* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64ToUint8Clamped(
    Float64ToUint8Clamped* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedNumberOrOddballToUint8Clamped(
    CheckedNumberOrOddballToUint8Clamped* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

template <ValueRepresentation kRepresentation>
void MaglevGraphOptimizer::RegisterUntaggedAlternative(ValueNode* tagged) {
  ValueNode* input = tagged->input_node(0);
  DCHECK_EQ(input->value_representation(), kRepresentation);
  auto& alternative =
      known_node_aspects().GetOrCreateInfoFor(broker(), tagged)->alternative();
  switch (kRepresentation) {
    case ValueRepresentation::kInt32:
      if (!alternative.int32()) alternative.set_int32(input);
      break;
    case ValueRepresentation::kFloat64:
      if (!alternative.float64()) alternative.set_float64(input);
      break;
    case ValueRepresentation::kHoleyFloat64:
      if (!alternative.holey_float64()) alternative.set_holey_float64(input);
      break;
    case ValueRepresentation::kTagged:
    case ValueRepresentation::kUint32:
    case ValueRepresentation::kIntPtr:
    case ValueRepresentation::kRawPtr:
    case ValueRepresentation::kNone:
      UNREACHABLE();
  }
}

ProcessResult MaglevGraphOptimizer::VisitInt32ToNumber(
    Int32ToNumber* node, const ProcessingState& state) {
  if (node->conversion_mode() != NumberConversionMode::kForceHeapNumber) {
    REPLACE_AND_RETURN_IF_DONE(
        TrySmiTag<UnsafeSmiTagInt32>(node->ValueInput()));
  }
  RegisterUntaggedAlternative<ValueRepresentation::kInt32>(node);
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUint32ToNumber(
    Uint32ToNumber* node, const ProcessingState& state) {
  // No untagged alternative slot for uint32, so nothing to register.
  REPLACE_AND_RETURN_IF_DONE(TrySmiTag<UnsafeSmiTagUint32>(node->ValueInput()));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitIntPtrToNumber(
    IntPtrToNumber* node, const ProcessingState& state) {
  // No untagged alternative slot for intptr, so nothing to register.
  REPLACE_AND_RETURN_IF_DONE(TrySmiTag<UnsafeSmiTagIntPtr>(node->ValueInput()));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32CountLeadingZeros(
    Int32CountLeadingZeros* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(
      reducer_.TryFoldInt32CountLeadingZeros(node->input_node(0)));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTaggedCountLeadingZeros(
    TaggedCountLeadingZeros* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64CountLeadingZeros(
    Float64CountLeadingZeros* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(
      reducer_.TryFoldFloat64CountLeadingZeros(node->input_node(0)));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitIntPtrToBoolean(
    IntPtrToBoolean* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64ToTagged(
    Float64ToTagged* node, const ProcessingState& state) {
  RegisterUntaggedAlternative<ValueRepresentation::kFloat64>(node);
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitHoleyFloat64ToTagged(
    HoleyFloat64ToTagged* node, const ProcessingState& state) {
  RegisterUntaggedAlternative<ValueRepresentation::kHoleyFloat64>(node);
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiTagFloat64(
    CheckedSmiTagFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiTagHoleyFloat64(
    CheckedSmiTagHoleyFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedNumberOrOddballToHoleyFloat64(
    CheckedNumberOrOddballToHoleyFloat64* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(GetUntaggedValueWithRepresentation(
      node->input_node(0), UseRepresentation::kHoleyFloat64,
      node->conversion_type()));
  return ProcessResult::kContinue;
}

#define UNTAGGING_CASE(Node, Repr, ConvType)                         \
  ProcessResult MaglevGraphOptimizer::Visit##Node(                   \
      Node* node, const ProcessingState& state) {                    \
    REPLACE_AND_RETURN_IF_DONE(GetUntaggedValueWithRepresentation(   \
        node->input_node(0), UseRepresentation::k##Repr, ConvType)); \
    return ProcessResult::kContinue;                                 \
  }
UNTAGGING_CASE(UnsafeSmiUntag, Int32, {})
UNTAGGING_CASE(CheckedNumberToInt32, Int32, {})
UNTAGGING_CASE(TruncateCheckedNumberOrOddballToInt32, TruncatedInt32,
               node->conversion_type())
UNTAGGING_CASE(TruncateUnsafeNumberOrOddballToInt32, TruncatedInt32,
               node->conversion_type())
UNTAGGING_CASE(CheckedNumberOrOddballToFloat64, Float64,
               node->conversion_type())
UNTAGGING_CASE(UnsafeNumberOrOddballToFloat64, Float64, node->conversion_type())
UNTAGGING_CASE(UnsafeNumberToFloat64, Float64,
               TaggedToFloat64ConversionType::kOnlyNumber)
UNTAGGING_CASE(UnsafeNumberOrOddballToHoleyFloat64, HoleyFloat64,
               node->conversion_type())
#undef UNTAGGING_CASE

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiUntag(
    CheckedSmiUntag* node, const ProcessingState& state) {
  if (SmiConstant* constant = node->input_node(0)->TryCast<SmiConstant>()) {
    return ReplaceWith(reducer_.GetInt32Constant(constant->value().value()));
  }
  // TODO(b/496266449): The current optimization is unsound, since the input of
  // this node could flow to a StoreTaggedFieldNoWriteBarrier, which expects a
  // Smi, not a Smi-sized number, which could be a HeapNumber.
  // Re-enable this optimization once we add a Smi value representation.

  // MaybeReduceResult maybe_input = GetUntaggedValueWithRepresentation(
  //     node->input_node(0), UseRepresentation::kInt32, {});
  // if (maybe_input.IsDoneWithValue()) {
  //   ValueNode* input = maybe_input.value();
  //   if (SmiValuesAre31Bits()) {
  //     // When the graph builder introduced the CheckedSmiUntag, it also
  //     recorded
  //     // in the alternatives that its input was a known Smi from this point
  //     on.
  //     // This information could have been later used to avoid Smi checks when
  //     // using this input in contexts that require Smis (like storing the
  //     length
  //     // of an array for instance). We can thus bypass the CheckedSmiUntag,
  //     but
  //     // still need to keep a CheckSmi.
  //     // TODO(dmercadier): during graph building, record whether the
  //     "CheckSmi"
  //     // part of CheckSmiUntag is useful or not.
  //     ReduceResult result = reducer_.BuildCheckedSmiSizedInt32(input);
  //     CHECK(result.IsDone());
  //   }
  //   return ReplaceWith(input);
  // } else if (maybe_input.IsDoneWithAbort()) {
  //   return ProcessResult::kTruncateBlock;
  // }
  // DCHECK(maybe_input.IsFail());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedNumberToFloat64(
    CheckedNumberToFloat64* node, const ProcessingState& state) {
  MaybeReduceResult maybe_alt = GetUntaggedValueWithRepresentation(
      node->input_node(0), UseRepresentation::kFloat64,
      TaggedToFloat64ConversionType::kOnlyNumber);
  if (maybe_alt.IsDoneWithValue()) {
    ValueNode* alt = maybe_alt.value();
    if (alt->Is<CheckedNumberOrOddballToFloat64>() ||
        alt->Is<UnsafeNumberOrOddballToFloat64>()) {
      // The alternative check is weaker then the current node. We should not
      // elide it.
      // TODO(victorgomes): consider having a to_number alternative?
      return ProcessResult::kContinue;
    }
    return ReplaceWith(alt);
  } else if (maybe_alt.IsDoneWithAbort()) {
    return ProcessResult::kTruncateBlock;
  }
  DCHECK(maybe_alt.IsFail());
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedHoleyFloat64ToFloat64(
    CheckedHoleyFloat64ToFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeHoleyFloat64ToFloat64(
    UnsafeHoleyFloat64ToFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnsafeFloat64ToHoleyFloat64(
    UnsafeFloat64ToHoleyFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64ToSilencedFloat64(
    Float64ToSilencedFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitHoleyFloat64ToSilencedFloat64(
    HoleyFloat64ToSilencedFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitChangeFloat64ToHoleyFloat64(
    ChangeFloat64ToHoleyFloat64* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#ifdef V8_ENABLE_UNDEFINED_DOUBLE

ProcessResult MaglevGraphOptimizer::VisitHoleyFloat64ConvertHoleToUndefined(
    HoleyFloat64ConvertHoleToUndefined* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitHoleyFloat64IsUndefinedOrHole(
    HoleyFloat64IsUndefinedOrHole* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::
    VisitLoadHoleyFixedDoubleArrayElementCheckedNotUndefinedOrHole(
        LoadHoleyFixedDoubleArrayElementCheckedNotUndefinedOrHole* node,
        const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#else

ProcessResult MaglevGraphOptimizer::VisitHoleyFloat64IsHole(
    HoleyFloat64IsHole* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#endif  // V8_ENABLE_UNDEFINED_DOUBLE

ProcessResult MaglevGraphOptimizer::VisitLogicalNot(
    LogicalNot* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(reducer_.TryFoldLogicalNot(node->input_node(0)));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSetPendingMessage(
    SetPendingMessage* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStringAt(
    StringAt* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStringEqual(
    StringEqual* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStringLength(
    StringLength* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(
      reducer_.TryReduceStringLength(node->StringInput().node()));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStringIndexOf(
    StringIndexOf* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#ifdef V8_INTL_SUPPORT
ProcessResult MaglevGraphOptimizer::VisitStringLocaleCompareIntl(
    StringLocaleCompareIntl* node, const ProcessingState& state) {
  // Lowered to an inline ASCII fast path by Turboshaft (see
  // MachineLoweringReducer::REDUCE(StringLocaleCompareIntl)).
  return ProcessResult::kContinue;
}
#endif  // V8_INTL_SUPPORT

ProcessResult MaglevGraphOptimizer::VisitStringConcat(
    StringConcat* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSeqOneByteStringAt(
    SeqOneByteStringAt* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitConsStringMap(
    ConsStringMap* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUnwrapStringWrapper(
    UnwrapStringWrapper* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitToBoolean(
    ToBoolean* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(
      reducer_.TryFoldToBoolean<false>(node->input_node(0)));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitToBooleanLogicalNot(
    ToBooleanLogicalNot* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(
      reducer_.TryFoldToBoolean<true>(node->input_node(0)));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitAllocateElementsArray(
    AllocateElementsArray* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTaggedEqual(
    TaggedEqual* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTaggedNotEqual(
    TaggedNotEqual* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTestInstanceOf(
    TestInstanceOf* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(reducer_.TryBuildFastInstanceOfWithFeedback(
      node->ContextInput().node(), node->ObjectInput().node(),
      node->CallableInput().node(), node->feedback()));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTestUndetectable(
    TestUndetectable* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(
      reducer_.TryFoldTestUndetectable(node->input_node(0)));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTestTypeOf(
    TestTypeOf* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(
      reducer_.TryFoldTestTypeOf(node->input_node(0), node->literal()));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitToName(ToName* node,
                                                const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitToNumberOrNumeric(
    ToNumberOrNumeric* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitToObject(
    ToObject* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitToString(
    ToString* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTransitionElementsKind(
    TransitionElementsKind* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ToString(
    Int32ToString* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(
      reducer_.TryFoldNumberToString(node->input_node(0)));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64ToString(
    Float64ToString* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(
      reducer_.TryFoldNumberToString(node->input_node(0)));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSmiToString(
    SmiToString* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(
      reducer_.TryFoldNumberToString(node->input_node(0)));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitNumberToString(
    NumberToString* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(
      reducer_.TryFoldNumberToString(node->input_node(0)));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitUpdateJSArrayLength(
    UpdateJSArrayLength* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGetContinuationPreservedEmbedderData(
    GetContinuationPreservedEmbedderData* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#define CONSTANT_CASE(Node)                        \
  ProcessResult MaglevGraphOptimizer::Visit##Node( \
      Node* node, const ProcessingState& state) {  \
    return ProcessResult::kContinue;               \
  }
CONSTANT_VALUE_NODE_LIST(CONSTANT_CASE)
#undef CONSTANT_CASE

ProcessResult MaglevGraphOptimizer::VisitInt32AbsWithOverflow(
    Int32AbsWithOverflow* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32Increment(
    Int32Increment* node, const ProcessingState& state) {
  // TODO(victorgomes): TryFoldInt32Operation can emit a
  // Int32IncrementWithOverflow which needs an eager deopt point. We need to
  // propagate this information and we can add a non-deopting version of the
  // increment.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32Decrement(
    Int32Decrement* node, const ProcessingState& state) {
  // TODO(victorgomes): TryFoldInt32Operation can emit a
  // Int32DecrementWithOverflow which needs an eager deopt point. We need to
  // propagate this information and we can add a non-deopting version of the
  // increment.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32Add(
    Int32Add* node, const ProcessingState& state) {
  // TODO(victorgomes): TryFoldInt32Operation can emit a
  // Int32IncrementWithOverflow which needs an eager deopt point. We need to
  // propagate this information and we can add a non-deopting version of the
  // increment.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32Subtract(
    Int32Subtract* node, const ProcessingState& state) {
  // TODO(victorgomes): TryFoldInt32Operation can emit a
  // Int32DecrementWithOverflow which needs an eager deopt point. We need to
  // propagate this information and we can add a non-deopting version of the
  // increment.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32Multiply(
    Int32Multiply* node, const ProcessingState& state) {
  // TODO(victorgomes): TryFoldInt32Operation can emit a CheckInt32Condition
  // which needs an eager deopt point. We need to propagate this information.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32MultiplyOverflownBits(
    Int32MultiplyOverflownBits* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32AddWithOverflow(
    Int32AddWithOverflow* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kAdd>(node));
  if (auto lhs_range = GetRange(node->input_node(0))) {
    if (auto rhs_range = GetRange(node->input_node(1))) {
      if (Range::Add(*lhs_range, *rhs_range).IsInt32()) {
        return ReplaceWith<Int32Add>(
            {node->input_node(0), node->input_node(1)});
      }
    }
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32SubtractWithOverflow(
    Int32SubtractWithOverflow* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kSubtract>(node));
  if (auto lhs_range = GetRange(node->input_node(0))) {
    if (auto rhs_range = GetRange(node->input_node(1))) {
      if (Range::Sub(*lhs_range, *rhs_range).IsInt32()) {
        return ReplaceWith<Int32Subtract>(
            {node->input_node(0), node->input_node(1)});
      }
    }
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32MultiplyWithOverflow(
    Int32MultiplyWithOverflow* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kMultiply>(node));
  if (auto lhs_range = GetRange(node->input_node(0))) {
    if (auto rhs_range = GetRange(node->input_node(1))) {
      if (Range::Mul(*lhs_range, *rhs_range).IsInt32()) {
        bool lhs_can_be_zero = lhs_range->min() <= 0 && lhs_range->max() >= 0;
        bool rhs_can_be_zero = rhs_range->min() <= 0 && rhs_range->max() >= 0;
        bool lhs_can_be_neg = lhs_range->min() < 0;
        bool rhs_can_be_neg = rhs_range->min() < 0;
        bool can_be_neg_zero = (lhs_can_be_zero && rhs_can_be_neg) ||
                               (rhs_can_be_zero && lhs_can_be_neg);
        if (!can_be_neg_zero) {
          return ReplaceWith<Int32Multiply>(
              {node->input_node(0), node->input_node(1)});
        }
      }
    }
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32DivideWithOverflow(
    Int32DivideWithOverflow* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kDivide>(node));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ModulusWithOverflow(
    Int32ModulusWithOverflow* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kModulus>(node));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32BitwiseAnd(
    Int32BitwiseAnd* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kBitwiseAnd>(node));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32BitwiseOr(
    Int32BitwiseOr* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kBitwiseOr>(node));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32BitwiseXor(
    Int32BitwiseXor* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kBitwiseXor>(node));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ShiftLeft(
    Int32ShiftLeft* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kShiftLeft>(node));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ShiftRight(
    Int32ShiftRight* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kShiftRight>(node));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ShiftRightLogical(
    Int32ShiftRightLogical* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kShiftRightLogical>(node));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32BitwiseNot(
    Int32BitwiseNot* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kBitwiseNot>(node));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32NegateWithOverflow(
    Int32NegateWithOverflow* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kNegate>(node));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32IncrementWithOverflow(
    Int32IncrementWithOverflow* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kIncrement>(node));
  if (auto range = GetRange(node->input_node(0))) {
    // TODO(victorgomes): We should actually DCHECK(IsInt32) here, but the range
    // seems to sometimes be bigger than int32, I guess it is because we are
    // missing some refinement in the range analysis.
    if (range->IsInt32() && *range->max() != INT32_MAX) {
      return ReplaceWith<Int32Increment>({node->input_node(0)});
    }
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32DecrementWithOverflow(
    Int32DecrementWithOverflow* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldInt32Operation<Operation::kDecrement>(node));
  if (auto range = GetRange(node->input_node(0))) {
    // TODO(victorgomes): We should actually DCHECK(IsInt32) here, but the range
    // seems to sometimes be bigger than int32, I guess it is because we are
    // missing some refinement in the range analysis.
    if (range->IsInt32() && *range->min() != INT32_MIN) {
      return ReplaceWith<Int32Decrement>({node->input_node(0)});
    }
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32Compare(
    Int32Compare* node, const ProcessingState& state) {
  if (auto result = reducer_.TryFoldInt32CompareOperation(
          node->operation(), node->input_node(0), node->input_node(1))) {
    return ReplaceWith(reducer_.GetBooleanConstant(result.value()));
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitInt32ToBoolean(
    Int32ToBoolean* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetInt32Constant(node->input_node(0))) {
    bool value = cst.value() != 0;
    return ReplaceWith(
        reducer_.GetBooleanConstant(node->flip() ? !value : value));
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Abs(
    Float64Abs* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kFloat64, node->input_node(0),
          TaggedToFloat64ConversionType::kNumberOrOddball)) {
    return ReplaceWith(
        reducer_.GetFloat64Constant(std::abs(cst.value().get_scalar())));
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Add(
    Float64Add* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldFloat64Operation<Operation::kAdd>(node));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Subtract(
    Float64Subtract* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldFloat64Operation<Operation::kSubtract>(node));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Multiply(
    Float64Multiply* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldFloat64Operation<Operation::kMultiply>(node));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Divide(
    Float64Divide* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldFloat64Operation<Operation::kDivide>(node));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Exponentiate(
    Float64Exponentiate* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldFloat64Operation<Operation::kExponentiate>(node));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Modulus(
    Float64Modulus* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldFloat64Operation<Operation::kModulus>(node));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Negate(
    Float64Negate* node, const ProcessingState& state) {
  RETURN_IF_SUCCESS(TryFoldFloat64Operation<Operation::kNegate>(node));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64RoundToFloat32(
    Float64RoundToFloat32* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kFloat64, node->ValueInput().node(),
          TaggedToFloat64ConversionType::kNumberOrOddball)) {
    float value = static_cast<float>(cst.value().get_scalar());
    return ReplaceWith(reducer_.GetFloat64Constant(static_cast<double>(value)));
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Round(
    Float64Round* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kFloat64, node->input_node(0),
          TaggedToFloat64ConversionType::kNumberOrOddball)) {
    double value = cst.value().get_scalar();
    switch (node->kind()) {
      case Float64Round::Kind::kFloor:
        value = std::floor(value);
        break;
      case Float64Round::Kind::kCeil:
        value = std::ceil(value);
        break;
      case Float64Round::Kind::kTrunc:
        value = std::trunc(value);
        break;
      case Float64Round::Kind::kNearest:
        return ProcessResult::kContinue;
    }
    return ReplaceWith(reducer_.GetFloat64Constant(value));
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Compare(
    Float64Compare* node, const ProcessingState& state) {
  if (auto result = reducer_.TryFoldFloat64CompareOperation(
          node->operation(), node->input_node(0), node->input_node(1))) {
    return ReplaceWith(reducer_.GetBooleanConstant(result.value()));
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64ToBoolean(
    Float64ToBoolean* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kFloat64, node->input_node(0),
          TaggedToFloat64ConversionType::kNumberOrOddball)) {
    double value = cst.value().get_scalar();
    bool boolean_value = value != 0.0 && !std::isnan(value);
    return ReplaceWith(reducer_.GetBooleanConstant(
        node->flip() ? !boolean_value : boolean_value));
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Min(
    Float64Min* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(reducer_.TryFoldFloat64Min(
      node->LeftInput().node(), node->RightInput().node()));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Max(
    Float64Max* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(reducer_.TryFoldFloat64Max(
      node->LeftInput().node(), node->RightInput().node()));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Ieee754Unary(
    Float64Ieee754Unary* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(reducer_.TryFoldFloat64Ieee754Unary(
      node->ieee_function(), node->input_node(0)));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Ieee754Binary(
    Float64Ieee754Binary* node, const ProcessingState& state) {
  REPLACE_AND_RETURN_IF_DONE(reducer_.TryFoldFloat64Ieee754Binary(
      node->ieee_function(), node->input_node(0), node->input_node(1)));
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitFloat64Sqrt(
    Float64Sqrt* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kFloat64, node->input_node(0),
          TaggedToFloat64ConversionType::kNumberOrOddball)) {
    return ReplaceWith(
        reducer_.GetFloat64Constant(std::sqrt(cst.value().get_scalar())));
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiIncrement(
    CheckedSmiIncrement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckedSmiDecrement(
    CheckedSmiDecrement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericAdd(
    GenericAdd* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBigIntBinaryOperation(
    BigIntBinaryOperation* node, const ProcessingState& state) {
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBigIntCompare(
    BigIntCompare* node, const ProcessingState& state) {
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBigIntNegate(
    BigIntNegate* node, const ProcessingState& state) {
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericSubtract(
    GenericSubtract* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericMultiply(
    GenericMultiply* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericDivide(
    GenericDivide* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericModulus(
    GenericModulus* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericExponentiate(
    GenericExponentiate* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericBitwiseAnd(
    GenericBitwiseAnd* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericBitwiseOr(
    GenericBitwiseOr* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericBitwiseXor(
    GenericBitwiseXor* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericShiftLeft(
    GenericShiftLeft* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericShiftRight(
    GenericShiftRight* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericShiftRightLogical(
    GenericShiftRightLogical* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericBitwiseNot(
    GenericBitwiseNot* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericNegate(
    GenericNegate* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericIncrement(
    GenericIncrement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericDecrement(
    GenericDecrement* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericEqual(
    GenericEqual* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericStrictEqual(
    GenericStrictEqual* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericLessThan(
    GenericLessThan* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericLessThanOrEqual(
    GenericLessThanOrEqual* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericGreaterThan(
    GenericGreaterThan* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitGenericGreaterThanOrEqual(
    GenericGreaterThanOrEqual* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBuiltinStringFromCharCode(
    BuiltinStringFromCharCode* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult
MaglevGraphOptimizer::VisitBuiltinStringPrototypeCharCodeOrCodePointAt(
    BuiltinStringPrototypeCharCodeOrCodePointAt* node,
    const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBuiltinSeqOneByteStringCharCodeAt(
    BuiltinSeqOneByteStringCharCodeAt* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCreateFastArrayElements(
    CreateFastArrayElements* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitNewConsString(
    NewConsString* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitMapPrototypeGet(
    MapPrototypeGet* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitMapPrototypeGetInt32Key(
    MapPrototypeGetInt32Key* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSetPrototypeHas(
    SetPrototypeHas* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitWeakMapPrototypeGet(
    WeakMapPrototypeGet* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStringSlice(
    StringSlice* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitStringSubstring(
    StringSubstring* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitObjectIsArray(
    ObjectIsArray* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitProcessWasmArgument(
    ProcessWasmArgument*, const ProcessingState&) {
  // Identity node used to carry an eager deopt frame state for JS-to-Wasm
  // wrapper inlining (crbug.com/493307329). No optimization needed.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitAbort(Abort* node,
                                               const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTrap(Trap* node,
                                              const ProcessingState& state) {
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitReturn(Return* node,
                                                const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitDeopt(Deopt* node,
                                               const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitThrow(Throw* node,
                                               const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitSwitch(Switch* node,
                                                const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfSmi(
    BranchIfSmi* node, const ProcessingState& state) {
  ValueNode* input = node->input_node(0);
  if (input->Is<SmiConstant>()) {
    FoldBranch(state.block(), node, true);
    return ProcessResult::kRevisit;
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfRootConstant(
    BranchIfRootConstant* node, const ProcessingState& state) {
  ValueNode* input = node->input_node(0);
  if (input->Is<RootConstant>()) {
    bool match = input->Cast<RootConstant>()->index() == node->root_index();
    FoldBranch(state.block(), node, match);
    return ProcessResult::kRevisit;
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfToBooleanTrue(
    BranchIfToBooleanTrue* node, const ProcessingState& state) {
  if (IsConstantNode(node->input_node(0)->opcode())) {
    // Holes shouldn't flow up to here: there should be either a ThrowXXXIfHole
    // or a CheckNotHole before, which should have been constant-folded into
    // unconditional deopt/throw if their input is a hole.
    DCHECK_IMPLIES(node->input_node(0)->Is<HeapConstant>(),
                   !node->input_node(0)->Cast<HeapConstant>()->IsTheHole());

    bool condition =
        FromConstantToBool(reducer_.local_isolate(), node->input_node(0));
    FoldBranch(state.block(), node, condition);
    return ProcessResult::kRevisit;
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfInt32ToBooleanTrue(
    BranchIfInt32ToBooleanTrue* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetInt32Constant(node->ConditionInput().node())) {
    bool condition_value = cst.value() != 0;
    FoldBranch(state.block(), node, condition_value);
    return ProcessResult::kRevisit;
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfIntPtrToBooleanTrue(
    BranchIfIntPtrToBooleanTrue* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetIntPtrConstant(node->input_node(0))) {
    FoldBranch(state.block(), node, cst.value() != 0);
    return ProcessResult::kRevisit;
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfFloat64ToBooleanTrue(
    BranchIfFloat64ToBooleanTrue* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kFloat64, node->ConditionInput().node(),
          TaggedToFloat64ConversionType::kNumberOrOddball)) {
    double value = cst.value().get_scalar();
    bool condition_value = value != 0.0 && !std::isnan(value);
    FoldBranch(state.block(), node, condition_value);
    return ProcessResult::kRevisit;
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfHoleyFloat64ToBooleanTrue(
    BranchIfHoleyFloat64ToBooleanTrue* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kHoleyFloat64, node->ConditionInput().node(),
          TaggedToFloat64ConversionType::kNumberOrOddball)) {
    bool result;
    if (cst.value().is_undefined_or_hole_nan()) {
      result = false;
    } else {
      double value = cst.value().get_scalar();
      result = (value != 0.0 && !std::isnan(value));
    }
    FoldBranch(state.block(), node, result);
    return ProcessResult::kRevisit;
  }
  return ProcessResult::kContinue;
}

#ifdef V8_ENABLE_UNDEFINED_DOUBLE

ProcessResult MaglevGraphOptimizer::VisitBranchIfFloat64IsUndefinedOrHole(
    BranchIfFloat64IsUndefinedOrHole* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kHoleyFloat64, node->input_node(0),
          TaggedToFloat64ConversionType::kNumberOrOddball)) {
    FoldBranch(state.block(), node, cst.value().is_undefined_or_hole_nan());
    return ProcessResult::kRevisit;
  }
  return ProcessResult::kContinue;
}

#endif

ProcessResult MaglevGraphOptimizer::VisitBranchIfFloat64IsHole(
    BranchIfFloat64IsHole* node, const ProcessingState& state) {
  if (auto cst = reducer_.TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kHoleyFloat64, node->input_node(0),
          TaggedToFloat64ConversionType::kNumberOrOddball)) {
    FoldBranch(state.block(), node, cst.value().is_undefined_or_hole_nan());
    return ProcessResult::kRevisit;
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfReferenceEqual(
    BranchIfReferenceEqual* node, const ProcessingState& state) {
  ValueNode* left = node->input_node(0);
  ValueNode* right = node->input_node(1);
  if (left == right) {
    FoldBranch(state.block(), node, true);
    return ProcessResult::kRevisit;
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfTypedArrayBounds(
    BranchIfTypedArrayBounds* node, const ProcessingState& state) {
  // TODO(mrcvtl): We could try to fold this if index and length are constant.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfInt32Compare(
    BranchIfInt32Compare* node, const ProcessingState& state) {
  if (auto result = reducer_.TryFoldInt32CompareOperation(
          node->operation(), node->input_node(0), node->input_node(1))) {
    FoldBranch(state.block(), node, result.value());
    return ProcessResult::kRevisit;
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfUint32Compare(
    BranchIfUint32Compare* node, const ProcessingState& state) {
  if (auto result = reducer_.TryFoldUint32CompareOperation(
          node->operation(), node->input_node(0), node->input_node(1))) {
    FoldBranch(state.block(), node, result.value());
    return ProcessResult::kRevisit;
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfFloat64Compare(
    BranchIfFloat64Compare* node, const ProcessingState& state) {
  if (auto result = reducer_.TryFoldFloat64CompareOperation(
          node->operation(), node->input_node(0), node->input_node(1))) {
    FoldBranch(state.block(), node, result.value());
    return ProcessResult::kRevisit;
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfUndefinedOrNull(
    BranchIfUndefinedOrNull* node, const ProcessingState& state) {
  ValueNode* input = node->input_node(0);
  if (auto cst = reducer_.TryGetConstant<HeapObject>(input)) {
    bool match = cst->IsUndefined() || cst->IsNull();
    FoldBranch(state.block(), node, match);
    return ProcessResult::kRevisit;
  }
  if (input->Is<RootConstant>()) {
    RootIndex index = input->Cast<RootConstant>()->index();
    bool match =
        (index == RootIndex::kUndefinedValue || index == RootIndex::kNullValue);
    FoldBranch(state.block(), node, match);
    return ProcessResult::kRevisit;
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfUndetectable(
    BranchIfUndetectable* node, const ProcessingState& state) {
  ValueNode* input = node->input_node(0);
  if (auto cst = reducer_.TryGetConstant<HeapObject>(input)) {
    bool match = cst->map(broker()).is_undetectable();
    FoldBranch(state.block(), node, match);
    return ProcessResult::kRevisit;
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitBranchIfJSReceiver(
    BranchIfJSReceiver* node, const ProcessingState& state) {
  ValueNode* input = node->input_node(0);
  if (auto cst = reducer_.TryGetConstant<HeapObject>(input)) {
    bool match = cst->map(broker()).IsJSReceiverMap();
    FoldBranch(state.block(), node, match);
    return ProcessResult::kRevisit;
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitJump(Jump* node,
                                              const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitCheckpointedJump(
    CheckpointedJump* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  return ProcessResult::kContinue;
}

#define UNIMPLEMENTED_NODE(name)                                            \
  ProcessResult MaglevGraphOptimizer::Visit##name(name*,                    \
                                                  const ProcessingState&) { \
    return ProcessResult::kContinue;                                        \
  }
UNIMPLEMENTED_NODE(AssertRangeInt32)
UNIMPLEMENTED_NODE(AssertRangeFloat64)
#undef UNIMPLEMENTED_NODE

ProcessResult MaglevGraphOptimizer::VisitFloat64SpeculateSafeAdd(
    Float64SpeculateSafeAdd* node, const ProcessingState& state) {
  // Don't do anything.
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitTruncateFloat64AsSafeIntToInt32(
    TruncateFloat64AsSafeIntToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  if (node->input_node(0)->Is<ChangeInt32ToFloat64>()) {
    return ReplaceWith(node->input_node(0)->input_node(0));
  }
  return ProcessResult::kContinue;
}

// TODO(victorgomes): Use UNTAGGING_CASE and investigating why Int32ToNumber as
// input as not been unwrapped.
ProcessResult MaglevGraphOptimizer::VisitTruncateCheckedNumberAsSafeIntToInt32(
    TruncateCheckedNumberAsSafeIntToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  if (node->input_node(0)->Is<Int32ToNumber>()) {
    return ReplaceWith(node->input_node(0)->input_node(0));
  }
  return ProcessResult::kContinue;
}

// TODO(victorgomes): Use UNTAGGING_CASE and investigating why Int32ToNumber as
// input as not been unwrapped.
ProcessResult MaglevGraphOptimizer::VisitTruncateUnsafeNumberAsSafeIntToInt32(
    TruncateUnsafeNumberAsSafeIntToInt32* node, const ProcessingState& state) {
  // TODO(b/424157317): Optimize.
  if (node->input_node(0)->Is<Int32ToNumber>()) {
    return ReplaceWith(node->input_node(0)->input_node(0));
  }
  return ProcessResult::kContinue;
}

ProcessResult MaglevGraphOptimizer::VisitJumpLoop(
    JumpLoop* node, const ProcessingState& state) {
  // We need to unwrap backedges of loop phis (since it's possible that they
  // weren't identities when the loop header was visited initially, but they
  // later became Identities while visiting the loop's body).
  BasicBlock* loop_header = node->target();
  if (loop_header->has_phi()) {
    for (Phi* phi : *loop_header->phis()) {
      for (int i = 0; i < phi->input_count(); i++) {
        ValueNode* input = phi->input(i).node();
        if (!input) continue;
        phi->change_input(i, input->UnwrapIdentities());
      }
    }
  }

  loop_depth_--;
  return ProcessResult::kContinue;
}

// Nodes never emitted before Graph optimizer.
#define UNREACHABLE_NODES(X) \
  X(ConstantGapMove)         \
  X(GapMove)                 \
  X(Int32Divide)             \
  X(DeadValue)               \
  X(VirtualObject)

#define UNREACHEABLE_VISITOR(Node)                                          \
  ProcessResult MaglevGraphOptimizer::Visit##Node(Node* node,               \
                                                  const ProcessingState&) { \
    UNREACHABLE();                                                          \
  }

UNREACHABLE_NODES(UNREACHEABLE_VISITOR)
#undef UNREACHABLE_VISITOR
#undef UNREACHABLE_NODES

#undef REPLACE_AND_RETURN_IF_DONE
#undef RETURN_IF_SUCCESS
#undef TRACE

}  // namespace maglev
}  // namespace internal
}  // namespace v8
