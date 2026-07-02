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
#include "src/base/logging.h"
#include "src/common/scoped-modification.h"
#include "src/compiler/processed-feedback.h"
#include "src/maglev/maglev-ir-inl.h"
#include "src/maglev/maglev-map-inference.h"
#include "src/maglev/maglev-node-type.h"
#include "src/numbers/conversions.h"
#include "src/numbers/ieee754.h"
#include "src/objects/heap-number-inl.h"
#ifdef V8_INTL_SUPPORT
#include "src/objects/intl-objects.h"
#endif
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
  if (node->properties().can_throw()) period_added_throwing_node_ = true;
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
    UNREACHABLE();
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
      while (c->opcode() == Opcode::kCheckedInternalizedString) {
        c = c->input(0).node();
      }
      if (IsConstantNode(c->opcode())) {
        return c;
      }
    }
  }
  return {};
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildLoadTaggedField(ValueNode* object,
                                                        uint32_t offset,
                                                        NodeType type,
                                                        bool is_const,
                                                        PropertyKey key) {
  if constexpr (ReducerBaseWithAllocationTracking<BaseT>) {
    if (std::optional<ValueNode*> val =
            base_->TryBuildLoadTaggedFieldFromAllocation(object, offset)) {
      return *val;
    }
  }
  return AddNewNode<LoadTaggedField>({object}, offset, type, is_const, key);
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildLoadFixedDoubleArrayElement(
    ValueNode* elements, ValueNode* index) {
  // We won't try to reason about the type of the elements array and thus also
  // cannot end up with an empty type for it.
  DCHECK(!IsEmptyNodeType(GetType(elements)));
  if constexpr (ReducerBaseWithAllocationTracking<BaseT>) {
    if (auto constant = TryGetInt32Constant(index)) {
      RETURN_IF_DONE(base_->TryBuildLoadFixedDoubleArrayElementFromAllocation(
          elements, constant.value()));
    }
  }
  return AddNewNode<LoadFixedDoubleArrayElement>({elements, index});
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildStoreTaggedField(
    ValueNode* object, ValueNode* value, int offset, StoreTaggedMode store_mode,
    PropertyKey property_key, MaybeAssignedFlag maybe_assigned) {
  DCHECK_IMPLIES(!IsInitializing(store_mode), !value->is_conversion());
  if constexpr (ReducerBaseWithAllocationTracking<BaseT>) {
    if (!IsInitializing(store_mode)) {
      base_->TryBuildStoreTaggedFieldToAllocation(object, value, offset);
    }
  }
  if (CanElideWriteBarrier(object, value)) {
    return AddNewNode<StoreTaggedFieldNoWriteBarrier>(
        {object, value}, offset, store_mode, property_key, maybe_assigned);
  } else {
    // Detect stores that would create old-to-new references and pretenure the
    // value.
    if (v8_flags.maglev_pretenure_store_values) {
      if (auto alloc = object->template TryCast<InlinedAllocation>()) {
        if (alloc->allocation_block()->allocation_type() ==
            AllocationType::kOld) {
          alloc->allocation_block()->TryPretenure(value);
        }
      }
    }
    bool value_can_be_smi =
        GetCheckType(GetType(value)) == CheckType::kCheckHeapObject;
    return AddNewNode<StoreTaggedFieldWithWriteBarrier>(
        {object, value}, offset, store_mode, value_can_be_smi, property_key,
        maybe_assigned);
  }
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildStoreTaggedFieldNoWriteBarrier(
    ValueNode* object, ValueNode* value, int offset, StoreTaggedMode store_mode,
    PropertyKey property_key) {
  DCHECK_IMPLIES(!IsInitializing(store_mode), !value->is_conversion());
  DCHECK(CanElideWriteBarrier(object, value));
  if constexpr (ReducerBaseWithAllocationTracking<BaseT>) {
    if (!IsInitializing(store_mode)) {
      base_->TryBuildStoreTaggedFieldToAllocation(object, value, offset);
    }
  }
  return AddNewNode<StoreTaggedFieldNoWriteBarrier>({object, value}, offset,
                                                    store_mode, property_key);
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildStoreTrustedPointerField(
    ValueNode* object, ValueNode* value, int offset, IndirectPointerTag tag,
    StoreTaggedMode store_mode) {
#ifdef V8_ENABLE_SANDBOX
  return AddNewNode<StoreTrustedPointerFieldWithWriteBarrier>(
      {object, value}, offset, tag, store_mode);
#else
  return BuildStoreTaggedField(object, value, offset, store_mode);
#endif  // V8_ENABLE_SANDBOX
}

template <typename BaseT>
bool MaglevReducer<BaseT>::CanElideWriteBarrier(ValueNode* object,
                                                ValueNode* value) {
  if (value->Is<RootConstant>() || value->Is<ConsStringMap>()) return true;
  if (!IsEmptyNodeType(GetType(value)) && CheckType(value, NodeType::kSmi)) {
    return true;
  }

  if constexpr (ReducerBaseWithAllocationTracking<BaseT>) {
    if (base_->TryElideWriteBarrierForAllocation(object, value)) {
      return true;
    }
  }

  // If tagged and not Smi, we cannot elide write barrier.
  if (value->is_tagged()) return false;

  // If its alternative conversion node is Smi, {value} will be converted to
  // a Smi when tagged.
  NodeInfo* node_info = GetOrCreateInfoFor(value);
  if (ValueNode* tagged_alt = node_info->alternative().tagged()) {
    DCHECK(tagged_alt->is_conversion());
    return CheckType(tagged_alt, NodeType::kSmi);
  }
  return false;
}

template <typename BaseT>
MaybeAssignedFlag MaglevReducer<BaseT>::GetContextMaybeAssigned(
    compiler::ScopeInfoRef scope_info, int index, VariableMode* mode) {
  CHECK_LT(index, scope_info.ContextLength());
  if (index < Context::MIN_CONTEXT_SLOTS) {
    *mode = VariableMode::kConst;
    return kNotAssigned;
  }
  if (scope_info.scope_type() == REPL_MODE_SCOPE) {
    *mode = VariableMode::kVar;
    return kMaybeAssigned;
  }
  int header_length = scope_info.ContextHeaderLength();
  if (index < header_length) {
    *mode = VariableMode::kConst;
    return kNotAssigned;
  }
  if (index == scope_info.FunctionContextSlotIndex()) {
    *mode = VariableMode::kConst;
    return kNotAssigned;
  }
  // Context allocated receivers in derived constructors could be initialized
  // through super() calls in arrow functions or eval. This implies that it's
  // not always possible to know where the `this` binding is finally
  // initialized. Track such bindings as kMaybeAssigned.
  if (index == scope_info.ReceiverContextSlotIndex() &&
      IsDerivedConstructor(scope_info.function_kind())) {
    *mode = VariableMode::kConst;
    return kMaybeAssigned;
  }
  int var_index = index - header_length;
  *mode = scope_info.ContextLocalMode(var_index);
  return scope_info.ContextLocalMaybeAssignedFlag(var_index);
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildStoreMap(ValueNode* object,
                                                 compiler::MapRef map,
                                                 StoreMap::Kind kind) {
  RETURN_IF_ABORT(AddNewNode<StoreMap>({object}, map, kind));
  NodeType object_type = StaticTypeForMap(map, broker());
  NodeInfo* node_info = GetOrCreateInfoFor(object);
  if (map.is_stable()) {
    node_info->SetPossibleMaps(PossibleMaps{map}, false, object_type, broker(),
                               known_node_aspects());
    broker()->dependencies()->DependOnStableMap(map);
  } else {
    node_info->SetPossibleMaps(PossibleMaps{map}, true, object_type, broker(),
                               known_node_aspects());
    known_node_aspects().MarkSideEffectsRequireInvalidation();
  }
  return ReduceResult::Done();
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::ConvertForField(
    ValueNode* value, const vobj::Field& desc, AllocationType allocation_type) {
  switch (desc.type) {
    case vobj::FieldType::kTagged: {
      // Subtle: we don't use `NodeTypeIs(...)` since the predicate must NOT
      // be true for NodeType::kNone.
      // TODO(jgruber): NodeType::kNone should never reach here.
      if (GetType(value) == NodeType::kSmi) {
        // TODO(jgruber): This is needed because HoleyFloat64ToTagged does not
        // canonicalize smis by default in GetTaggedValue. We rely on
        // canonicalization though in TryReduceConstructArrayConstructor.
        // We should make this more robust.
        // TODO(454485895): Consider removing this workaround since
        // HoleyFloat64ToTagged now canonicalizes by default.
        MaybeReduceResult res = GetSmiValue(value);
        CHECK(res.IsDoneWithValue());
        return res.value();
      }
      if (value->Is<Float64Constant>()) {
        // Note that NodeType::kSmi MUST go through GetSmiValue for proper
        // canonicalization. If we see a Float64Constant with type kSmi, it has
        // passed BuildCheckSmi, i.e. the runtime value is guaranteed to be
        // convertible to smi (we would have deoptimized otherwise).
        //
        // TODO(jgruber): We have to allocate a new object since the
        // object field could contain a mutable HeapNumber and thus cannot
        // share instances. However, we could specify such mutable fields
        // through vobj::Field; and then only allocate a new object here
        // if needed.
        return BuildInlinedAllocation(CreateHeapNumber(value), allocation_type);
      }
      return GetTaggedValue(value);
    }
    case vobj::FieldType::kTrustedPointer:
      DCHECK(value->Is<TrustedConstant>());
      return value;
    case vobj::FieldType::kInt32:
      // TODO(jgruber): Add conversions here once needed.
      DCHECK_EQ(value->properties().value_representation(),
                ValueRepresentation::kInt32);
      return value;
    case vobj::FieldType::kFloat64:
      return GetFloat64(value);
    case vobj::FieldType::kNone:
      UNREACHABLE();
  }
  UNREACHABLE();
}

template <typename BaseT>
void MaglevReducer<BaseT>::BuildInitializeStore_Tagged(
    vobj::Field desc, InlinedAllocation* object, AllocationType allocation_type,
    ValueNode* value, StoreTaggedMode store_mode,
    MaybeAssignedFlag maybe_assigned) {
  DCHECK_EQ(desc.type, vobj::FieldType::kTagged);

  // Intercept stores of constant map objects here.
  if (desc.offset == offsetof(HeapObject, map_)) {
    if (auto map = TryGetConstant<Map>(value)) {
      ReduceResult result = BuildStoreMap(object, map.value(),
                                          StoreMap::Kind::kInlinedAllocation);
      CHECK(!result.IsDoneWithAbort());
      return;
    }
  }

  DCHECK(value->is_tagged());
  if (InlinedAllocation* inlined_value = value->TryCast<InlinedAllocation>()) {
    // Add to the escape set.
    auto escape_deps = graph()->allocations_escape_map().find(object);
    CHECK(escape_deps != graph()->allocations_escape_map().end());
    escape_deps->second.push_back(inlined_value);
    // Add to the elided set.
    auto& elided_map = graph()->allocations_elide_map();
    auto elided_deps = elided_map.try_emplace(inlined_value, zone()).first;
    elided_deps->second.push_back(object);
    inlined_value->AddNonEscapingUses();
  }

  // Since `value` is tagged, BuildStoreTaggedField doesn't need to do
  // input conversions and won't abort.
  ReduceResult result =
      BuildStoreTaggedField(object, value, desc.offset, store_mode,
                            PropertyKey::None(), maybe_assigned);
  CHECK(!result.IsDoneWithAbort());
}

template <typename BaseT>
void MaglevReducer<BaseT>::BuildInitializeStore_TrustedPointer(
    vobj::Field desc, InlinedAllocation* object, AllocationType allocation_type,
    ValueNode* value) {
  DCHECK_EQ(desc.type, vobj::FieldType::kTrustedPointer);
  DCHECK(value->Is<TrustedConstant>());
  DCHECK(value->is_tagged());

  // Since `value` is tagged, BuildStoreTaggedField doesn't need to do input
  // conversions and won't abort.
  ReduceResult result = BuildStoreTrustedPointerField(
      object, value, desc.offset, value->Cast<TrustedConstant>()->tag(),
      StoreTaggedMode::kInitializing);
  CHECK(!result.IsDoneWithAbort());
}

template <typename BaseT>
void MaglevReducer<BaseT>::BuildInitializeStore(
    vobj::Field desc, InlinedAllocation* object, AllocationType allocation_type,
    ValueNode* value, StoreTaggedMode store_mode,
    MaybeAssignedFlag maybe_assigned) {
  DCHECK_EQ(value->Is<TrustedConstant>(),
            desc.type == vobj::FieldType::kTrustedPointer);

  switch (desc.type) {
    case vobj::FieldType::kTagged:
      BuildInitializeStore_Tagged(desc, object, allocation_type, value,
                                  store_mode, maybe_assigned);
      break;
    case vobj::FieldType::kTrustedPointer:
      BuildInitializeStore_TrustedPointer(desc, object, allocation_type, value);
      break;
    case vobj::FieldType::kInt32:
      AddNewNodeNoInputConversion<StoreInt32>({object, value}, desc.offset);
      break;
    case vobj::FieldType::kFloat64:
      AddNewNodeNoInputConversion<StoreFloat64>({object, value}, desc.offset);
      break;
    case vobj::FieldType::kNone:
      UNREACHABLE();
  }
}

template <typename BaseT>
InlinedAllocation*
MaglevReducer<BaseT>::ExtendOrReallocateCurrentAllocationBlock(
    AllocationType allocation_type, VirtualObject* vobject) {
  DCHECK_LE(vobject->size(), kMaxRegularHeapObjectSize);
  if (!current_allocation_block_ || v8_flags.maglev_allocation_folding == 0 ||
      current_allocation_block_->allocation_type() != allocation_type ||
      !v8_flags.inline_new || is_turbolev()) {
    current_allocation_block_ =
        AddNewNodeNoInputConversion<AllocationBlock>({}, allocation_type);
  }

  int current_size = current_allocation_block_->size();
  if (current_size + vobject->size() > kMaxRegularHeapObjectSize) {
    current_allocation_block_ =
        AddNewNodeNoInputConversion<AllocationBlock>({}, allocation_type);
  }

  DCHECK_GE(current_size, 0);
  InlinedAllocation* allocation =
      AddNewNodeNoInputConversion<InlinedAllocation>(
          {current_allocation_block_}, vobject);
  graph()->allocations_escape_map().emplace(allocation, zone());
  current_allocation_block_->Add(allocation);
  vobject->set_allocation(allocation);
  return allocation;
}

template <typename BaseT>
void MaglevReducer<BaseT>::ClearCurrentAllocationBlock() {
  current_allocation_block_ = nullptr;
}

template <typename BaseT>
void MaglevReducer<BaseT>::AddNonEscapingUses(InlinedAllocation* allocation,
                                              int use_count) {
  if (!v8_flags.maglev_escape_analysis) return;
  allocation->AddNonEscapingUses(use_count);
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildInlinedAllocation(
    VirtualObject* vobject, AllocationType allocation_type) {
  known_node_aspects().virtual_objects().Add(vobject);
  InlinedAllocation* allocation;

  using ValueAndDesc = std::pair<ValueNode*, vobj::Field>;
  SmallZoneVector<ValueAndDesc, 8> values(zone());
  bool result =
      vobject->ForEachSlot([&](ValueNode* node, vobj::Field desc) -> bool {
        CHECK_NE(node, VirtualObject::kUninitializedSlotValue);
        if (node->Is<VirtualObject>()) {
          VirtualObject* nested = node->Cast<VirtualObject>();
          ReduceResult result = BuildInlinedAllocation(nested, allocation_type);
          if (result.IsDoneWithAbort()) {
            return false;
          }
          GET_VALUE(node, result);
          // Update the vobject's slot value.
          vobject->set(desc.offset, node);
        }
        ReduceResult result = ConvertForField(node, desc, allocation_type);
        if (result.IsDoneWithAbort()) {
          return false;
        }
        GET_VALUE(node, result);
        values.push_back({node, desc});
        return true;
      });
  if (!result) {
    return ReduceResult::DoneWithAbort();
  }
  allocation =
      ExtendOrReallocateCurrentAllocationBlock(allocation_type, vobject);
  AddNonEscapingUses(allocation, static_cast<int>(values.size()));
  StoreTaggedMode store_mode = StoreTaggedMode::kInitializing;
  compiler::OptionalScopeInfoRef scope_info;
  if (vobject->has_static_map() && vobject->map()->IsContextMap()) {
    store_mode = StoreTaggedMode::kInitializingToContext;
    if (auto maybe_constant =
            vobject->get(Context::OffsetOfElementAt(Context::SCOPE_INFO_INDEX))
                ->TryGetConstant(broker())) {
      scope_info = maybe_constant->AsScopeInfo();
    }
  }

  for (uint32_t i = 0; i < values.size(); i++) {
    const auto [value, desc] = values[i];
    MaybeAssignedFlag maybe_assigned = kMaybeAssigned;
    if (store_mode == StoreTaggedMode::kInitializingToContext && scope_info &&
        desc.offset >= Context::OffsetOfElementAt(0)) {
      int index = (desc.offset - Context::OffsetOfElementAt(0)) / kTaggedSize;
      VariableMode mode;
      maybe_assigned =
          GetContextMaybeAssigned(scope_info.value(), index, &mode);
    }
    BuildInitializeStore(desc, allocation, allocation_type, value, store_mode,
                         maybe_assigned);
  }
  if constexpr (ReducerBaseWithLoopEffectTracking<BaseT>) {
    if (base_->loop_effects()) {
      base_->loop_effects()->allocations.insert(allocation);
    }
  }

  if (v8_flags.maglev_allocation_folding < 2) {
    ClearCurrentAllocationBlock();
  }
  return allocation;
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildAndAllocateJSArrayIterator(
    ValueNode* array, IterationKind iteration_kind) {
  compiler::MapRef map =
      broker()->target_native_context().initial_array_iterator_map(broker());
  VirtualObject* iterator = CreateJSArrayIterator(map, array, iteration_kind);
  return BuildInlinedAllocation(iterator, AllocationType::kYoung);
}

inline bool CanInlineArrayIteratingBuiltin(compiler::JSHeapBroker* broker,
                                           const PossibleMaps& maps,
                                           ElementsKind* kind_return) {
  DCHECK_NE(0, maps.size());
  *kind_return = maps.at(0).elements_kind();
  for (compiler::MapRef map : maps) {
    if (!map.supports_fast_array_iteration(broker) ||
        !UnionElementsKindUptoSize(kind_return, map.elements_kind())) {
      return false;
    }
  }
  return true;
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceArrayIsArray(
    compiler::JSFunctionRef target, CallArguments& args) {
  if (args.count() == 0) return GetBooleanConstant(false);

  ValueNode* node = args[0];

  if (CheckType(node, NodeType::kJSArray)) {
    return GetBooleanConstant(true);
  }

  MapInference<MaglevReducer<BaseT>> inference(this, node);
  if (auto possible_maps = inference.TryGetPossibleMaps()) {
    bool has_array_map = false;
    bool has_proxy_map = false;
    bool has_other_map = false;
    for (compiler::MapRef map : *possible_maps) {
      InstanceType type = map.instance_type();
      if (InstanceTypeChecker::IsJSArray(type)) {
        has_array_map = true;
      } else if (InstanceTypeChecker::IsJSProxy(type)) {
        has_proxy_map = true;
      } else {
        has_other_map = true;
      }
    }
    if ((has_array_map ^ has_other_map) && !has_proxy_map) {
      if (has_array_map) {
        if (auto node_info = known_node_aspects().TryGetInfoFor(node)) {
          node_info->IntersectType(NodeType::kJSArray);
        }
      }
      RETURN_IF_ABORT(inference.InsertMapChecks(zone()));
      return GetBooleanConstant(has_array_map);
    }
  }

  if (is_turbolev()) {
    return AddNewNode<ObjectIsArray>({node});
  }
  // TODO(dmercadier): consider supporting ObjectIsArray in Maglev.
  return {};
}
template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildLoadElements(
    ValueNode* object, std::optional<ElementsKind> kind) {
  ValueNode* known_elements = known_node_aspects().TryFindLoadedProperty(
      object, PropertyKey::Elements());
  if (known_elements) {
    TRACE("  * Reusing non-constant [Elements] "
          << PrintNodeLabel(known_elements) << ": "
          << PrintNode(known_elements));
    return known_elements;
  }

  ValueNode* elements;
  GET_VALUE_OR_ABORT(
      elements,
      BuildLoadTaggedField(object, offsetof(JSObject, elements_),
                           NodeType::kAnyHeapObject,
                           /*is_const=*/false, PropertyKey::Elements()));
  RecordKnownProperty(object, PropertyKey::Elements(), elements, false,
                      compiler::AccessMode::kLoad);
  if (is_turbolev() && kind.has_value()) {
    // We record a map hint for Turbolev so that LateEscapeAnalysis knows that
    // we just loaded an elements array, which means that it can't alias with
    // a property array.
    // TODO(dmercadier): also record from which object this elements array came
    // from, since 2 elements arrays for objects with different maps cannot
    // alias.
    RETURN_IF_ABORT(BuildAssumeMapForElements(elements, kind.value()));
  }
  return elements;
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildAssumeMapForElements(
    ValueNode* elements, ElementsKind kind) {
  DCHECK(is_turbolev());
  const auto maps =
      IsDoubleElementsKind(kind)
          ? compiler::ZoneRefSet<Map>({broker()->fixed_array_map(),
                                       broker()->fixed_double_array_map()},
                                      zone())
          : compiler::ZoneRefSet<Map>(
                {broker()->fixed_array_map(), broker()->fixed_cow_array_map()},
                zone());
  return AddNewNode<AssumeMap>({elements}, maps);
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceArrayPrototypeAt(
    compiler::JSFunctionRef target, CallArguments& args) {
  if (!CanSpeculateCall()) return {};

  if (!broker()->dependencies()->DependOnNoElementsProtector()) {
    TRACE(TraceColor::kRed << "! Failed to reduce Array.prototype.at - "
                              "NoElementsProtector invalidated");
    return {};
  }

  ValueNode* receiver = GetValueOrUndefined(args.receiver());
  MapInference<MaglevReducer<BaseT>> inference(this, receiver);
  auto possible_maps = inference.TryGetPossibleMaps();
  ElementsKind elements_kind = NO_ELEMENTS;
  // TODO(42204525): Support polymorphism. I.e., DOUBLE_ELEMENTS and ELEMENTS
  // together.
  if (!possible_maps || !CanInlineArrayIteratingBuiltin(
                            broker(), *possible_maps, &elements_kind)) {
    return {};
  }
  RETURN_IF_ABORT(inference.InsertMapChecks(zone()));

  ValueNode* length;
  if (ValueNode* loaded_property = known_node_aspects().TryFindLoadedProperty(
          receiver, broker()->length_string())) {
    length = loaded_property;
  } else {
    GET_VALUE_OR_ABORT(
        length, BuildLoadTaggedField(receiver, offsetof(JSArray, length_),
                                     NodeType::kUnknown, false,
                                     broker()->length_string()));
    RecordKnownProperty(receiver, broker()->length_string(), length, false,
                        compiler::AccessMode::kLoad);
  }

  ValueNode* index = nullptr;
  if (args.count() == 0) {
    // Index is the undefined object. ToIntegerOrInfinity(undefined) = 0.
    index = GetInt32Constant(0);
  } else {
    GET_VALUE_OR_ABORT(index,
                       Select(
                           [&](auto& sg, auto* label) -> BranchResult {
                             return BuildBranchIfInt32Compare(
                                 sg, label, Operation::kLessThan, args[0],
                                 GetInt32Constant(0));
                           },
                           [&]() -> ReduceResult {
                             return AddNewNode<Int32Add>({args[0], length});
                           },
                           [&]() -> ReduceResult { return args[0]; }));
  }

  ValueNode* elements;
  GET_VALUE_OR_ABORT(elements, BuildLoadElements(receiver, elements_kind));

  return Select(
      [&](auto& sg, auto* label) -> BranchResult {
        return BuildBranchIfInt32Compare(sg, label,
                                         Operation::kGreaterThanOrEqual, index,
                                         GetInt32Constant(0));
      },
      [&]() {
        return Select(
            [&](auto& sg, auto* label) -> BranchResult {
              return BuildBranchIfInt32Compare(sg, label, Operation::kLessThan,
                                               index, length);
            },
            [&]() -> ReduceResult {
              ValueNode* element;
              if (elements_kind == HOLEY_DOUBLE_ELEMENTS) {
                GET_VALUE_OR_ABORT(element,
                                   AddNewNode<LoadHoleyFixedDoubleArrayElement>(
                                       {elements, index}));
              } else if (elements_kind == PACKED_DOUBLE_ELEMENTS) {
                GET_VALUE_OR_ABORT(
                    element, BuildLoadFixedDoubleArrayElement(elements, index));
              } else {
                LoadType type = elements_kind == PACKED_SMI_ELEMENTS
                                    ? LoadType::kSmi
                                    : LoadType::kUnknown;
                if (auto constant = TryGetInt32Constant(index)) {
                  auto const_res = TryBuildLoadFixedArrayElementConstantIndex(
                      elements, constant.value(), type);
                  if (const_res.IsDone()) {
                    element = const_res.value();
                  } else {
                    GET_VALUE_OR_ABORT(element,
                                       AddNewNode<LoadFixedArrayElement>(
                                           {elements, index}, type));
                  }
                } else {
                  GET_VALUE_OR_ABORT(element, AddNewNode<LoadFixedArrayElement>(
                                                  {elements, index}, type));
                }
              }
              if (IsHoleyElementsKind(elements_kind)) {
                GET_VALUE_OR_ABORT(
                    element, AddNewNode<ConvertHoleToUndefined>({element}));
              }

              return element;
            },
            [&]() -> ReduceResult {
              return GetRootConstant(RootIndex::kUndefinedValue);
            });
      },
      [&]() -> ReduceResult {
        return GetRootConstant(RootIndex::kUndefinedValue);
      });
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceArrayPrototypeEntries(
    compiler::JSFunctionRef target, CallArguments& args) {
  if (!CanSpeculateCall()) return {};
  ValueNode* receiver = GetValueOrUndefined(args.receiver());
  if (!CheckType(receiver, NodeType::kJSReceiver)) {
    return {};
  }
  return BuildAndAllocateJSArrayIterator(receiver, IterationKind::kEntries);
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceArrayPrototypeKeys(
    compiler::JSFunctionRef target, CallArguments& args) {
  if (!CanSpeculateCall()) return {};
  ValueNode* receiver = GetValueOrUndefined(args.receiver());
  if (!CheckType(receiver, NodeType::kJSReceiver)) {
    return {};
  }
  return BuildAndAllocateJSArrayIterator(receiver, IterationKind::kKeys);
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceArrayPrototypeValues(
    compiler::JSFunctionRef target, CallArguments& args) {
  if (!CanSpeculateCall()) return {};
  ValueNode* receiver = GetValueOrUndefined(args.receiver());
  if (!CheckType(receiver, NodeType::kJSReceiver)) {
    return {};
  }
  return BuildAndAllocateJSArrayIterator(receiver, IterationKind::kValues);
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
            AddNewNodeNoInputConversion<UnsafeSmiTagFloat64>({value}));
      }
      return alternative.set_tagged(
          AddNewNodeNoInputConversion<Float64ToTagged>(
              {value}, NumberConversionMode::kCanonicalizeSmi));
    }
    case ValueRepresentation::kHoleyFloat64: {
      if (node_info->is_smi()) {
        return alternative.set_tagged(
            AddNewNodeNoInputConversion<UnsafeSmiTagHoleyFloat64>({value}));
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
      GET_VALUE_OR_ABORT(untagged, BuildSmiUntag(value));
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
template <typename NodeT, typename... Args>
ReduceResult MaglevReducer<BaseT>::EmitAbruptBlockEnd(
    std::initializer_list<ValueNode*> inputs, Args&&... args) {
  BasicBlock* block = current_block();
  block->set_deferred(true);
  const bool current_block_is_terminated = block->control_node() != nullptr;
  if (current_block_is_terminated) {
    block->RemovePredecessorFollowing(block->reset_control_node());
  }
  ReduceResult result =
      AddNewControlNode<NodeT>(inputs, std::forward<Args>(args)...);
  if (result.IsDoneWithAbort()) {
    // Converting an input already finished the block with a deopt.
    DCHECK(!current_block_is_terminated);
    return ReduceResult::DoneWithAbort();
  }
  if (!current_block_is_terminated) {
    FlushNodesToBlock();
    set_current_block(nullptr);
    if constexpr (ReducerBaseWithAbruptBlockEnd<BaseT>) {
      base_->OnAbruptBlockEnd(block);
    }
  }
  return ReduceResult::DoneWithAbort();
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::EmitUnconditionalDeopt(
    DeoptimizeReason reason) {
  return EmitAbruptBlockEnd<Deopt>({}, reason);
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildAbort(AbortReason reason) {
  return EmitAbruptBlockEnd<Abort>({}, reason);
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::EmitThrow(Throw::Function function,
                                             ValueNode* input) {
  bool has_input;
  if (input == nullptr) {
    has_input = false;
    // To avoid a nullptr input, we use Smi(0) as dummy input.
    input = GetSmiConstant(0);
  } else {
    has_input = true;
  }
  return EmitAbruptBlockEnd<Throw>({input}, function, has_input);
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
  if (node->opcode() == Opcode::kCheckedInternalizedString) {
    return TryGetHeapObjectConstant(node->input(0).node(), constant_node);
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
      // TODO(dmercadier): make EnsureType return a 3-value enum, something like
      // kAlreadyTargetType, kNeedsCheck, and kImpossible, and handle
      // kImpossible by emitting an unconditional deopt, instead of having to
      // check after EnsureType whether its input now has type kNone or not.
      RecordType(value, allowed_input_type, &old_type);

      // Check for the empty type first, so that we don't emit unsafe conversion
      // nodes below.
      if (IsEmptyNodeType(old_type) || IsEmptyNodeType(value)) {
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
MaybeReduceResult
MaglevReducer<BaseT>::TryBuildLoadFixedArrayElementConstantIndex(
    ValueNode* elements, int32_t index, LoadType type) {
  if (index < 0 || static_cast<uint32_t>(index) >= FixedArray::kMaxLength) {
    return {};
  }
  if (compiler::OptionalFixedArrayRef fixed_array_ref =
          TryGetConstant<FixedArray>(elements)) {
    if (static_cast<uint32_t>(index) < fixed_array_ref->length()) {
      compiler::OptionalObjectRef maybe_value =
          fixed_array_ref->TryGet(broker(), static_cast<uint32_t>(index));
      if (maybe_value) return GetConstant(*maybe_value);
    } else {
      return {};
    }
  }
  int offset = FixedArray::OffsetOfElementAt(index);
  return AddNewNodeNoInputConversion<LoadTaggedField>(
      {elements}, offset, NodeTypeFromLoadType(type), false,
      PropertyKey::None());
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
        {object}, merger.intersect_set(), GetCheckType(known_info->type())));
  } else if (has_deprecated_map_without_migration_target &&
             !migration_done_outside) {
    RETURN_IF_ABORT(AddNewNode<CheckMapsWithMigrationAndDeopt>(
        {object}, merger.intersect_set(), GetCheckType(known_info->type())));
  } else if (map) {
    RETURN_IF_ABORT(AddNewNode<CheckMapsWithAlreadyLoadedMap>(
        {object, *map}, merger.intersect_set()));
  } else {
    RETURN_IF_ABORT(AddNewNode<CheckMaps>({object}, merger.intersect_set(),
                                          GetCheckType(known_info->type())));
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

  return AddNewNode<std::conditional_t<flip, ToBooleanLogicalNot, ToBoolean>>(
      {value}, GetCheckType(GetType(value)));
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
ReduceResult MaglevReducer<BaseT>::BuildCheckSmi(ValueNode* object) {
  if (object->StaticTypeIs(broker(), NodeType::kSmi)) return object;
  // Check for the empty type first so that we catch the case where
  // GetType(object) is already empty.
  if (IsEmptyNodeType(IntersectType(GetType(object), NodeType::kSmi))) {
    return EmitUnconditionalDeopt(DeoptimizeReason::kSmi);
  }
  if (EnsureType(object, NodeType::kSmi)) return object;
  // For non-tagged constants, we may be able to skip the runtime check: every
  // non-tagged arm of the switch below emits a value-range check, which is
  // exactly what `Smi::IsValid` proves. For tagged inputs the runtime check
  // (CheckSmi) is a tag-bit check, and value-equivalence (e.g. via the
  // checked_value alternative, which may hold a HeapNumber constant) does not
  // imply Smi tagging.
  if (object->value_representation() != ValueRepresentation::kTagged) {
    if (std::optional<int32_t> constant_value = TryGetInt32Constant(object)) {
      if (Smi::IsValid(constant_value.value())) return object;
    }
  }
  switch (object->value_representation()) {
    case ValueRepresentation::kInt32:
      if (!SmiValuesAre32Bits()) {
        AddNewNodeNoInputConversion<CheckInt32IsSmi>({object});
      }
      break;
    case ValueRepresentation::kUint32:
      AddNewNodeNoInputConversion<CheckUint32IsSmi>({object});
      break;
    case ValueRepresentation::kFloat64:
      AddNewNodeNoInputConversion<CheckFloat64IsSmi>({object});
      break;
    case ValueRepresentation::kHoleyFloat64:
      AddNewNodeNoInputConversion<CheckHoleyFloat64IsSmi>({object});
      break;
    case ValueRepresentation::kTagged:
      AddNewNodeNoInputConversion<CheckSmi>({object});
      break;
    case ValueRepresentation::kIntPtr:
      AddNewNodeNoInputConversion<CheckIntPtrIsSmi>({object});
      break;
    case ValueRepresentation::kRawPtr:
    case ValueRepresentation::kNone:
      UNREACHABLE();
  }
  return object;
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildSmiUntag(ValueNode* node) {
  // This is called when converting inputs in AddNewNode. We might already have
  // an empty type for `node` here. Make sure we don't add unsafe conversion
  // nodes in that case by checking for the empty node type explicitly.
  if (IsEmptyNodeType(node) || !NodeTypeCanBe(GetType(node), NodeType::kSmi)) {
    return EmitUnconditionalDeopt(DeoptimizeReason::kNotASmi);
  }
  if (EnsureType(node, NodeType::kSmi)) {
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

#if V8_ENABLE_WEBASSEMBLY
// When calling a JS-to-Wasm wrapper and Turbolev Wasm inlining is enabled,
// callers wrap all arguments with ProcessWasmArgument. This identity node
// carries an eager deopt frame state (the pre-call checkpoint) so that when
// the wrapper is later inlined by Turbolev, the conversion builtins can
// never lazily-deoptimize with a JSReceiver triggering valueOf
// (crbug.com/493307329). We wrap all args here (Maglev doesn't know the wasm
// signature); the reducer only uses the frame state for numeric params.
// LINT.IfChange(WasmWrapperInliningConditions)
template <typename BaseT>
bool MaglevReducer<BaseT>::ShouldWrapArgsForWasmInlining(
    compiler::SharedFunctionInfoRef shared, JSDispatchHandle dispatch_handle) {
  if (!is_turbolev()) return false;
  if (!v8_flags.wasm_in_js_inlining_wrapper) return false;
  // The SharedFunctionInfo of a Wasm exported function does not carry a
  // builtin ID, so the check below filters out regular JS builtins.
  // However, the Code installed in the dispatch table can be either:
  //  - The generic kJSToWasmWrapper builtin (used before a per-signature
  //    wrapper has been compiled), or
  //  - A jitted per-signature wrapper (CodeKind::JS_TO_WASM_FUNCTION).
  // We detect both cases by inspecting the Code object directly.
  if (!shared.object()->HasWasmExportedFunctionData(local_isolate())) {
    return false;
  }
  Tagged<Code> code =
      local_isolate()->js_dispatch_table().GetCode(dispatch_handle);
  return code->builtin_id() == Builtin::kJSToWasmWrapper ||
         code->kind() == CodeKind::JS_TO_WASM_FUNCTION;
}
// LINT.ThenChange(src/compiler/turboshaft/turbolev-graph-builder.cc:WasmWrapperInliningConditions)
#endif  // V8_ENABLE_WEBASSEMBLY

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildCallKnownJSFunction(
    JSDispatchHandle dispatch_handle, compiler::SharedFunctionInfoRef shared,
    ValueNode* tagged_function, ValueNode* tagged_context,
    ValueNode* tagged_receiver, ValueNode* tagged_new_target, int arg_count,
    base::FunctionRef<ReduceResult(int)> get_arg,
    compiler::FeedbackSource const& feedback_source) {
#if V8_ENABLE_WEBASSEMBLY
  const bool wrap_args_for_wasm =
      ShouldWrapArgsForWasmInlining(shared, dispatch_handle);
#endif  // V8_ENABLE_WEBASSEMBLY

  size_t input_count = arg_count + CallKnownJSFunction::kFixedInputCount;
  return AddNewNode<CallKnownJSFunction>(
      input_count,
      [&](CallKnownJSFunction* call) {
        for (int i = 0; i < arg_count; i++) {
          ValueNode* arg;
          GET_VALUE_OR_ABORT(arg, get_arg(i));
#if V8_ENABLE_WEBASSEMBLY
          if (wrap_args_for_wasm) {
            GET_VALUE_OR_ABORT(arg, AddNewNode<ProcessWasmArgument>({arg}));
          }
#endif  // V8_ENABLE_WEBASSEMBLY
          call->set_arg(i, arg);
        }
        return ReduceResult::Done();
      },
      dispatch_handle, shared, tagged_function, tagged_context, tagged_receiver,
      tagged_new_target, feedback_source);
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
        RecordType(sign_bit, NodeType::kSmi);
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
      if (cst_right == -1) {
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
MaybeReduceResult MaglevReducer<BaseT>::TryFoldTestTypeOf(
    ValueNode* value, interpreter::TestTypeOfFlags::LiteralFlag literal) {
  // TODO(v8:7700): Add a branch version of TestTypeOf that does not need to
  // materialise the boolean value.
  const auto node_type = GetType(value);
  if (node_type == NodeType::kNone) {
    return BuildAbort(AbortReason::kUnreachable);
  }

  const auto check_type = [&](NodeType match_type,
                              NodeType can_be_type) -> MaybeReduceResult {
    if (NodeTypeIs(node_type, match_type)) {
      return GetBooleanConstant(true);
    }
    if (!NodeTypeCanBe(node_type, can_be_type)) {
      return GetBooleanConstant(false);
    }
    return MaybeReduceResult::Fail();
  };

  using LiteralFlag = interpreter::TestTypeOfFlags::LiteralFlag;
  switch (literal) {
    case LiteralFlag::kNumber:
      return check_type(NodeType::kNumber, NodeType::kNumber);
    case LiteralFlag::kString:
      return check_type(NodeType::kString, NodeType::kString);
    case LiteralFlag::kSymbol:
      return check_type(NodeType::kSymbol, NodeType::kSymbol);
    case LiteralFlag::kBoolean:
      return check_type(NodeType::kBoolean, NodeType::kBoolean);
    case LiteralFlag::kBigInt:
      return check_type(NodeType::kBigInt, NodeType::kBigInt);

    case LiteralFlag::kUndefined:
      return check_type(NodeType::kUndefined,
                        UnionType(NodeType::kUndefined, NodeType::kJSReceiver));

    case LiteralFlag::kFunction:
      return check_type(NodeType::kJSFunction, NodeType::kCallable);

    case LiteralFlag::kObject: {
      constexpr NodeType kObjectTypes = UnionType(
          NodeType::kNull,
          UnionType(NodeType::kJSArray, UnionType(NodeType::kStringWrapper,
                                                  NodeType::kJSDataView)));
      if (NodeTypeIs(node_type, kObjectTypes)) {
        return GetBooleanConstant(true);
      }
      if (!NodeTypeCanBe(node_type,
                         UnionType(NodeType::kJSReceiver, NodeType::kNull)) ||
          NodeTypeIs(node_type, NodeType::kCallable)) {
        return GetBooleanConstant(false);
      }
      return {};
    }

    case LiteralFlag::kOther:
      return GetBooleanConstant(false);
  }

  UNREACHABLE();
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
template <typename CondFn>
ReduceResult MaglevReducer<BaseT>::Select(
    CondFn cond, base::FunctionRef<ReduceResult()> if_true,
    base::FunctionRef<ReduceResult()> if_false) {
  Subgraph<BaseT> sg(this, /*variable_count=*/1);
  using Variable = typename Subgraph<BaseT>::Variable;
  using Label = typename Subgraph<BaseT>::Label;

  Label else_branch(&sg, /*predecessor_count=*/1);

  BranchResult br = cond(sg, &else_branch);
  switch (br) {
    case BranchResult::kAbort:
      return ReduceResult::DoneWithAbort();
    case BranchResult::kAlwaysTrue:
      return if_true();
    case BranchResult::kAlwaysFalse:
      return if_false();
    case BranchResult::kDefault:
      break;
  }

  Variable v(0);
  Label done(&sg, /*predecessor_count=*/2, {&v});
  ReduceResult t = if_true();
  CHECK(t.IsDone());
  if (t.IsDoneWithValue()) sg.set(v, t.value());
  sg.GotoOrTrim(&done);

  sg.Bind(&else_branch);
  ReduceResult f = if_false();
  CHECK(f.IsDone());
  if (t.IsDoneWithAbort() && f.IsDoneWithAbort()) {
    return ReduceResult::DoneWithAbort();
  }
  CHECK(f.IsDoneWithValue());
  sg.set(v, f.value());
  sg.GotoOrTrim(&done);

  RETURN_IF_ABORT(sg.TrimPredecessorsAndBind(&done));
  return sg.get(v);
}

template <typename BaseT>
template <typename Sub>
BranchResult MaglevReducer<BaseT>::BuildBranchIfInt32Compare(
    Sub& sg, typename Sub::Label* false_target, Operation op, ValueNode* lhs,
    ValueNode* rhs) {
  if (auto cl = TryGetInt32Constant(lhs)) {
    if (auto cr = TryGetInt32Constant(rhs)) {
      return CompareInt32(*cl, *cr, op) ? BranchResult::kAlwaysTrue
                                        : BranchResult::kAlwaysFalse;
    }
  }
  ReduceResult r = sg.template GotoIfFalse<BranchIfInt32Compare>(
      false_target, {lhs, rhs}, op);
  if (r.IsDoneWithAbort()) return BranchResult::kAbort;
  return BranchResult::kDefault;
}

template <typename BaseT>
template <typename Sub>
BranchResult MaglevReducer<BaseT>::BuildBranchIfUint32Compare(
    Sub& sg, typename Sub::Label* false_target, Operation op, ValueNode* lhs,
    ValueNode* rhs) {
  if (auto cl = TryGetUint32Constant(lhs)) {
    if (auto cr = TryGetUint32Constant(rhs)) {
      return CompareUint32(*cl, *cr, op) ? BranchResult::kAlwaysTrue
                                         : BranchResult::kAlwaysFalse;
    }
  }
  ReduceResult r = sg.template GotoIfFalse<BranchIfUint32Compare>(
      false_target, {lhs, rhs}, op);
  if (r.IsDoneWithAbort()) return BranchResult::kAbort;
  return BranchResult::kDefault;
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildInt32Max(ValueNode* a, ValueNode* b) {
  if (auto cl = TryGetInt32Constant(a)) {
    if (auto cr = TryGetInt32Constant(b)) {
      return GetInt32Constant(std::max(*cl, *cr));
    }
  }
  return Select(
      [&](auto& sg, auto* label) -> BranchResult {
        return BuildBranchIfInt32Compare(sg, label, Operation::kLessThan, a, b);
      },
      [&]() -> ReduceResult { return b; }, [&]() -> ReduceResult { return a; });
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildInt32Min(ValueNode* a, ValueNode* b) {
  if (auto cl = TryGetInt32Constant(a)) {
    if (auto cr = TryGetInt32Constant(b)) {
      return GetInt32Constant(std::min(*cl, *cr));
    }
  }
  return Select(
      [&](auto& sg, auto* label) -> BranchResult {
        return BuildBranchIfInt32Compare(sg, label, Operation::kLessThan, a, b);
      },
      [&]() -> ReduceResult { return a; }, [&]() -> ReduceResult { return b; });
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
MaybeReduceResult MaglevReducer<BaseT>::TryReduceMathMax(
    compiler::JSFunctionRef target, CallArguments& args) {
  if (args.count() == 0) {
    return GetConstant(broker()->minus_infinity_value());
  }
  return TryReduceMathMinMax(
      args,
      [&](ValueNode* lhs, ValueNode* rhs) -> ReduceResult {
        return BuildInt32Max(lhs, rhs);
      },
      [&](ValueNode* lhs, ValueNode* rhs) -> ReduceResult {
        ValueNode* lhs_float;
        GET_VALUE_OR_ABORT(
            lhs_float, GetFloat64ForToNumber(lhs, NodeType::kNumberOrOddball));
        ValueNode* rhs_float;
        GET_VALUE_OR_ABORT(
            rhs_float, GetFloat64ForToNumber(rhs, NodeType::kNumberOrOddball));
        RETURN_IF_DONE(TryFoldFloat64Max(lhs_float, rhs_float));
        return AddNewNode<Float64Max>({lhs_float, rhs_float});
      });
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceMathMin(
    compiler::JSFunctionRef target, CallArguments& args) {
  if (args.count() == 0) {
    return GetConstant(broker()->infinity_value());
  }
  return TryReduceMathMinMax(
      args,
      [&](ValueNode* lhs, ValueNode* rhs) -> ReduceResult {
        return BuildInt32Min(lhs, rhs);
      },
      [&](ValueNode* lhs, ValueNode* rhs) -> ReduceResult {
        ValueNode* lhs_float;
        GET_VALUE_OR_ABORT(
            lhs_float, GetFloat64ForToNumber(lhs, NodeType::kNumberOrOddball));
        ValueNode* rhs_float;
        GET_VALUE_OR_ABORT(
            rhs_float, GetFloat64ForToNumber(rhs, NodeType::kNumberOrOddball));
        RETURN_IF_DONE(TryFoldFloat64Min(lhs_float, rhs_float));
        return AddNewNode<Float64Min>({lhs_float, rhs_float});
      });
}

template <typename BaseT>
template <typename Int32Binop, typename Float64Binop>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceMathMinMax(
    CallArguments& args, Int32Binop&& int32_case, Float64Binop&& float64_case) {
  bool all_args_are_int32_or_smi = true;
  for (size_t i = 0; i < args.count(); ++i) {
    ValueNode* arg = args[i];
    if (GetType(arg) != NodeType::kSmi &&
        arg->properties().value_representation() !=
            ValueRepresentation::kInt32) {
      all_args_are_int32_or_smi = false;
      break;
    }
  }

  if (all_args_are_int32_or_smi) {
    ValueNode* acc = args[0];
    for (size_t i = 1; i < args.count(); ++i) {
      GET_VALUE_OR_ABORT(acc, int32_case(acc, args[i]));
    }
    return acc;
  }

  // TODO(marja): Investigate whether a non-speculative Float64 case helps.
  if (!CanSpeculateCall()) return {};

  // float64_case converts inputs to Float64. Only the first one has to be
  // converted explicitly to seed the accumulator.
  ValueNode* acc;
  GET_VALUE_OR_ABORT(
      acc, GetFloat64ForToNumber(args[0], NodeType::kNumberOrOddball));
  acc = acc->Unwrap();
  for (size_t i = 1; i < args.count(); ++i) {
    GET_VALUE_OR_ABORT(acc, float64_case(acc, args[i]));
  }
  return acc;
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceMathAbs(
    compiler::JSFunctionRef target, CallArguments& args) {
  if (args.count() == 0) {
    return GetRootConstant(RootIndex::kNanValue);
  }
  ValueNode* arg = args[0];

  switch (arg->value_representation()) {
    case ValueRepresentation::kUint32:
    case ValueRepresentation::kIntPtr:
      // TODO(388844115): Rename IntPtr to make it clear it's non-negative.
      return arg;
    case ValueRepresentation::kInt32:
      if (!CanSpeculateCall()) return {};
      return AddNewNode<Int32AbsWithOverflow>({arg});
    case ValueRepresentation::kTagged:
      switch (CheckTypes(arg, {NodeType::kSmi, NodeType::kNumberOrOddball})) {
        case NodeType::kSmi:
          if (!CanSpeculateCall()) return {};
          return AddNewNode<Int32AbsWithOverflow>({arg});
        case NodeType::kNumberOrOddball: {
          ValueNode* float64_value;
          GET_VALUE_OR_ABORT(
              float64_value,
              GetFloat64ForToNumber(arg, NodeType::kNumberOrOddball));
          return AddNewNode<Float64Abs>({float64_value});
        }
        // TODO(verwaest): Add support for ToNumberOrNumeric and deopt.
        default:
          break;
      }
      break;
    case ValueRepresentation::kHoleyFloat64:
      arg = AddNewNodeNoInputConversion<UnsafeHoleyFloat64ToFloat64>({arg});
      [[fallthrough]];
    case ValueRepresentation::kFloat64:
      return AddNewNode<Float64Abs>({arg});
    case ValueRepresentation::kRawPtr:
    case ValueRepresentation::kNone:
      UNREACHABLE();
  }
  return {};
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceMathCeil(
    compiler::JSFunctionRef target, CallArguments& args) {
  return DoTryReduceMathRound(args, Float64Round::Kind::kCeil);
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceMathFloor(
    compiler::JSFunctionRef target, CallArguments& args) {
  return DoTryReduceMathRound(args, Float64Round::Kind::kFloor);
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceMathRound(
    compiler::JSFunctionRef target, CallArguments& args) {
  return DoTryReduceMathRound(args, Float64Round::Kind::kNearest);
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::DoTryReduceMathRound(
    CallArguments& args, Float64Round::Kind kind) {
  if (args.count() == 0) {
    return GetRootConstant(RootIndex::kNanValue);
  }
  ValueNode* arg = args[0];
  auto arg_repr = arg->value_representation();
  if (arg_repr == ValueRepresentation::kInt32 ||
      arg_repr == ValueRepresentation::kUint32 ||
      arg_repr == ValueRepresentation::kIntPtr) {
    return arg;
  }
  if (CheckType(arg, NodeType::kSmi)) return arg;
  if (!IsSupported(CpuOperation::kFloat64Round)) {
    return {};
  }
  if (arg_repr == ValueRepresentation::kFloat64 ||
      arg_repr == ValueRepresentation::kHoleyFloat64) {
    if (arg_repr == ValueRepresentation::kHoleyFloat64) {
      arg = AddNewNodeNoInputConversion<UnsafeHoleyFloat64ToFloat64>({arg});
    }
    return AddNewNode<Float64Round>({arg}, kind);
  }
  DCHECK_EQ(arg_repr, ValueRepresentation::kTagged);
  if (CheckType(arg, NodeType::kNumberOrOddball)) {
    ValueNode* float64_value;
    GET_VALUE_OR_ABORT(float64_value,
                       GetFloat64ForToNumber(arg, NodeType::kNumberOrOddball));
    return AddNewNode<Float64Round>({float64_value}, kind);
  }
  if (!CanSpeculateCall()) return {};
  if constexpr (ReducerBaseWithLazyDeoptScope<BaseT>) {
    typename BaseT::LazyDeoptFrameScope continuation_scope(
        base_, Float64Round::continuation(kind));
    ToNumberOrNumeric* conversion;
    GET_VALUE_OR_ABORT(conversion, AddNewNode<ToNumberOrNumeric>(
                                       {arg}, Object::Conversion::kToNumber));
    // TODO(victorgomes): rely on automatic input conversion here rather than
    // calling UncheckedNumberToFloat64 manually.
    ValueNode* float64_value;
    GET_VALUE_OR_ABORT(float64_value,
                       AddNewNode<UnsafeNumberToFloat64>({conversion}));
    return AddNewNode<Float64Round>({float64_value}, kind);
  } else {
    return {};
  }
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceMathTrunc(
    compiler::JSFunctionRef target, CallArguments& args) {
  if (args.count() < 1) {
    return GetRootConstant(RootIndex::kNanValue);
  }

  if (!CanSpeculateCall() && args[0]->is_tagged()) {
    return {};
  }

  if (!IsSupported(CpuOperation::kFloat64Round)) {
    return {};
  }

  ValueNode* value;
  GET_VALUE_OR_ABORT(
      value, GetFloat64ForToNumber(args[0], NodeType::kNumberOrOddball));
  return AddNewNode<Float64Round>({value}, Float64Round::Kind::kTrunc);
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceMathClz32(
    compiler::JSFunctionRef target, CallArguments& args) {
  if (args.count() < 1) {
    return GetInt32Constant(32);
  }

  if (!IsSupported(CpuOperation::kMathClz32)) {
    return {};
  }

  ValueNode* arg = args[0];
  auto arg_repr = arg->value_representation();
  if (arg_repr == ValueRepresentation::kInt32 ||
      arg_repr == ValueRepresentation::kUint32 ||
      arg_repr == ValueRepresentation::kIntPtr) {
    RETURN_IF_DONE(TryFoldInt32CountLeadingZeros(arg));
    return AddNewNode<Int32CountLeadingZeros>({arg});
  }
  if (arg_repr == ValueRepresentation::kFloat64 ||
      arg_repr == ValueRepresentation::kHoleyFloat64) {
    RETURN_IF_DONE(TryFoldFloat64CountLeadingZeros(arg));
    if (IsSupported(CpuOperation::kFloat64Round)) {
      if (arg_repr == ValueRepresentation::kHoleyFloat64) {
        GET_VALUE_OR_ABORT(
            arg, GetFloat64ForToNumber(arg, NodeType::kNumberOrOddball));
      }
      return AddNewNode<Float64CountLeadingZeros>({arg});
    }
    return {};
  }

  DCHECK_EQ(arg_repr, ValueRepresentation::kTagged);
  if (CheckType(arg, NodeType::kNumber)) {
    return AddNewNode<TaggedCountLeadingZeros>({arg});
  }

  if (!CanSpeculateCall()) {
    return {};
  }

  if constexpr (ReducerBaseWithLazyDeoptScope<BaseT>) {
    typename BaseT::LazyDeoptFrameScope continuation_scope(
        base_, Float64CountLeadingZeros::continuation());
    ToNumberOrNumeric* conversion;
    GET_VALUE_OR_ABORT(conversion, AddNewNode<ToNumberOrNumeric>(
                                       {arg}, Object::Conversion::kToNumber));
    // TODO(victorgomes): rely on automatic input conversion here rather than
    // calling UnsafeNumberToFloat64 manually.
    ValueNode* float64_value;
    GET_VALUE_OR_ABORT(float64_value,
                       AddNewNode<UnsafeNumberToFloat64>({conversion}));
    return AddNewNode<Float64CountLeadingZeros>({float64_value});
  } else {
    return {};
  }
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceMathImul(
    compiler::JSFunctionRef target, CallArguments& args) {
  if (args.count() == 0) {
    return GetInt32Constant(0);
  }
  if (args.count() == 1) {
    // Fall back to builtin for 1 argument to ensure ToNumber side-effects.
    return {};
  }
  if (!CanSpeculateCall() && (args[0]->is_tagged() || args[1]->is_tagged())) {
    return {};
  }
  ValueNode *left, *right;
  GET_VALUE_OR_ABORT(
      left, GetTruncatedInt32ForToNumber(args[0], NodeType::kNumberOrOddball));
  GET_VALUE_OR_ABORT(
      right, GetTruncatedInt32ForToNumber(args[1], NodeType::kNumberOrOddball));
  return AddNewNode<Int32Multiply>({left, right});
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceMathFround(
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
  return AddNewNode<Float64RoundToFloat32>({value});
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildCheckInstanceType(ValueNode* object,
                                                          NodeType target_type,
                                                          InstanceType first,
                                                          InstanceType last) {
  NodeType known_type;
  // Check for the empty type first so that we catch the case where
  // GetType(object) is already empty or disjoint.
  if (IsEmptyNodeType(IntersectType(GetType(object), target_type))) {
    return EmitUnconditionalDeopt(DeoptimizeReason::kWrongInstanceType);
  }
  if (EnsureType(object, target_type, &known_type)) {
    return ReduceResult::Done();
  }
  return AddNewNode<CheckInstanceType>({object}, GetCheckType(known_type),
                                       first, last);
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::GetInt32ElementIndex(ValueNode* object) {
  object->MaybeRecordUseReprHint(UseRepresentation::kInt32);

  switch (object->properties().value_representation()) {
    case ValueRepresentation::kIntPtr:
      return AddNewNodeNoInputConversion<CheckedIntPtrToInt32>({object});
    case ValueRepresentation::kTagged:
      NodeType old_type;
      if (SmiConstant* constant = object->TryCast<SmiConstant>()) {
        return GetInt32Constant(constant->value().value());
      } else if (CheckType(object, NodeType::kSmi, &old_type)) {
        auto& alternative = GetOrCreateInfoFor(object)->alternative();
        bool bailout = false;
        ValueNode* value = alternative.get_or_set_int32([&]() -> ValueNode* {
          ReduceResult result = BuildSmiUntag(object);
          if (result.IsDoneWithAbort()) {
            bailout = true;
            return nullptr;
          }
          return result.value();
        });
        if (bailout) {
          return ReduceResult::DoneWithAbort();
        }
        return value;
      } else {
        // TODO(leszeks): Cache this knowledge/converted value somehow on
        // the node info.
        return AddNewNodeNoInputConversion<CheckedObjectToIndex>(
            {object}, GetCheckType(old_type));
      }
    case ValueRepresentation::kInt32:
      // Already good.
      return object;
    case ValueRepresentation::kUint32:
    case ValueRepresentation::kFloat64:
    case ValueRepresentation::kHoleyFloat64:
      return GetInt32(object);
    case ValueRepresentation::kRawPtr:
    case ValueRepresentation::kNone:
      UNREACHABLE();
  }
  UNREACHABLE();
}

template <typename BaseT>
void MaglevReducer<BaseT>::RecordKnownProperty(
    ValueNode* lookup_start_object, PropertyKey key, ValueNode* value,
    bool is_const, compiler::AccessMode access_mode) {
  DCHECK(!value->is_conversion());
  auto& props_for_key =
      known_node_aspects().GetLoadedPropertiesForKey(zone(), is_const, key);
  if (!is_const && IsAnyStore(access_mode)) {
    if constexpr (ReducerBaseWithLoopEffectTracking<BaseT>) {
      if (base_->loop_effects()) {
        base_->loop_effects()->keys_cleared.insert(key);
      }
    }
    // We don't do any aliasing analysis, so stores clobber all other cached
    // loads of a property with that key. We only need to do this for
    // non-constant properties, since constant properties are known not to
    // change and therefore can't be clobbered.
    // TODO(leszeks): Do some light aliasing analysis here, e.g. checking
    // whether there's an intersection of known maps.
    TRACE("  * Removing all non-constant cached properties with " << key);
    props_for_key.clear();
  }

  TRACE("  * Recording " << (is_const ? "constant" : "non-constant")
                         << " known property "
                         << PrintNodeLabel(lookup_start_object) << ": "
                         << PrintNode(lookup_start_object) << " [" << key
                         << "] = " << PrintNodeLabel(value) << ": "
                         << PrintNode(value));

  if constexpr (ReducerBaseWithLoopEffectTracking<BaseT>) {
    if (IsAnyStore(access_mode) && !is_const && base_->loop_effects()) {
      auto updated = props_for_key.emplace(lookup_start_object, value);
      if (updated.second) {
        base_->loop_effects()->objects_written.insert(lookup_start_object);
      } else if (updated.first->second != value) {
        updated.first->second = value;
        base_->loop_effects()->objects_written.insert(lookup_start_object);
      }
      return;
    }
  }
  props_for_key[lookup_start_object] = value;
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildLoadJSDataViewByteLength(
    ValueNode* js_data_view) {
  // Note: We can't use broker()->byte_length_string() here, because it could
  // conflict with redefinitions of the ArrayBufferView byteLength property.
  if (ValueNode* byte_length =
          known_node_aspects().TryFindLoadedConstantProperty(
              js_data_view, PropertyKey::ArrayBufferViewByteLength())) {
    return byte_length;
  }

  ValueNode* result;
  GET_VALUE_OR_ABORT(result,
                     AddNewNode<LoadDataViewByteLength>({js_data_view}));
  RecordKnownProperty(js_data_view, PropertyKey::ArrayBufferViewByteLength(),
                      result, true, compiler::AccessMode::kLoad);
  return result;
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::BuildLoadJSDataViewDataPointer(
    ValueNode* js_data_view) {
  if (ValueNode* backing_store =
          known_node_aspects().TryFindLoadedConstantProperty(
              js_data_view, PropertyKey::ArrayBufferViewDataPointer())) {
    return backing_store;
  }

  ValueNode* result;
  GET_VALUE_OR_ABORT(result,
                     AddNewNode<LoadDataViewDataPointer>({js_data_view}));
  RecordKnownProperty(js_data_view, PropertyKey::ArrayBufferViewDataPointer(),
                      result, true, compiler::AccessMode::kLoad);
  return result;
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceStringLength(
    ValueNode* string) {
  NodeType node_type = GetType(string);
  if (node_type == NodeType::kNone ||
      !NodeTypeCanBe(node_type, NodeType::kString)) {
    return BuildAbort(AbortReason::kUnreachable);
  }
  if (auto vo_string = string->template TryCast<InlinedAllocation>()) {
    VirtualObject* vobj = vo_string->object();
    if (vobj->object_type() == vobj::ObjectType::kConsString) {
      return vobj->get(offsetof(String, length_));
    }
  }
  if (auto const_string = TryGetConstant<String>(string)) {
    return GetInt32Constant(const_string->length());
  }
  if (ValueNode* const_length =
          known_node_aspects().TryFindLoadedConstantProperty(
              string, PropertyKey::StringLength())) {
    TRACE("  * Reusing constant [String length]"
          << PrintNodeLabel(const_length) << ": " << PrintNode(const_length));
    return const_length;
  }
  return MaybeReduceResult::Fail();
}

template <typename BaseT>
template <typename LoadNode>
MaybeReduceResult MaglevReducer<BaseT>::TryBuildLoadDataView(
    const CallArguments& args, ExternalArrayType type) {
  if (!CanSpeculateCall()) return {};
  if (!broker()->dependencies()->DependOnArrayBufferDetachingProtector()) {
    // TODO(victorgomes): Add checks whether the array has been detached or is
    // immutable.
    return {};
  }
  ValueNode* receiver = GetValueOrUndefined(args.receiver());
  RETURN_IF_ABORT(BuildCheckInstanceType(receiver, NodeType::kJSDataView,
                                         JS_DATA_VIEW_TYPE, JS_DATA_VIEW_TYPE));
  // TODO(v8:11111): Optimize for JS_RAB_GSAB_DATA_VIEW_TYPE too.
  ValueNode* offset;
  if (args[0]) {
    GET_VALUE_OR_ABORT(offset, GetInt32ElementIndex(args[0]));
  } else {
    offset = GetInt32Constant(0);
  }

  ValueNode* byte_length;
  GET_VALUE_OR_ABORT(byte_length, BuildLoadJSDataViewByteLength(receiver));

  RETURN_IF_ABORT(
      AddNewNode<CheckJSDataViewBounds>({offset, byte_length}, type));

  ValueNode* data_pointer;
  GET_VALUE_OR_ABORT(data_pointer, BuildLoadJSDataViewDataPointer(receiver));

  ValueNode* is_little_endian = args[1] ? args[1] : GetBooleanConstant(false);
  return AddNewNode<LoadNode>(
      {receiver, data_pointer, offset, is_little_endian}, type);
}

template <typename BaseT>
template <typename StoreNode, typename Function>
MaybeReduceResult MaglevReducer<BaseT>::TryBuildStoreDataView(
    const CallArguments& args, ExternalArrayType type, Function&& getValue) {
  if (!CanSpeculateCall()) return {};
  if (!broker()->dependencies()->DependOnArrayBufferDetachingProtector() ||
      !broker()->dependencies()->DependOnArrayBufferMutableProtector()) {
    // TODO(victorgomes): Add checks whether the array has been detached.
    return {};
  }
  ValueNode* receiver = GetValueOrUndefined(args.receiver());
  RETURN_IF_ABORT(BuildCheckInstanceType(receiver, NodeType::kJSDataView,
                                         JS_DATA_VIEW_TYPE, JS_DATA_VIEW_TYPE));
  // TODO(v8:11111): Optimize for JS_RAB_GSAB_DATA_VIEW_TYPE too.
  ValueNode* offset;
  if (args[0]) {
    GET_VALUE_OR_ABORT(offset, GetInt32ElementIndex(args[0]));
  } else {
    offset = GetInt32Constant(0);
  }
  ValueNode* byte_length;
  GET_VALUE_OR_ABORT(byte_length, BuildLoadJSDataViewByteLength(receiver));

  RETURN_IF_ABORT(
      AddNewNode<CheckJSDataViewBounds>({offset, byte_length}, type));

  ValueNode* data_pointer;
  GET_VALUE_OR_ABORT(data_pointer, BuildLoadJSDataViewDataPointer(receiver));

  ValueNode* value;
  GET_VALUE_OR_ABORT(value, getValue(args[1]));
  ValueNode* is_little_endian = args[2] ? args[2] : GetBooleanConstant(false);
  RETURN_IF_ABORT(AddNewNode<StoreNode>(
      {receiver, data_pointer, offset, value, is_little_endian}, type));
  return GetRootConstant(RootIndex::kUndefinedValue);
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceDataViewPrototypeGetInt8(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryBuildLoadDataView<LoadSignedIntDataViewElement>(
      args, ExternalArrayType::kExternalInt8Array);
}
template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceDataViewPrototypeSetInt8(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryBuildStoreDataView<StoreSignedIntDataViewElement>(
      args, ExternalArrayType::kExternalInt8Array,
      [&](ValueNode* value) { return value ? value : GetInt32Constant(0); });
}
template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceDataViewPrototypeGetInt16(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryBuildLoadDataView<LoadSignedIntDataViewElement>(
      args, ExternalArrayType::kExternalInt16Array);
}
template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceDataViewPrototypeSetInt16(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryBuildStoreDataView<StoreSignedIntDataViewElement>(
      args, ExternalArrayType::kExternalInt16Array,
      [&](ValueNode* value) { return value ? value : GetInt32Constant(0); });
}
template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceDataViewPrototypeGetInt32(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryBuildLoadDataView<LoadSignedIntDataViewElement>(
      args, ExternalArrayType::kExternalInt32Array);
}
template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceDataViewPrototypeSetInt32(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryBuildStoreDataView<StoreSignedIntDataViewElement>(
      args, ExternalArrayType::kExternalInt32Array,
      [&](ValueNode* value) { return value ? value : GetInt32Constant(0); });
}
template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceDataViewPrototypeGetFloat64(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryBuildLoadDataView<LoadDoubleDataViewElement>(
      args, ExternalArrayType::kExternalFloat64Array);
}
template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceDataViewPrototypeSetFloat64(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryBuildStoreDataView<StoreDoubleDataViewElement>(
      args, ExternalArrayType::kExternalFloat64Array, [&](ValueNode* value) {
#ifdef V8_ENABLE_UNDEFINED_DOUBLE
        // Produce the same bit pattern we would get through computation.
        auto ud = Float64::FromBits(kUndefinedNanInt64);
        const double silenced_nan = ud.to_quiet_nan().get_scalar();
#else
        const double silenced_nan = std::numeric_limits<double>::quiet_NaN();
#endif  // V8_ENABLE_UNDEFINED_DOUBLE
        return value ? GetFloat64ForToNumber(value, NodeType::kNumberOrOddball)
                     : GetFloat64Constant(silenced_nan);
      });
}

#define MATH_UNARY_IEEE_BUILTIN_REDUCER(MathName, ExtName, EnumName)       \
  template <typename BaseT>                                                \
  MaybeReduceResult MaglevReducer<BaseT>::TryReduce##MathName(             \
      compiler::JSFunctionRef target, CallArguments& args) {               \
    if (args.count() < 1) {                                                \
      return GetRootConstant(RootIndex::kNanValue);                        \
    }                                                                      \
    RETURN_IF_DONE(TryFoldFloat64Ieee754Unary(                             \
        Float64Ieee754Unary::Ieee754Function::k##EnumName, args[0]));      \
    if (!CanSpeculateCall() && !CheckType(args[0], NodeType::kNumber)) {   \
      return {};                                                           \
    }                                                                      \
    ValueNode* value;                                                      \
    GET_VALUE_OR_ABORT(value,                                              \
                       GetFloat64ForToNumber(args[0], NodeType::kNumber)); \
    return AddNewNode<Float64Ieee754Unary>(                                \
        {value}, Float64Ieee754Unary::Ieee754Function::k##EnumName);       \
  }
IEEE_754_UNARY_LIST(MATH_UNARY_IEEE_BUILTIN_REDUCER)
#undef MATH_UNARY_IEEE_BUILTIN_REDUCER

#define MATH_BINARY_IEEE_BUILTIN_REDUCER(MathName, ExtName, EnumName)      \
  template <typename BaseT>                                                \
  MaybeReduceResult MaglevReducer<BaseT>::TryReduce##MathName(             \
      compiler::JSFunctionRef target, CallArguments& args) {               \
    if (args.count() < 2) {                                                \
      if (args.count() == 1 && !CheckType(args[0], NodeType::kNumber)) {   \
        return {};                                                         \
      }                                                                    \
      return GetRootConstant(RootIndex::kNanValue);                        \
    }                                                                      \
    RETURN_IF_DONE(TryFoldFloat64Ieee754Binary(                            \
        Float64Ieee754Binary::Ieee754Function::k##EnumName, args[0],       \
        args[1]));                                                         \
    if (!CanSpeculateCall() && (!CheckType(args[0], NodeType::kNumber) ||  \
                                !CheckType(args[1], NodeType::kNumber))) { \
      return {};                                                           \
    }                                                                      \
    ValueNode* lhs;                                                        \
    GET_VALUE_OR_ABORT(lhs,                                                \
                       GetFloat64ForToNumber(args[0], NodeType::kNumber)); \
    ValueNode* rhs;                                                        \
    GET_VALUE_OR_ABORT(rhs,                                                \
                       GetFloat64ForToNumber(args[1], NodeType::kNumber)); \
    return AddNewNode<Float64Ieee754Binary>(                               \
        {lhs, rhs}, Float64Ieee754Binary::Ieee754Function::k##EnumName);   \
  }
IEEE_754_BINARY_LIST(MATH_BINARY_IEEE_BUILTIN_REDUCER)
#undef MATH_BINARY_IEEE_BUILTIN_REDUCER

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceDatePrototypeGetFieldPrologue(
    compiler::JSFunctionRef target, CallArguments& args) {
  if (!v8_flags.maglev_inline_date_accessors) return {};
  if (!CanSpeculateCall()) return {};

  if (args.receiver_mode() == ConvertReceiverMode::kNullOrUndefined) {
    return {};
  }

  ValueNode* receiver = GetValueOrUndefined(args.receiver());
  // If the map set is not found, then we don't know anything about the map of
  // the receiver, so bail.
  MapInference<MaglevReducer<BaseT>> inference(this, receiver);
  auto possible_receiver_maps = inference.TryGetPossibleMaps();
  if (!possible_receiver_maps) {
    return {};
  }

  // If the set of possible maps is empty, then there's no possible map for this
  // receiver, therefore this path is unreachable at runtime. We're unlikely to
  // ever hit this case, BuildCheckMaps should already unconditionally deopt,
  // but check it in case another checking operation fails to statically
  // unconditionally deopt.
  if (possible_receiver_maps->is_empty()) {
    // TODO(leszeks): Add an unreachable assert here.
    return ReduceResult::DoneWithAbort();
  }

  // Don't optimize if any of the known maps is something else than a Date map.
  for (compiler::MapRef map : *possible_receiver_maps) {
    if (map.instance_type() != JS_DATE_TYPE) {
      return {};
    }
  }

  if (!broker()
           ->dependencies()
           ->DependOnNoDateTimeConfigurationChangeProtector()) {
    return {};
  }

  RETURN_IF_ABORT(inference.InsertMapChecks(zone()));

  return ReduceResult::Done();
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceDatePrototypeGetField(
    compiler::JSFunctionRef target, CallArguments& args,
    JSDate::FieldIndex field_index) {
  DCHECK_LT(field_index, JSDate::kFirstUncachedField);
  auto prologue_result = TryReduceDatePrototypeGetFieldPrologue(target, args);
  if (!prologue_result.IsDoneWithoutPayload()) return prologue_result;

  ValueNode* receiver = args.receiver();
  DCHECK_NOT_NULL(receiver);
  int field_offset = offsetof(JSDate, year_) + field_index * kTaggedSize;
  ValueNode* field_value;
  GET_VALUE_OR_ABORT(field_value, BuildLoadTaggedField(receiver, field_offset));
  // All cached JSDate fields are Smi|NaN.
  RETURN_IF_ABORT(BuildCheckSmi(field_value));
  // TODO(jgruber): Presumably NaN time values are rare, so we simply deopt when
  // encountering such an object. Alternatively, to avoid the deopt we could
  // fall back to Number: EnsureType(field_value, NodeType::kNumber);
  return field_value;
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceDatePrototypeGetTime(
    compiler::JSFunctionRef target, CallArguments& args) {
  auto prologue_result = TryReduceDatePrototypeGetFieldPrologue(target, args);
  if (!prologue_result.IsDoneWithoutPayload()) return prologue_result;

  ValueNode* receiver = args.receiver();
  DCHECK_NOT_NULL(receiver);
  return AddNewNode<LoadFloat64>({receiver},
                                 static_cast<int>(offsetof(JSDate, value_)));
}

template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceDatePrototypeGetFullYear(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryReduceDatePrototypeGetField(target, args, JSDate::kYear);
}
template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceDatePrototypeGetMonth(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryReduceDatePrototypeGetField(target, args, JSDate::kMonth);
}
template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceDatePrototypeGetDate(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryReduceDatePrototypeGetField(target, args, JSDate::kDay);
}
template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceDatePrototypeGetDay(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryReduceDatePrototypeGetField(target, args, JSDate::kWeekday);
}
template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceDatePrototypeGetHours(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryReduceDatePrototypeGetField(target, args, JSDate::kHour);
}
template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceDatePrototypeGetMinutes(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryReduceDatePrototypeGetField(target, args, JSDate::kMinute);
}
template <typename BaseT>
MaybeReduceResult MaglevReducer<BaseT>::TryReduceDatePrototypeGetSeconds(
    compiler::JSFunctionRef target, CallArguments& args) {
  return TryReduceDatePrototypeGetField(target, args, JSDate::kSecond);
}

#ifdef V8_INTL_SUPPORT
template <typename BaseT>
MaybeReduceResult
MaglevReducer<BaseT>::TryReduceStringPrototypeLocaleCompareIntl(
    compiler::JSFunctionRef target, CallArguments& args) {
  if (args.count() < 1 || args.count() > 3) return {};

  LocalFactory* factory = local_isolate()->factory();
  compiler::ObjectRef undefined_ref = broker()->undefined_value();

  DirectHandle<Object> locales_handle;
  ValueNode* locales_node = nullptr;
  if (args.count() > 1) {
    compiler::OptionalHeapObjectRef maybe_locales =
        TryGetConstant<HeapObject>(args[1]);
    if (!maybe_locales) return {};
    compiler::HeapObjectRef locales = maybe_locales.value();
    if (locales.equals(undefined_ref)) {
      locales_handle = factory->undefined_value();
      locales_node = GetRootConstant(RootIndex::kUndefinedValue);
    } else {
      if (!locales.IsString()) return {};
      compiler::StringRef sref = locales.AsString();
      std::optional<Handle<String>> maybe_locales_handle =
          sref.ObjectIfContentAccessible(broker());
      if (!maybe_locales_handle) return {};
      locales_handle = *maybe_locales_handle;
      locales_node = args[1];
    }
  } else {
    locales_handle = factory->undefined_value();
    locales_node = GetRootConstant(RootIndex::kUndefinedValue);
  }

  if (args.count() > 2) {
    compiler::OptionalHeapObjectRef maybe_options =
        TryGetConstant<HeapObject>(args[2]);
    if (!maybe_options) return {};
    if (!maybe_options.value().equals(undefined_ref)) return {};
  }

  DCHECK(!locales_handle.is_null());
  DCHECK_NOT_NULL(locales_node);

  if (Intl::CompareStringsOptionsFor(local_isolate(), locales_handle,
                                     factory->undefined_value()) !=
      Intl::CompareStringsOptions::kTryFastPath) {
    return {};
  }
  return AddNewNode<StringLocaleCompareIntl>(
      {GetConstant(target), GetValueOrUndefined(args.receiver()), args[0],
       locales_node});
}
#endif  // V8_INTL_SUPPORT

#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
template <typename BaseT>
MaybeReduceResult
MaglevReducer<BaseT>::TryReduceGetContinuationPreservedEmbedderData(
    compiler::JSFunctionRef target, CallArguments& args) {
  return AddNewNode<GetContinuationPreservedEmbedderData>({});
}

template <typename BaseT>
MaybeReduceResult
MaglevReducer<BaseT>::TryReduceSetContinuationPreservedEmbedderData(
    compiler::JSFunctionRef target, CallArguments& args) {
  if (args.count() == 0) return {};

  RETURN_IF_ABORT(AddNewNode<SetContinuationPreservedEmbedderData>({args[0]}));
  return GetRootConstant(RootIndex::kUndefinedValue);
}
#endif  // V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::GetSmiValue(
    ValueNode* value, UseReprHintRecording record_use_repr_hint) {
  if (V8_LIKELY(record_use_repr_hint == UseReprHintRecording::kRecord)) {
    value->MaybeRecordUseReprHint(UseRepresentation::kTagged);
  }

  NodeInfo* node_info = GetOrCreateInfoFor(value);

  ValueRepresentation representation =
      value->properties().value_representation();
  if (representation == ValueRepresentation::kTagged) {
    return BuildCheckSmi(value);
  }

  auto& alternative = node_info->alternative();

  if (ValueNode* alt = alternative.tagged()) {
#ifdef DEBUG
    if (HoleyFloat64ToTagged* conversion_node =
            alt->TryCast<HoleyFloat64ToTagged>()) {
      DCHECK_EQ(conversion_node->conversion_mode(),
                NumberConversionMode::kCanonicalizeSmi);
    }
#endif  // DEBUG
    return BuildCheckSmi(alt);
  }

  switch (representation) {
    case ValueRepresentation::kInt32: {
      if (NodeTypeIsSmi(node_info->type())) {
        return alternative.set_tagged(
            AddNewNodeNoInputConversion<UnsafeSmiTagInt32>({value}));
      }
      return alternative.set_tagged(
          AddNewNodeNoInputConversion<CheckedSmiTagInt32>({value}));
    }
    case ValueRepresentation::kUint32: {
      if (NodeTypeIsSmi(node_info->type())) {
        return alternative.set_tagged(
            AddNewNodeNoInputConversion<UnsafeSmiTagUint32>({value}));
      }
      return alternative.set_tagged(
          AddNewNodeNoInputConversion<CheckedSmiTagUint32>({value}));
    }
    case ValueRepresentation::kFloat64: {
      return alternative.set_tagged(
          AddNewNodeNoInputConversion<CheckedSmiTagFloat64>({value}));
    }
    case ValueRepresentation::kHoleyFloat64: {
      return alternative.set_tagged(
          AddNewNodeNoInputConversion<CheckedSmiTagHoleyFloat64>({value}));
    }
    case ValueRepresentation::kIntPtr:
      return alternative.set_tagged(
          AddNewNodeNoInputConversion<CheckedSmiTagIntPtr>({value}));
    case ValueRepresentation::kTagged:
    case ValueRepresentation::kRawPtr:
    case ValueRepresentation::kNone:
      UNREACHABLE();
  }
  UNREACHABLE();
}

template <typename BaseT>
VirtualObject* MaglevReducer<BaseT>::CreateHeapNumber(ValueNode* value) {
  using Shape = VirtualHeapNumberShape;
  int slot_count = Shape::header_slot_count;
  SBXCHECK_EQ(slot_count, 2);
  compiler::MapRef map = broker()->heap_number_map();
  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(), &Shape::kObjectLayout, map, slot_count);
  vobj->set(offsetof(HeapObject, map_), GetConstant(map));
  vobj->set(offsetof(HeapNumber, value_), value);
  return vobj;
}

template <typename BaseT>
VirtualObject* MaglevReducer<BaseT>::CreateConsString(ValueNode* map,
                                                      ValueNode* length,
                                                      ValueNode* first,
                                                      ValueNode* second) {
  using Shape = VirtualConsStringShape;
  int slot_count = Shape::header_slot_count;
  SBXCHECK_EQ(slot_count, 5);
  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(), &Shape::kObjectLayout,
      compiler::OptionalMapRef{}, slot_count);
  vobj->set(offsetof(HeapObject, map_), map);
  vobj->set(offsetof(ConsString, raw_hash_field_),
            GetInt32Constant(Name::kEmptyHashField));
  vobj->set(offsetof(ConsString, length_), length);
  vobj->set(offsetof(ConsString, first_), first);
  vobj->set(offsetof(ConsString, second_), second);
  return vobj;
}

template <typename BaseT>
VirtualObject* MaglevReducer<BaseT>::CreateJSObject(compiler::MapRef map) {
  using Shape = VirtualJSObjectShape;
  DCHECK(!map.is_dictionary_map());
  DCHECK(!map.IsInobjectSlackTrackingInProgress());
  int slot_count = map.instance_size() / kTaggedSize;
  SBXCHECK_GE(slot_count, 3);
  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(), &Shape::kObjectLayout, map, slot_count);
  vobj->set(offsetof(HeapObject, map_), GetConstant(map));
  vobj->set(offsetof(JSObject, properties_or_hash_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSObject, elements_),
            GetRootConstant(RootIndex::kEmptyFixedArray));

  // Initialize all in-object property slots to undefined.
  if (map.GetInObjectProperties() > 0) {
    ValueNode* undefined = GetRootConstant(RootIndex::kUndefinedValue);
    for (int i = 0; i < map.GetInObjectProperties(); i++) {
      vobj->set(map.GetInObjectPropertyOffset(i), undefined);
    }
  }

  return vobj;
}

template <typename BaseT>
ReduceResult MaglevReducer<BaseT>::CreateJSArray(compiler::MapRef map,
                                                 int instance_size,
                                                 ValueNode* length) {
  using Shape = VirtualJSArrayShape;
  int slot_count = instance_size / kTaggedSize;
  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(), &Shape::kObjectLayout, map, slot_count);
  vobj->set(offsetof(HeapObject, map_), GetConstant(map));
  vobj->set(offsetof(JSArray, properties_or_hash_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  // Either the value is a Smi already, or we force a conversion to Smi and
  // cache the value in its alternative representation node.
  // TODO(454485895): Consider removing this workaround since
  // HoleyFloat64ToTagged now canonicalizes by default.
  RETURN_IF_ABORT(GetSmiValue(length));
  vobj->set(offsetof(JSObject, elements_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSArray, length_), length);
  return vobj;
}

template <typename BaseT>
VirtualObject* MaglevReducer<BaseT>::CreateJSStringWrapper(ValueNode* value) {
  using Shape = VirtualJSPrimitiveWrapperShape;
  compiler::MapRef map =
      broker()->target_native_context().string_function(broker()).initial_map(
          broker());
  int slot_count = Shape::header_slot_count;
  SBXCHECK_EQ(slot_count, 4);
  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(), &Shape::kObjectLayout, map, slot_count);
  vobj->set(offsetof(HeapObject, map_), GetConstant(map));
  vobj->set(offsetof(JSObject, properties_or_hash_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSObject, elements_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSPrimitiveWrapper, value_), value);
  return vobj;
}

template <typename BaseT>
VirtualObject* MaglevReducer<BaseT>::CreateJSArrayIterator(
    compiler::MapRef map, ValueNode* iterated_object, IterationKind kind) {
  using Shape = VirtualJSArrayIteratorShape;
  int slot_count = Shape::header_slot_count;
  SBXCHECK_EQ(slot_count, 6);
  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(), &Shape::kObjectLayout, map, slot_count);
  vobj->set(offsetof(HeapObject, map_), GetConstant(map));
  vobj->set(offsetof(JSArrayIterator, properties_or_hash_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSArrayIterator, elements_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSArrayIterator, iterated_object_), iterated_object);
  vobj->set(offsetof(JSArrayIterator, next_index_), GetInt32Constant(0));
  vobj->set(offsetof(JSArrayIterator, kind_),
            GetInt32Constant(static_cast<int>(kind)));
  return vobj;
}

template <typename BaseT>
VirtualObject* MaglevReducer<BaseT>::CreateJSConstructor(
    compiler::JSFunctionRef constructor) {
  DCHECK(constructor.has_initial_map(broker()));
  using Shape = VirtualJSObjectShape;
  // TODO(jgruber): SlackTrackingPrediction should store the initial_map.
  compiler::SlackTrackingPrediction prediction =
      broker()->dependencies()->DependOnInitialMapInstanceSizePrediction(
          constructor);
  compiler::MapRef map = constructor.initial_map(broker());
  int slot_count = prediction.instance_size() / kTaggedSize;
  SBXCHECK_GE(slot_count, 3);
  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(), &Shape::kObjectLayout, map, slot_count);
  vobj->set(offsetof(HeapObject, map_), GetConstant(map));
  vobj->set(offsetof(JSObject, properties_or_hash_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSObject, elements_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  if (prediction.inobject_property_count() != 0) {
    ValueNode* undefined = GetRootConstant(RootIndex::kUndefinedValue);
    for (int i = 0; i < prediction.inobject_property_count(); i++) {
      vobj->set(map.GetInObjectPropertyOffset(i), undefined);
    }
  }
  return vobj;
}

template <typename BaseT>
VirtualObject* MaglevReducer<BaseT>::CreateFixedArray(
    base::Vector<ValueNode* const> values) {
  const compiler::MapRef& map = broker()->fixed_array_map();

  using Shape = VirtualFixedArrayShape;
  int length = values.length();
  DCHECK_NE(length, 0);  // Use kEmptyFixedArray instead.
  DCHECK_EQ(FixedArray::SizeFor(length) % FieldSizeOf(Shape::kBodyFieldType),
            0);
#if TAGGED_SIZE_8_BYTES
  // FixedArray header fields are: map, length, optional_padding
  DCHECK_EQ(Shape::header_slot_count, 3);
#else
  // FixedArray header fields are: map, length
  DCHECK_EQ(Shape::header_slot_count, 2);
#endif  // TAGGED_SIZE_8_BYTES
  DCHECK_EQ(length,
            (FixedArray::SizeFor(length) - FixedArrayBase::kHeaderSize) /
                FieldSizeOf(Shape::kBodyFieldType));

  int slot_count = Shape::header_slot_count + length;
  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(), &Shape::kObjectLayout, map, slot_count);
  DCHECK_EQ(vobj->size(), FixedArray::SizeFor(length));

  vobj->set(offsetof(HeapObject, map_), GetConstant(map));
  vobj->set(FixedArrayBase::kLengthOffset, GetInt32Constant(length));
#if TAGGED_SIZE_8_BYTES
  vobj->set(FixedArrayBase::kPaddingOffset, GetInt32Constant(0));
#endif  // TAGGED_SIZE_8_BYTES
  for (int i = 0; i < length; i++) {
    DCHECK_NOT_NULL(values[i]);
    vobj->set(FixedArray::OffsetOfElementAt(i), values[i]);
  }

  return vobj;
}

template <typename BaseT>
VirtualObject* MaglevReducer<BaseT>::CreateFixedDoubleArray(
    base::Vector<ValueNode* const> values) {
  compiler::MapRef map = broker()->fixed_double_array_map();

  using T = FixedDoubleArray;
  using Shape = VirtualFixedDoubleArrayShape;
  uint32_t length = values.length();
  DCHECK_NE(length, 0);  // Use kEmptyFixedArray instead.

  int slot_count = Shape::header_slot_count + length;
  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(), &Shape::kObjectLayout, map, slot_count);
  DCHECK_EQ(vobj->size(), T::SizeFor(length));

#if TAGGED_SIZE_8_BYTES
  DCHECK_EQ(Shape::header_slot_count, 3);
#else
  DCHECK_EQ(Shape::header_slot_count, 2);
#endif  // TAGGED_SIZE_8_BYTES
  vobj->set(offsetof(HeapObject, map_), GetConstant(map));
  vobj->set(FixedArrayBase::kLengthOffset, GetInt32Constant(length));
#if TAGGED_SIZE_8_BYTES
  vobj->set(FixedArrayBase::kPaddingOffset, GetInt32Constant(0));
#endif  // TAGGED_SIZE_8_BYTES
  for (uint32_t i = 0; i < length; i++) {
    vobj->set(T::OffsetOfElementAt(i), values[i]);
  }
  return vobj;
}

template <typename BaseT>
VirtualObject* MaglevReducer<BaseT>::CreateContext(
    compiler::MapRef map, int length, compiler::ScopeInfoRef scope_info,
    ValueNode* previous_context, std::optional<ValueNode*> extension) {
  using Shape = ContextShape;
  SBXCHECK_GE(length, Context::MIN_CONTEXT_SLOTS);
  DCHECK_EQ(Context::SizeFor(length) % FieldSizeOf(Shape::kBodyFieldType), 0);
  DCHECK_EQ(Shape::header_slot_count + length,
            Context::SizeFor(length) / FieldSizeOf(Shape::kBodyFieldType));

  int slot_count = Shape::header_slot_count + length;
  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(), &Shape::kObjectLayout, map, slot_count);

  vobj->set(offsetof(HeapObject, map_), GetConstant(map));
  vobj->set(offsetof(Context, length_), GetSmiConstant(length));
  vobj->set(Context::OffsetOfElementAt(Context::SCOPE_INFO_INDEX),
            GetConstant(scope_info));
  vobj->set(Context::OffsetOfElementAt(Context::PREVIOUS_INDEX),
            previous_context);
  int index = Context::PREVIOUS_INDEX + 1;
  if (extension.has_value()) {
    SBXCHECK_GE(length, Context::MIN_CONTEXT_EXTENDED_SLOTS);
    vobj->set(Context::OffsetOfElementAt(Context::EXTENSION_INDEX),
              extension.value());
    index++;
  }
  for (; index < length; index++) {
    vobj->set(Context::OffsetOfElementAt(index),
              GetRootConstant(RootIndex::kUndefinedValue));
  }
  RecordType(vobj, NodeType::kContext);
  return vobj;
}

template <typename BaseT>
VirtualObject* MaglevReducer<BaseT>::CreateArgumentsObject(
    compiler::MapRef map, ValueNode* length, ValueNode* elements,
    std::optional<ValueNode*> callee) {
  using Shape = VirtualJSObjectShape;
  DCHECK_EQ(JSSloppyArgumentsObject::kLengthOffset, offsetof(JSArray, length_));
  DCHECK_EQ(JSStrictArgumentsObject::kLengthOffset, offsetof(JSArray, length_));
  int slot_count = map.instance_size() / kTaggedSize;
  SBXCHECK_EQ(slot_count, callee.has_value() ? 5 : 4);
  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(), &Shape::kObjectLayout, map, slot_count);
  vobj->set(offsetof(HeapObject, map_), GetConstant(map));
  vobj->set(offsetof(JSArray, properties_or_hash_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSObject, elements_), elements);
  CHECK(length->Is<Int32Constant>() || length->Is<ArgumentsLength>() ||
        length->Is<RestLength>());
  vobj->set(offsetof(JSArray, length_), length);
  if (callee.has_value()) {
    vobj->set(JSSloppyArgumentsObject::kCalleeOffset, callee.value());
  }
  DCHECK(vobj->map()->IsJSArgumentsObjectMap() || vobj->map()->IsJSArrayMap());
  return vobj;
}

template <typename BaseT>
VirtualObject* MaglevReducer<BaseT>::CreateMappedArgumentsElements(
    compiler::MapRef map, int mapped_count, ValueNode* context,
    ValueNode* unmapped_elements) {
  using Shape = VirtualSloppyArgumentsElementsShape;
  int slot_count = Shape::header_slot_count + mapped_count;
#if TAGGED_SIZE_8_BYTES
  DCHECK_EQ(Shape::header_slot_count, 5);
#else
  DCHECK_EQ(Shape::header_slot_count, 4);
#endif  // TAGGED_SIZE_8_BYTES
  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(), &Shape::kObjectLayout, map, slot_count);
  vobj->set(offsetof(HeapObject, map_), GetConstant(map));
  vobj->set(offsetof(SloppyArgumentsElements, length_),
            GetInt32Constant(mapped_count));
#if TAGGED_SIZE_8_BYTES
  vobj->set(offsetof(SloppyArgumentsElements, optional_padding_),
            GetInt32Constant(0));
#endif  // TAGGED_SIZE_8_BYTES
  vobj->set(offsetof(SloppyArgumentsElements, context_), context);
  vobj->set(offsetof(SloppyArgumentsElements, arguments_), unmapped_elements);
  return vobj;
}

template <typename BaseT>
VirtualObject* MaglevReducer<BaseT>::CreateRegExpLiteralObject(
    compiler::MapRef map, compiler::RegExpBoilerplateDescriptionRef literal) {
  using Shape = VirtualJSRegExpShape;
  DCHECK_EQ(JSRegExp::Size(), JSRegExp::kLastIndexOffset + kTaggedSize);
  int slot_count = Shape::header_slot_count + JSRegExp::kInObjectFieldCount;
  SBXCHECK_EQ(slot_count, 6);
  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(), &Shape::kObjectLayout, map, slot_count);
  vobj->set(offsetof(HeapObject, map_), GetConstant(map));
  vobj->set(offsetof(JSRegExp, properties_or_hash_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSObject, elements_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSRegExp, data_),
            GetTrustedConstant(literal.data(broker()),
                               kRegExpDataIndirectPointerTag));
  vobj->set(offsetof(JSRegExp, flags_), GetInt32Constant(literal.flags()));
  vobj->set(JSRegExp::kLastIndexOffset,
            GetInt32Constant(JSRegExp::kInitialLastIndexValue));
  return vobj;
}

template <typename BaseT>
VirtualObject* MaglevReducer<BaseT>::CreateJSGeneratorObject(
    compiler::MapRef map, int instance_size, ValueNode* context,
    ValueNode* closure, ValueNode* receiver, ValueNode* register_file) {
  InstanceType instance_type = map.instance_type();
  DCHECK(instance_type == JS_GENERATOR_OBJECT_TYPE ||
         instance_type == JS_ASYNC_GENERATOR_OBJECT_TYPE);
  const bool is_async = instance_type == JS_ASYNC_GENERATOR_OBJECT_TYPE;
  const vobj::ObjectLayout* object_layout =
      is_async ? &VirtualJSAsyncGeneratorObjectShape::kObjectLayout
               : &VirtualJSGeneratorObjectShape::kObjectLayout;

  int slot_count = instance_size / kTaggedSize;
  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(), object_layout, map, slot_count);
  vobj->set(offsetof(HeapObject, map_), GetConstant(map));
  vobj->set(offsetof(JSGeneratorObject, properties_or_hash_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSGeneratorObject, elements_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSGeneratorObject, context_), context);
  vobj->set(offsetof(JSGeneratorObject, function_), closure);
  vobj->set(offsetof(JSGeneratorObject, receiver_), receiver);
  vobj->set(offsetof(JSGeneratorObject, input_or_debug_pos_),
            GetRootConstant(RootIndex::kUndefinedValue));
  vobj->set(offsetof(JSGeneratorObject, resume_mode_),
            GetInt32Constant(JSGeneratorObject::kNext));
  vobj->set(offsetof(JSGeneratorObject, continuation_),
            GetInt32Constant(JSGeneratorObject::kGeneratorExecuting));
  vobj->set(offsetof(JSGeneratorObject, parameters_and_registers_),
            register_file);
  if (is_async) {
    vobj->set(offsetof(JSAsyncGeneratorObject, queue_),
              GetRootConstant(RootIndex::kUndefinedValue));
    vobj->set(offsetof(JSAsyncGeneratorObject, is_awaiting_),
              GetInt32Constant(0));
  }
  return vobj;
}

template <typename BaseT>
VirtualObject* MaglevReducer<BaseT>::CreateJSAsyncFunctionObject(
    ValueNode* context, ValueNode* closure, ValueNode* receiver,
    ValueNode* register_file, ValueNode* promise) {
  compiler::MapRef map =
      broker()->target_native_context().async_function_object_map(broker());
  const vobj::ObjectLayout* object_layout =
      &VirtualJSAsyncFunctionObjectShape::kObjectLayout;

  constexpr int slot_count = sizeof(JSAsyncFunctionObject) / kTaggedSize;
  static_assert(slot_count == 13,
                "If the number of slots in JSAsyncFunctionObject changes, then "
                "the additional slots need to be initialized below");

  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(), object_layout, map, slot_count);
  vobj->set(offsetof(HeapObject, map_), GetConstant(map));
  vobj->set(offsetof(JSAsyncFunctionObject, properties_or_hash_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSAsyncFunctionObject, elements_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSAsyncFunctionObject, context_), context);
  vobj->set(offsetof(JSAsyncFunctionObject, function_), closure);
  vobj->set(offsetof(JSAsyncFunctionObject, receiver_), receiver);
  vobj->set(offsetof(JSAsyncFunctionObject, input_or_debug_pos_),
            GetRootConstant(RootIndex::kUndefinedValue));
  vobj->set(offsetof(JSAsyncFunctionObject, resume_mode_),
            GetInt32Constant(JSGeneratorObject::kNext));
  vobj->set(offsetof(JSAsyncFunctionObject, continuation_),
            GetInt32Constant(JSGeneratorObject::kGeneratorExecuting));
  vobj->set(offsetof(JSAsyncFunctionObject, parameters_and_registers_),
            register_file);
  vobj->set(offsetof(JSAsyncFunctionObject, promise_), promise);
  vobj->set(offsetof(JSAsyncFunctionObject, await_resolve_closure_),
            GetRootConstant(RootIndex::kUndefinedValue));
  vobj->set(offsetof(JSAsyncFunctionObject, await_reject_closure_),
            GetRootConstant(RootIndex::kUndefinedValue));

  return vobj;
}

template <typename BaseT>
VirtualObject* MaglevReducer<BaseT>::CreateJSPromiseObject() {
  compiler::MapRef promise_map =
      broker()->target_native_context().promise_function(broker()).initial_map(
          broker());
  int instance_size = promise_map.instance_size();
  int slot_count = instance_size / kTaggedSize;
  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(),
      &VirtualJSPromiseObjectShape::kObjectLayout, promise_map, slot_count);
  vobj->set(offsetof(HeapObject, map_), GetConstant(promise_map));
  vobj->set(offsetof(JSPromise, properties_or_hash_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSObject, elements_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSPromise, reactions_or_result_), GetSmiConstant(0));
  static_assert(v8::Promise::kPending == 0);
  vobj->set(offsetof(JSPromise, flags_), GetSmiConstant(0));
  static_assert(sizeof(JSPromise) == 5 * kTaggedSize);
  for (int offset = sizeof(JSPromise);
       offset < static_cast<int>(sizeof(JSPromise)) +
                    v8::Promise::kEmbedderFieldCount * kEmbedderDataSlotSize;
       offset += kTaggedSize) {
    vobj->set(offset, GetSmiConstant(0));
  }
  return vobj;
}

template <typename BaseT>
VirtualObject* MaglevReducer<BaseT>::CreateAsyncResumeTask(ValueNode* generator,
                                                           ValueNode* value,
                                                           ValueNode* kind) {
  compiler::MapRef map = broker()->async_resume_task_map();
  int instance_size = map.instance_size();
  int slot_count = instance_size / kTaggedSize;
  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(),
      &VirtualAsyncResumeTaskShape::kObjectLayout, map, slot_count);
  vobj->set(offsetof(HeapObject, map_), GetConstant(map));
#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
  ValueNode* cped =
      AddNewNodeNoInputConversion<GetContinuationPreservedEmbedderData>({});
  vobj->set(ObjectTraits<Microtask>::kContinuationPreservedEmbedderDataOffset,
            cped);
#endif
  vobj->set(ObjectTraits<AsyncResumeTask>::kGeneratorOffset, generator);
  vobj->set(ObjectTraits<AsyncResumeTask>::kValueOffset, value);
  vobj->set(ObjectTraits<AsyncResumeTask>::kKindOffset, kind);
  return vobj;
}

template <typename BaseT>
VirtualObject* MaglevReducer<BaseT>::CreateJSIteratorResult(
    compiler::MapRef map, ValueNode* value, ValueNode* done) {
  using Shape = VirtualJSIteratorResultShape;
  static_assert(JSIteratorResult::kSize == 5 * kTaggedSize);
  int slot_count = Shape::header_slot_count;
  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(), &Shape::kObjectLayout, map, slot_count);
  vobj->set(offsetof(HeapObject, map_), GetConstant(map));
  vobj->set(offsetof(JSIteratorResult, properties_or_hash_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSObject, elements_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(JSIteratorResult::kValueOffset, value);
  vobj->set(JSIteratorResult::kDoneOffset, done);
  return vobj;
}

template <typename BaseT>
VirtualObject* MaglevReducer<BaseT>::CreateJSStringIterator(
    compiler::MapRef map, ValueNode* string) {
  using Shape = VirtualJSStringIteratorShape;
  static_assert(sizeof(JSStringIterator) == 5 * kTaggedSize);
  int slot_count = Shape::header_slot_count;
  VirtualObject* vobj = NodeBase::New<VirtualObject>(
      zone(), 0, NewObjectId(), zone(), &Shape::kObjectLayout, map, slot_count);
  vobj->set(offsetof(HeapObject, map_), GetConstant(map));
  vobj->set(offsetof(JSStringIterator, properties_or_hash_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSStringIterator, elements_),
            GetRootConstant(RootIndex::kEmptyFixedArray));
  vobj->set(offsetof(JSStringIterator, string_), string);
  vobj->set(offsetof(JSStringIterator, index_), GetInt32Constant(0));
  return vobj;
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
#define CASE(Name, ...)                     \
  case Builtin::k##Name:                    \
    result = TryReduce##Name(target, args); \
    break;
    MAGLEV_REDUCER_BUILTIN(CASE)
#undef CASE
    default:
      // Not yet ported. The remaining builtins still go through the
      // bytecode-builder dispatcher.
      break;
  }

  current_speculation_feedback_ = saved_feedback;
  current_speculation_mode_ = saved_mode;
  return result;
}

template <typename DerivedT, typename BaseT>
SubgraphBase<DerivedT, BaseT>::Label::Label(SubgraphBase* sg,
                                            int predecessor_count)
    : predecessor_count_(predecessor_count),
      variable_liveness_(
          sg->reducer_->zone()->template New<compiler::BytecodeLivenessState>(
              sg->dummy_unit_->register_count(), sg->reducer_->zone())) {}

template <typename DerivedT, typename BaseT>
SubgraphBase<DerivedT, BaseT>::Label::Label(
    SubgraphBase* sg, int predecessor_count,
    std::initializer_list<Variable*> vars)
    : Label(sg, predecessor_count) {
  for (Variable* var : vars) {
    variable_liveness_->MarkRegisterLive(var->pseudo_register_.index());
  }
}

template <typename DerivedT, typename BaseT>
SubgraphBase<DerivedT, BaseT>::Label::Label(
    const LabelForTrackingInterpreterFrameState& label)
    : Label(label.sg_, label.predecessor_count_) {
  for (Variable* var : label.vars_) {
    variable_liveness_->MarkRegisterLive(var->pseudo_register_.index());
  }
  future_bind_offset_ = label.future_bind_offset_;
}

template <typename DerivedT, typename BaseT>
void SubgraphBase<DerivedT, BaseT>::ReducePredecessorCount(Label* label,
                                                           unsigned num) {
  DCHECK_GE(label->predecessor_count_, static_cast<int>(num));
  if (num == 0) return;
  label->predecessor_count_ -= num;
  if (label->variable_merge_state_ != nullptr) {
    label->variable_merge_state_->MergeDead(*dummy_unit_, num);
    if (label->ShouldTrackInterpreterFrameState()) {
      static_cast<DerivedT*>(this)->MergeDeadInterpreterFrameState(label, num);
    }
  }
}

template <typename DerivedT, typename BaseT>
ReduceResult SubgraphBase<DerivedT, BaseT>::TrimPredecessorsAndBind(
    Label* label) {
  int predecessors_so_far =
      label->variable_merge_state_ == nullptr
          ? 0
          : label->variable_merge_state_->predecessors_so_far();
  if (label->ShouldTrackInterpreterFrameState()) {
    DCHECK_EQ(predecessors_so_far,
              label->merge_state_ == nullptr
                  ? 0
                  : label->merge_state_->predecessors_so_far());
  }
  DCHECK_LE(predecessors_so_far, label->predecessor_count_);
  reducer_->set_current_block(nullptr);
  ReducePredecessorCount(label,
                         label->predecessor_count_ - predecessors_so_far);
  if (predecessors_so_far == 0) return ReduceResult::DoneWithAbort();
  static_cast<DerivedT*>(this)->Bind(label);
  return ReduceResult::Done();
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#undef TRACE

#endif  // V8_MAGLEV_MAGLEV_REDUCER_INL_H_
