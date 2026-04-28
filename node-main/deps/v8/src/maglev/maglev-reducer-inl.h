// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_REDUCER_INL_H_
#define V8_MAGLEV_MAGLEV_REDUCER_INL_H_

#include "src/maglev/maglev-reducer.h"
// Include the non-inl header before the rest of the headers.

#include "src/base/bits.h"
#include "src/base/division-by-constant.h"
#include "src/common/scoped-modification.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/numbers/conversions.h"
#include "src/numbers/ieee754.h"
#include "src/objects/heap-number-inl.h"

namespace v8 {
namespace internal {
namespace maglev {

// Some helpers for CSE
namespace cse {
static inline size_t fast_hash_combine(size_t seed, size_t h) {
  // Implementation from boost. Good enough for GVN.
  return h + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename T>
static inline size_t gvn_hash_value(const T& in) {
  return base::hash_value(in);
}

static inline size_t gvn_hash_value(const compiler::MapRef& map) {
  return map.hash_value();
}

static inline size_t gvn_hash_value(const interpreter::Register& reg) {
  return base::hash_value(reg.index());
}

static inline size_t gvn_hash_value(const Representation& rep) {
  return base::hash_value(rep.kind());
}

static inline size_t gvn_hash_value(const ExternalReference& ref) {
  return base::hash_value(ref.address());
}

static inline size_t gvn_hash_value(const PolymorphicAccessInfo& access_info) {
  return access_info.hash_value();
}

template <typename T>
static inline size_t gvn_hash_value(
    const v8::internal::ZoneCompactSet<T>& vector) {
  size_t hash = base::hash_value(vector.size());
  for (auto e : vector) {
    hash = fast_hash_combine(hash, gvn_hash_value(e));
  }
  return hash;
}

template <typename T>
static inline size_t gvn_hash_value(const v8::internal::ZoneVector<T>& vector) {
  size_t hash = base::hash_value(vector.size());
  for (auto e : vector) {
    hash = fast_hash_combine(hash, gvn_hash_value(e));
  }
  return hash;
}

template <typename... Args>
static inline size_t fast_hash_combine(size_t seed, Args&&... args) {
  size_t hash = seed;
  ([&] { hash = cse::fast_hash_combine(hash, cse::gvn_hash_value(args)); }(),
   ...);
  return hash;
}

template <size_t kInputCount, typename... Args>
static inline size_t fast_hash_combine(
    size_t seed, std::array<ValueNode*, kInputCount>& inputs) {
  size_t hash = seed;
  for (const auto& inp : inputs) {
    hash = cse::fast_hash_combine(hash, base::hash_value(inp));
  }
  return hash;
}
}  // namespace cse

template <typename BaseT>
template <typename NodeT, typename Function, typename... Args>
ReduceResult MaglevReducer<BaseT>::AddNewNode(
    size_t input_count, Function&& post_create_input_initializer,
    Args&&... args) {
  NodeT* node =
      NodeBase::New<NodeT>(zone(), input_count, std::forward<Args>(args)...);
  RETURN_IF_ABORT(post_create_input_initializer(node));
  return AttachExtraInfoAndAddToGraph(node);
}

template <typename BaseT>
template <typename NodeT, typename... Args>
ReduceResult MaglevReducer<BaseT>::AddNewNode(
    std::initializer_list<ValueNode*> inputs, Args&&... args) {
  static_assert(IsFixedInputNode<NodeT>());
  if constexpr (Node::participate_in_cse(Node::opcode_of<NodeT>) &&
                ReducerBaseWithKNA<BaseT>) {
    if (v8_flags.maglev_cse) {
      return AddNewNodeOrGetEquivalent<NodeT>(true, inputs,
                                              std::forward<Args>(args)...);
    }
  }
  NodeT* node =
      NodeBase::New<NodeT>(zone(), inputs.size(), std::forward<Args>(args)...);
  RETURN_IF_ABORT(SetNodeInputs(node, inputs));
  return AttachExtraInfoAndAddToGraph(node);
}

template <typename BaseT>
template <typename NodeT, typename... Args>
NodeT* MaglevReducer<BaseT>::AddNewNodeNoAbort(
    std::initializer_list<ValueNode*> inputs, Args&&... args) {
  static_assert(IsFixedInputNode<NodeT>());
  if constexpr (Node::participate_in_cse(Node::opcode_of<NodeT>) &&
                ReducerBaseWithKNA<BaseT>) {
    if (v8_flags.maglev_cse) {
      return AddNewNodeOrGetEquivalent<NodeT>(true, inputs,
                                              std::forward<Args>(args)...);
    }
  }
  NodeT* node =
      NodeBase::New<NodeT>(zone(), inputs.size(), std::forward<Args>(args)...);
  SetNodeInputsOld(node, inputs);
  return AttachExtraInfoAndAddToGraph(node);
}

template <typename BaseT>
template <typename NodeT, typename... Args>
NodeT* MaglevReducer<BaseT>::AddNewNodeNoInputConversion(
    std::initializer_list<ValueNode*> inputs, Args&&... args) {
  static_assert(IsFixedInputNode<NodeT>());
  if constexpr (Node::participate_in_cse(Node::opcode_of<NodeT>) &&
                ReducerBaseWithKNA<BaseT>) {
    if (v8_flags.maglev_cse) {
      return AddNewNodeOrGetEquivalent<NodeT>(false, inputs,
                                              std::forward<Args>(args)...);
    }
  }
  NodeT* node =
      NodeBase::New<NodeT>(zone(), inputs.size(), std::forward<Args>(args)...);
  SetNodeInputsNoConversion(node, inputs);
  return AttachExtraInfoAndAddToGraph(node);
}

template <typename BaseT>
template <typename NodeT, typename... Args>
NodeT* MaglevReducer<BaseT>::AddUnbufferedNewNodeNoInputConversion(
    BasicBlock* block, std::initializer_list<ValueNode*> inputs,
    Args&&... args) {
  ScopedModification<BasicBlock*> save_block(&current_block_, block);
  DCHECK_EQ(add_new_node_mode_, AddNewNodeMode::kBuffered);
  add_new_node_mode_ = AddNewNodeMode::kUnbuffered;
  NodeT* node =
      AddNewNodeNoInputConversion<NodeT>(inputs, std::forward<Args>(args)...);
  add_new_node_mode_ = AddNewNodeMode::kBuffered;
  return node;
}

template <typename BaseT>
template <typename ControlNodeT, typename... Args>
void MaglevReducer<BaseT>::AddNewControlNode(
    std::initializer_list<ValueNode*> inputs, Args&&... args) {
  ControlNodeT* control_node = NodeBase::New<ControlNodeT>(
      zone(), inputs.size(), std::forward<Args>(args)...);
  SetNodeInputsOld(control_node, inputs);
  AttachEagerDeoptInfo(control_node);
  AttachDeoptCheckpoint(control_node);
  static_assert(!ControlNodeT::kProperties.can_lazy_deopt());
  static_assert(!ControlNodeT::kProperties.can_throw());
  static_assert(!ControlNodeT::kProperties.can_write());
  control_node->set_owner(current_block());
  current_block()->set_control_node(control_node);
  if (has_graph_labeller()) {
    RegisterNode(control_node);
    if (V8_UNLIKELY(v8_flags.trace_maglev_graph_building &&
                    is_tracing_enabled())) {
      bool kHasRegallocData = false;
      bool kSkipTargets = true;
      std::cout << "  " << control_node << "  " << PrintNodeLabel(control_node)
                << ": "
                << PrintNode(control_node, kHasRegallocData, kSkipTargets)
                << std::endl;
    }
  }
}

template <typename BaseT>
template <typename NodeT, typename... Args>
NodeT* MaglevReducer<BaseT>::AddNewNodeOrGetEquivalent(
    bool convert_inputs, std::initializer_list<ValueNode*> raw_inputs,
    Args&&... args) {
  DCHECK(v8_flags.maglev_cse);
  static constexpr Opcode op = Node::opcode_of<NodeT>;
  static_assert(Node::participate_in_cse(op));
  using options_result =
      std::invoke_result_t<decltype(&NodeT::options), const NodeT>;
  static_assert(std::is_assignable_v<options_result, std::tuple<Args...>>,
                "Instruction participating in CSE needs options() returning "
                "a tuple matching the constructor arguments");
  static_assert(IsFixedInputNode<NodeT>());
  static_assert(NodeT::kInputCount <= 3);

  std::array<ValueNode*, NodeT::kInputCount> inputs;
  // Nodes with zero input count don't have kInputTypes defined.
  if constexpr (NodeT::kInputCount > 0) {
    int i = 0;
    constexpr UseReprHintRecording hint = ShouldRecordUseReprHint<NodeT>();
    for (ValueNode* raw_input : raw_inputs) {
      if (convert_inputs) {
        // TODO(marja): Here we might already have the empty type for the
        // node. Generate a deopt and make callers handle it.
        inputs[i] = ConvertInputTo<hint>(raw_input, NodeT::kInputTypes[i]);
      } else {
        CHECK(ValueRepresentationIs(
            raw_input->properties().value_representation(),
            NodeT::kInputTypes[i]));
        inputs[i] = raw_input;
      }
      i++;
    }
    if constexpr (IsCommutativeNode(Node::opcode_of<NodeT>)) {
      static_assert(NodeT::kInputCount == 2);
      if ((IsConstantNode(inputs[0]->opcode()) || inputs[0] > inputs[1]) &&
          !IsConstantNode(inputs[1]->opcode())) {
        std::swap(inputs[0], inputs[1]);
      }
    }
  }

  uint32_t hash = static_cast<uint32_t>(cse::fast_hash_combine(
      cse::fast_hash_combine(base::hash_value(op), std::forward<Args>(args)...),
      inputs));
  NodeT* node = known_node_aspects().template FindExpression<NodeT>(
      hash, inputs, std::forward<Args>(args)...);
  if (node) return node;

  node =
      NodeBase::New<NodeT>(zone(), inputs.size(), std::forward<Args>(args)...);
  SetNodeInputsNoConversion(node, inputs);
  DCHECK_EQ(node->options(), std::tuple{std::forward<Args>(args)...});
  known_node_aspects().AddExpression(hash, node);
  return AttachExtraInfoAndAddToGraph(node);
}

template <typename BaseT>
void MaglevReducer<BaseT>::AddInitializedNodeToGraph(Node* node) {
  // VirtualObjects should never be add to the Maglev graph.
  DCHECK(!node->Is<VirtualObject>());
  if (current_block_position_.is_at_end()) {
    if (V8_UNLIKELY(add_new_node_mode_ == AddNewNodeMode::kUnbuffered)) {
      current_block_->nodes().push_back(node);
    } else {
      new_nodes_at_end_.push_back(node);
    }
  } else {
    DCHECK_EQ(add_new_node_mode_, AddNewNodeMode::kBuffered);
    new_nodes_at_.push_back(
        std::make_pair(current_block_position_.index(), node));
  }
  node->set_owner(current_block());
  if (V8_UNLIKELY(has_graph_labeller())) RegisterNode(node);
  if (V8_UNLIKELY(v8_flags.trace_maglev_graph_building &&
                  is_tracing_enabled())) {
    std::cout << "  " << node << "  " << PrintNodeLabel(node) << ": "
              << PrintNode(node) << std::endl;
  }
#ifdef DEBUG
  new_nodes_current_period_.insert(node);
#endif  // debug
}

template <typename BaseT>
template <UseReprHintRecording hint>
ValueNode* MaglevReducer<BaseT>::ConvertInputTo(ValueNode* input,
                                                ValueRepresentation expected) {
  ValueRepresentation repr = input->properties().value_representation();
  if (repr == expected) return input;
  // If the reducer base does not track KNA, it must convert its own input.
  if constexpr (ReducerBaseWithKNA<BaseT>) {
    switch (expected) {
      case ValueRepresentation::kTagged:
        return GetTaggedValue(input, hint);
      case ValueRepresentation::kInt32:
        return GetInt32(input);
      case ValueRepresentation::kFloat64:
      case ValueRepresentation::kHoleyFloat64:
        return GetFloat64(input);
      case ValueRepresentation::kUint32:
      case ValueRepresentation::kIntPtr:
      case ValueRepresentation::kNone:
        // These conversion should be explicitly done beforehand.
        UNREACHABLE();
    }
  } else {
    // If the input's value representation is not equal to the expected one,
    // it must be because of HoleyFloats.
    DCHECK(expected == ValueRepresentation::kHoleyFloat64 &&
           repr == ValueRepresentation::kFloat64);
    return input;
  }
}

template <typename BaseT>
template <typename NodeT, typename InputsT>
ReduceResult MaglevReducer<BaseT>::SetNodeInputs(NodeT* node, InputsT inputs) {
  // TODO(marja): Abort if input conversion fails.
  SetNodeInputsOld(node, inputs);
  return ReduceResult::Done();
}

template <typename BaseT>
template <typename NodeT, typename InputsT>
void MaglevReducer<BaseT>::SetNodeInputsOld(NodeT* node, InputsT inputs) {
  // Nodes with zero input count don't have kInputTypes defined.
  if constexpr (NodeT::kInputCount > 0) {
    constexpr UseReprHintRecording hint = ShouldRecordUseReprHint<NodeT>();
    int i = 0;
    for (ValueNode* input : inputs) {
      DCHECK_NOT_NULL(input);
      node->set_input(i, ConvertInputTo<hint>(input, NodeT::kInputTypes[i]));
      i++;
    }
  }
}

template <typename BaseT>
template <typename NodeT, typename InputsT>
void MaglevReducer<BaseT>::SetNodeInputsNoConversion(NodeT* node,
                                                     InputsT inputs) {
  // Nodes with zero input count don't have kInputTypes defined.
  if constexpr (NodeT::kInputCount > 0) {
    int i = 0;
    for (ValueNode* input : inputs) {
      DCHECK_NOT_NULL(input);
      CHECK(ValueRepresentationIs(input->properties().value_representation(),
                                  NodeT::kInputTypes[i]));
      node->set_input(i, input);
      i++;
    }
  }
}

template <typename BaseT>
template <typename NodeT>
NodeT* MaglevReducer<BaseT>::AttachExtraInfoAndAddToGraph(NodeT* node) {
  static_assert(NodeT::kProperties.is_deopt_checkpoint() +
                    NodeT::kProperties.can_eager_deopt() +
                    NodeT::kProperties.can_lazy_deopt() <=
                1);
  AttachDeoptCheckpoint(node);
  AttachEagerDeoptInfo(node);
  AttachLazyDeoptInfo(node);
  AttachExceptionHandlerInfo(node);
  AddInitializedNodeToGraph(node);
  MarkPossibleSideEffect(node);
  // MarkPossibleSideEffects coming from the base class.
  if constexpr (ReducerBaseWithEffectTracking<NodeT, BaseT>) {
    base_->template MarkPossibleSideEffect<NodeT>(node);
  }
  MarkForInt32Truncation(node);
  return node;
}

template <typename BaseT>
template <typename NodeT>
void MaglevReducer<BaseT>::AttachDeoptCheckpoint(NodeT* node) {
  if constexpr (NodeT::kProperties.is_deopt_checkpoint()) {
    static_assert(ReducerBaseWithEagerDeopt<BaseT>);
    node->SetEagerDeoptInfo(zone(), base_->GetDeoptFrameForEagerDeopt());
  }
}

template <typename BaseT>
template <typename NodeT>
void MaglevReducer<BaseT>::AttachEagerDeoptInfo(NodeT* node) {
  if constexpr (NodeT::kProperties.can_eager_deopt()) {
    static_assert(ReducerBaseWithEagerDeopt<BaseT>);
    DeoptFrame* top_frame = base_->GetDeoptFrameForEagerDeopt();
    graph_->AddEagerTopFrame(top_frame);
    node->SetEagerDeoptInfo(zone(), top_frame, current_speculation_feedback_);
  }
}

template <typename BaseT>
template <typename NodeT>
void MaglevReducer<BaseT>::AttachLazyDeoptInfo(NodeT* node) {
  if constexpr (NodeT::kProperties.can_lazy_deopt()) {
    static_assert(ReducerBaseWithLazyDeopt<BaseT>);
    auto [top_frame, result_location, result_size] =
        base_->GetDeoptFrameForLazyDeopt(NodeT::kProperties.can_throw());
    graph_->AddLazyTopFrame(top_frame, result_location, result_size);
    new (node->lazy_deopt_info())
        LazyDeoptInfo(zone(), top_frame, result_location, result_size,
                      current_speculation_feedback_);
  }
}

template <typename BaseT>
template <typename NodeT>
void MaglevReducer<BaseT>::AttachExceptionHandlerInfo(NodeT* node) {
  if constexpr (NodeT::kProperties.can_throw()) {
    static_assert(ReducerBaseWithLazyDeopt<BaseT>);
    base_->AttachExceptionHandlerInfo(node);
  }
}

template <typename BaseT>
template <typename NodeT>
void MaglevReducer<BaseT>::MarkPossibleSideEffect(NodeT* node) {
  if constexpr (CanTriggerTruncationPass(Node::opcode_of<NodeT>)) {
    graph()->set_may_have_truncation();
  }

  // Don't do anything for nodes without side effects.
  if constexpr (!NodeT::kProperties.can_write()) return;

  if constexpr (ReducerBaseWithKNA<BaseT>) {
    known_node_aspects().increment_effect_epoch();
  }
}

template <typename BaseT>
template <typename NodeT>
void MaglevReducer<BaseT>::MarkForInt32Truncation(NodeT* node) {
  if (!v8_flags.maglev_truncation) return;
  if constexpr (NodeT::kProperties.value_representation() ==
                ValueRepresentation::kFloat64) {
    UpdateRange(node);
  }
  if constexpr (NodeT::template opcode_of<NodeT> ==
                    Opcode::kInt32AddWithOverflow ||
                NodeT::template opcode_of<NodeT> ==
                    Opcode::kInt32SubtractWithOverflow) {
    node->set_can_truncate_to_int32(true);
  }
}

template <typename BaseT>
template <typename NodeT>
void MaglevReducer<BaseT>::UpdateRange(NodeT* node) {
  static_assert(NodeT::kProperties.value_representation() ==
                ValueRepresentation::kFloat64);
  if constexpr (HasRangeType(Node::opcode_of<NodeT>)) {
    RangeType r1 = node->input(0).node()->GetRange();
    RangeType r2 = node->input(1).node()->GetRange();
    if (!r1.is_valid() || !r2.is_valid()) return;
    RangeType result;
    switch (Node::opcode_of<NodeT>) {
      case Opcode::kFloat64Add:
        result =
            RangeType::Join([](double x, double y) { return x + y; }, r1, r2);
        break;
      case Opcode::kFloat64Subtract:
        result =
            RangeType::Join([](double x, double y) { return x - y; }, r1, r2);
        break;
      case Opcode::kFloat64Multiply:
        result =
            RangeType::Join([](double x, double y) { return x * y; }, r1, r2);
        break;
      case Opcode::kFloat64Divide:
        result = {};
        break;
      default:
        UNREACHABLE();
    }
    // TODO(victorgomes): Calculating the range of a LoopPhi might need a
    // fixpoint.
    static_assert(!std::is_same_v<NodeT, Phi>);
    node->set_range(result);
    if (node->range().IsSafeIntegerRange()) {
      node->set_can_truncate_to_int32(true);
    }
  }
}

template <typename BaseT>
std::optional<ValueNode*> MaglevReducer<BaseT>::TryGetConstantAlternative(
    ValueNode* node) {
  const NodeInfo* info = known_node_aspects().TryGetInfoFor(node);
  if (info) {
    if (auto c = info->alternative().checked_value()) {
      if (IsConstantNode(c->opcode())) {
        return c;
      }
    }
  }
  return {};
}

template <typename BaseT>
ValueNode* MaglevReducer<BaseT>::GetTaggedValue(
    ValueNode* value, UseReprHintRecording record_use_repr_hint) {
  if (V8_LIKELY(record_use_repr_hint == UseReprHintRecording::kRecord)) {
    value->MaybeRecordUseReprHint(UseRepresentation::kTagged);
  }

  // TODO(victorgomes): Change identity value representation to unknown. Or
  // modify OpProperties::value_representation to support Identity with multiple
  // representations.
  if (value->Is<Identity>()) {
    return GetTaggedValue(value->input(0).node(), record_use_repr_hint);
  }

  ValueRepresentation representation =
      value->properties().value_representation();
  if (representation == ValueRepresentation::kTagged) return value;

  if (auto as_int32_constant = TryGetInt32Constant(value)) {
    if (Smi::IsValid(*as_int32_constant)) {
      return graph()->GetSmiConstant(*as_int32_constant);
    }
  }
  if (auto as_float64_constant = TryGetFloat64Constant(
          value, TaggedToFloat64ConversionType::kOnlyNumber)) {
    if (std::isnan(*as_float64_constant)) {
      return graph()->GetRootConstant(RootIndex::kNanValue);
    }
    if (*as_float64_constant == kMaxUInt32) {
      return graph()->GetRootConstant(RootIndex::kMaxUInt32);
    }
    return graph()->GetHeapNumberConstant(*as_float64_constant);
  }

  NodeInfo* node_info =
      known_node_aspects().GetOrCreateInfoFor(broker(), value);
  auto& alternative = node_info->alternative();

  if (ValueNode* alt = alternative.tagged()) {
    return alt;
  }

  // This is called when converting inputs in AddNewNode. We might already have
  // an empty type for `value` here. Make sure we don't add unsafe conversion
  // nodes in that case by checking for the empty node type explicitly.
  // TODO(marja): The checks can be removed after we're able to bail out
  // earlier.
  switch (representation) {
    case ValueRepresentation::kInt32: {
      if (!IsEmptyNodeType(node_info->type()) &&
          NodeTypeIsSmi(node_info->type())) {
        return alternative.set_tagged(
            AddNewNodeNoInputConversion<UnsafeSmiTagInt32>({value}));
      }
      return alternative.set_tagged(
          AddNewNodeNoInputConversion<Int32ToNumber>({value}));
    }
    case ValueRepresentation::kUint32: {
      if (!IsEmptyNodeType(node_info->type()) &&
          NodeTypeIsSmi(node_info->type())) {
        return alternative.set_tagged(
            AddNewNodeNoInputConversion<UnsafeSmiTagUint32>({value}));
      }
      return alternative.set_tagged(
          AddNewNodeNoInputConversion<Uint32ToNumber>({value}));
    }
    case ValueRepresentation::kFloat64: {
      if (!IsEmptyNodeType(node_info->type()) && node_info->is_smi()) {
        return alternative.set_tagged(
            AddNewNodeNoInputConversion<CheckedSmiTagFloat64>({value}));
      }
      return alternative.set_tagged(
          AddNewNodeNoInputConversion<Float64ToTagged>(
              {value}, Float64ToTagged::ConversionMode::kCanonicalizeSmi));
    }
    case ValueRepresentation::kHoleyFloat64: {
      if (!IsEmptyNodeType(node_info->type()) && node_info->is_smi()) {
        return alternative.set_tagged(
            AddNewNodeNoInputConversion<CheckedSmiTagFloat64>({value}));
      }
      return alternative.set_tagged(
          AddNewNodeNoInputConversion<HoleyFloat64ToTagged>(
              {value}, HoleyFloat64ToTagged::ConversionMode::kForceHeapNumber));
    }

    case ValueRepresentation::kIntPtr:
      if (!IsEmptyNodeType(node_info->type()) &&
          NodeTypeIsSmi(node_info->type())) {
        return alternative.set_tagged(
            AddNewNodeNoInputConversion<UnsafeSmiTagIntPtr>({value}));
      }
      return alternative.set_tagged(
          AddNewNodeNoInputConversion<IntPtrToNumber>({value}));

    case ValueRepresentation::kTagged:
    case ValueRepresentation::kNone:
      UNREACHABLE();
  }
  UNREACHABLE();
}

template <typename BaseT>
ValueNode* MaglevReducer<BaseT>::GetInt32(ValueNode* value,
                                          bool can_be_heap_number) {
  value->MaybeRecordUseReprHint(UseRepresentation::kInt32);

  ValueRepresentation representation =
      value->properties().value_representation();
  if (representation == ValueRepresentation::kInt32) return value;

  // Process constants first to avoid allocating NodeInfo for them.
  if (auto cst = TryGetInt32Constant(value)) {
    return graph()->GetInt32Constant(cst.value());
  }
  // We could emit unconditional eager deopts for other kinds of constant, but
  // it's not necessary, the appropriate checking conversion nodes will deopt.

  NodeInfo* node_info =
      known_node_aspects().GetOrCreateInfoFor(broker(), value);
  auto& alternative = node_info->alternative();

  if (ValueNode* alt = alternative.int32()) {
    return alt;
  }

  switch (representation) {
    case ValueRepresentation::kTagged: {
      if (can_be_heap_number &&
          !known_node_aspects().CheckType(broker(), value, NodeType::kSmi)) {
        return alternative.set_int32(
            AddNewNodeNoInputConversion<CheckedNumberToInt32>({value}));
      }
      return alternative.set_int32(BuildSmiUntag(value));
    }
    case ValueRepresentation::kUint32: {
      if (!IsEmptyNodeType(known_node_aspects().GetType(broker(), value)) &&
          node_info->is_smi()) {
        return alternative.set_int32(
            AddNewNodeNoInputConversion<TruncateUint32ToInt32>({value}));
      }
      return alternative.set_int32(
          AddNewNodeNoInputConversion<CheckedUint32ToInt32>({value}));
    }
    case ValueRepresentation::kFloat64:
    // The check here will also work for the hole NaN, so we can treat
    // HoleyFloat64 as Float64.
    case ValueRepresentation::kHoleyFloat64: {
      return alternative.set_int32(
          AddNewNodeNoInputConversion<CheckedHoleyFloat64ToInt32>({value}));
    }

    case ValueRepresentation::kIntPtr:
      return alternative.set_int32(
          AddNewNodeNoInputConversion<CheckedIntPtrToInt32>({value}));

    case ValueRepresentation::kInt32:
    case ValueRepresentation::kNone:
      UNREACHABLE();
  }
  UNREACHABLE();
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::EmitUnconditionalDeopt(
    DeoptimizeReason reason) {
  static_assert(ReducerBaseWithUnconditonalDeopt<BaseT>);
  return base_->EmitUnconditionalDeopt(reason);
}

template <typename BaseT>
compiler::OptionalHeapObjectRef MaglevReducer<BaseT>::TryGetConstant(
    ValueNode* node, ValueNode** constant_node) {
  if (auto result = node->TryGetConstant(broker())) {
    if (constant_node) *constant_node = node;
    return result;
  }
  if (auto c = TryGetConstantAlternative(node)) {
    return TryGetConstant(*c, constant_node);
  }
  return {};
}

template <typename BaseT>
std::optional<int32_t> MaglevReducer<BaseT>::TryGetInt32Constant(
    ValueNode* value) {
  switch (value->opcode()) {
    case Opcode::kConstant: {
      compiler::ObjectRef object = value->Cast<Constant>()->object();
      if (object.IsHeapNumber() &&
          IsInt32Double(object.AsHeapNumber().value())) {
        return static_cast<int32_t>(object.AsHeapNumber().value());
      }
      return {};
    }
    case Opcode::kInt32Constant:
      return value->Cast<Int32Constant>()->value();
    case Opcode::kUint32Constant: {
      uint32_t uint32_value = value->Cast<Uint32Constant>()->value();
      if (uint32_value <= INT32_MAX) {
        return static_cast<int32_t>(uint32_value);
      }
      return {};
    }
    case Opcode::kSmiConstant:
      return value->Cast<SmiConstant>()->value().value();
    case Opcode::kFloat64Constant: {
      double double_value =
          value->Cast<Float64Constant>()->value().get_scalar();
      if (!IsInt32Double(double_value)) return {};
      return FastD2I(value->Cast<Float64Constant>()->value().get_scalar());
    }
    default:
      break;
  }
  if (auto c = TryGetConstantAlternative(value)) {
    return TryGetInt32Constant(*c);
  }
  return {};
}

template <typename BaseT>
std::optional<uint32_t> MaglevReducer<BaseT>::TryGetUint32Constant(
    ValueNode* value) {
  switch (value->opcode()) {
    case Opcode::kInt32Constant: {
      int32_t int32_value = value->Cast<Int32Constant>()->value();
      if (int32_value >= 0) {
        return static_cast<uint32_t>(int32_value);
      }
      return {};
    }
    case Opcode::kUint32Constant:
      return value->Cast<Uint32Constant>()->value();
    case Opcode::kSmiConstant: {
      int32_t smi_value = value->Cast<SmiConstant>()->value().value();
      if (smi_value >= 0) {
        return static_cast<uint32_t>(smi_value);
      }
      return {};
    }
    case Opcode::kFloat64Constant: {
      double double_value =
          value->Cast<Float64Constant>()->value().get_scalar();
      if (!IsUint32Double(double_value)) return {};
      return FastD2UI(value->Cast<Float64Constant>()->value().get_scalar());
    }
    default:
      break;
  }
  if (auto c = TryGetConstantAlternative(value)) {
    return TryGetUint32Constant(*c);
  }
  return {};
}

template <typename BaseT>
ValueNode* MaglevReducer<BaseT>::GetTruncatedInt32ForToNumber(
    ValueNode* value, NodeType allowed_input_type) {
  value->MaybeRecordUseReprHint(UseRepresentation::kTruncatedInt32);

  ValueRepresentation representation =
      value->properties().value_representation();
  if (representation == ValueRepresentation::kInt32) return value;
  if (representation == ValueRepresentation::kUint32) {
    // This node is cheap (no code gen, just a bitcast), so don't cache it.
    return AddNewNodeNoInputConversion<TruncateUint32ToInt32>({value});
  }

  // Process constants first to avoid allocating NodeInfo for them.
  switch (value->opcode()) {
    case Opcode::kConstant: {
      compiler::ObjectRef object = value->Cast<Constant>()->object();
      if (!object.IsHeapNumber()) break;
      int32_t truncated_value = DoubleToInt32(object.AsHeapNumber().value());
      return GetInt32Constant(truncated_value);
    }
    case Opcode::kSmiConstant:
      return GetInt32Constant(value->Cast<SmiConstant>()->value().value());
    case Opcode::kRootConstant: {
      Tagged<Object> root_object =
          local_isolate()->root(value->Cast<RootConstant>()->index());
      if (!IsOddball(root_object, local_isolate())) break;
      int32_t truncated_value =
          DoubleToInt32(Cast<Oddball>(root_object)->to_number_raw());
      // All oddball ToNumber truncations are valid Smis.
      DCHECK(Smi::IsValid(truncated_value));
      return GetInt32Constant(truncated_value);
    }
    case Opcode::kFloat64Constant: {
      int32_t truncated_value =
          DoubleToInt32(value->Cast<Float64Constant>()->value().get_scalar());
      return GetInt32Constant(truncated_value);
    }

    // We could emit unconditional eager deopts for other kinds of constant, but
    // it's not necessary, the appropriate checking conversion nodes will deopt.
    default:
      break;
  }

  NodeInfo* node_info = GetOrCreateInfoFor(value);
  auto& alternative = node_info->alternative();

  // If there is an int32_alternative, then that works as a truncated value
  // too.
  if (ValueNode* alt = alternative.int32()) {
    return alt;
  }
  if (ValueNode* alt = alternative.truncated_int32_to_number()) {
    return alt;
  }

  switch (representation) {
    case ValueRepresentation::kTagged: {
      NodeType old_type;
      EnsureType(value, allowed_input_type, &old_type);
      if (NodeTypeIsSmi(old_type)) {
        // Smi untagging can be cached as an int32 alternative, not just a
        // truncated alternative.
        return alternative.set_int32(BuildSmiUntag(value));
      }
      if (allowed_input_type == NodeType::kSmi) {
        return alternative.set_int32(
            AddNewNodeNoInputConversion<CheckedSmiUntag>({value}));
      }
      if (NodeTypeIs(old_type, allowed_input_type)) {
        return alternative.set_truncated_int32_to_number(
            AddNewNodeNoInputConversion<TruncateUnsafeNumberOrOddballToInt32>(
                {value}, GetTaggedToFloat64ConversionType(allowed_input_type)));
      }
      return alternative.set_truncated_int32_to_number(
          AddNewNodeNoInputConversion<TruncateCheckedNumberOrOddballToInt32>(
              {value}, GetTaggedToFloat64ConversionType(allowed_input_type)));
    }
    case ValueRepresentation::kFloat64:
    // Ignore conversion_type for HoleyFloat64, and treat them like Float64.
    // ToNumber of undefined is anyway a NaN, so we'll simply truncate away
    // the NaN-ness of the hole, and don't need to do extra oddball checks so
    // we can ignore the hint (though we'll miss updating the feedback).
    case ValueRepresentation::kHoleyFloat64: {
      return alternative.set_truncated_int32_to_number(
          AddNewNodeNoInputConversion<TruncateHoleyFloat64ToInt32>({value}));
    }

    case ValueRepresentation::kIntPtr: {
      // This is not an efficient implementation, but this only happens in
      // corner cases.
      ValueNode* value_to_number =
          AddNewNodeNoInputConversion<IntPtrToNumber>({value});
      return alternative.set_truncated_int32_to_number(
          AddNewNodeNoInputConversion<TruncateUnsafeNumberOrOddballToInt32>(
              {value_to_number}, TaggedToFloat64ConversionType::kOnlyNumber));
    }
    case ValueRepresentation::kInt32:
    case ValueRepresentation::kUint32:
    case ValueRepresentation::kNone:
      UNREACHABLE();
  }
  UNREACHABLE();
}

template <typename BaseT>
ValueNode* MaglevReducer<BaseT>::GetFloat64(ValueNode* value) {
  value->MaybeRecordUseReprHint(UseRepresentation::kFloat64);
  return GetFloat64ForToNumber(value, NodeType::kNumber);
}

template <typename BaseT>
ValueNode* MaglevReducer<BaseT>::GetFloat64ForToNumber(
    ValueNode* value, NodeType allowed_input_type) {
  ValueRepresentation representation =
      value->properties().value_representation();
  if (representation == ValueRepresentation::kFloat64) return value;

  // Process constants first to avoid allocating NodeInfo for them.
  if (auto cst = TryGetFloat64Constant(
          value, GetTaggedToFloat64ConversionType(allowed_input_type))) {
    return graph()->GetFloat64Constant(cst.value());
  }
  // We could emit unconditional eager deopts for other kinds of constant, but
  // it's not necessary, the appropriate checking conversion nodes will deopt.

  NodeInfo* node_info =
      known_node_aspects().GetOrCreateInfoFor(broker(), value);
  auto& alternative = node_info->alternative();

  if (ValueNode* alt = alternative.float64()) {
    return alt;
  }

  // This is called when converting inputs in AddNewNode. We might already have
  // an empty type for `value` here. Make sure we don't add unsafe conversion
  // nodes in that case by checking for the empty node type explicitly.
  // TODO(marja): The checks can be removed after we're able to bail out
  // earlier.
  switch (representation) {
    case ValueRepresentation::kTagged: {
      auto combined_type = IntersectType(allowed_input_type, node_info->type());
      if (!IsEmptyNodeType(node_info->type()) &&
          NodeTypeIs(combined_type, NodeType::kSmi)) {
        // Get the float64 value of a Smi value its int32 representation.
        return GetFloat64(GetInt32(value));
      }
      if (!IsEmptyNodeType(node_info->type()) &&
          NodeTypeIs(combined_type, NodeType::kNumber)) {
        // Number->Float64 conversions are exact alternatives, so they can
        // also become the canonical float64_alternative.
        return alternative.set_float64(
            BuildNumberOrOddballToFloat64(value, NodeType::kNumber));
      }
      if (!IsEmptyNodeType(node_info->type()) &&
          NodeTypeIs(combined_type, NodeType::kNumberOrOddball)) {
        // NumberOrOddball->Float64 conversions are not exact alternatives,
        // since they lose the information that this is an oddball, so they
        // can only become the canonical float64_alternative if they are a
        // known number (and therefore not oddball).
        return BuildNumberOrOddballToFloat64(value, combined_type);
      }
      // The type is impossible. We could generate an unconditional deopt here,
      // but it's too invasive. So we just generate a check which will always
      // deopt.
      return BuildNumberOrOddballToFloat64(value, allowed_input_type);
    }
    case ValueRepresentation::kInt32:
      return alternative.set_float64(
          AddNewNodeNoInputConversion<ChangeInt32ToFloat64>({value}));
    case ValueRepresentation::kUint32:
      return alternative.set_float64(
          AddNewNodeNoInputConversion<ChangeUint32ToFloat64>({value}));
    case ValueRepresentation::kHoleyFloat64: {
      switch (allowed_input_type) {
        case NodeType::kSmi:
        case NodeType::kNumber:
        case NodeType::kNumberOrBoolean:
          // Number->Float64 conversions are exact alternatives, so they can
          // also become the canonical float64_alternative. The HoleyFloat64
          // representation can represent undefined but no other oddballs, so
          // booleans cannot occur here and kNumberOrBoolean can be grouped with
          // kNumber.
          return alternative.set_float64(
              AddNewNodeNoInputConversion<CheckedHoleyFloat64ToFloat64>(
                  {value}));
        case NodeType::kNumberOrOddball:
          // NumberOrOddball->Float64 conversions are not exact alternatives,
          // since they lose the information that this is an oddball, so they
          // cannot become the canonical float64_alternative.
          return AddNewNodeNoInputConversion<HoleyFloat64ToMaybeNanFloat64>(
              {value});
        default:
          UNREACHABLE();
      }
    }
    case ValueRepresentation::kIntPtr:
      return alternative.set_float64(
          AddNewNodeNoInputConversion<ChangeIntPtrToFloat64>({value}));
    case ValueRepresentation::kFloat64:
    case ValueRepresentation::kNone:
      UNREACHABLE();
  }
  UNREACHABLE();
}

template <typename BaseT>
ValueNode* MaglevReducer<BaseT>::GetHoleyFloat64ForToNumber(
    ValueNode* value, NodeType allowed_input_type) {
  value->MaybeRecordUseReprHint(UseRepresentation::kHoleyFloat64);
  if (value->is_holey_float64()) return value;
  return GetFloat64ForToNumber(value, allowed_input_type);
}

template <typename BaseT>
void MaglevReducer<BaseT>::EnsureInt32(ValueNode* value,
                                       bool can_be_heap_number) {
  // Either the value is Int32 already, or we force a conversion to Int32 and
  // cache the value in its alternative representation node.
  GetInt32(value, can_be_heap_number);
}

template <typename BaseT>
std::optional<double> MaglevReducer<BaseT>::TryGetFloat64Constant(
    ValueNode* value, TaggedToFloat64ConversionType conversion_type) {
  switch (value->opcode()) {
    case Opcode::kConstant: {
      compiler::ObjectRef object = value->Cast<Constant>()->object();
      if (object.IsHeapNumber()) {
        return object.AsHeapNumber().value();
      }
      // Oddballs should be RootConstants.
      DCHECK(!IsOddball(*object.object()));
      return {};
    }
    case Opcode::kInt32Constant:
      return value->Cast<Int32Constant>()->value();
    case Opcode::kSmiConstant:
      return value->Cast<SmiConstant>()->value().value();
    case Opcode::kFloat64Constant:
      return value->Cast<Float64Constant>()->value().get_scalar();
    case Opcode::kRootConstant: {
      Tagged<Object> root_object =
          broker()->local_isolate()->root(value->Cast<RootConstant>()->index());
      if (conversion_type == TaggedToFloat64ConversionType::kNumberOrBoolean &&
          IsBoolean(root_object)) {
        return Cast<Oddball>(root_object)->to_number_raw();
      }
      if (conversion_type == TaggedToFloat64ConversionType::kNumberOrOddball &&
          IsOddball(root_object)) {
#ifdef V8_ENABLE_UNDEFINED_DOUBLE
        if (IsUndefined(root_object)) {
          // We use the undefined nan and silence it to produce the same result
          // as a computation from non-constants would.
          auto ud = Float64::FromBits(kUndefinedNanInt64);
          return ud.to_quiet_nan().get_scalar();
        }
#endif  // V8_ENABLE_UNDEFINED_DOUBLE
        return Cast<Oddball>(root_object)->to_number_raw();
      }
      if (IsHeapNumber(root_object)) {
        return Cast<HeapNumber>(root_object)->value();
      }
      return {};
    }
    default:
      break;
  }
  if (auto c = TryGetConstantAlternative(value)) {
    return TryGetFloat64Constant(*c, conversion_type);
  }
  return {};
}

template <typename BaseT>
void MaglevReducer<BaseT>::FlushNodesToBlock() {
  ZoneVector<Node*>& nodes = current_block()->nodes();
  if (new_nodes_at_end_.size() > 0) {
    size_t old_size = nodes.size();
    nodes.resize(old_size + new_nodes_at_end_.size());
    std::copy(new_nodes_at_end_.begin(), new_nodes_at_end_.end(),
              nodes.begin() + old_size);
    new_nodes_at_end_.clear();
  }

  if (new_nodes_at_.size() > 0) {
    // Sort by index.
    auto pos_cmp = [](const auto& p1, const auto& p2) {
      return p1.first < p2.first;
    };
    std::stable_sort(new_nodes_at_.begin(), new_nodes_at_.end(), pos_cmp);
    // Resize nodes and copy all nodes to the end.
    size_t diff = new_nodes_at_.size();
    DCHECK_GT(diff, 0);
    size_t old_size = nodes.size();
    nodes.resize(old_size + new_nodes_at_.size());
    std::copy_backward(nodes.begin(), nodes.begin() + old_size, nodes.end());
    // Now we move nodes either from the end or from new_nodes_at.
    int orig_idx = 0;
    auto it = nodes.begin();
    auto it_back = nodes.begin() + diff;
    auto it_new_nodes = new_nodes_at_.begin();
    while (it_new_nodes != new_nodes_at_.end()) {
      auto [new_node_idx, new_node] = *it_new_nodes;
      if (new_node_idx == orig_idx) {
        // Add the new node to the block buffer.
        // Do not advance the index.
        *it = new_node;
        it_new_nodes++;
      } else {
        // Add the old node in that index and advance the original index.
        *it = *it_back;
        it_back++;
        orig_idx++;
      }
      it++;
      DCHECK_LE(it, nodes.end());
    }
    new_nodes_at_.clear();
  }
}

template <typename BaseT>
template <typename MapContainer>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldCheckMaps(
    ValueNode* object, const MapContainer& maps) {
  // For constants with stable maps that match one of the desired maps, we
  // don't need to emit a map check, and can use the dependency -- we
  // can't do this for unstable maps because the constant could migrate
  // during compilation.
  if (compiler::OptionalHeapObjectRef constant = TryGetConstant(object)) {
    compiler::MapRef constant_map = constant->map(broker());
    if (std::find(maps.begin(), maps.end(), constant_map) == maps.end()) {
      return EmitUnconditionalDeopt(DeoptimizeReason::kWrongMap);
    }
    // TODO(verwaest): Reduce maps to the constant map.
    if (constant_map.is_stable()) {
      broker()->dependencies()->DependOnStableMap(constant_map);
      return ReduceResult::Done();
    }
    return {};
  }

  if (NodeTypeIs(GetType(object), NodeType::kNumber)) {
    auto heap_number_map =
        MakeRef(broker(), local_isolate()->factory()->heap_number_map());
    if (std::find(maps.begin(), maps.end(), heap_number_map) != maps.end()) {
      return ReduceResult::Done();
    }
    return EmitUnconditionalDeopt(DeoptimizeReason::kWrongMap);
  }

  return {};
}

template <typename BaseT>
ValueNode* MaglevReducer<BaseT>::BuildSmiUntag(ValueNode* node) {
  // This is called when converting inputs in AddNewNode. We might already have
  // an empty type for `node` here. Make sure we don't add unsafe conversion
  // nodes in that case by checking for the empty node type explicitly.
  // TODO(marja): The checks can be removed after we're able to bail out
  // earlier.
  if (!IsEmptyNodeType(GetType(node)) && EnsureType(node, NodeType::kSmi)) {
    if (SmiValuesAre31Bits()) {
      if (auto phi = node->TryCast<Phi>()) {
        phi->SetUseRequires31BitValue();
      }
    }
    return AddNewNodeNoAbort<UnsafeSmiUntag>({node});
  } else {
    return AddNewNodeNoAbort<CheckedSmiUntag>({node});
  }
}

template <typename BaseT>
ValueNode* MaglevReducer<BaseT>::BuildNumberOrOddballToFloat64(
    ValueNode* node, NodeType allowed_input_type) {
  NodeType old_type;
  auto conversion_type = GetTaggedToFloat64ConversionType(allowed_input_type);
  if (EnsureType(node, allowed_input_type, &old_type)) {
    if (old_type == NodeType::kSmi) {
      ValueNode* untagged_smi = BuildSmiUntag(node);
      return AddNewNodeNoAbort<ChangeInt32ToFloat64>({untagged_smi});
    }
    return AddNewNodeNoAbort<UncheckedNumberOrOddballToFloat64>(
        {node}, conversion_type);
  } else {
    return AddNewNodeNoAbort<CheckedNumberOrOddballToFloat64>({node},
                                                              conversion_type);
  }
}

template <typename BaseT>
ValueNode* MaglevReducer<BaseT>::GetNumberConstant(double constant) {
  if (IsSmiDouble(constant)) {
    return GetInt32Constant(FastD2I(constant));
  }
  return GetFloat64Constant(constant);
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildCheckedSmiSizedInt32(ValueNode* input) {
  if (auto cst = TryGetInt32Constant(input)) {
    if (Smi::IsValid(cst.value())) {
      return ReduceResult::Done();
    }
    // TODO(victorgomes): Emit deopt.
  }
  return AddNewNode<CheckedSmiSizedInt32>({input});
}

template <typename BaseT>
template <Operation kOperation>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldInt32UnaryOperation(
    ValueNode* node) {
  auto cst = TryGetInt32Constant(node);
  if (!cst.has_value()) return {};
  switch (kOperation) {
    case Operation::kBitwiseNot:
      return GetInt32Constant(~cst.value());
    case Operation::kIncrement:
      if (cst.value() < INT32_MAX) {
        return GetInt32Constant(cst.value() + 1);
      }
      return {};
    case Operation::kDecrement:
      if (cst.value() > INT32_MIN) {
        return GetInt32Constant(cst.value() - 1);
      }
      return {};
    case Operation::kNegate:
      if (cst.value() == 0) {
        return {};
      }
      if (cst.value() != INT32_MIN) {
        return GetInt32Constant(-cst.value());
      }
      return {};
    default:
      UNREACHABLE();
  }
}

template <typename BaseT>
template <Operation kOperation>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldInt32BinaryOperation(
    ValueNode* left, ValueNode* right) {
  auto cst_right = TryGetInt32Constant(right);
  if (!cst_right.has_value()) return {};
  return TryFoldInt32BinaryOperation<kOperation>(left, cst_right.value());
}

template <typename BaseT>
template <Operation kOperation>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldInt32BinaryOperation(
    ValueNode* left, int32_t cst_right) {
  if (auto cst_left = TryGetInt32Constant(left)) {
    return TryFoldInt32BinaryOperation<kOperation>(cst_left.value(), cst_right);
  }
  if (cst_right == Int32Identity<kOperation>()) {
    if (v8_flags.maglev_truncation && IsBitwiseBinaryOperation<kOperation>() &&
        (left->opcode() == Opcode::kInt32AddWithOverflow ||
         left->opcode() == Opcode::kInt32SubtractWithOverflow)) {
      // Don't fold the |0 in (a + b)|0 and similar expressions, so that we can
      // track whether removing the overflow checking from the "a + b" operation
      // is fine. This requires differentiating between the users of the "a + b"
      // node and the users of the "(a + b)|0" node.

      // TODO(marja): To support Int32MultiplyWithOverflow and
      // Int32DivideWithOverflow, we need to be able to reason about ranges.
      //
      // TODO(marja): We can add a limited version of that, to support cases
      // where one of the operands is a constant and thus we can be sure the
      // result stays in the safe range.
      return {};
    }

    // Deopt if {left} is not an Int32.
    EnsureInt32(left);
    if (left->properties().is_conversion()) {
      return left->input(0).node();
    }
    return left;
  }
  // TODO(v8:7700): Add more peephole cases.
  switch (kOperation) {
    case Operation::kAdd:
      // x + 1 => x++
      if (cst_right == 1) {
        return AddNewNode<Int32IncrementWithOverflow>({left});
      }
      return {};
    case Operation::kSubtract:
      // x - 1 => x--
      if (cst_right == 1) {
        return AddNewNode<Int32DecrementWithOverflow>({left});
      }
      return {};
    case Operation::kMultiply:
      // x * 0 = 0
      if (cst_right == 0) {
        RETURN_IF_ABORT(AddNewNode<CheckInt32Condition>(
            {left, GetInt32Constant(0)}, AssertCondition::kGreaterThanEqual,
            DeoptimizeReason::kMinusZero));
        return GetInt32Constant(0);
      }
      return {};
    case Operation::kDivide:
      // x / -1 = 0 - x
      if (cst_right == -1) {
        RETURN_IF_ABORT(AddNewNode<CheckInt32Condition>(
            {left, GetInt32Constant(0)}, AssertCondition::kNotEqual,
            DeoptimizeReason::kMinusZero));
        return AddNewNode<Int32SubtractWithOverflow>(
            {GetInt32Constant(0), left});
      }
      if (cst_right != 0) {
        // x / n = x reciprocal_int_mult(x, n)
        if (cst_right < 0) {
          // Deopt if division would result in -0.
          RETURN_IF_ABORT(AddNewNode<CheckInt32Condition>(
              {left, GetInt32Constant(0)}, AssertCondition::kNotEqual,
              DeoptimizeReason::kMinusZero));
        }
        base::MagicNumbersForDivision<int32_t> magic =
            base::SignedDivisionByConstant(cst_right);
        ValueNode* quot;
        GET_VALUE_OR_ABORT(quot,
                           AddNewNode<Int32MultiplyOverflownBits>(
                               {left, GetInt32Constant(magic.multiplier)}));
        if (cst_right > 0 && magic.multiplier < 0) {
          GET_VALUE_OR_ABORT(quot, AddNewNode<Int32Add>({quot, left}));
        } else if (cst_right < 0 && magic.multiplier > 0) {
          GET_VALUE_OR_ABORT(quot, AddNewNode<Int32Subtract>({quot, left}));
        }
        ValueNode* sign_bit;
        GET_VALUE_OR_ABORT(sign_bit, AddNewNode<Int32ShiftRightLogical>(
                                         {left, GetInt32Constant(31)}));
        ValueNode* shifted_quot;
        GET_VALUE_OR_ABORT(
            shifted_quot,
            AddNewNode<Int32ShiftRight>({quot, GetInt32Constant(magic.shift)}));
        // TODO(victorgomes): This should actually be NodeType::kInt32, but we
        // don't have it. The idea here is that the value is either 0 or 1, so
        // we can cast Uint32 to Int32 without a check.
        EnsureType(sign_bit, NodeType::kSmi);
        ValueNode* result;
        GET_VALUE_OR_ABORT(result,
                           AddNewNode<Int32Add>({shifted_quot, sign_bit}));
        ValueNode* mult;
        GET_VALUE_OR_ABORT(mult, AddNewNode<Int32Multiply>(
                                     {result, GetInt32Constant(cst_right)}));
        RETURN_IF_ABORT(AddNewNode<CheckInt32Condition>(
            {left, mult}, AssertCondition::kEqual,
            DeoptimizeReason::kNotInt32));
        return result;
      }
      return {};
    case Operation::kBitwiseAnd:
      // x & 0 = 0
      if (cst_right == 0) {
        EnsureInt32(left);
        return GetInt32Constant(0);
      }
      return {};
    case Operation::kBitwiseOr:
      // x | -1 = -1
      if (cst_right == 0) {
        EnsureInt32(left);
        return GetInt32Constant(-1);
      }
      return {};
    default:
      return {};
  }
}

template <typename BaseT>
template <Operation kOperation>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldInt32BinaryOperation(
    int32_t cst_left, int32_t cst_right) {
  int32_t result;
  switch (kOperation) {
    case Operation::kAdd:
      if (base::bits::SignedAddOverflow32(cst_left, cst_right, &result)) {
        return {};
      }
      return GetInt32Constant(result);
    case Operation::kSubtract:
      if (base::bits::SignedSubOverflow32(cst_left, cst_right, &result)) {
        return {};
      }
      return GetInt32Constant(result);
    case Operation::kMultiply:
      if (base::bits::SignedMulOverflow32(cst_left, cst_right, &result)) {
        return {};
      }
      return GetInt32Constant(result);
    case Operation::kModulus:
      // TODO(v8:7700): Constant fold mod.
      return {};
    case Operation::kDivide:
      // TODO(v8:7700): Constant fold division.
      return {};
    case Operation::kBitwiseAnd:
      return GetInt32Constant(cst_left & cst_right);
    case Operation::kBitwiseOr:
      return GetInt32Constant(cst_left | cst_right);
    case Operation::kBitwiseXor:
      return GetInt32Constant(cst_left ^ cst_right);
    case Operation::kShiftLeft:
      return GetInt32Constant(cst_left
                              << (static_cast<uint32_t>(cst_right) % 32));
    case Operation::kShiftRight:
      return GetInt32Constant(cst_left >>
                              (static_cast<uint32_t>(cst_right) % 32));
    case Operation::kShiftRightLogical:
      return GetUint32Constant(static_cast<uint32_t>(cst_left) >>
                               (static_cast<uint32_t>(cst_right) % 32));
    default:
      UNREACHABLE();
  }
}

template <typename BaseT>
std::optional<bool> MaglevReducer<BaseT>::TryFoldInt32CompareOperation(
    Operation op, ValueNode* left, ValueNode* right) {
  if (op == Operation::kEqual || op == Operation::kStrictEqual) {
    if (left == right) {
      return true;
    }
  }
  if (auto cst_right = TryGetInt32Constant(right)) {
    return TryFoldInt32CompareOperation(op, left, cst_right.value());
  }
  return {};
}

template <typename BaseT>
std::optional<bool> MaglevReducer<BaseT>::TryFoldInt32CompareOperation(
    Operation op, ValueNode* left, int32_t cst_right) {
  if (auto cst_left = TryGetInt32Constant(left)) {
    return TryFoldInt32CompareOperation(op, cst_left.value(), cst_right);
  }
  return {};
}

template <typename BaseT>
bool MaglevReducer<BaseT>::TryFoldInt32CompareOperation(Operation op,
                                                        int32_t left,
                                                        int32_t right) {
  switch (op) {
    case Operation::kEqual:
    case Operation::kStrictEqual:
      return left == right;
    case Operation::kLessThan:
      return left < right;
    case Operation::kLessThanOrEqual:
      return left <= right;
    case Operation::kGreaterThan:
      return left > right;
    case Operation::kGreaterThanOrEqual:
      return left >= right;
    default:
      UNREACHABLE();
  }
}

template <typename BaseT>
template <Operation kOperation>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldFloat64UnaryOperationForToNumber(
    TaggedToFloat64ConversionType conversion_type, ValueNode* value) {
  auto cst = TryGetFloat64Constant(value, conversion_type);
  if (!cst.has_value()) return {};
  switch (kOperation) {
    case Operation::kNegate:
      return GetNumberConstant(-cst.value());
    case Operation::kIncrement:
      return GetNumberConstant(cst.value() + 1);
    case Operation::kDecrement:
      return GetNumberConstant(cst.value() - 1);
    default:
      UNREACHABLE();
  }
}

template <typename BaseT>
template <Operation kOperation>
MaybeReduceResult
MaglevReducer<BaseT>::TryFoldFloat64BinaryOperationForToNumber(
    TaggedToFloat64ConversionType conversion_type, ValueNode* left,
    ValueNode* right) {
  auto cst_right = TryGetFloat64Constant(right, conversion_type);
  if (!cst_right.has_value()) return {};
  return TryFoldFloat64BinaryOperationForToNumber<kOperation>(
      conversion_type, left, cst_right.value());
}

template <typename BaseT>
template <Operation kOperation>
MaybeReduceResult
MaglevReducer<BaseT>::TryFoldFloat64BinaryOperationForToNumber(
    TaggedToFloat64ConversionType conversion_type, ValueNode* left,
    double cst_right) {
  auto cst_left = TryGetFloat64Constant(left, conversion_type);
  if (!cst_left.has_value()) return {};
  switch (kOperation) {
    case Operation::kAdd:
      return GetNumberConstant(cst_left.value() + cst_right);
    case Operation::kSubtract:
      return GetNumberConstant(cst_left.value() - cst_right);
    case Operation::kMultiply:
      return GetNumberConstant(cst_left.value() * cst_right);
    case Operation::kDivide:
      return GetNumberConstant(cst_left.value() / cst_right);
    case Operation::kModulus:
      // TODO(v8:7700): Constant fold mod.
      return {};
    case Operation::kExponentiate:
      return GetNumberConstant(math::pow(cst_left.value(), cst_right));
    default:
      UNREACHABLE();
  }
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_REDUCER_INL_H_
