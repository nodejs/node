// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_REDUCER_H_
#define V8_MAGLEV_MAGLEV_REDUCER_H_

#include <algorithm>
#include <utility>

#include "src/base/logging.h"
#include "src/codegen/source-position.h"
#include "src/compiler/feedback-source.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace maglev {

class ReduceResult;
class V8_NODISCARD MaybeReduceResult {
 public:
  enum Kind {
    kDoneWithValue = 0,  // No need to mask while returning the pointer.
    kDoneWithAbort,
    kDoneWithoutValue,
    kFail,
  };

  MaybeReduceResult() : payload_(kFail) {}

  // NOLINTNEXTLINE
  MaybeReduceResult(ValueNode* value) : payload_(value) {
    DCHECK_NOT_NULL(value);
  }

  static MaybeReduceResult Fail() { return MaybeReduceResult(kFail); }

  MaybeReduceResult(const MaybeReduceResult&) V8_NOEXCEPT = default;
  MaybeReduceResult& operator=(const MaybeReduceResult&) V8_NOEXCEPT = default;

  ValueNode* value() const {
    DCHECK(HasValue());
    return payload_.GetPointerWithKnownPayload(kDoneWithValue);
  }
  bool HasValue() const { return kind() == kDoneWithValue; }

  // Either DoneWithValue, DoneWithoutValue or DoneWithAbort.
  bool IsDone() const { return !IsFail(); }

  // MaybeReduceResult failed.
  bool IsFail() const { return kind() == kFail; }

  // Done with a ValueNode.
  bool IsDoneWithValue() const { return HasValue(); }

  // Done without producing a ValueNode.
  bool IsDoneWithoutValue() const { return kind() == kDoneWithoutValue; }

  // Done with an abort (unconditional deopt, infinite loop in an inlined
  // function, etc)
  bool IsDoneWithAbort() const { return kind() == kDoneWithAbort; }

  Kind kind() const { return payload_.GetPayload(); }

  inline ReduceResult Checked();

  base::PointerWithPayload<ValueNode, Kind, 3> GetPayload() const {
    return payload_;
  }

 protected:
  explicit MaybeReduceResult(Kind kind) : payload_(kind) {}
  explicit MaybeReduceResult(
      base::PointerWithPayload<ValueNode, Kind, 3> payload)
      : payload_(payload) {}
  base::PointerWithPayload<ValueNode, Kind, 3> payload_;
};

class V8_NODISCARD ReduceResult : public MaybeReduceResult {
 public:
  // NOLINTNEXTLINE
  ReduceResult(ValueNode* value) : MaybeReduceResult(value) {}

  explicit ReduceResult(const MaybeReduceResult& other)
      : MaybeReduceResult(other.GetPayload()) {
    CHECK(!IsFail());
  }

  static ReduceResult Done(ValueNode* value) { return ReduceResult(value); }
  static ReduceResult Done() { return ReduceResult(kDoneWithoutValue); }
  static ReduceResult DoneWithAbort() { return ReduceResult(kDoneWithAbort); }

  bool IsFail() const { return false; }
  ReduceResult Checked() { return *this; }

 protected:
  explicit ReduceResult(Kind kind) : MaybeReduceResult(kind) {}
};

inline ReduceResult MaybeReduceResult::Checked() { return ReduceResult(*this); }

#define RETURN_IF_DONE(result) \
  do {                         \
    auto res = (result);       \
    if (res.IsDone()) {        \
      return res.Checked();    \
    }                          \
  } while (false)

#define RETURN_IF_ABORT(result)             \
  do {                                      \
    if ((result).IsDoneWithAbort()) {       \
      return ReduceResult::DoneWithAbort(); \
    }                                       \
  } while (false)

#define PROCESS_AND_RETURN_IF_DONE(result, value_processor) \
  do {                                                      \
    auto res = (result);                                    \
    if (res.IsDone()) {                                     \
      if (res.IsDoneWithValue()) {                          \
        value_processor(res.value());                       \
      }                                                     \
      return res.Checked();                                 \
    }                                                       \
  } while (false)

#define GET_VALUE_OR_ABORT(variable, result)                           \
  do {                                                                 \
    MaybeReduceResult res = (result);                                  \
    if (res.IsDoneWithAbort()) {                                       \
      return ReduceResult::DoneWithAbort();                            \
    }                                                                  \
    DCHECK(res.IsDoneWithValue());                                     \
    using T = std::remove_pointer_t<std::decay_t<decltype(variable)>>; \
    variable = res.value()->Cast<T>();                                 \
  } while (false)

template <typename BaseT>
concept ReducerBaseWithKNA = requires(BaseT* b) { b->known_node_aspects(); };

template <typename BaseT>
concept ReducerBaseWithEagerDeopt =
    requires(BaseT* b) { b->GetDeoptFrameForEagerDeopt(); };

template <typename BaseT>
concept ReducerBaseWithLazyDeopt = requires(BaseT* b) {
  b->GetDeoptFrameForLazyDeopt();
  // TODO(victorgomes): Bring exception handler logic to the reducer?
  b->AttachExceptionHandlerInfo(std::declval<Node*>());
};

template <typename NodeT, typename BaseT>
concept ReducerBaseWithEffectTracking = requires(BaseT* b) {
  b->template MarkPossibleSideEffect<NodeT>(std::declval<NodeT*>());
};

enum class UseReprHintRecording { kRecord, kDoNotRecord };

class BasicBlockPosition {
 public:
  static BasicBlockPosition Start() { return BasicBlockPosition(0); }
  static BasicBlockPosition End() { return BasicBlockPosition(-1); }
  static BasicBlockPosition At(int index) { return BasicBlockPosition(index); }
  inline friend bool operator==(const BasicBlockPosition& lhs,
                                const BasicBlockPosition& rhs);

  bool is_at_end() const { return index_ == kEnd; }

  int index() const { return index_; }

 private:
  int index_;
  static constexpr int kEnd = -1;
  explicit constexpr BasicBlockPosition(int index) : index_(index) {}
};

inline bool operator==(const BasicBlockPosition& lhs,
                       const BasicBlockPosition& rhs) {
  return lhs.index_ == rhs.index_;
}

template <typename BaseT>
class MaglevReducer {
 public:
  MaglevReducer(BaseT* base, Graph* graph,
                MaglevCompilationUnit* compilation_unit = nullptr)
      : base_(base),
        graph_(graph),
        zone_(graph->zone()),
        broker_(graph->broker()),
        current_provenance_{compilation_unit, BytecodeOffset::None(),
                            SourcePosition::Unknown()},
#ifdef DEBUG
        new_nodes_current_period_(zone()),
#endif  // DEBUG
        new_nodes_at_(zone()),
        new_nodes_at_end_(zone()) {
    new_nodes_at_.reserve(8);
    new_nodes_at_end_.reserve(32);
  }

  ~MaglevReducer() {
    DCHECK(new_nodes_at_.empty());
    DCHECK(new_nodes_at_end_.empty());
  }

  // Add a new node with a dynamic set of inputs which are initialized by the
  // `post_create_input_initializer` function before the node is added to the
  // graph.
  template <typename NodeT, typename Function, typename... Args>
  NodeT* AddNewNode(size_t input_count,
                    Function&& post_create_input_initializer, Args&&... args);
  // Add a new node with a static set of inputs.
  template <typename NodeT, typename... Args>
  NodeT* AddNewNode(std::initializer_list<ValueNode*> inputs, Args&&... args);
  template <typename NodeT, typename... Args>
  NodeT* AddUnbufferedNewNodeNoInputConversion(
      BasicBlock* block, std::initializer_list<ValueNode*> inputs,
      Args&&... args);
  // Add a new node with a static set of inputs.
  template <typename NodeT, typename... Args>
  NodeT* AddNewNodeNoInputConversion(std::initializer_list<ValueNode*> inputs,
                                     Args&&... args);
  template <typename ControlNodeT, typename... Args>
  void AddNewControlNode(std::initializer_list<ValueNode*> inputs,
                         Args&&... args);

  void AddInitializedNodeToGraph(Node* node);

  std::optional<int32_t> TryGetInt32Constant(ValueNode* value);
  std::optional<double> TryGetFloat64Constant(
      ValueNode* value, TaggedToFloat64ConversionType conversion_type);

  ValueNode* BuildSmiUntag(ValueNode* node);

  ValueNode* BuildNumberOrOddballToFloat64(ValueNode* node,
                                           NodeType allowed_input_type);

  // Get a tagged representation node whose value is equivalent to the given
  // node.
  ValueNode* GetTaggedValue(ValueNode* value,
                            UseReprHintRecording record_use_repr_hint =
                                UseReprHintRecording::kRecord);

  // Get an Int32 representation node whose value is equivalent to the given
  // node.
  //
  // Deopts if the value is not exactly representable as an Int32.
  ValueNode* GetInt32(ValueNode* value, bool can_be_heap_number = false);

  // Get an Int32 representation node whose value is equivalent to the ToInt32
  // truncation of the given node (including a ToNumber call). Only trivial
  // ToNumber is allowed -- values that are already numeric, and optionally
  // oddballs.
  //
  // Deopts if the ToNumber is non-trivial.
  ValueNode* GetTruncatedInt32ForToNumber(ValueNode* value,
                                          NodeType allowed_input_type);

  // Get a Float64 representation node whose value is equivalent to the given
  // node.
  //
  // Deopts if the value is not exactly representable as a Float64.
  ValueNode* GetFloat64(ValueNode* value);

  ValueNode* GetFloat64ForToNumber(ValueNode* value,
                                   NodeType allowed_input_type);

  ValueNode* GetHoleyFloat64ForToNumber(ValueNode* value,
                                        NodeType allowed_input_type);

  void EnsureInt32(ValueNode* value, bool can_be_heap_number = false);

  BasicBlock* current_block() const { return current_block_; }
  void set_current_block(BasicBlock* block) {
    DCHECK(new_nodes_at_.empty());
    DCHECK(new_nodes_at_end_.empty());
    current_block_ = block;
  }

  const MaglevGraphLabeller::Provenance& current_provenance() const {
    return current_provenance_;
  }

  void SetBytecodeOffset(int offset) {
    current_provenance_.bytecode_offset = BytecodeOffset(offset);
  }

  void SetSourcePosition(int pos, int inlining_id) {
    current_provenance_.position = SourcePosition(pos, inlining_id);
  }

  void SetStartSourcePosition(int inlining_id) {
    SetSourcePosition(
        current_provenance_.unit->shared_function_info().StartPosition(),
        inlining_id);
  }

  void SetCurrentProvenance(MaglevGraphLabeller::Provenance provenance) {
    current_provenance_ = provenance;
  }

  void RegisterNode(NodeBase* node) {
    graph_labeller()->RegisterNode(node, &current_provenance_);
  }

  bool has_graph_labeller() const { return graph()->has_graph_labeller(); }
  MaglevGraphLabeller* graph_labeller() const {
    return graph()->graph_labeller();
  }

  bool is_tracing_enabled() const { return graph()->is_tracing_enabled(); }

  // TODO(victorgomes): Delete these access (or move to private) when the
  // speculation scope is moved inside MaglevReducer.
  compiler::FeedbackSource current_speculation_feedback() const {
    return current_speculation_feedback_;
  }
  void set_current_speculation_feedback(
      compiler::FeedbackSource feedback_source) {
    current_speculation_feedback_ = feedback_source;
  }

  void FlushNodesToBlock();

  void SetNewNodePosition(BasicBlockPosition position) {
    current_block_position_ = position;
  }

  template <UseReprHintRecording hint = UseReprHintRecording::kRecord>
  ValueNode* ConvertInputTo(ValueNode* input, ValueRepresentation expected);

#ifdef DEBUG
  // TODO(victorgomes): Investigate if we can create a better API for this!
  void StartNewPeriod() { new_nodes_current_period_.clear(); }
  bool WasNodeCreatedDuringCurrentPeriod(ValueNode* node) const {
    return new_nodes_current_period_.count(node) > 0;
  }

  Node* GetLastNewNodeInCurrentBlockPosition() {
    if (current_block_position_.is_at_end()) {
      DCHECK(!new_nodes_at_end_.empty());
      return new_nodes_at_end_.back();
    }
    auto pos_cmp = [](const auto& p1, const auto& p2) {
      return p1.first < p2.first;
    };
    std::stable_sort(new_nodes_at_.begin(), new_nodes_at_.end(), pos_cmp);
    auto it_upper = std::upper_bound(
        new_nodes_at_.begin(), new_nodes_at_.end(),
        std::make_pair(current_block_position_.index(), nullptr), pos_cmp);
    DCHECK_NE(it_upper, new_nodes_at_.begin());
    return std::prev(it_upper)->second;
  }
#endif  // DEBUG

  SmiConstant* GetSmiConstant(int constant) const {
    return graph()->GetSmiConstant(constant);
  }
  TaggedIndexConstant* GetTaggedIndexConstant(int constant) {
    return graph()->GetTaggedIndexConstant(constant);
  }
  Int32Constant* GetInt32Constant(int32_t constant) {
    return graph()->GetInt32Constant(constant);
  }
  IntPtrConstant* GetIntPtrConstant(intptr_t constant) {
    return graph()->GetIntPtrConstant(constant);
  }
  Uint32Constant* GetUint32Constant(int constant) {
    return graph()->GetUint32Constant(constant);
  }
  Float64Constant* GetFloat64Constant(double constant) {
    return graph()->GetFloat64Constant(constant);
  }
  Float64Constant* GetFloat64Constant(Float64 constant) {
    return graph()->GetFloat64Constant(constant);
  }
  RootConstant* GetRootConstant(RootIndex index) {
    return graph()->GetRootConstant(index);
  }
  RootConstant* GetBooleanConstant(bool value) {
    return graph()->GetBooleanConstant(value);
  }
  ValueNode* GetConstant(compiler::ObjectRef ref) {
    return graph()->GetConstant(ref);
  }
  ValueNode* GetTrustedConstant(compiler::HeapObjectRef ref,
                                IndirectPointerTag tag) {
    return graph()->GetTrustedConstant(ref, tag);
  }

  ValueNode* GetNumberConstant(double constant);

  template <Operation kOperation>
  MaybeReduceResult TryFoldInt32UnaryOperation(ValueNode* value);
  template <Operation kOperation>
  MaybeReduceResult TryFoldInt32BinaryOperation(ValueNode* left,
                                                ValueNode* right);
  template <Operation kOperation>
  MaybeReduceResult TryFoldInt32BinaryOperation(ValueNode* left,
                                                int32_t cst_right);
  template <Operation kOperation>
  MaybeReduceResult TryFoldInt32BinaryOperation(int32_t left, int32_t right);

  template <Operation kOperation>
  MaybeReduceResult TryFoldFloat64UnaryOperationForToNumber(
      TaggedToFloat64ConversionType conversion_type, ValueNode* value);
  template <Operation kOperation>
  MaybeReduceResult TryFoldFloat64BinaryOperationForToNumber(
      TaggedToFloat64ConversionType conversion_type, ValueNode* left,
      ValueNode* right);
  template <Operation kOperation>
  MaybeReduceResult TryFoldFloat64BinaryOperationForToNumber(
      TaggedToFloat64ConversionType conversion_type, ValueNode* left,
      double cst_right);

 protected:
  class LazyDeoptResultLocationScope;

  enum class AddNewNodeMode {
    kBuffered,
    kUnbuffered,  // Only support insertion at block end.
  };

  template <typename NodeT, typename... Args>
  NodeT* AddNewNodeOrGetEquivalent(bool convert_inputs,
                                   std::initializer_list<ValueNode*> raw_inputs,
                                   Args&&... args);

  template <typename NodeT>
  static constexpr UseReprHintRecording ShouldRecordUseReprHint() {
    // We do not record a Tagged use on Return, since they are never on the hot
    // path, and will lead to a maximum of one additional Tagging operation in
    // the worst case. This allows loop accumulator to be untagged even if they
    // are later returned.
    if constexpr (std::is_same_v<NodeT, Return>) {
      return UseReprHintRecording::kDoNotRecord;
    } else {
      return UseReprHintRecording::kRecord;
    }
  }

  // TODO(marja): When we have C++26, `inputs` can be std::span<ValueNode*>,
  // since std::intializer_list can be converted to std::span.
  template <typename NodeT, typename InputsT>
  void SetNodeInputs(NodeT* node, InputsT inputs);

  // TODO(marja): When we have C++26, `inputs` can be std::span<ValueNode*>,
  // since std::intializer_list can be converted to std::span.
  template <typename NodeT, typename InputsT>
  void SetNodeInputsNoConversion(NodeT* node, InputsT inputs);
  template <typename NodeT>
  NodeT* AttachExtraInfoAndAddToGraph(NodeT* node);
  template <typename NodeT>
  void AttachDeoptCheckpoint(NodeT* node);
  template <typename NodeT>
  void AttachEagerDeoptInfo(NodeT* node);
  template <typename NodeT>
  void AttachLazyDeoptInfo(NodeT* node);
  template <typename NodeT>
  void AttachExceptionHandlerInfo(NodeT* node);
  template <typename NodeT>
  void MarkPossibleSideEffect(NodeT* node);
  template <typename NodeT>
  void UpdateRange(NodeT* node);

  std::optional<ValueNode*> TryGetConstantAlternative(ValueNode* node);

  bool CheckType(ValueNode* node, NodeType type, NodeType* old = nullptr) {
    return known_node_aspects().CheckType(broker(), node, type, old);
  }
  NodeType CheckTypes(ValueNode* node, std::initializer_list<NodeType> types) {
    return known_node_aspects().CheckTypes(broker(), node, types);
  }
  bool EnsureType(ValueNode* node, NodeType type, NodeType* old = nullptr) {
    return known_node_aspects().EnsureType(broker(), node, type, old);
  }
  NodeType GetType(ValueNode* node) {
    return known_node_aspects().GetType(broker(), node);
  }
  NodeInfo* GetOrCreateInfoFor(ValueNode* node) {
    return known_node_aspects().GetOrCreateInfoFor(broker(), node);
  }
  // Returns true if we statically know that {lhs} and {rhs} have disjoint
  // types.
  bool HaveDisjointTypes(ValueNode* lhs, ValueNode* rhs) {
    return known_node_aspects().HaveDisjointTypes(broker(), lhs, rhs);
  }
  bool HasDisjointType(ValueNode* lhs, NodeType rhs_type) {
    return known_node_aspects().HasDisjointType(broker(), lhs, rhs_type);
  }

  KnownNodeAspects& known_node_aspects() {
    static_assert(ReducerBaseWithKNA<BaseT>);
    return base_->known_node_aspects();
  }

  Zone* zone() const { return zone_; }
  Graph* graph() const { return graph_; }
  compiler::JSHeapBroker* broker() const { return broker_; }
  LocalIsolate* local_isolate() const { return broker()->local_isolate(); }

 private:
  BaseT* base_;

  Graph* graph_;
  Zone* zone_;
  compiler::JSHeapBroker* broker_;

  MaglevGraphLabeller::Provenance current_provenance_;
  BasicBlock* current_block_ = nullptr;
  BasicBlockPosition current_block_position_ = BasicBlockPosition::End();
  AddNewNodeMode add_new_node_mode_ = AddNewNodeMode::kBuffered;

#ifdef DEBUG
  // This is used for dcheck purposes, it is the set of all nodes created in
  // the current "period". Where period is defined by Base. In the case of the
  // GraphBuilder is all nodes created while visiting a bytecode. In the case
  // of GraphOptimizer is all nodes created while visiting a maglev node.
  ZoneSet<Node*> new_nodes_current_period_;
#endif  // DEBUG

  // New node buffers.
  ZoneVector<std::pair<int, Node*>> new_nodes_at_;
  ZoneVector<Node*> new_nodes_at_end_;

  compiler::FeedbackSource current_speculation_feedback_ = {};
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_REDUCER_H_
