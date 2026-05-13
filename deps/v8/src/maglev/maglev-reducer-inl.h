// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_REDUCER_INL_H_
#define V8_MAGLEV_MAGLEV_REDUCER_INL_H_

#include "src/maglev/maglev-reducer.h"
// Include the non-inl header before the rest of the headers.

#include <cmath>

#include "src/base/bits.h"
#include "src/base/container-utils.h"
#include "src/base/division-by-constant.h"
#include "src/base/ieee754.h"
#include "src/common/scoped-modification.h"
#include "src/compiler/processed-feedback.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-map-inference.h"
#include "src/numbers/conversions.h"
#include "src/numbers/ieee754.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/objects-inl.h"

#define TRACE(...)                        \
  if (V8_UNLIKELY(is_tracing())) {        \
    TraceLogger(tracer()) << __VA_ARGS__; \
  }

namespace v8 {
namespace internal {
namespace maglev {

// Some helpers for CSE
namespace cse {
inline size_t fast_hash_combine(size_t seed, size_t h) {
  // Implementation from boost. Good enough for GVN.
  return h + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename T>
size_t gvn_hash_value(const T& in) {
  return base::hash_value(in);
}

inline size_t gvn_hash_value(const compiler::MapRef& map) {
  return map.hash_value();
}

inline size_t gvn_hash_value(const interpreter::Register& reg) {
  return base::hash_value(reg.index());
}

inline size_t gvn_hash_value(const Representation& rep) {
  return base::hash_value(rep.kind());
}

inline size_t gvn_hash_value(const ExternalReference& ref) {
  return base::hash_value(ref.address());
}

inline size_t gvn_hash_value(const PolymorphicAccessInfo& access_info) {
  return access_info.hash_value();
}

inline size_t gvn_hash_value(const PropertyKey& key) {
  return base::hash_value(key.data());
}

template <typename T>
size_t gvn_hash_value(const v8::internal::ZoneCompactSet<T>& vector) {
  size_t hash = base::hash_value(vector.size());
  for (auto e : vector) {
    hash = fast_hash_combine(hash, gvn_hash_value(e));
  }
  return hash;
}

template <typename T>
size_t gvn_hash_value(const v8::internal::ZoneVector<T>& vector) {
  size_t hash = base::hash_value(vector.size());
  for (auto e : vector) {
    hash = fast_hash_combine(hash, gvn_hash_value(e));
  }
  return hash;
}

template <typename... Args>
size_t fast_hash_combine(size_t seed, Args&&... args) {
  size_t hash = seed;
  ([&] { hash = cse::fast_hash_combine(hash, cse::gvn_hash_value(args)); }(),
   ...);
  return hash;
}

template <size_t kInputCount, typename... Args>
size_t fast_hash_combine(size_t seed,
                         std::array<ValueNode*, kInputCount>& inputs) {
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
NodeT* MaglevReducer<BaseT>::AddNewNodeNoInputConversion(
    std::initializer_list<ValueNode*> inputs, Args&&... args) {
  static_assert(IsFixedInputNode<NodeT>());
  if constexpr (Node::participate_in_cse(Node::opcode_of<NodeT>) &&
                ReducerBaseWithKNA<BaseT>) {
    if (v8_flags.maglev_cse) {
      ReduceResult result = AddNewNodeOrGetEquivalent<NodeT>(
          false, inputs, std::forward<Args>(args)...);
      // Without input conversion, AddNewNodeOrGetEquivalent cannot bail out.
      DCHECK(result.IsDoneWithPayload());
      return result.node()->Cast<NodeT>();
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
ReduceResult MaglevReducer<BaseT>::AddNewControlNode(
    std::initializer_list<ValueNode*> inputs, Args&&... args) {
  ControlNodeT* control_node = NodeBase::New<ControlNodeT>(
      zone(), inputs.size(), std::forward<Args>(args)...);
  RETURN_IF_ABORT(SetNodeInputs(control_node, inputs));
  AttachEagerDeoptInfo(control_node);
  AttachDeoptCheckpoint(control_node);
  AttachLazyDeoptInfo(control_node);
  AttachExceptionHandlerInfo(control_node);
  static_assert(!ControlNodeT::kProperties.can_write());
  control_node->set_owner(current_block());
  current_block()->set_control_node(control_node);
  if (has_graph_labeller()) {
    RegisterNode(control_node);
    TRACE(TraceNewControlNode(control_node));
  }
  return ReduceResult::Done();
}

template <typename BaseT>
template <typename NodeT, typename... Args>
ReduceResult MaglevReducer<BaseT>::AddNewNodeOrGetEquivalent(
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

  std::array<ValueNode*, NodeT::kInputCount> inputs;
  // Nodes with zero input count don't have kInputRepresentations defined.
  if constexpr (NodeT::kInputCount > 0) {
    int i = 0;
    constexpr UseReprHintRecording hint = ShouldRecordUseReprHint<NodeT>();
    for (ValueNode* raw_input : raw_inputs) {
      if (convert_inputs) {
        GET_VALUE_OR_ABORT(
            inputs[i],
            ConvertInputTo<hint>(raw_input, NodeT::kInputRepresentations[i]));
      } else {
        CHECK(ValueRepresentationIs(
            raw_input->properties().value_representation(),
            NodeT::kInputRepresentations[i]));
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
  graph_->increment_total_nodes();
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
  TRACE(TraceNewNode{node});
#ifdef DEBUG
  new_nodes_current_period_.insert(node);
#endif  // debug
}

template <typename BaseT>
template <UseReprHintRecording hint>
ReduceResult MaglevReducer<BaseT>::ConvertInputTo(
    ValueNode* input, ValueRepresentation expected) {
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
        return GetFloat64(input);
      case ValueRepresentation::kHoleyFloat64:
        return GetHoleyFloat64(input);
      case ValueRepresentation::kUint32:
      case ValueRepresentation::kIntPtr:
      case ValueRepresentation::kRawPtr:
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
  // Nodes with zero input count don't have kInputRepresentations defined.
  if constexpr (NodeT::kInputCount > 0) {
    constexpr UseReprHintRecording hint = ShouldRecordUseReprHint<NodeT>();
    int i = 0;
    for (ValueNode* input : inputs) {
      DCHECK_NOT_NULL(input);
      ValueNode* converted;
      GET_VALUE_OR_ABORT(
          converted,
          ConvertInputTo<hint>(input, NodeT::kInputRepresentations[i]));
      node->set_input(i, converted);
      i++;
    }
  }
  return ReduceResult::Done();
}

template <typename BaseT>
template <typename NodeT, typename InputsT>
void MaglevReducer<BaseT>::SetNodeInputsNoConversion(NodeT* node,
                                                     InputsT inputs) {
  // Nodes with zero input count don't have kInputRepresentations defined.
  if constexpr (NodeT::kInputCount > 0) {
    int i = 0;
    for (ValueNode* input : inputs) {
      DCHECK_NOT_NULL(input);
      CHECK(ValueRepresentationIs(input->properties().value_representation(),
                                  NodeT::kInputRepresentations[i]));
      node->set_input(i, input);
      i++;
    }
  }
}

template <typename BaseT>
template <typename NodeT>
NodeT* MaglevReducer<BaseT>::AttachExtraInfoAndAddToGraph(NodeT* node) {
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
    DeoptFrame* top_frame = base_->GetDeoptFrameForEagerDeopt();
    graph_->AddEagerTopFrame(top_frame);
    node->SetEagerDeoptInfo(zone(), top_frame);
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
    known_node_aspects().MarkPossibleSideEffect(node, broker(),
                                                is_tracing_enabled());
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
  if constexpr (HasRangeField(Node::opcode_of<NodeT>)) {
    Range r1 = node->input_node(0)->GetStaticRange();
    Range r2 = node->input_node(1)->GetStaticRange();
    DCHECK(!r1.is_empty());
    DCHECK(!r2.is_empty());
    if (r1.is_all() || r2.is_all()) return;
    Range result = Range::All();
    switch (Node::opcode_of<NodeT>) {
      case Opcode::kFloat64SpeculateSafeAdd:
      case Opcode::kFloat64Add:
        result = Range::Add(r1, r2);
        break;
      case Opcode::kFloat64Subtract:
        result = Range::Sub(r1, r2);
        break;
      case Opcode::kFloat64Multiply:
        result = Range::Mul(r1, r2);
        break;
      case Opcode::kFloat64Divide:
        break;
      default:
        UNREACHABLE();
    }
    // TODO(victorgomes): Calculating the range of a LoopPhi might need a
    // fixpoint.
    static_assert(!std::is_same_v<NodeT, Phi>);
    node->set_range(result);
    if (node->range().IsSafeInt()) {
      // The node can be considered for truncation in the truncation pass.
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
ReduceResult MaglevReducer<BaseT>::GetTaggedValue(
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
  if (auto as_float64_constant = TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kHoleyFloat64, value,
          TaggedToFloat64ConversionType::kNumberOrUndefined)) {
    if (as_float64_constant->is_undefined_or_hole_nan()) {
      return graph()->GetRootConstant(RootIndex::kUndefinedValue);
    }
    if (as_float64_constant->is_nan()) {
      return graph()->GetRootConstant(RootIndex::kNanValue);
    }
    if (as_float64_constant->get_scalar() == static_cast<double>(kMaxUInt32)) {
      return graph()->GetRootConstant(RootIndex::kMaxUInt32);
    }
    return graph()->GetHeapNumberConstant(as_float64_constant->get_scalar());
  }

  NodeInfo* node_info =
      known_node_aspects().GetOrCreateInfoFor(broker(), value);
  auto& alternative = node_info->alternative();

  if (ValueNode* alt = alternative.tagged()) {
    return alt;
  }

  // Check for the empty type first, so that we don't emit unsafe conversion
  // nodes below.
  if (IsEmptyNodeType(node_info->type())) {
    return EmitUnconditionalDeopt(DeoptimizeReason::kWrongValue);
  }

  switch (representation) {
    case ValueRepresentation::kInt32: {
      if (NodeTypeIsSmi(node_info->type())) {
        return alternative.set_tagged(
            AddNewNodeNoInputConversion<UnsafeSmiTagInt32>({value}));
      }
      return alternative.set_tagged(AddNewNodeNoInputConversion<Int32ToNumber>(
          {value}, NumberConversionMode::kCanonicalizeSmi));
    }
    case ValueRepresentation::kUint32: {
      if (NodeTypeIsSmi(node_info->type())) {
        return alternative.set_tagged(
            AddNewNodeNoInputConversion<UnsafeSmiTagUint32>({value}));
      }
      return alternative.set_tagged(
          AddNewNodeNoInputConversion<Uint32ToNumber>({value}));
    }
    case ValueRepresentation::kFloat64: {
      if (node_info->is_smi()) {
        return alternative.set_tagged(
            AddNewNodeNoInputConversion<CheckedSmiTagFloat64>({value}));
      }
      return alternative.set_tagged(
          AddNewNodeNoInputConversion<Float64ToTagged>(
              {value}, NumberConversionMode::kCanonicalizeSmi));
    }
    case ValueRepresentation::kHoleyFloat64: {
      if (node_info->is_smi()) {
        return alternative.set_tagged(
            AddNewNodeNoInputConversion<CheckedSmiTagHoleyFloat64>({value}));
      }
      return alternative.set_tagged(
          AddNewNodeNoInputConversion<HoleyFloat64ToTagged>(
              {value}, NumberConversionMode::kCanonicalizeSmi));
    }

    case ValueRepresentation::kIntPtr:
      if (NodeTypeIsSmi(node_info->type())) {
        return alternative.set_tagged(
            AddNewNodeNoInputConversion<UnsafeSmiTagIntPtr>({value}));
      }
      return alternative.set_tagged(
          AddNewNodeNoInputConversion<IntPtrToNumber>({value}));

    case ValueRepresentation::kTagged:
    case ValueRepresentation::kRawPtr:
    case ValueRepresentation::kNone:
      UNREACHABLE();
  }
  UNREACHABLE();
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::GetInt32(ValueNode* value,
                                            bool can_be_heap_number) {
  value->MaybeRecordUseReprHint(UseRepresentation::kInt32);

  if (ValueNode* int32_value = TryGetInt32(value)) {
    return int32_value;
  }

  // We could emit unconditional eager deopts for other kinds of constant, but
  // it's not necessary, the appropriate checking conversion nodes will deopt.

  NodeInfo* node_info =
      known_node_aspects().GetOrCreateInfoFor(broker(), value);
  auto& alternative = node_info->alternative();

  switch (value->properties().value_representation()) {
    case ValueRepresentation::kTagged: {
      if (can_be_heap_number &&
          !known_node_aspects().CheckType(broker(), value, NodeType::kSmi)) {
        return alternative.set_int32(
            AddNewNodeNoInputConversion<CheckedNumberToInt32>({value}));
      }
      ValueNode* untagged;
      GET_VALUE_OR_ABORT(untagged,
                         BuildSmiUntag(value, AllowWideningSmiToInt32::kAllow));
      return alternative.set_int32(untagged);
    }
    case ValueRepresentation::kUint32: {
      if (!IsEmptyNodeType(GetType(value)) && node_info->is_smi()) {
        return alternative.set_int32(
            AddNewNodeNoInputConversion<TruncateUint32ToInt32>({value}));
      }
      return alternative.set_int32(
          AddNewNodeNoInputConversion<CheckedUint32ToInt32>({value}));
    }
    case ValueRepresentation::kFloat64: {
      return alternative.set_int32(
          AddNewNodeNoInputConversion<CheckedFloat64ToInt32>({value}));
    }
    case ValueRepresentation::kHoleyFloat64: {
      return alternative.set_int32(
          AddNewNodeNoInputConversion<CheckedHoleyFloat64ToInt32>({value}));
    }

    case ValueRepresentation::kIntPtr:
      return alternative.set_int32(
          AddNewNodeNoInputConversion<CheckedIntPtrToInt32>({value}));

    case ValueRepresentation::kInt32:
    case ValueRepresentation::kRawPtr:
    case ValueRepresentation::kNone:
      UNREACHABLE();
  }
  UNREACHABLE();
}

template <typename BaseT>
ValueNode* MaglevReducer<BaseT>::TryGetInt32(ValueNode* value) {
  if (value->is_int32()) return value;

  if (auto cst = TryGetInt32Constant(value)) {
    return graph()->GetInt32Constant(cst.value());
  }

  if (ValueNode* alt = known_node_aspects().TryGetAlternativeFor(
          value, UseRepresentation::kInt32)) {
    return alt;
  }

  return nullptr;
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::EmitUnconditionalDeopt(
    DeoptimizeReason reason) {
  static_assert(ReducerBaseWithUnconditonalDeopt<BaseT>);
  return base_->EmitUnconditionalDeopt(reason);
}

template <typename BaseT>
compiler::OptionalHeapObjectRef MaglevReducer<BaseT>::TryGetHeapObjectConstant(
    ValueNode* node, ValueNode** constant_node) {
  if (auto result = node->TryGetConstant(broker())) {
    if (constant_node) *constant_node = node;
    return result;
  }
  if (auto c = TryGetConstantAlternative(node)) {
    return TryGetHeapObjectConstant(*c, constant_node);
  }
  return {};
}

template <typename BaseT>
std::optional<int32_t> MaglevReducer<BaseT>::TryGetInt32Constant(
    ValueNode* value) {
  switch (value->opcode()) {
    case Opcode::kHeapConstant: {
      compiler::ObjectRef object = value->Cast<HeapConstant>()->object();
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
std::optional<intptr_t> MaglevReducer<BaseT>::TryGetIntPtrConstant(
    ValueNode* value) {
  switch (value->opcode()) {
    case Opcode::kIntPtrConstant:
      return value->Cast<IntPtrConstant>()->value();
    default:
      break;
  }
  if (auto c = TryGetConstantAlternative(value)) {
    return TryGetIntPtrConstant(*c);
  }
  return {};
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::GetTruncatedInt32ForToNumber(
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
    case Opcode::kHeapConstant: {
      compiler::ObjectRef object = value->Cast<HeapConstant>()->object();
      if (!object.IsHeapNumber()) break;
      int32_t truncated_value = DoubleToInt32(object.AsHeapNumber().value());
      return GetInt32Constant(truncated_value);
    }
    case Opcode::kSmiConstant:
      return GetInt32Constant(value->Cast<SmiConstant>()->value().value());
    case Opcode::kRootConstant: {
      Tagged<Object> root_object =
          local_isolate()->root(value->Cast<RootConstant>()->index());
      if (!IsOddball(root_object)) break;
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

      // Check for the empty type first, so that we don't emit unsafe conversion
      // nodes below.
      if (IsEmptyNodeType(old_type)) {
        return EmitUnconditionalDeopt(DeoptimizeReason::kWrongValue);
      }

      if (NodeTypeIsSmi(old_type)) {
        // Smi untagging can be cached as an int32 alternative, not just a
        // truncated alternative.
        ValueNode* int32_value;
        GET_VALUE_OR_ABORT(int32_value, BuildSmiUntag(value));
        return alternative.set_int32(int32_value);
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
      return alternative.set_truncated_int32_to_number(
          AddNewNodeNoInputConversion<TruncateFloat64ToInt32>({value}));
    case ValueRepresentation::kHoleyFloat64: {
      // Ignore conversion_type for HoleyFloat64, and treat them like Float64.
      // ToNumber of undefined is anyway a NaN, so we'll simply truncate away
      // the NaN-ness of the hole, and don't need to do extra oddball checks so
      // we can ignore the hint (though we'll miss updating the feedback).
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
    case ValueRepresentation::kRawPtr:
    case ValueRepresentation::kNone:
      UNREACHABLE();
  }
  UNREACHABLE();
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::GetFloat64OrHoleyFloat64Impl(
    ValueNode* value, UseRepresentation use_rep, NodeType allowed_input_type) {
  DCHECK(use_rep == UseRepresentation::kFloat64 ||
         use_rep == UseRepresentation::kHoleyFloat64);
  ValueRepresentation representation =
      value->properties().value_representation();
  if (representation == ValueRepresentation::kHoleyFloat64 &&
      use_rep == UseRepresentation::kHoleyFloat64) {
    DCHECK_NE(allowed_input_type, NodeType::kNumber);
    return value;
  } else if (representation == ValueRepresentation::kFloat64 &&
             use_rep == UseRepresentation::kFloat64) {
    return value;
  }

  // Process constants first to avoid allocating NodeInfo for them.
  if (auto cst = TryGetFloat64OrHoleyFloat64Constant(
          use_rep, value,
          GetTaggedToFloat64ConversionType(allowed_input_type))) {
    if (use_rep == UseRepresentation::kHoleyFloat64) {
      return graph()->GetHoleyFloat64Constant(cst.value());
    }
    return graph()->GetFloat64Constant(cst.value());
  }

  // We could emit unconditional eager deopts for other kinds of constant, but
  // it's not necessary, the appropriate checking conversion nodes will deopt.

  NodeInfo* node_info =
      known_node_aspects().GetOrCreateInfoFor(broker(), value);
  auto& alternative = node_info->alternative();

  if (use_rep == UseRepresentation::kHoleyFloat64) {
    // When we want to use the `holey_float64` alternative, we need to make sure
    // that this doesn't contain any values that are not outside of
    // `allowed_input_type`. We do only set this alternative (see below) when
    // this is a (reversible) conversion, which means that it can only represent
    // numbers and undefined. So for us to use this alternative here, the
    // `allowed_input_type` must at least allow those, too. If we ever decide to
    // allow more narrow types (e.g. kSmi) we need to explicitly check for that
    // range, because the `holey_float64` alternative can contain values outside
    // of smi range.
    if (ValueNode* alt_hf64 = alternative.holey_float64()) {
      if (NodeTypeIs(
              IntersectType(NodeType::kNumberOrUndefined, node_info->type()),
              allowed_input_type)) {
        return alt_hf64;
      }
    }
  } else {
    if (ValueNode* alt_f64 = alternative.float64()) {
      return alt_f64;
    }
  }

  // Check for the empty type first, so that we don't emit unsafe conversion
  // nodes below.
  if (IsEmptyNodeType(node_info->type())) {
    return EmitUnconditionalDeopt(DeoptimizeReason::kWrongValue);
  }

  switch (value->properties().value_representation()) {
    case ValueRepresentation::kTagged: {
      auto combined_type = IntersectType(allowed_input_type, node_info->type());
      if (!IsEmptyNodeType(combined_type) &&
          NodeTypeIs(combined_type, NodeType::kSmi)) {
        // Get the float64 value of a Smi value its int32 representation.
        ValueNode* int32_value;
        GET_VALUE_OR_ABORT(int32_value, GetInt32(value));
        return GetFloat64OrHoleyFloat64Impl(int32_value, use_rep,
                                            combined_type);
      }
      if (!IsEmptyNodeType(combined_type) &&
          NodeTypeIs(combined_type, NodeType::kNumber)) {
        ValueNode* float64_value;
        GET_VALUE_OR_ABORT(float64_value,
                           BuildNumberOrOddballToFloat64OrHoleyFloat64(
                               value, use_rep, NodeType::kNumber));
        if (use_rep == UseRepresentation::kFloat64) {
          // Number->Float64 conversions are exact alternatives, so they can
          // also become the canonical float64_alternative.
          return alternative.set_float64(float64_value);
        }
        return float64_value;
      }
      if (!IsEmptyNodeType(combined_type) &&
          NodeTypeIs(combined_type, NodeType::kNumberOrOddball)) {
        // NumberOrOddball->Float64 conversions are not exact alternatives,
        // since they lose the information that this is an oddball, so they
        // can only become the canonical float64_alternative if they are a
        // known number (and therefore not oddball).
        return BuildNumberOrOddballToFloat64OrHoleyFloat64(value, use_rep,
                                                           combined_type);
      }
      // The type is impossible. We could generate an unconditional deopt here,
      // but it's too invasive. So we just generate a check which will always
      // deopt.
      return BuildNumberOrOddballToFloat64OrHoleyFloat64(value, use_rep,
                                                         allowed_input_type);
    }
    case ValueRepresentation::kInt32: {
      ValueNode* float64 =
          AddNewNodeNoInputConversion<ChangeInt32ToFloat64>({value});
      if (use_rep == UseRepresentation::kHoleyFloat64) {
        // We only set the holey_float64 alternative if all feasible values are
        // allowed according to `allowed_input_type`.
        return alternative.set_holey_float64(
            AddNewNodeNoInputConversion<UnsafeFloat64ToHoleyFloat64>(
                {float64}));
      }
      return alternative.set_float64(float64);
    }
    case ValueRepresentation::kUint32: {
      ValueNode* float64 =
          AddNewNodeNoInputConversion<ChangeUint32ToFloat64>({value});
      if (use_rep == UseRepresentation::kHoleyFloat64) {
        // We only set the holey_float64 alternative if all feasible values are
        // allowed according to `allowed_input_type`.
        return alternative.set_holey_float64(
            AddNewNodeNoInputConversion<UnsafeFloat64ToHoleyFloat64>(
                {float64}));
      }
      return alternative.set_float64(float64);
    }
    case ValueRepresentation::kFloat64:
      DCHECK_EQ(use_rep, UseRepresentation::kHoleyFloat64);
      // We only set the holey_float64 alternative if all feasible values are
      // allowed according to `allowed_input_type`.
      return alternative.set_holey_float64(
          AddNewNodeNoInputConversion<ChangeFloat64ToHoleyFloat64>({value}));
    case ValueRepresentation::kHoleyFloat64: {
      DCHECK_EQ(use_rep, UseRepresentation::kFloat64);
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
        case NodeType::kNumberOrUndefined:
          return AddNewNodeNoInputConversion<UnsafeHoleyFloat64ToFloat64>(
              {value});
        case NodeType::kNumberOrOddball:
          // NumberOrOddball->Float64 conversions are not exact alternatives,
          // since they lose the information that this is an oddball, so they
          // cannot become the canonical float64_alternative.
          return AddNewNodeNoInputConversion<HoleyFloat64ToSilencedFloat64>(
              {value});
        default:
          UNREACHABLE();
      }
    }
    case ValueRepresentation::kIntPtr: {
      ValueNode* float64 =
          AddNewNodeNoInputConversion<ChangeIntPtrToFloat64>({value});
      if (use_rep == UseRepresentation::kHoleyFloat64) {
        // We only set the holey_float64 alternative if all feasible values are
        // allowed according to `allowed_input_type`.
        return alternative.set_holey_float64(
            AddNewNodeNoInputConversion<UnsafeFloat64ToHoleyFloat64>(
                {float64}));
      }
      return alternative.set_float64(float64);
    }
    case ValueRepresentation::kNone:
    case ValueRepresentation::kRawPtr:
      UNREACHABLE();
  }
  UNREACHABLE();
}

template <typename BaseT>
ValueNode* MaglevReducer<BaseT>::TryGetFloat64ForToNumber(
    ValueNode* value, NodeType allowed_input_type) {
  if (value->is_float64()) return value;

  if (auto cst = TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kFloat64, value,
          GetTaggedToFloat64ConversionType(allowed_input_type))) {
    return graph()->GetFloat64Constant(cst.value());
  }

  if (ValueNode* alt = known_node_aspects().TryGetAlternativeFor(
          value, UseRepresentation::kFloat64)) {
    return alt;
  }

  return nullptr;
}

template <typename BaseT>
ValueNode* MaglevReducer<BaseT>::TryGetFloat64(ValueNode* value) {
  return TryGetFloat64ForToNumber(value, NodeType::kNumber);
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::GetFloat64(ValueNode* value) {
  value->MaybeRecordUseReprHint(UseRepresentation::kFloat64);
  return GetFloat64ForToNumber(value, NodeType::kNumber);
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::GetFloat64ForToNumber(
    ValueNode* value, NodeType allowed_input_type) {
  value->MaybeRecordUseReprHint(UseRepresentation::kFloat64);
  return GetFloat64OrHoleyFloat64Impl(value, UseRepresentation::kFloat64,
                                      allowed_input_type);
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::GetHoleyFloat64(ValueNode* value) {
  value->MaybeRecordUseReprHint(UseRepresentation::kHoleyFloat64);
  return GetFloat64OrHoleyFloat64Impl(value, UseRepresentation::kHoleyFloat64,
                                      NodeType::kNumberOrUndefined);
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::GetHoleyFloat64ForToNumber(
    ValueNode* value, NodeType allowed_input_type) {
  value->MaybeRecordUseReprHint(UseRepresentation::kHoleyFloat64);
  return GetFloat64OrHoleyFloat64Impl(value, UseRepresentation::kHoleyFloat64,
                                      allowed_input_type);
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::EnsureInt32(ValueNode* value,
                                               bool can_be_heap_number) {
  // Either the value is Int32 already, or we force a conversion to Int32 and
  // cache the value in its alternative representation node.
  return GetInt32(value, can_be_heap_number);
}

template <typename BaseT>
std::optional<Float64>
MaglevReducer<BaseT>::TryGetFloat64OrHoleyFloat64Constant(
    UseRepresentation use_repr, ValueNode* value,
    TaggedToFloat64ConversionType conversion_type) {
  DCHECK(use_repr == UseRepresentation::kFloat64 ||
         use_repr == UseRepresentation::kHoleyFloat64);
  switch (value->opcode()) {
    case Opcode::kHeapConstant: {
      compiler::ObjectRef object = value->Cast<HeapConstant>()->object();
      if (object.IsHeapNumber()) {
        double cst = object.AsHeapNumber().value();
        if (std::isnan(cst)) {
          return Float64::quiet_nan();
        }
        return Float64{cst};
      }
      // Oddballs should be RootConstants.
      DCHECK(!IsOddball(*object.object()));
      return {};
    }
    case Opcode::kInt32Constant:
      return Float64{
          static_cast<double>(value->Cast<Int32Constant>()->value())};
    case Opcode::kUint32Constant:
      return Float64{
          static_cast<double>(value->Cast<Uint32Constant>()->value())};
    case Opcode::kSmiConstant:
      return Float64{
          static_cast<double>(value->Cast<SmiConstant>()->value().value())};
    case Opcode::kFloat64Constant: {
      Float64 cst = value->Cast<Float64Constant>()->value();
      if (use_repr == UseRepresentation::kFloat64) return cst;
      // TODO(nicohartmann): We could optimize the HoleyFloat64 case here, too.
      return {};
    }
    case Opcode::kHoleyFloat64Constant: {
      Float64 cst = value->Cast<HoleyFloat64Constant>()->value();
      if (use_repr == UseRepresentation::kHoleyFloat64) return cst;
      // TODO(nicohartmann): We could optimize the Float64 case here, too.
      return {};
    }
    case Opcode::kRootConstant: {
      Tagged<Object> root_object =
          broker()->local_isolate()->root(value->Cast<RootConstant>()->index());
      if (conversion_type == TaggedToFloat64ConversionType::kNumberOrBoolean &&
          IsBoolean(root_object)) {
        return Float64{Cast<Oddball>(root_object)->to_number_raw()};
      }
      if (conversion_type ==
              TaggedToFloat64ConversionType::kNumberOrUndefined &&
          IsUndefined(root_object)) {
#ifdef V8_ENABLE_UNDEFINED_DOUBLE
        // We use the undefined nan and silence it to produce the same result
        // as a computation from non-constants would.
        auto ud = Float64::undefined_nan();
        if (use_repr != UseRepresentation::kHoleyFloat64) {
          ud = ud.to_quiet_nan();
        }
        return ud;
#else
        return Float64::FromBits(base::double_to_uint64(
            Cast<Oddball>(root_object)->to_number_raw()));
#endif
      }
      if (conversion_type == TaggedToFloat64ConversionType::kNumberOrOddball &&
          IsOddball(root_object)) {
#ifdef V8_ENABLE_UNDEFINED_DOUBLE
        if (IsUndefined(root_object)) {
          // We use the undefined nan and silence it to produce the same result
          // as a computation from non-constants would.
          auto ud = Float64::undefined_nan();
          if (use_repr != UseRepresentation::kHoleyFloat64) {
            ud = ud.to_quiet_nan();
          }
          return ud;
        }
#endif  // V8_ENABLE_UNDEFINED_DOUBLE
        return Float64::FromBits(base::double_to_uint64(
            Cast<Oddball>(root_object)->to_number_raw()));
      }
      if (IsHeapNumber(root_object)) {
        double cst = Cast<HeapNumber>(root_object)->value();
        if (std::isnan(cst)) {
          return Float64::quiet_nan();
        }
        return Float64{cst};
      }
      return {};
    }
    default:
      break;
  }
  if (auto c = TryGetConstantAlternative(value)) {
    return TryGetFloat64OrHoleyFloat64Constant(use_repr, *c, conversion_type);
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

#define TRACE_CANNOT_INLINE(...) \
  TRACE_INLINING(TraceSkip(shared) << __VA_ARGS__)

template <typename BaseT>
bool MaglevReducer<BaseT>::CanInlineCall(
    const MaglevCompilationUnit* current_unit,
    compiler::SharedFunctionInfoRef shared, float call_frequency) {
  auto tracer_ = tracer();
  if (static_cast<int>(graph()->inlined_functions().size()) >=
      SourcePosition::MaxInliningId()) {
    graph()->compilation_info()->set_could_not_inline_all_candidates();
    TRACE_CANNOT_INLINE("maximum inlining ids");
    return false;
  }

  if (current_unit && current_unit->shared_function_info().equals(shared)) {
    TRACE_CANNOT_INLINE("direct recursion");
    return false;
  }
  SharedFunctionInfo::Inlineability inlineability =
      shared.GetInlineability(CodeKind::MAGLEV, broker());
  if (inlineability != SharedFunctionInfo::Inlineability::kIsInlineable) {
    TRACE_CANNOT_INLINE(inlineability);
    return false;
  }
  compiler::BytecodeArrayRef bytecode = shared.GetBytecodeArray(broker());
  const CompilationFlags& flags = graph()->compilation_info()->flags();
  if (call_frequency < flags.min_inlining_frequency) {
    TRACE_CANNOT_INLINE("call frequency ("
                        << call_frequency << ") < minimum threshold ("
                        << flags.min_inlining_frequency << ")");
    return false;
  }
  if (bytecode.length() > flags.max_inlined_bytecode_size) {
    TRACE_CANNOT_INLINE("big function, size ("
                        << bytecode.length() << ") >= max-size ("
                        << flags.max_inlined_bytecode_size << ")");
    return false;
  }
  return true;
}
#undef TRACE_CANNOT_INLINE

template <typename BaseT>
template <typename MapContainer>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldCheckConstantMaps(
    compiler::MapRef map, const MapContainer& maps) {
  if (!base::contains(maps, map)) {
    return EmitUnconditionalDeopt(DeoptimizeReason::kWrongMap);
  }
  if (map.IsHeapNumberMap()) return ReduceResult::Done();
  if (map.is_stable()) {
    broker()->dependencies()->DependOnStableMap(map);
    return ReduceResult::Done();
  }
  return {};
}

template <typename BaseT>
template <typename MapContainer>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldCheckConstantMaps(
    ValueNode* object, const MapContainer& maps) {
  // For constants with stable maps that match one of the desired maps, we
  // don't need to emit a map check, and can use the dependency -- we
  // can't do this for unstable maps because the constant could migrate
  // during compilation.
  if (compiler::OptionalHeapObjectRef constant =
          TryGetConstant<HeapObject>(object)) {
    return TryFoldCheckConstantMaps(constant->map(broker()), maps);
  }

  if (NodeTypeIs(GetType(object), NodeType::kNumber)) {
    compiler::MapRef heap_number_map =
        MakeRef(broker(), local_isolate()->factory()->heap_number_map());
    return TryFoldCheckConstantMaps(heap_number_map, maps);
  }

  // TODO(verwaest): Support other objects with possible known stable maps as
  // well.

  return {};
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildCheckMaps(
    ValueNode* object, base::Vector<const compiler::MapRef> maps,
    std::optional<ValueNode*> map,
    bool has_deprecated_map_without_migration_target,
    bool migration_done_outside) {
  KnownMapsMerger<base::Vector<const compiler::MapRef>> merger(broker(), zone(),
                                                               maps);
  RETURN_IF_DONE(TryFoldCheckMaps(object, nullptr, maps, merger));

  NodeInfo* known_info = GetOrCreateInfoFor(object);

  // TODO(v8:7700): Check if the {maps} - {known_maps} size is smaller than
  // {maps} \intersect {known_maps}, we can emit CheckNotMaps instead.

  // Emit checks.
  if (merger.emit_check_with_migration() && !migration_done_outside) {
    RETURN_IF_ABORT(AddNewNode<CheckMapsWithMigration>(
        {object}, merger.intersect_set(),
        GetCheckType(known_info->type(), object)));
  } else if (has_deprecated_map_without_migration_target &&
             !migration_done_outside) {
    RETURN_IF_ABORT(AddNewNode<CheckMapsWithMigrationAndDeopt>(
        {object}, merger.intersect_set(),
        GetCheckType(known_info->type(), object)));
  } else if (map) {
    RETURN_IF_ABORT(AddNewNode<CheckMapsWithAlreadyLoadedMap>(
        {object, *map}, merger.intersect_set()));
  } else {
    RETURN_IF_ABORT(
        AddNewNode<CheckMaps>({object}, merger.intersect_set(),
                              GetCheckType(known_info->type(), object)));
  }

  merger.UpdateKnownNodeAspects(object, known_node_aspects());
  return ReduceResult::Done();
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildCheckValueByReference(
    ValueNode* context, ValueNode* node, compiler::HeapObjectRef ref,
    DeoptimizeReason reason) {
  DCHECK(!ref.IsSmi());
  DCHECK(!ref.IsHeapNumber());

  if (!IsInstanceOfNodeType(ref.map(broker()), GetType(node), broker())) {
    return EmitUnconditionalDeopt(reason);
  }
  if (compiler::OptionalHeapObjectRef maybe_constant =
          TryGetConstant<HeapObject>(node)) {
    if (maybe_constant.value().equals(ref)) {
      return ReduceResult::Done();
    }
    return EmitUnconditionalDeopt(reason);
  }
  RETURN_IF_ABORT(AddNewNode<CheckValue>({node}, ref, reason));
  SetKnownValue(node, ref, StaticTypeForConstant(broker(), ref));

  return ReduceResult::Done();
}

template <typename BaseT>
void MaglevReducer<BaseT>::SetKnownValue(ValueNode* node,
                                         compiler::ObjectRef ref,
                                         NodeType new_node_type) {
  DCHECK(!node->Is<HeapConstant>());
  DCHECK(!node->Is<RootConstant>());
  NodeInfo* known_info = GetOrCreateInfoFor(node);
  // ref type should be compatible with type.
  DCHECK(NodeTypeIs(StaticTypeForConstant(broker(), ref), new_node_type));
  if (ref.IsHeapObject()) {
    DCHECK(IsInstanceOfNodeType(ref.AsHeapObject().map(broker()),
                                known_info->type(), broker()));
  } else {
    DCHECK(!NodeTypeIs(known_info->type(), NodeType::kAnyHeapObject));
  }
  known_info->IntersectType(new_node_type);
  known_info->alternative().set_checked_value(GetConstant(ref));
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldTestUndetectable(
    ValueNode* value) {
  if (value->properties().value_representation() ==
      ValueRepresentation::kHoleyFloat64) {
#ifdef V8_ENABLE_UNDEFINED_DOUBLE
    return AddNewNodeNoInputConversion<HoleyFloat64IsUndefinedOrHole>({value});
#else
    return AddNewNodeNoInputConversion<HoleyFloat64IsHole>({value});
#endif  // V8_ENABLE_UNDEFINED_DOUBLE
  } else if (value->properties().value_representation() !=
             ValueRepresentation::kTagged) {
    return GetBooleanConstant(false);
  }

  if (auto maybe_constant = TryGetConstant<HeapObject>(value)) {
    auto map = maybe_constant.value().map(broker());
    return GetBooleanConstant(map.is_undetectable());
  }

  NodeType node_type;
  if (CheckType(value, NodeType::kSmi, &node_type)) {
    return GetBooleanConstant(false);
  }

  MapInference<MaglevReducer<BaseT>> inference(
      this, value, MapInference<MaglevReducer<BaseT>>::kOnlyFresh);
  if (auto possible_maps = inference.TryGetPossibleMaps()) {
    DCHECK_GT(possible_maps->size(), 0);
    bool first_is_undetectable = possible_maps->at(0).is_undetectable();
    bool all_the_same_value =
        std::all_of(possible_maps->begin(), possible_maps->end(),
                    [first_is_undetectable](compiler::MapRef map) {
                      bool is_undetectable = map.is_undetectable();
                      return (first_is_undetectable && is_undetectable) ||
                             (!first_is_undetectable && !is_undetectable);
                    });
    if (all_the_same_value) {
      return GetBooleanConstant(first_is_undetectable);
    }
  }

  return {};
}

template <typename BaseT>
template <bool flip>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldToBoolean(ValueNode* value) {
  if (IsConstantNode(value->opcode())) {
    return GetBooleanConstant(FromConstantToBool(local_isolate(), value) ^
                              flip);
  }

  switch (value->value_representation()) {
    case ValueRepresentation::kHoleyFloat64:
      value = AddNewNodeNoInputConversion<UnsafeHoleyFloat64ToFloat64>({value});
      [[fallthrough]];
    case ValueRepresentation::kFloat64:
      return AddNewNodeNoInputConversion<Float64ToBoolean>({value}, flip);

    case ValueRepresentation::kUint32:
      value = AddNewNodeNoInputConversion<TruncateUint32ToInt32>({value});
      [[fallthrough]];
    case ValueRepresentation::kInt32:
      return AddNewNodeNoInputConversion<Int32ToBoolean>({value}, flip);

    case ValueRepresentation::kIntPtr:
      return AddNewNodeNoInputConversion<IntPtrToBoolean>({value}, flip);

    case ValueRepresentation::kTagged:
      break;
    case ValueRepresentation::kRawPtr:
    case ValueRepresentation::kNone:
      UNREACHABLE();
  }

  NodeInfo* node_info = known_node_aspects().TryGetInfoFor(value);
  if (node_info) {
    if (ValueNode* as_int32 = node_info->alternative().int32()) {
      return AddNewNodeNoInputConversion<Int32ToBoolean>({as_int32}, flip);
    }
    if (ValueNode* as_float64 = node_info->alternative().float64()) {
      return AddNewNodeNoInputConversion<Float64ToBoolean>({as_float64}, flip);
    }
  }

  NodeType value_type;
  if (CheckType(value, NodeType::kJSReceiver, &value_type)) {
    auto test_result = TryFoldTestUndetectable(value);
    if (test_result.IsDoneWithAbort()) return test_result;
    ValueNode* result;
    if (test_result.HasValue()) {
      result = test_result.value();
    } else {
      result = AddNewNodeNoInputConversion<TestUndetectable>(
          {value}, CheckType::kOmitHeapObjectCheck);
    }
    if constexpr (!flip) {
      auto not_result = TryFoldLogicalNot(result);
      if (not_result.HasValue()) {
        return not_result.value();
      }
      return AddNewNode<LogicalNot>({result});
    }
    return result;
  }

  ValueNode* falsy_value = nullptr;
  if (CheckType(value, NodeType::kString)) {
    falsy_value = graph()->GetRootConstant(RootIndex::kempty_string);
  } else if (CheckType(value, NodeType::kSmi)) {
    falsy_value = graph()->GetSmiConstant(0);
  }
  if (falsy_value != nullptr) {
    return AddNewNode<std::conditional_t<flip, TaggedEqual, TaggedNotEqual>>(
        {value, falsy_value});
  }

  if (CheckType(value, NodeType::kBoolean)) {
    if constexpr (flip) {
      auto not_result = TryFoldLogicalNot(value);
      if (not_result.HasValue()) {
        return not_result.value();
      }
      return AddNewNode<LogicalNot>({value});
    }
    return value;
  }

  return {};
}

template <typename BaseT>
template <typename MapContainer>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldCheckMaps(
    ValueNode* object, ValueNode* object_map, const MapContainer& maps,
    KnownMapsMerger<MapContainer>& merger) {
  RETURN_IF_DONE(TryFoldCheckConstantMaps(object, maps));
  if (object_map) {
    if (compiler::OptionalHeapObjectRef constant =
            TryGetConstant<HeapObject>(object_map)) {
      CHECK(constant->IsMap());
      RETURN_IF_DONE(TryFoldCheckConstantMaps(constant->AsMap(), maps));
    }
  }

  // Calculates if known maps are a subset of maps, their map intersection and
  // whether we should emit check with migration.
  merger.IntersectWithKnownNodeAspects(object, known_node_aspects());

  if (IsEmptyNodeType(IntersectType(merger.node_type(), GetType(object)))) {
    return EmitUnconditionalDeopt(DeoptimizeReason::kWrongMap);
  }

  // If the known maps are the subset of the maps to check, we are done.
  if (merger.known_maps_are_subset_of_requested_maps()) {
    // The node type of known_info can get out of sync with the possible maps.
    // For instance after merging with an effectively dead branch (i.e., check
    // contradicting all possible maps).
    // TODO(olivf) Try to combine node_info and possible maps and ensure that
    // narrowing the type also clears impossible possible_maps.
    NodeInfo* known_info = GetOrCreateInfoFor(object);
    if (!NodeTypeIs(known_info->type(), merger.node_type())) {
      known_info->UnionType(merger.node_type());
    }
#ifdef DEBUG
    // Double check that, for every possible map, it's one of the maps we'd
    // want to check.
    for (compiler::MapRef possible_map :
         known_node_aspects().TryGetInfoFor(object)->possible_maps()) {
      DCHECK_NE(std::find(maps.begin(), maps.end(), possible_map), maps.end());
    }
#endif
    return ReduceResult::Done();
  }

  if (merger.intersect_set().is_empty()) {
    return EmitUnconditionalDeopt(DeoptimizeReason::kWrongMap);
  }

  return {};
}

template <typename BaseT>
typename MaglevReducer<BaseT>::InferHasInPrototypeChainResult
MaglevReducer<BaseT>::InferHasInPrototypeChain(
    ValueNode* receiver, compiler::HeapObjectRef prototype) {
  MapInference<MaglevReducer<BaseT>> inference(this, receiver);
  auto possible_maps = inference.TryGetPossibleMaps();
  // If the map set is not found, then we don't know anything about the map of
  // the receiver, so bail.
  if (!possible_maps) {
    return kMayBeInPrototypeChain;
  }

  // If the set of possible maps is empty, then there's no possible map for this
  // receiver, therefore this path is unreachable at runtime. We're unlikely to
  // ever hit this case, BuildCheckMaps should already unconditionally deopt,
  // but check it in case another checking operation fails to statically
  // unconditionally deopt.
  if (possible_maps->is_empty()) {
    // TODO(leszeks): Add an unreachable assert here.
    return kIsNotInPrototypeChain;
  }

  // Since we only consider fresh maps, it is not necessary to emit map checks.
  const bool all_maps_are_fresh = inference.all_maps_are_fresh();

  ZoneVector<compiler::MapRef> receiver_map_refs(zone());

  // Try to determine either that all of the {receiver_maps} have the given
  // {prototype} in their chain, or that none do. If we can't tell, return
  // kMayBeInPrototypeChain.
  bool all = true;
  bool none = true;
  for (compiler::MapRef map : *possible_maps) {
    receiver_map_refs.push_back(map);
    if (!all_maps_are_fresh && !map.is_stable()) {
      return kMayBeInPrototypeChain;
    }
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
  DCHECK(!receiver_map_refs.empty());
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
      if (!prototype.IsJSObject() || !prototype.map(broker()).is_stable()) {
        return kMayBeInPrototypeChain;
      }
      last_prototype = prototype.AsJSObject();
    }
    // TODO(jgruber): Investigate whether it's possible for all_maps_are_fresh
    // to be false here, considering we bail out on unstable maps above. For
    // now, match TurboFan behavior in
    // JSNativeContextSpecialization::InferHasInPrototypeChain.
    WhereToStart start =
        all_maps_are_fresh ? kStartAtPrototype : kStartAtReceiver;
    broker()->dependencies()->DependOnStablePrototypeChains(
        receiver_map_refs, start, last_prototype);
  }

  DCHECK_EQ(all, !none);
  return all ? kIsInPrototypeChain : kIsNotInPrototypeChain;
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryBuildFastHasInPrototypeChain(
    ValueNode* object, compiler::HeapObjectRef prototype) {
  auto in_prototype_chain = InferHasInPrototypeChain(object, prototype);
  if (in_prototype_chain == kMayBeInPrototypeChain) return {};

  return GetBooleanConstant(in_prototype_chain == kIsInPrototypeChain);
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryBuildFastOrdinaryHasInstance(
    ValueNode* context, ValueNode* object, compiler::JSObjectRef callable,
    ValueNode* callable_node_if_not_constant) {
  const bool is_constant = callable_node_if_not_constant == nullptr;
  if (!is_constant) return {};

  if (callable.IsJSBoundFunction()) {
    compiler::JSBoundFunctionRef function = callable.AsJSBoundFunction();
    compiler::JSReceiverRef bound_target_function =
        function.bound_target_function(broker());

    if (bound_target_function.IsJSObject()) {
      RETURN_IF_DONE(TryBuildFastInstanceOf(
          context, object, bound_target_function.AsJSObject(), nullptr));
    }

    return BuildCallBuiltinWithTaggedInputs<Builtin::kInstanceOf>(
        context, {object, GetConstant(bound_target_function)});
  }

  if (callable.IsJSFunctionWithPrototype()) {
    compiler::JSFunctionRef function = callable.AsJSFunction();

    if (!function.has_instance_prototype(broker()) ||
        function.PrototypeRequiresRuntimeLookup(broker())) {
      return {};
    }

    compiler::HeapObjectRef prototype =
        broker()->dependencies()->DependOnPrototypeProperty(function);
    RETURN_IF_DONE(TryBuildFastHasInPrototypeChain(object, prototype));
    return AddNewNode<HasInPrototypeChain>({object}, prototype);
  }

  return {};
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildOrdinaryHasInstance(
    ValueNode* context, ValueNode* object, compiler::JSObjectRef callable,
    ValueNode* callable_node_if_not_constant) {
  RETURN_IF_DONE(TryBuildFastOrdinaryHasInstance(
      context, object, callable, callable_node_if_not_constant));

  return BuildCallBuiltinWithTaggedInputs<Builtin::kOrdinaryHasInstance>(
      context, {callable_node_if_not_constant ? callable_node_if_not_constant
                                              : GetConstant(callable),
                object});
}

template <typename BaseT>
template <bool flip>
ReduceResult MaglevReducer<BaseT>::BuildToBoolean(ValueNode* value) {
  RETURN_IF_DONE(TryFoldToBoolean<flip>(value));

  // Note that we don't pass {value} to GetCheckType since
  // PhiRepresentationSelection has a special-case to optimize
  // ToBoolean/ToBooleanLogicalNot whose inputs are untagged phis, and thus we
  // don't need to preserve HeapObjectness here.
  constexpr ValueNode* kTargetForCheckType = nullptr;
  return AddNewNode<std::conditional_t<flip, ToBooleanLogicalNot, ToBoolean>>(
      {value}, GetCheckType(GetType(value), kTargetForCheckType));
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryBuildFastInstanceOf(
    ValueNode* context, ValueNode* object, compiler::JSObjectRef callable,
    ValueNode* callable_node_if_not_constant) {
  compiler::MapRef receiver_map = callable.map(broker());
  compiler::NameRef name = broker()->has_instance_symbol();
  compiler::PropertyAccessInfo access_info = broker()->GetPropertyAccessInfo(
      receiver_map, name, compiler::AccessMode::kLoad);

  if (access_info.IsInvalid() || access_info.HasDictionaryHolder()) {
    return {};
  }
  access_info.RecordDependencies(broker()->dependencies());

  if (access_info.IsNotFound()) {
    if (!receiver_map.is_callable()) return {};

    broker()->dependencies()->DependOnStablePrototypeChains(
        access_info.lookup_start_object_maps(), kStartAtPrototype);

    if (callable_node_if_not_constant) {
      RETURN_IF_ABORT(BuildCheckMaps(
          callable_node_if_not_constant,
          base::VectorOf(access_info.lookup_start_object_maps())));
    } else {
      if (receiver_map.is_stable()) {
        broker()->dependencies()->DependOnStableMap(receiver_map);
      } else {
        RETURN_IF_ABORT(BuildCheckMaps(
            GetConstant(callable),
            base::VectorOf(access_info.lookup_start_object_maps())));
      }
    }

    return BuildOrdinaryHasInstance(context, object, callable,
                                    callable_node_if_not_constant);
  }

  if constexpr (!ReducerBaseCanBuildCall<BaseT>) {
    return {};
  }

  if (access_info.IsFastDataConstant()) {
    compiler::OptionalJSObjectRef holder = access_info.holder();
    bool found_on_proto = holder.has_value();
    compiler::JSObjectRef holder_ref =
        found_on_proto ? holder.value() : callable;
    if (access_info.field_representation().IsDouble()) return {};
    compiler::OptionalObjectRef has_instance_field =
        holder_ref.GetOwnFastConstantDataProperty(
            broker(), access_info.field_representation(),
            access_info.field_index(), broker()->dependencies());
    if (!has_instance_field.has_value() ||
        !has_instance_field->IsHeapObject() ||
        !has_instance_field->AsHeapObject().map(broker()).is_callable()) {
      return {};
    }

    if (!has_instance_field->IsJSFunction()) return {};

    if (found_on_proto) {
      broker()->dependencies()->DependOnStablePrototypeChains(
          access_info.lookup_start_object_maps(), kStartAtPrototype,
          holder.value());
    }

    ValueNode* callable_node;
    if (callable_node_if_not_constant) {
      RETURN_IF_ABORT(
          BuildCheckValueByReference(context, callable_node_if_not_constant,
                                     callable, DeoptimizeReason::kWrongValue));
      callable_node = callable_node_if_not_constant;
    } else {
      callable_node = GetConstant(callable);
    }

    RETURN_IF_ABORT(BuildCheckMaps(
        callable_node, base::VectorOf(access_info.lookup_start_object_maps())));

    if (has_instance_field->IsJSFunction()) {
      compiler::SharedFunctionInfoRef shared =
          has_instance_field->AsJSFunction().shared(broker());
      if (shared.HasBuiltinId() &&
          shared.builtin_id() == Builtin::kFunctionPrototypeHasInstance) {
        return BuildOrdinaryHasInstance(context, object, callable,
                                        callable_node_if_not_constant);
      }
    }

    // TODO(victorgomes): Add support in MaglevReducer for generic calls.
    if constexpr (ReducerBaseCanBuildCall<BaseT>) {
      typename BaseT::CallArguments args(
          ConvertReceiverMode::kNotNullOrUndefined, {callable_node, object});
      ValueNode* call_result;
      {
        typename BaseT::LazyDeoptFrameScope continuation_scope(
            base_, Builtin::kToBooleanLazyDeoptContinuation);

        if (has_instance_field->IsJSFunction()) {
          GET_VALUE_OR_ABORT(call_result,
                             base_->TryReduceCallForConstant(
                                 has_instance_field->AsJSFunction(), args));
        } else {
          GET_VALUE_OR_ABORT(call_result, base_->BuildGenericCall(
                                              GetConstant(*has_instance_field),
                                              Call::TargetType::kAny, args));
        }
      }
      return BuildToBoolean(call_result);
    }

    UNREACHABLE();
  }

  return {};
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryBuildFastInstanceOfWithFeedback(
    ValueNode* context, ValueNode* object, ValueNode* callable,
    compiler::FeedbackSource feedback_source) {
  compiler::ProcessedFeedback const& feedback =
      broker()->GetFeedbackForInstanceOf(feedback_source);

  if (feedback.IsInsufficient()) {
    return EmitUnconditionalDeopt(
        DeoptimizeReason::kInsufficientTypeFeedbackForInstanceOf);
  }

  // Check if the right hand side is a known receiver, or
  // we have feedback from the InstanceOfIC.
  if (compiler::OptionalJSObjectRef maybe_constant =
          TryGetConstant<JSObject>(callable)) {
    return TryBuildFastInstanceOf(context, object, maybe_constant.value(),
                                  nullptr);
  }
  if (feedback_source.IsValid()) {
    compiler::OptionalJSObjectRef callable_from_feedback =
        feedback.AsInstanceOf().value();
    if (callable_from_feedback) {
      return TryBuildFastInstanceOf(context, object, *callable_from_feedback,
                                    callable);
    }
  }
  return {};
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildSmiUntag(
    ValueNode* node, AllowWideningSmiToInt32 allow_widening_smi_to_int32) {
  // This is called when converting inputs in AddNewNode. We might already have
  // an empty type for `node` here. Make sure we don't add unsafe conversion
  // nodes in that case by checking for the empty node type explicitly.
  if (IsEmptyNodeType(GetType(node, allow_widening_smi_to_int32))) {
    return EmitUnconditionalDeopt(DeoptimizeReason::kNotASmi);
  }
  if (EnsureType(node, NodeType::kSmi)) {
    if (SmiValuesAre31Bits()) {
      if (auto phi = node->TryCast<Phi>()) {
        phi->SetUseRequiresSmi();
      }
    }
    return AddNewNodeNoInputConversion<UnsafeSmiUntag>({node});
  } else {
    return AddNewNodeNoInputConversion<CheckedSmiUntag>({node});
  }
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildNumberOrOddballToFloat64OrHoleyFloat64(
    ValueNode* node, UseRepresentation use_rep, NodeType allowed_input_type) {
  DCHECK(use_rep == UseRepresentation::kFloat64 ||
         use_rep == UseRepresentation::kHoleyFloat64);
  NodeType old_type;
  TaggedToFloat64ConversionType conversion_type =
      GetTaggedToFloat64ConversionType(allowed_input_type);
  if (EnsureType(node, allowed_input_type, &old_type)) {
    if (old_type == NodeType::kSmi) {
      ValueNode* untagged_smi;
      GET_VALUE_OR_ABORT(untagged_smi, BuildSmiUntag(node));
      ValueNode* float64;
      GET_VALUE_OR_ABORT(float64,
                         AddNewNode<ChangeInt32ToFloat64>({untagged_smi}));
      if (use_rep == UseRepresentation::kFloat64) return float64;
      return AddNewNode<UnsafeFloat64ToHoleyFloat64>({float64});
    }
    if (conversion_type == TaggedToFloat64ConversionType::kOnlyNumber) {
      ValueNode* float64;
      GET_VALUE_OR_ABORT(float64, AddNewNode<UnsafeNumberToFloat64>({node}));
      if (use_rep == UseRepresentation::kFloat64) return float64;
      return AddNewNode<ChangeFloat64ToHoleyFloat64>({float64});
    } else {
      if (use_rep == UseRepresentation::kHoleyFloat64) {
        return AddNewNode<UnsafeNumberOrOddballToHoleyFloat64>({node},
                                                               conversion_type);
      }
      return AddNewNode<UnsafeNumberOrOddballToFloat64>({node},
                                                        conversion_type);
    }
  } else {
    if (conversion_type == TaggedToFloat64ConversionType::kOnlyNumber) {
      ValueNode* float64;
      GET_VALUE_OR_ABORT(float64, AddNewNode<CheckedNumberToFloat64>({node}));
      if (use_rep == UseRepresentation::kFloat64) return float64;
      return AddNewNode<ChangeFloat64ToHoleyFloat64>({node});
    } else {
      if (use_rep == UseRepresentation::kHoleyFloat64) {
        return AddNewNode<CheckedNumberOrOddballToHoleyFloat64>(
            {node}, conversion_type);
      }
      return AddNewNode<CheckedNumberOrOddballToFloat64>({node},
                                                         conversion_type);
    }
  }
}

template <typename BaseT>
template <Builtin kBuiltin>
void MaglevReducer<BaseT>::SetCallBuiltinFeedback(
    CallBuiltin* call_builtin, compiler::FeedbackSource const& feedback,
    CallBuiltin::FeedbackSlotType slot_type) {
  call_builtin->set_feedback(feedback, slot_type);
#ifdef DEBUG
  using Descriptor = typename CallInterfaceDescriptorFor<kBuiltin>::type;
  int slot_index = call_builtin->InputCountWithoutContext();
  int vector_index = slot_index + 1;
  DCHECK_EQ(slot_index, Descriptor::kSlot);
  // TODO(victorgomes): Rename all kFeedbackVector parameters in the builtins
  // to kVector.
  DCHECK_EQ(vector_index, Descriptor::kVector);
#endif  // DEBUG
}

template <typename BaseT>
template <Builtin kBuiltin>
ReduceResult MaglevReducer<BaseT>::BuildCallBuiltinWithTaggedInputs(
    std::initializer_list<ValueNode*> inputs) {
  using Descriptor = typename CallInterfaceDescriptorFor<kBuiltin>::type;
  static_assert(!Descriptor::HasContextParameter());
  return AddNewNode<CallBuiltin>(
      inputs.size(),
      [&](CallBuiltin* call_builtin) {
        int arg_index = 0;
        for (auto* input : inputs) {
          ValueNode* tagged_arg;
          GET_VALUE_OR_ABORT(tagged_arg, GetTaggedValue(input));
          call_builtin->set_arg(arg_index++, tagged_arg);
        }
        return ReduceResult::Done();
      },
      kBuiltin);
}

template <typename BaseT>
template <Builtin kBuiltin>
ReduceResult MaglevReducer<BaseT>::BuildCallBuiltinWithTaggedInputs(
    ValueNode* context, std::initializer_list<ValueNode*> inputs) {
  using Descriptor = typename CallInterfaceDescriptorFor<kBuiltin>::type;
  static_assert(Descriptor::HasContextParameter());
  return AddNewNode<CallBuiltin>(
      inputs.size() + 1,
      [&](CallBuiltin* call_builtin) {
        int arg_index = 0;
        for (auto* input : inputs) {
          ValueNode* tagged_arg;
          GET_VALUE_OR_ABORT(tagged_arg, GetTaggedValue(input));
          call_builtin->set_arg(arg_index++, tagged_arg);
        }
        return ReduceResult::Done();
      },
      kBuiltin, context);
}

template <typename BaseT>
template <Builtin kBuiltin>
ReduceResult MaglevReducer<BaseT>::BuildCallBuiltinWithTaggedInputs(
    std::initializer_list<ValueNode*> inputs,
    compiler::FeedbackSource const& feedback,
    CallBuiltin::FeedbackSlotType slot_type) {
  ReduceResult result = BuildCallBuiltinWithTaggedInputs<kBuiltin>(inputs);
  RETURN_IF_ABORT(result);
  SetCallBuiltinFeedback<kBuiltin>(result.value()->Cast<CallBuiltin>(),
                                   feedback, slot_type);
  return result;
}

template <typename BaseT>
template <Builtin kBuiltin>
ReduceResult MaglevReducer<BaseT>::BuildCallBuiltinWithTaggedInputs(
    ValueNode* context, std::initializer_list<ValueNode*> inputs,
    compiler::FeedbackSource const& feedback,
    CallBuiltin::FeedbackSlotType slot_type) {
  ReduceResult result =
      BuildCallBuiltinWithTaggedInputs<kBuiltin>(context, inputs);
  RETURN_IF_ABORT(result);
  SetCallBuiltinFeedback<kBuiltin>(result.value()->Cast<CallBuiltin>(),
                                   feedback, slot_type);
  return result;
}

template <typename BaseT>
template <Builtin kBuiltin>
CallBuiltin* MaglevReducer<BaseT>::BuildCallBuiltin(
    std::initializer_list<ValueNode*> inputs) {
  using Descriptor = typename CallInterfaceDescriptorFor<kBuiltin>::type;
  static_assert(!Descriptor::HasContextParameter());
  ReduceResult result = AddNewNode<CallBuiltin>(
      inputs.size(),
      [&](CallBuiltin* call_builtin) {
        int arg_index = 0;
        for (auto* input : inputs) {
          call_builtin->set_arg(arg_index++, input);
        }
        return ReduceResult::Done();
      },
      kBuiltin);
  CHECK(result.IsDoneWithValue());
  return result.value()->template Cast<CallBuiltin>();
}

template <typename BaseT>
template <Builtin kBuiltin>
CallBuiltin* MaglevReducer<BaseT>::BuildCallBuiltin(
    ValueNode* context, std::initializer_list<ValueNode*> inputs) {
  using Descriptor = typename CallInterfaceDescriptorFor<kBuiltin>::type;
  static_assert(Descriptor::HasContextParameter());
  ReduceResult result = AddNewNode<CallBuiltin>(
      inputs.size() + 1,
      [&](CallBuiltin* call_builtin) {
        int arg_index = 0;
        for (auto* input : inputs) {
          call_builtin->set_arg(arg_index++, input);
        }
        return ReduceResult::Done();
      },
      kBuiltin, context);
  CHECK(result.IsDoneWithValue());
  return result.value()->template Cast<CallBuiltin>();
}

template <typename BaseT>
template <Builtin kBuiltin>
CallBuiltin* MaglevReducer<BaseT>::BuildCallBuiltin(
    std::initializer_list<ValueNode*> inputs,
    compiler::FeedbackSource const& feedback,
    CallBuiltin::FeedbackSlotType slot_type) {
  CallBuiltin* call_builtin = BuildCallBuiltin<kBuiltin>(inputs);
  SetCallBuiltinFeedback<kBuiltin>(call_builtin, feedback, slot_type);
  return call_builtin;
}

template <typename BaseT>
template <Builtin kBuiltin>
CallBuiltin* MaglevReducer<BaseT>::BuildCallBuiltin(
    ValueNode* context, std::initializer_list<ValueNode*> inputs,
    compiler::FeedbackSource const& feedback,
    CallBuiltin::FeedbackSlotType slot_type) {
  CallBuiltin* call_builtin = BuildCallBuiltin<kBuiltin>(context, inputs);
  SetCallBuiltinFeedback<kBuiltin>(call_builtin, feedback, slot_type);
  return call_builtin;
}

template <typename BaseT>
compiler::OptionalStringRef MaglevReducer<BaseT>::GetStringFromInt32(
    int32_t value) {
  switch (value) {
    case 0:
      return broker()->zero_string();
    case 1:
      return broker()->one_string();
    case 2:
      return broker()->two_string();
    case 3:
      return broker()->three_string();
    case 4:
      return broker()->four_string();
    case 5:
      return broker()->five_string();
    case 6:
      return broker()->six_string();
    case 7:
      return broker()->seven_string();
    case 8:
      return broker()->eight_string();
    case 9:
      return broker()->nine_string();
    // TODO(victorgomes): Should we embed the string instead?
    default:
      return {};
  }
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldNumberToString(
    ValueNode* value) {
  if (auto cst_value = TryGetInt32Constant(value)) {
    if (auto cst_string = GetStringFromInt32(*cst_value)) {
      return GetConstant(*cst_string);
    }
  }
  return {};
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
  if (input->Is<CheckedSmiUntag>()) {
    // Smi-ness is already checked!
    return input;
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
    RETURN_IF_ABORT(EnsureInt32(left));
    if (left->is_conversion()) {
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
        RETURN_IF_ABORT(EnsureInt32(left));
        return GetInt32Constant(0);
      }
      return {};
    case Operation::kBitwiseOr:
      // x | -1 = -1
      if (cst_right == 0) {
        RETURN_IF_ABORT(EnsureInt32(left));
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
std::optional<bool> MaglevReducer<BaseT>::TryFoldUint32CompareOperation(
    Operation op, ValueNode* left, ValueNode* right) {
  if (auto cst_left = TryGetUint32Constant(left)) {
    if (auto cst_right = TryGetUint32Constant(right)) {
      return TryFoldUint32CompareOperation(op, cst_left.value(),
                                           cst_right.value());
    }
  }
  return {};
}

template <typename BaseT>
bool MaglevReducer<BaseT>::TryFoldUint32CompareOperation(Operation op,
                                                         uint32_t left,
                                                         uint32_t right) {
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
std::optional<bool> MaglevReducer<BaseT>::TryFoldFloat64CompareOperation(
    Operation op, ValueNode* left, ValueNode* right) {
  if (auto cst_right = TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kFloat64, right,
          TaggedToFloat64ConversionType::kNumberOrOddball)) {
    return TryFoldFloat64CompareOperation(op, left, cst_right->get_scalar());
  }
  return {};
}

template <typename BaseT>
std::optional<bool> MaglevReducer<BaseT>::TryFoldFloat64CompareOperation(
    Operation op, ValueNode* left, double cst_right) {
  if (auto cst_left = TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kFloat64, left,
          TaggedToFloat64ConversionType::kNumberOrOddball)) {
    return TryFoldFloat64CompareOperation(op, cst_left->get_scalar(),
                                          cst_right);
  }
  return {};
}

template <typename BaseT>
bool MaglevReducer<BaseT>::TryFoldFloat64CompareOperation(Operation op,
                                                          double left,
                                                          double right) {
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
  auto cst = TryGetFloat64OrHoleyFloat64Constant(UseRepresentation::kFloat64,
                                                 value, conversion_type);
  if (!cst.has_value()) return {};
  const double scalar = cst->get_scalar();
  switch (kOperation) {
    case Operation::kNegate:
      return GetNumberConstant(-scalar);
    case Operation::kIncrement:
      return GetNumberConstant(scalar + 1);
    case Operation::kDecrement:
      return GetNumberConstant(scalar - 1);
    default:
      UNREACHABLE();
  }
}

namespace details {
inline bool Float64Equal(std::optional<double> left,
                         std::optional<double> right) {
  if (!left.has_value() || !right.has_value()) return false;
  // This is basically `==` but it returns false for mismatching +0.0/-0.0 and
  // it returns true for NaN.
  return base::bit_cast<uint64_t>(*left) == base::bit_cast<uint64_t>(*right) ||
         (std::isnan(*left) && std::isnan(*right));
}
}  // namespace details

template <typename BaseT>
template <Operation kOperation>
MaybeReduceResult
MaglevReducer<BaseT>::TryFoldFloat64BinaryOperationForToNumber(
    TaggedToFloat64ConversionType conversion_type, ValueNode* left,
    ValueNode* right) {
  auto cst_right = TryGetFloat64OrHoleyFloat64Constant(
      UseRepresentation::kFloat64, right, conversion_type);
  if (!cst_right.has_value()) return {};
  return TryFoldFloat64BinaryOperationForToNumber<kOperation>(
      conversion_type, left, cst_right->get_scalar());
}

template <typename BaseT>
template <Operation kOperation>
MaybeReduceResult
MaglevReducer<BaseT>::TryFoldFloat64BinaryOperationForToNumber(
    TaggedToFloat64ConversionType conversion_type, ValueNode* left,
    double cst_right) {
  auto cst_left = TryGetFloat64OrHoleyFloat64Constant(
      UseRepresentation::kFloat64, left, conversion_type);
  if (!cst_left.has_value()) {
    if (details::Float64Equal(cst_right, Float64Identity<kOperation>())) {
      // This needs to return a Float64.
      if (left->is_holey_float64()) {
        // However we can treat Undefineds (Holes) as NaNs.
        left = AddNewNodeNoInputConversion<UnsafeHoleyFloat64ToFloat64>({left});
      } else {
        GET_VALUE_OR_ABORT(left, GetFloat64(left));
      }
      return left->Unwrap();
    }
    // TODO(dmercadier): we could still do strength reduction, like
    //     x * 2  ==> x + x
    //     x ** 2 ==> x * x
    //     etc.
    // For inspiration, REDUCE(FloatBinop) in machine-optimization-reducer.h
    // contains a lot of these.
    return {};
  }
  const double left_scalar = cst_left->get_scalar();
  switch (kOperation) {
    case Operation::kAdd:
      return GetNumberConstant(left_scalar + cst_right);
    case Operation::kSubtract:
      return GetNumberConstant(left_scalar - cst_right);
    case Operation::kMultiply:
      return GetNumberConstant(left_scalar * cst_right);
    case Operation::kDivide:
      return GetNumberConstant(left_scalar / cst_right);
    case Operation::kModulus:
      // TODO(v8:7700): Constant fold mod.
      return {};
    case Operation::kExponentiate:
      return GetNumberConstant(math::pow(left_scalar, cst_right));
    default:
      UNREACHABLE();
  }
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldFloat64Min(ValueNode* lhs,
                                                          ValueNode* rhs) {
  // lhs and rhs need to be already converted to Float64. Otherwise
  // equality checking is not valid.
  DCHECK(ValueRepresentationIs(lhs->value_representation(),
                               ValueRepresentation::kFloat64));
  DCHECK(ValueRepresentationIs(rhs->value_representation(),
                               ValueRepresentation::kFloat64));
  if (lhs == rhs) {
    return lhs->Unwrap();
  }

  std::optional<Float64> lhs_const = TryGetFloat64OrHoleyFloat64Constant(
      UseRepresentation::kFloat64, lhs,
      TaggedToFloat64ConversionType::kNumberOrOddball);
  if (!lhs_const) return {};
  std::optional<Float64> rhs_const = TryGetFloat64OrHoleyFloat64Constant(
      UseRepresentation::kFloat64, rhs,
      TaggedToFloat64ConversionType::kNumberOrOddball);
  if (!rhs_const) return {};

  const double lhs_scalar = lhs_const->get_scalar();
  const double rhs_scalar = rhs_const->get_scalar();
  if (std::isnan(lhs_scalar) || std::isnan(rhs_scalar)) {
    return GetFloat64Constant(std::numeric_limits<double>::quiet_NaN());
  }
  if (lhs_scalar == 0 && rhs_scalar == 0) {
    // Handle -0 vs 0.
    if (std::signbit(lhs_scalar)) {
      return GetFloat64Constant(lhs_scalar);
    }
    return GetFloat64Constant(rhs_scalar);
  }
  if (lhs_scalar <= rhs_scalar) {
    return GetFloat64Constant(lhs_scalar);
  }
  return GetFloat64Constant(rhs_scalar);
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldFloat64Max(ValueNode* lhs,
                                                          ValueNode* rhs) {
  // lhs and rhs need to be already converted to HoleyFloat64. Otherwise
  // equality checking is not valid.
  DCHECK(ValueRepresentationIs(lhs->value_representation(),
                               ValueRepresentation::kFloat64));
  DCHECK(ValueRepresentationIs(rhs->value_representation(),
                               ValueRepresentation::kFloat64));
  if (lhs == rhs) {
    return lhs->Unwrap();
  }

  std::optional<Float64> lhs_const = TryGetFloat64OrHoleyFloat64Constant(
      UseRepresentation::kFloat64, lhs,
      TaggedToFloat64ConversionType::kNumberOrOddball);
  if (!lhs_const) return {};

  std::optional<Float64> rhs_const = TryGetFloat64OrHoleyFloat64Constant(
      UseRepresentation::kFloat64, rhs,
      TaggedToFloat64ConversionType::kNumberOrOddball);
  if (!rhs_const) return {};

  const double lhs_scalar = lhs_const->get_scalar();
  const double rhs_scalar = rhs_const->get_scalar();
  if (std::isnan(lhs_scalar) || std::isnan(rhs_scalar)) {
    return GetFloat64Constant(std::numeric_limits<double>::quiet_NaN());
  }
  if (lhs_scalar == 0 && rhs_scalar == 0) {
    // Handle -0 vs 0.
    if (std::signbit(lhs_scalar)) {
      return GetFloat64Constant(rhs_scalar);
    }
    return GetFloat64Constant(lhs_scalar);
  }
  if (lhs_scalar >= rhs_scalar) {
    return GetFloat64Constant(lhs_scalar);
  }
  return GetFloat64Constant(rhs_scalar);
}

#ifdef V8_USE_LIBM_TRIG_FUNCTIONS
#define IF_LIBM(Macro, ...) Macro(__VA_ARGS__)
#define IF_NOT_LIBM(Macro, ...)
#else
#define IF_LIBM(Macro, ...)
#define IF_NOT_LIBM(Macro, ...) Macro(__VA_ARGS__)
#endif  // V8_USE_LIBM_TRIG_FUNCTIONS

#define IEEE_754_FUNCTION_MAPPER(V)                                       \
  V(Acos, base::ieee754::acos)                                            \
  V(Acosh, base::ieee754::acosh)                                          \
  V(Asin, base::ieee754::asin)                                            \
  V(Asinh, base::ieee754::asinh)                                          \
  V(Atan, base::ieee754::atan)                                            \
  V(Atanh, base::ieee754::atanh)                                          \
  V(Cbrt, base::ieee754::cbrt)                                            \
  IF_LIBM(V, Cos,                                                         \
          (v8_flags.use_libm_trig_functions ? base::ieee754::libm_cos     \
                                            : base::ieee754::fdlibm_cos)) \
  IF_NOT_LIBM(V, Cos, base::ieee754::cos)                                 \
  V(Cosh, base::ieee754::cosh)                                            \
  V(Exp, base::ieee754::exp)                                              \
  V(Expm1, base::ieee754::expm1)                                          \
  V(Log, base::ieee754::log)                                              \
  V(Log1p, base::ieee754::log1p)                                          \
  V(Log10, base::ieee754::log10)                                          \
  V(Log2, base::ieee754::log2)                                            \
  IF_LIBM(V, Sin,                                                         \
          (v8_flags.use_libm_trig_functions ? base::ieee754::libm_sin     \
                                            : base::ieee754::fdlibm_sin)) \
  IF_NOT_LIBM(V, Sin, base::ieee754::sin)                                 \
  V(Sinh, base::ieee754::sinh)                                            \
  V(Tan, base::ieee754::tan)                                              \
  V(Tanh, base::ieee754::tanh)

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldFloat64Ieee754Unary(
    Float64Ieee754Unary::Ieee754Function ieee_function, ValueNode* input) {
  if (auto cst = TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kFloat64, input,
          TaggedToFloat64ConversionType::kNumberOrOddball)) {
    double value = cst.value().get_scalar();
    double result;
    switch (ieee_function) {
#define CASE(Name, Func)                              \
  case Float64Ieee754Unary::Ieee754Function::k##Name: \
    result = Func(value);                             \
    break;
      IEEE_754_FUNCTION_MAPPER(CASE)
#undef CASE
    }
    return GetFloat64Constant(result);
  }
  return {};
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldFloat64Ieee754Binary(
    Float64Ieee754Binary::Ieee754Function ieee_function, ValueNode* left,
    ValueNode* right) {
  if (auto lhs = TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kFloat64, left,
          TaggedToFloat64ConversionType::kNumberOrOddball)) {
    if (auto rhs = TryGetFloat64OrHoleyFloat64Constant(
            UseRepresentation::kFloat64, right,
            TaggedToFloat64ConversionType::kNumberOrOddball)) {
      double lhs_val = lhs.value().get_scalar();
      double rhs_val = rhs.value().get_scalar();
      double result;
      switch (ieee_function) {
        case Float64Ieee754Binary::Ieee754Function::kAtan2:
          result = base::ieee754::atan2(lhs_val, rhs_val);
          break;
        case Float64Ieee754Binary::Ieee754Function::kPower:
          result = math::pow(lhs_val, rhs_val);
          break;
      }
      return GetFloat64Constant(result);
    }
  }
  return {};
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldInt32CountLeadingZeros(
    ValueNode* input) {
  if (auto cst = TryGetInt32Constant(input)) {
    return GetInt32Constant(base::bits::CountLeadingZeros32(cst.value()));
  }
  return {};
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldFloat64CountLeadingZeros(
    ValueNode* input) {
  if (auto cst = TryGetFloat64OrHoleyFloat64Constant(
          UseRepresentation::kFloat64, input,
          TaggedToFloat64ConversionType::kNumberOrOddball)) {
    uint32_t value = DoubleToUint32(cst.value().get_scalar());
    return GetInt32Constant(base::bits::CountLeadingZeros32(value));
  }
  return {};
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryFoldLogicalNot(ValueNode* input) {
  switch (input->opcode()) {
#define CASE(Name)                                         \
  case Opcode::k##Name: {                                  \
    return GetBooleanConstant(                             \
        !input->Cast<Name>()->ToBoolean(local_isolate())); \
  }
    CONSTANT_VALUE_NODE_LIST(CASE)
#undef CASE
    default:
      break;
  }

  if (auto c = TryGetConstantAlternative(input)) {
    return TryFoldLogicalNot(*c);
  }

  return {};
}
template <typename BaseT>
bool MaglevReducer<BaseT>::IsTheHoleConstant(ValueNode* node) {
  if (node != nullptr) {
    if (compiler::OptionalHeapObjectRef maybe_constant =
            TryGetConstant<HeapObject>(node)) {
      return maybe_constant->IsTheHole();
    }
  }
  return false;
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::GetConvertReceiver(
    compiler::SharedFunctionInfoRef shared, ValueNode* receiver,
    ConvertReceiverMode mode) {
  DCHECK(!IsTheHoleConstant(receiver));
  if (shared.native() || shared.language_mode() == LanguageMode::kStrict) {
    if (mode == ConvertReceiverMode::kNullOrUndefined) {
      return GetRootConstant(RootIndex::kUndefinedValue);
    } else {
      return receiver;
    }
  }
  if (mode == ConvertReceiverMode::kNullOrUndefined) {
    return GetConstant(
        broker()->target_native_context().global_proxy_object(broker()));
  }
  if (CheckType(receiver, NodeType::kJSReceiver)) return receiver;
  if (compiler::OptionalHeapObjectRef maybe_constant =
          TryGetConstant<HeapObject>(receiver)) {
    compiler::HeapObjectRef constant = maybe_constant.value();
    if (constant.IsNullOrUndefined()) {
      return GetConstant(
          broker()->target_native_context().global_proxy_object(broker()));
    }
  }
  return AddNewNode<ConvertReceiver>({receiver},
                                     broker()->target_native_context(), mode);
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceMathSqrt(
    compiler::JSFunctionRef target, CallArguments& args) {
  if (args.count() < 1) {
    return GetRootConstant(RootIndex::kNanValue);
  }
  if (!CanSpeculateCall() && args[0]->is_tagged()) {
    return {};
  }
  ValueNode* value;
  GET_VALUE_OR_ABORT(
      value, GetFloat64ForToNumber(args[0], NodeType::kNumberOrOddball));
  return AddNewNode<Float64Sqrt>({value});
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceBuiltin(
    Builtin builtin_id, compiler::JSFunctionRef target, CallArguments& args,
    const compiler::FeedbackSource& feedback_source) {
  if (args.mode() != CallArguments::kDefault) {
    // TODO(victorgomes): Maybe inline the spread stub? Or call known function
    // directly if arguments list is an array.
    return {};
  }

  const compiler::FeedbackSource saved_feedback = current_speculation_feedback_;
  const SpeculationMode saved_mode = current_speculation_mode_;
  SpeculationMode mode = SpeculationMode::kDisallowSpeculation;
  if (feedback_source.IsValid()) {
    compiler::ProcessedFeedback const& processed_feedback =
        broker_->GetFeedbackForCall(feedback_source);
    if (!processed_feedback.IsInsufficient()) {
      mode = processed_feedback.AsCall().speculation_mode();
    }
  }
  if (mode != SpeculationMode::kDisallowSpeculation) {
    current_speculation_feedback_ = feedback_source;
    current_speculation_mode_ = mode;
  } else {
    current_speculation_feedback_ = compiler::FeedbackSource();
    current_speculation_mode_ = SpeculationMode::kDisallowSpeculation;
  }

  MaybeReduceResult result;
  switch (builtin_id) {
    case Builtin::kMathSqrt:
      result = TryReduceMathSqrt(target, args);
      break;
    default:
      // Not yet ported. The remaining builtins still go through the
      // bytecode-builder dispatcher.
      break;
  }

  current_speculation_feedback_ = saved_feedback;
  current_speculation_mode_ = saved_mode;
  return result;
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#undef TRACE

#endif  // V8_MAGLEV_MAGLEV_REDUCER_INL_H_
