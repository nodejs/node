// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_REDUCER_H_
#define V8_MAGLEV_MAGLEV_REDUCER_H_

#include <algorithm>
#include <initializer_list>
#include <optional>
#include <utility>

#include "src/base/compiler-specific.h"
#include "src/base/functional/function-ref.h"
#include "src/base/logging.h"
#include "src/base/memcopy.h"
#include "src/codegen/bailout-reason.h"
#include "src/codegen/cpu-features.h"
#include "src/codegen/source-position.h"
#include "src/compiler/feedback-source.h"
#include "src/deoptimizer/deoptimize-reason.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-node-type.h"
#include "src/maglev/maglev-tracer.h"
#include "src/objects/js-objects.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {
namespace maglev {

struct MaglevCallSiteInfo;
class MaglevGraphBuilder;
template <typename BaseT>
class MaglevReducer;
template <typename BaseT>
class Subgraph;
template <typename DerivedT, typename BaseT>
class SubgraphBase;
class ReduceResult;

enum class CpuOperation {
  kFloat64Round,
  kMathClz32,
};

// TODO(leszeks): Add a generic mechanism for marking nodes as optionally
// supported.
inline bool IsSupported(CpuOperation op) {
  switch (op) {
    case CpuOperation::kFloat64Round:
#if defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_IA32)
      return CpuFeatures::IsSupported(SSE4_1) || CpuFeatures::IsSupported(AVX);
#elif defined(V8_TARGET_ARCH_ARM)
      return CpuFeatures::IsSupported(ARMv8);
#elif defined(V8_TARGET_ARCH_ARM64) || defined(V8_TARGET_ARCH_PPC64) ||   \
    defined(V8_TARGET_ARCH_S390X) || defined(V8_TARGET_ARCH_RISCV64) ||   \
    defined(V8_TARGET_ARCH_RISCV32) || defined(V8_TARGET_ARCH_LOONG64) || \
    defined(V8_TARGET_ARCH_MIPS64)
      return true;
#else
#error "V8 does not support this architecture."
#endif

    case CpuOperation::kMathClz32:
#if defined(V8_TARGET_ARCH_ARM64) || defined(V8_TARGET_ARCH_S390X) || \
    defined(V8_TARGET_ARCH_PPC64)
      return true;
#elif defined(V8_TARGET_ARCH_ARM)
      return CpuFeatures::IsSupported(ARMv8);
#elif defined(V8_TARGET_ARCH_X64)
      return CpuFeatures::IsSupported(LZCNT);
#elif defined(V8_TARGET_ARCH_RISCV64) || defined(V8_TARGET_ARCH_RISCV32)
      return CpuFeatures::IsSupported(ZBB);
#elif defined(V8_TARGET_ARCH_IA32) || defined(V8_TARGET_ARCH_PPC64) || \
    defined(V8_TARGET_ARCH_LOONG64) || defined(V8_TARGET_ARCH_MIPS64)
      return false;
#else
#error "V8 does not support this architecture."
#endif
  }
  UNREACHABLE();
}

class V8_NODISCARD MaybeReduceResult {
 public:
  enum Kind {
    kDoneWithPayload = 0,  // No need to mask while returning the pointer.
    kDoneWithAbort,
    kDoneWithoutPayload,
    kFail,
  };

  MaybeReduceResult() : payload_(kFail) {}

  // NOLINTNEXTLINE
  MaybeReduceResult(Node* node) : payload_(node) { DCHECK_NOT_NULL(node); }

  static MaybeReduceResult Fail() { return MaybeReduceResult(kFail); }

  MaybeReduceResult(const MaybeReduceResult&) V8_NOEXCEPT = default;
  MaybeReduceResult& operator=(const MaybeReduceResult&) V8_NOEXCEPT = default;

  ValueNode* value() const {
    DCHECK(HasValue());
    Node* value = payload_.GetPointerWithKnownPayload(kDoneWithPayload);
    return value->Cast<ValueNode>();
  }
  bool HasValue() const {
    if (kind() == kDoneWithPayload) {
      Node* value = payload_.GetPointerWithKnownPayload(kDoneWithPayload);
      return value->Is<ValueNode>();
    }
    return false;
  }

  bool HasNode() const { return kind() == kDoneWithPayload; }

  Node* node() const {
    DCHECK(HasNode());
    return payload_.GetPointerWithKnownPayload(kDoneWithPayload);
  }

  // Either DoneWithValue, DoneWithoutValue or DoneWithAbort.
  bool IsDone() const { return !IsFail(); }

  // MaybeReduceResult failed.
  bool IsFail() const { return kind() == kFail; }

  // Done with a ValueNode.
  bool IsDoneWithValue() const { return HasValue(); }

  // Done with a Node.
  bool IsDoneWithPayload() const { return kind() == kDoneWithPayload; }

  // Done without producing a Node.
  bool IsDoneWithoutPayload() const { return kind() == kDoneWithoutPayload; }

  // Done with an abort (unconditional deopt, infinite loop in an inlined
  // function, etc)
  bool IsDoneWithAbort() const { return kind() == kDoneWithAbort; }

  Kind kind() const { return payload_.GetPayload(); }

  inline ReduceResult Checked();

  base::PointerWithPayload<Node, Kind, 3> GetPayload() const {
    return payload_;
  }

 protected:
  explicit MaybeReduceResult(Kind kind) : payload_(kind) {}
  explicit MaybeReduceResult(base::PointerWithPayload<Node, Kind, 3> payload)
      : payload_(payload) {}
  base::PointerWithPayload<Node, Kind, 3> payload_;
};

class V8_NODISCARD ReduceResult : public MaybeReduceResult {
 public:
  // NOLINTNEXTLINE
  ReduceResult(Node* node) : MaybeReduceResult(node) {}

  explicit ReduceResult(const MaybeReduceResult& other)
      : MaybeReduceResult(other.GetPayload()) {
    CHECK(!IsFail());
  }

  static ReduceResult Done(Node* node) { return ReduceResult(node); }
  static ReduceResult Done() { return ReduceResult(kDoneWithoutPayload); }
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

#define GET_VALUE(variable, result)                                    \
  do {                                                                 \
    MaybeReduceResult res = (result);                                  \
    CHECK(res.IsDoneWithValue());                                      \
    using T = std::remove_pointer_t<std::decay_t<decltype(variable)>>; \
    variable = res.value()->Cast<T>();                                 \
  } while (false)

#define RETURN_VALUE(result)          \
  do {                                \
    MaybeReduceResult res = (result); \
    CHECK(res.IsDoneWithValue());     \
    return res.value();               \
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

// TODO(dmercadier): .Cast the result to the type of variable to avoid requiring
// callers to use a generic `Node*` type for {variable}.
#define GET_NODE_OR_ABORT(variable, result) \
  do {                                      \
    MaybeReduceResult res = (result);       \
    if (res.IsDoneWithAbort()) {            \
      return ReduceResult::DoneWithAbort(); \
    }                                       \
    DCHECK(res.IsDoneWithPayload());        \
    variable = res.node();                  \
  } while (false)

// Can be used for extracting a BasicBlock* from std::optional<BasicBlock*>.
#define GET_BLOCK_OR_ABORT(variable, result) \
  do {                                       \
    auto res = (result);                     \
    if (!res) {                              \
      return ReduceResult::DoneWithAbort();  \
    }                                        \
    variable = *res;                         \
    DCHECK_NOT_NULL(variable);               \
  } while (false)

template <typename BaseT>
concept ReducerBaseWithKNA = requires(BaseT* b) { b->known_node_aspects(); };

template <typename BaseT>
concept ReducerBaseWithKNASetter = requires(BaseT* b, KnownNodeAspects* kna) {
  b->set_known_node_aspects(kna);
};

template <typename BaseT>
concept ReducerBaseWithEagerDeopt =
    requires(BaseT* b) { b->GetDeoptFrameForEagerDeopt(); };

template <typename BaseT>
concept ReducerBaseWithAbruptBlockEnd =
    requires(BaseT* b, BasicBlock* block) { b->OnAbruptBlockEnd(block); };

template <typename BaseT>
concept ReducerBaseWithLazyDeopt = requires(BaseT* b) {
  b->GetDeoptFrameForLazyDeopt(false);
  // TODO(victorgomes): Bring exception handler logic to the reducer?
  b->AttachExceptionHandlerInfo(std::declval<Node*>());
};

template <typename BaseT>
concept ReducerBaseWithLazyDeoptScope =
    requires { typename BaseT::LazyDeoptFrameScope; };

template <typename NodeT, typename BaseT>
concept ReducerBaseWithEffectTracking = requires(BaseT* b) {
  b->template MarkPossibleSideEffect<NodeT>(std::declval<NodeT*>());
};

template <typename BaseT>
concept ReducerBaseWithLoopEffectTracking =
    requires(BaseT* b) { b->loop_effects(); };

template <typename BaseT>
concept ReducerBaseCanBuildCall = requires(BaseT* b) {
  b->TryReduceCallForConstant(std::declval<compiler::JSFunctionRef>(),
                              std::declval<typename BaseT::CallArguments&>());
  b->BuildGenericCall(std::declval<ValueNode*>(),
                      std::declval<Call::TargetType>(),
                      std::declval<typename BaseT::CallArguments&>());
};

template <typename BaseT>
concept ReducerBaseHasTracing = requires(BaseT* b) { b->is_tracing(); };

template <typename BaseT>
concept ReducerBaseWithAllocationTracking = requires(BaseT* b) {
  b->TryBuildStoreTaggedFieldToAllocation(std::declval<ValueNode*>(),
                                          std::declval<ValueNode*>(),
                                          std::declval<int>());
  b->TryBuildLoadTaggedFieldFromAllocation(std::declval<ValueNode*>(),
                                           std::declval<int>());
  b->TryBuildLoadFixedDoubleArrayElementFromAllocation(
      std::declval<ValueNode*>(), std::declval<int>());
  b->TryElideWriteBarrierForAllocation(std::declval<ValueNode*>(),
                                       std::declval<ValueNode*>());
};

enum class UseReprHintRecording { kRecord, kDoNotRecord };

enum class BranchResult {
  kDefault,
  kAlwaysTrue,
  kAlwaysFalse,
  // Bailed out before evaluating the condition.
  kAbort,
};

inline bool CompareInt32(int32_t lhs, int32_t rhs, Operation operation) {
  switch (operation) {
    case Operation::kEqual:
    case Operation::kStrictEqual:
      return lhs == rhs;
    case Operation::kLessThan:
      return lhs < rhs;
    case Operation::kLessThanOrEqual:
      return lhs <= rhs;
    case Operation::kGreaterThan:
      return lhs > rhs;
    case Operation::kGreaterThanOrEqual:
      return lhs >= rhs;
    default:
      UNREACHABLE();
  }
}

inline bool CompareUint32(uint32_t lhs, uint32_t rhs, Operation operation) {
  switch (operation) {
    case Operation::kEqual:
    case Operation::kStrictEqual:
      return lhs == rhs;
    case Operation::kLessThan:
      return lhs < rhs;
    case Operation::kLessThanOrEqual:
      return lhs <= rhs;
    case Operation::kGreaterThan:
      return lhs > rhs;
    case Operation::kGreaterThanOrEqual:
      return lhs >= rhs;
    default:
      UNREACHABLE();
  }
}

// CallArguments encapsulates the arguments of a JS-level call so that
// reducer code can read them uniformly regardless of where they came from.
//
// Two storage modes:
//   * kOwned: a SmallVector of ValueNode* owned by this object. Used at
//     bytecode-build time when arguments are gathered from a register list,
//     synthesised, or copied. Mutating helpers (set_receiver, PopReceiver,
//     PopSpread, PopArrayLikeArgument, ResizeDefaultArguments) operate on
//     this storage.
//   * kNodeView: a non-owning view over the input array of an existing
//     call node. No allocation. Used at graph-optimizer time when reducing
//     an existing Call / CallKnownBuiltin. Read-only; mutating helpers
//     DCHECK-fail.
class CallArguments {
 public:
  enum Mode {
    kDefault,
    kWithSpread,
    kWithArrayLike,
  };

  // === Owned constructors. ===
  CallArguments(ConvertReceiverMode receiver_mode,
                interpreter::RegisterList reglist,
                const InterpreterFrameState& frame, Mode mode = kDefault)
      : receiver_mode_(receiver_mode),
        mode_(mode),
        storage_(Storage::kOwned),
        args_(reglist.register_count()) {
    for (int i = 0; i < reglist.register_count(); i++) {
      args_[i] = frame.get(reglist[i]);
    }
    DCHECK_IMPLIES(args_.size() == 0,
                   receiver_mode == ConvertReceiverMode::kNullOrUndefined);
    DCHECK_IMPLIES(mode == kWithArrayLike,
                   receiver_mode == ConvertReceiverMode::kAny);
    DCHECK_IMPLIES(mode == kWithArrayLike, args_.size() == 2);
  }

  explicit CallArguments(ConvertReceiverMode receiver_mode)
      : receiver_mode_(receiver_mode),
        mode_(kDefault),
        storage_(Storage::kOwned),
        args_() {
    DCHECK_EQ(receiver_mode, ConvertReceiverMode::kNullOrUndefined);
  }

  CallArguments(ConvertReceiverMode receiver_mode,
                std::initializer_list<ValueNode*> args, Mode mode = kDefault)
      : receiver_mode_(receiver_mode),
        mode_(mode),
        storage_(Storage::kOwned),
        args_(args) {
    DCHECK_IMPLIES(mode != kDefault,
                   receiver_mode == ConvertReceiverMode::kAny);
    DCHECK_IMPLIES(mode == kWithArrayLike, args_.size() == 2);
    CheckArgumentsAreNotConversionNodes();
  }

  CallArguments(ConvertReceiverMode receiver_mode,
                base::SmallVector<ValueNode*, 8>&& args, Mode mode = kDefault)
      : receiver_mode_(receiver_mode),
        mode_(mode),
        storage_(Storage::kOwned),
        args_(std::move(args)) {
    DCHECK_IMPLIES(mode != kDefault,
                   receiver_mode == ConvertReceiverMode::kAny);
    DCHECK_IMPLIES(mode == kWithArrayLike, args_.size() == 2);
    CheckArgumentsAreNotConversionNodes();
  }

  explicit CallArguments(Call* call)
      : receiver_mode_(call->receiver_mode()),
        mode_(kDefault),
        storage_(Storage::kNodeView),
        node_view_call_(call),
        node_view_args_start_(Call::kFixedInputCount + 1),
        node_view_args_count_(call->num_args() - 1),
        node_view_receiver_(receiver_mode_ ==
                                    ConvertReceiverMode::kNullOrUndefined
                                ? nullptr
                                : call->arg(0).node()) {
    DCHECK_GE(call->num_args(), 1);  // Always at least the receiver slot.
  }

  explicit CallArguments(CallKnownBuiltin* call)
      : receiver_mode_(ConvertReceiverMode::kNotNullOrUndefined),
        mode_(kDefault),
        storage_(Storage::kNodeView),
        node_view_call_(call),
        node_view_args_start_(CallKnownBuiltin::kFixedInputCount),
        node_view_args_count_(call->num_args()),
        node_view_receiver_(call->ReceiverInput().node()) {}

  ValueNode* receiver() const {
    if (storage_ == Storage::kNodeView) return node_view_receiver_;
    if (receiver_mode_ == ConvertReceiverMode::kNullOrUndefined) {
      return nullptr;
    }
    return args_[0];
  }

  size_t count() const {
    if (storage_ == Storage::kNodeView) return node_view_args_count_;
    DCHECK_LE(index_offset(), args_.size());
    return args_.size() - index_offset();
  }

  size_t count_with_receiver() const { return count() + 1; }

  ValueNode* operator[](size_t i) const {
    if (storage_ == Storage::kNodeView) {
      if (i >= static_cast<size_t>(node_view_args_count_)) return nullptr;
      return node_view_call_->input(node_view_args_start_ + static_cast<int>(i))
          .node();
    }
    i += index_offset();
    if (i >= args_.size()) return nullptr;
    return args_[i];
  }

  Mode mode() const { return mode_; }
  ConvertReceiverMode receiver_mode() const { return receiver_mode_; }

  ValueNode* array_like_argument() {
    DCHECK_EQ(mode_, kWithArrayLike);
    DCHECK_GT(count(), 0);
    return (*this)[count() - 1];
  }

  ValueNode* spread() {
    DCHECK_EQ(mode_, kWithSpread);
    DCHECK_GT(count(), 0);
    return (*this)[count() - 1];
  }

  ValueNode** begin() {
    DCHECK_EQ(storage_, Storage::kOwned);
    return args_.begin() + index_offset();
  }
  const ValueNode* const* begin() const {
    DCHECK_EQ(storage_, Storage::kOwned);
    return args_.begin() + index_offset();
  }
  ValueNode** end() {
    DCHECK_EQ(storage_, Storage::kOwned);
    return args_.end();
  }
  const ValueNode* const* end() const {
    DCHECK_EQ(storage_, Storage::kOwned);
    return args_.end();
  }

  void set_receiver(ValueNode* receiver) {
    DCHECK_EQ(storage_, Storage::kOwned);
    if (receiver_mode_ == ConvertReceiverMode::kNullOrUndefined) {
      args_.insert(args_.data(), receiver);
      receiver_mode_ = ConvertReceiverMode::kAny;
    } else {
      DCHECK(!receiver->is_conversion());
      args_[0] = receiver;
    }
  }

  void PopArrayLikeArgument() {
    DCHECK_EQ(storage_, Storage::kOwned);
    DCHECK_EQ(mode_, kWithArrayLike);
    DCHECK_GT(count(), 0);
    args_.pop_back();
  }

  void PopSpread() {
    DCHECK_EQ(storage_, Storage::kOwned);
    DCHECK_EQ(mode_, kWithSpread);
    DCHECK_GT(count(), 0);
    args_.pop_back();
  }

  void ResizeDefaultArguments(size_t new_count) {
    DCHECK_EQ(storage_, Storage::kOwned);
    DCHECK_EQ(mode_, kDefault);
    DCHECK_GT(count(), new_count);
    args_.resize(new_count + index_offset());
    DCHECK_EQ(count(), new_count);
  }

  void PopReceiver(ConvertReceiverMode new_receiver_mode) {
    DCHECK_EQ(storage_, Storage::kOwned);
    DCHECK_NE(receiver_mode_, ConvertReceiverMode::kNullOrUndefined);
    DCHECK_NE(new_receiver_mode, ConvertReceiverMode::kNullOrUndefined);
    DCHECK_GT(args_.size(), 0);  // We have at least a receiver to pop!
    size_t new_args_size_in_bytes = (args_.size() - 1) * sizeof(args_[0]);
    MemMove(args_.data(), args_.data() + 1, new_args_size_in_bytes);
    args_.pop_back();

    // If there is no non-receiver argument to become the new receiver,
    // consider the new receiver to be known undefined.
    receiver_mode_ = args_.empty() ? ConvertReceiverMode::kNullOrUndefined
                                   : new_receiver_mode;
  }

 private:
  enum class Storage { kOwned, kNodeView };

  ConvertReceiverMode receiver_mode_;
  Mode mode_;
  Storage storage_;
  // Owned-mode storage. Empty in kNodeView mode.
  base::SmallVector<ValueNode*, 8> args_;
  // Node-view-mode storage. Unused in kOwned mode.
  NodeBase* node_view_call_ = nullptr;
  int node_view_args_start_ = 0;
  int node_view_args_count_ = 0;
  ValueNode* node_view_receiver_ = nullptr;

  int index_offset() const {
    return receiver_mode_ == ConvertReceiverMode::kNullOrUndefined ? 0 : 1;
  }

  void CheckArgumentsAreNotConversionNodes() {
#ifdef DEBUG
    // Arguments can leak to the interpreter frame if the call is inlined,
    // conversions should be stored in known_node_aspects/NodeInfo.
    for (ValueNode* arg : args_) {
      DCHECK(!arg->is_conversion());
    }
#endif  // DEBUG
  }
};

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

  static enum CheckType GetCheckType(NodeType type) {
    if (NodeTypeIs(type, NodeType::kAnyHeapObject)) {
      return CheckType::kOmitHeapObjectCheck;
    } else {
      return CheckType::kCheckHeapObject;
    }
  }

  ReduceResult BuildCheckMaps(
      ValueNode* object, base::Vector<const compiler::MapRef> maps,
      std::optional<ValueNode*> map = std::nullopt,
      bool has_deprecated_map_without_migration_target = false,
      bool migration_done_outside = false);

  ReduceResult BuildCheckValueByReference(ValueNode* context, ValueNode* node,
                                          compiler::HeapObjectRef ref,
                                          DeoptimizeReason reason);

  MaybeReduceResult TryBuildLoadFixedArrayElementConstantIndex(
      ValueNode* elements, int32_t index, LoadType type);

  // Add a new node with a dynamic set of inputs which are initialized by the
  // `post_create_input_initializer` function before the node is added to the
  // graph.
  template <typename NodeT, typename Function, typename... Args>
  ReduceResult AddNewNode(size_t input_count,
                          Function&& post_create_input_initializer,
                          Args&&... args);
  // Add a new node with a static set of inputs.
  template <typename NodeT, typename... Args>
  ReduceResult AddNewNode(std::initializer_list<ValueNode*> inputs,
                          Args&&... args);
  template <typename NodeT, typename... Args>
  NodeT* AddUnbufferedNewNodeNoInputConversion(
      BasicBlock* block, std::initializer_list<ValueNode*> inputs,
      Args&&... args);
  // Add a new node with a static set of inputs.
  template <typename NodeT, typename... Args>
  NodeT* AddNewNodeNoInputConversion(std::initializer_list<ValueNode*> inputs,
                                     Args&&... args);
  template <typename ControlNodeT, typename... Args>
  ReduceResult AddNewControlNode(std::initializer_list<ValueNode*> inputs,
                                 Args&&... args);

  void AddInitializedNodeToGraph(Node* node);

  // TODO(marja): When we have C++26, `inputs` can be std::span<ValueNode*>,
  // since std::intializer_list can be converted to std::span.
  template <typename NodeT, typename InputsT>
  ReduceResult SetNodeInputs(NodeT* node, InputsT inputs);

  ReduceResult EmitUnconditionalDeopt(DeoptimizeReason reason);
  ReduceResult BuildAbort(AbortReason reason);
  ReduceResult EmitThrow(Throw::Function function, ValueNode* input = nullptr);

  template <class T>
  compiler::OptionalRef<typename compiler::ref_traits<T>::ref_type>
  TryGetConstant(ValueNode* node, ValueNode** constant_node = nullptr) {
    compiler::OptionalHeapObjectRef ref =
        TryGetHeapObjectConstant(node, constant_node);
    if constexpr (std::is_same_v<T, HeapObject>) {
      return ref;
    }
    if (!ref.has_value() || !ref->Is<T>()) return {};
    return ref->As<T>();
  }

  std::optional<int32_t> TryGetInt32Constant(ValueNode* value);
  std::optional<uint32_t> TryGetUint32Constant(ValueNode* value);
  std::optional<intptr_t> TryGetIntPtrConstant(ValueNode* value);
  std::optional<Float64> TryGetFloat64OrHoleyFloat64Constant(
      UseRepresentation use_repr, ValueNode* value,
      TaggedToFloat64ConversionType conversion_type);

  template <typename MapContainer>
  MaybeReduceResult TryFoldCheckConstantMaps(ValueNode* object,
                                             const MapContainer& maps);
  template <typename MapContainer>
  MaybeReduceResult TryFoldCheckConstantMaps(compiler::MapRef map,
                                             const MapContainer& maps);
  template <typename MapContainer>
  MaybeReduceResult TryFoldCheckMaps(ValueNode* object, ValueNode* object_map,
                                     const MapContainer& maps,
                                     KnownMapsMerger<MapContainer>& merger);
  MaybeReduceResult TryFoldTestUndetectable(ValueNode* value);
  template <bool flip>
  MaybeReduceResult TryFoldToBoolean(ValueNode* value);

  enum InferHasInPrototypeChainResult {
    kMayBeInPrototypeChain,
    kIsInPrototypeChain,
    kIsNotInPrototypeChain
  };
  InferHasInPrototypeChainResult InferHasInPrototypeChain(
      ValueNode* receiver, compiler::HeapObjectRef prototype);
  MaybeReduceResult TryBuildFastHasInPrototypeChain(
      ValueNode* object, compiler::HeapObjectRef prototype);
  MaybeReduceResult TryBuildFastOrdinaryHasInstance(
      ValueNode* context, ValueNode* object, compiler::JSObjectRef callable,
      ValueNode* callable_node_if_not_constant);
  MaybeReduceResult TryBuildFastInstanceOf(ValueNode* context,
                                           ValueNode* object,
                                           compiler::JSObjectRef callable_ref,
                                           ValueNode* callable_node);
  MaybeReduceResult TryBuildFastInstanceOfWithFeedback(
      ValueNode* context, ValueNode* object, ValueNode* callable,
      compiler::FeedbackSource feedback_source);

  ReduceResult BuildSmiUntag(ValueNode* node);
  ReduceResult BuildCheckSmi(ValueNode* object);

  ReduceResult BuildNumberOrOddballToFloat64OrHoleyFloat64(
      ValueNode* node, UseRepresentation use_rep, NodeType allowed_input_type);

  ReduceResult BuildOrdinaryHasInstance(
      ValueNode* context, ValueNode* object, compiler::JSObjectRef callable,
      ValueNode* callable_node_if_not_constant);

  template <bool flip = false>
  ReduceResult BuildToBoolean(ValueNode* value);

  template <Builtin kBuiltin>
  void SetCallBuiltinFeedback(CallBuiltin* call_builtin,
                              compiler::FeedbackSource const& feedback,
                              CallBuiltin::FeedbackSlotType slot_type);

  template <Builtin kBuiltin>
  CallBuiltin* BuildCallBuiltin(std::initializer_list<ValueNode*> inputs);
  template <Builtin kBuiltin>
  CallBuiltin* BuildCallBuiltin(ValueNode* context,
                                std::initializer_list<ValueNode*> inputs);
  template <Builtin kBuiltin>
  CallBuiltin* BuildCallBuiltin(std::initializer_list<ValueNode*> inputs,
                                compiler::FeedbackSource const& feedback,
                                CallBuiltin::FeedbackSlotType slot_type);
  template <Builtin kBuiltin>
  CallBuiltin* BuildCallBuiltin(ValueNode* context,
                                std::initializer_list<ValueNode*> inputs,
                                compiler::FeedbackSource const& feedback,
                                CallBuiltin::FeedbackSlotType slot_type);

  template <Builtin kBuiltin>
  ReduceResult BuildCallBuiltinWithTaggedInputs(
      std::initializer_list<ValueNode*> inputs);
  template <Builtin kBuiltin>
  ReduceResult BuildCallBuiltinWithTaggedInputs(
      ValueNode* context, std::initializer_list<ValueNode*> inputs);
  template <Builtin kBuiltin>
  ReduceResult BuildCallBuiltinWithTaggedInputs(
      std::initializer_list<ValueNode*> inputs,
      compiler::FeedbackSource const& feedback,
      CallBuiltin::FeedbackSlotType slot_type);
  template <Builtin kBuiltin>
  ReduceResult BuildCallBuiltinWithTaggedInputs(
      ValueNode* context, std::initializer_list<ValueNode*> inputs,
      compiler::FeedbackSource const& feedback,
      CallBuiltin::FeedbackSlotType slot_type);

  ReduceResult BuildCallKnownJSFunction(
      JSDispatchHandle dispatch_handle, compiler::SharedFunctionInfoRef shared,
      ValueNode* tagged_function, ValueNode* tagged_context,
      ValueNode* tagged_receiver, ValueNode* tagged_new_target, int arg_count,
      base::FunctionRef<ReduceResult(int)> get_arg,
      compiler::FeedbackSource const& feedback_source);

#if V8_ENABLE_WEBASSEMBLY
  bool ShouldWrapArgsForWasmInlining(compiler::SharedFunctionInfoRef shared,
                                     JSDispatchHandle dispatch_handle);
#endif  // V8_ENABLE_WEBASSEMBLY

  compiler::OptionalStringRef GetStringFromInt32(int32_t value);

  MaybeReduceResult TryFoldNumberToString(ValueNode* value);

  // Get a tagged representation node whose value is equivalent to the given
  // node.
  ReduceResult GetTaggedValue(ValueNode* value,
                              UseReprHintRecording record_use_repr_hint =
                                  UseReprHintRecording::kRecord);

  ReduceResult GetSmiValue(ValueNode* value,
                           UseReprHintRecording record_use_repr_hint =
                               UseReprHintRecording::kRecord);

  uint32_t NewObjectId() { return graph()->NewObjectId(); }

  VirtualObject* CreateHeapNumber(ValueNode* value);
  VirtualObject* CreateJSObject(compiler::MapRef map);
  VirtualObject* CreateConsString(ValueNode* map, ValueNode* length,
                                  ValueNode* first, ValueNode* second);
  ReduceResult CreateJSArray(compiler::MapRef map, int instance_size,
                             ValueNode* length);
  VirtualObject* CreateJSArrayIterator(compiler::MapRef map,
                                       ValueNode* iterated_object,
                                       IterationKind kind);
  VirtualObject* CreateJSConstructor(compiler::JSFunctionRef constructor);
  VirtualObject* CreateFixedArray(base::Vector<ValueNode* const> values);
  VirtualObject* CreateFixedDoubleArray(base::Vector<ValueNode* const> values);
  VirtualObject* CreateContext(compiler::MapRef map, int length,
                               compiler::ScopeInfoRef scope_info,
                               ValueNode* previous_context,
                               std::optional<ValueNode*> extension = {});
  VirtualObject* CreateArgumentsObject(compiler::MapRef map, ValueNode* length,
                                       ValueNode* elements,
                                       std::optional<ValueNode*> callee = {});
  VirtualObject* CreateMappedArgumentsElements(compiler::MapRef map,
                                               int mapped_count,
                                               ValueNode* context,
                                               ValueNode* unmapped_elements);
  VirtualObject* CreateRegExpLiteralObject(
      compiler::MapRef map, compiler::RegExpBoilerplateDescriptionRef literal);
  VirtualObject* CreateJSGeneratorObject(compiler::MapRef map,
                                         int instance_size, ValueNode* context,
                                         ValueNode* closure,
                                         ValueNode* receiver,
                                         ValueNode* register_file);
  VirtualObject* CreateJSAsyncFunctionObject(ValueNode* context,
                                             ValueNode* closure,
                                             ValueNode* receiver,
                                             ValueNode* register_file,
                                             ValueNode* promise);
  VirtualObject* CreateJSIteratorResult(compiler::MapRef map, ValueNode* value,
                                        ValueNode* done);
  VirtualObject* CreateJSStringIterator(compiler::MapRef map,
                                        ValueNode* string);
  VirtualObject* CreateJSStringWrapper(ValueNode* value);
  VirtualObject* CreateJSPromiseObject();
  VirtualObject* CreateAsyncResumeTask(ValueNode* generator, ValueNode* value,
                                       ValueNode* kind);

  ReduceResult BuildLoadTaggedField(ValueNode* object, uint32_t offset,
                                    NodeType type = NodeType::kUnknown,
                                    bool is_const = false,
                                    PropertyKey key = PropertyKey::None());

  ReduceResult BuildLoadFixedDoubleArrayElement(ValueNode* elements,
                                                ValueNode* index);

  ReduceResult BuildStoreTaggedField(
      ValueNode* object, ValueNode* value, int offset,
      StoreTaggedMode store_mode,
      PropertyKey property_key = PropertyKey::None(),
      MaybeAssignedFlag maybe_assigned = kMaybeAssigned);

  ReduceResult BuildStoreTaggedFieldNoWriteBarrier(
      ValueNode* object, ValueNode* value, int offset,
      StoreTaggedMode store_mode,
      PropertyKey property_key = PropertyKey::None());

  ReduceResult BuildStoreTrustedPointerField(ValueNode* object,
                                             ValueNode* value, int offset,
                                             IndirectPointerTag tag,
                                             StoreTaggedMode store_mode);

  ReduceResult BuildStoreMap(ValueNode* object, compiler::MapRef map,
                             StoreMap::Kind kind);

  ReduceResult BuildInlinedAllocation(VirtualObject* object,
                                      AllocationType allocation);
  ReduceResult BuildAndAllocateJSArrayIterator(ValueNode* array,
                                               IterationKind iteration_kind);
  void ClearCurrentAllocationBlock();
  void AddNonEscapingUses(InlinedAllocation* allocation, int use_count);
  AllocationBlock* current_allocation_block() const {
    return current_allocation_block_;
  }

  MaybeAssignedFlag GetContextMaybeAssigned(compiler::ScopeInfoRef scope_info,
                                            int index, VariableMode* mode);

  bool CanElideWriteBarrier(ValueNode* object, ValueNode* value);

  // Get an Int32 representation node whose value is equivalent to the given
  // node.
  //
  // Deopts if the value is not exactly representable as an Int32.
  ReduceResult GetInt32(ValueNode* value, bool can_be_heap_number = false);

  // This does not emit any conversion.
  ValueNode* TryGetInt32(ValueNode* value);

  // Get an Int32 representation node whose value is equivalent to the ToInt32
  // truncation of the given node (including a ToNumber call). Only trivial
  // ToNumber is allowed -- values that are already numeric, and optionally
  // oddballs.
  //
  // Deopts if the ToNumber is non-trivial.
  ReduceResult GetTruncatedInt32ForToNumber(ValueNode* value,
                                            NodeType allowed_input_type);

  ReduceResult GetFloat64OrHoleyFloat64Impl(ValueNode* value,
                                            UseRepresentation use_rep,
                                            NodeType allowed_input_type);

  // Get a Float64 representation node whose value is equivalent to the given
  // node.
  //
  // Deopts if the value is not exactly representable as a Float64.
  ReduceResult GetFloat64(ValueNode* value);

  // This does not emit any conversion.
  ValueNode* TryGetFloat64(ValueNode* value);

  ReduceResult GetFloat64ForToNumber(ValueNode* value,
                                     NodeType allowed_input_type);

  // This does not emit any conversion.
  ValueNode* TryGetFloat64ForToNumber(ValueNode* value,
                                      NodeType allowed_input_type);

  ReduceResult GetHoleyFloat64(ValueNode* value);

  ReduceResult GetHoleyFloat64ForToNumber(ValueNode* value,
                                          NodeType allowed_input_type);

  ReduceResult EnsureInt32(ValueNode* value, bool can_be_heap_number = false);

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

  maglev::Tracer tracer() const {
    return maglev::Tracer(graph()->compilation_info());
  }

  // This indicates that the reducer base has tracing enabled.
  bool is_tracing() const {
    if constexpr (ReducerBaseHasTracing<BaseT>) {
      return base_->is_tracing();
    }
    return false;
  }

  // This indicates it passes the function filter and this compilation _can_ be
  // traced if some tracing flag is enabled.
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

  SpeculationMode current_speculation_mode() const {
    return current_speculation_mode_;
  }
  void set_current_speculation_mode(SpeculationMode mode) {
    current_speculation_mode_ = mode;
  }
  bool CanSpeculateCall() const {
    return current_speculation_mode_ == SpeculationMode::kAllowSpeculation;
  }
  bool CanSpeculateCall(
      std::initializer_list<SpeculationMode> supported_modes) const {
    return CanSpeculateCall() ||
           std::find(supported_modes.begin(), supported_modes.end(),
                     current_speculation_mode_) != supported_modes.end();
  }

#ifdef V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA
#define CONTINUATION_PRESERVED_EMBEDDER_DATA_LIST(V) \
  V(GetContinuationPreservedEmbedderData)            \
  V(SetContinuationPreservedEmbedderData)
#else
#define CONTINUATION_PRESERVED_EMBEDDER_DATA_LIST(V)
#endif  // V8_ENABLE_CONTINUATION_PRESERVED_EMBEDDER_DATA

#define MAGLEV_REDUCER_BUILTIN(V)              \
  V(ArrayIsArray)                              \
  V(ArrayPrototypeAt)                          \
  V(ArrayPrototypeEntries)                     \
  V(ArrayPrototypeKeys)                        \
  V(ArrayPrototypeValues)                      \
  V(DataViewPrototypeGetFloat64)               \
  V(DataViewPrototypeGetInt16)                 \
  V(DataViewPrototypeGetInt32)                 \
  V(DataViewPrototypeGetInt8)                  \
  V(DataViewPrototypeSetFloat64)               \
  V(DataViewPrototypeSetInt16)                 \
  V(DataViewPrototypeSetInt32)                 \
  V(DataViewPrototypeSetInt8)                  \
  V(DatePrototypeGetDate)                      \
  V(DatePrototypeGetDay)                       \
  V(DatePrototypeGetFullYear)                  \
  V(DatePrototypeGetHours)                     \
  V(DatePrototypeGetMinutes)                   \
  V(DatePrototypeGetMonth)                     \
  V(DatePrototypeGetSeconds)                   \
  V(DatePrototypeGetTime)                      \
  V(MathAbs)                                   \
  V(MathCeil)                                  \
  V(MathClz32)                                 \
  V(MathFloor)                                 \
  V(MathFround)                                \
  V(MathImul)                                  \
  V(MathMax)                                   \
  V(MathMin)                                   \
  V(MathRound)                                 \
  V(MathSqrt)                                  \
  V(MathTrunc)                                 \
  IEEE_754_UNARY_LIST(V)                       \
  IEEE_754_BINARY_LIST(V)                      \
  IF_INTL(V, StringPrototypeLocaleCompareIntl) \
  CONTINUATION_PRESERVED_EMBEDDER_DATA_LIST(V)

#define DECLARE_BUILTIN_REDUCER(Name, ...)                          \
  MaybeReduceResult TryReduce##Name(compiler::JSFunctionRef target, \
                                    CallArguments& args);
  MAGLEV_REDUCER_BUILTIN(DECLARE_BUILTIN_REDUCER)
#undef DECLARE_BUILTIN_REDUCER

  // Returns kDoneWithoutPayload if checks passed successfully.
  MaybeReduceResult TryReduceDatePrototypeGetFieldPrologue(
      compiler::JSFunctionRef target, CallArguments& args);
  MaybeReduceResult TryReduceDatePrototypeGetField(
      compiler::JSFunctionRef target, CallArguments& args,
      JSDate::FieldIndex field);

  MaybeReduceResult DoTryReduceMathRound(CallArguments& args,
                                         Float64Round::Kind kind);
  template <typename Int32Binop, typename Float64Binop>
  MaybeReduceResult TryReduceMathMinMax(CallArguments& args,
                                        Int32Binop&& int32_case,
                                        Float64Binop&& float64_case);
  MaybeReduceResult TryReduceBuiltin(
      Builtin builtin_id, compiler::JSFunctionRef target, CallArguments& args,
      const compiler::FeedbackSource& feedback_source);

  template <typename LoadNode>
  MaybeReduceResult TryBuildLoadDataView(const CallArguments& args,
                                         ExternalArrayType type);
  template <typename StoreNode, typename Function>
  MaybeReduceResult TryBuildStoreDataView(const CallArguments& args,
                                          ExternalArrayType type,
                                          Function&& getValue);
  ReduceResult BuildLoadJSDataViewByteLength(ValueNode* js_data_view);
  ReduceResult BuildLoadJSDataViewDataPointer(ValueNode* js_data_view);
  ReduceResult BuildLoadElements(
      ValueNode* object, std::optional<ElementsKind> kind = std::nullopt);

  ReduceResult BuildAssumeMapForElements(ValueNode* elements,
                                         ElementsKind kind);

  MaybeReduceResult TryReduceStringLength(ValueNode* string);

  ReduceResult BuildCheckInstanceType(ValueNode* object, NodeType target_type,
                                      InstanceType first, InstanceType last);
  ReduceResult GetInt32ElementIndex(ValueNode* index_object);
  void RecordKnownProperty(ValueNode* lookup_start_object, PropertyKey key,
                           ValueNode* value, bool is_const,
                           compiler::AccessMode access_mode);
  ValueNode* GetValueOrUndefined(ValueNode* maybe_value) {
    if (maybe_value == nullptr) {
      return GetRootConstant(RootIndex::kUndefinedValue);
    }
    return maybe_value;
  }

  ReduceResult BuildInt32Max(ValueNode* a, ValueNode* b);
  ReduceResult BuildInt32Min(ValueNode* a, ValueNode* b);

  // TODO(victorgomes): Maybe it makes sense to port BranchBuilder.
  template <typename Sub>
  BranchResult BuildBranchIfInt32Compare(Sub& sg,
                                         typename Sub::Label* false_target,
                                         Operation op, ValueNode* lhs,
                                         ValueNode* rhs);
  template <typename Sub>
  BranchResult BuildBranchIfUint32Compare(Sub& sg,
                                          typename Sub::Label* false_target,
                                          Operation op, ValueNode* lhs,
                                          ValueNode* rhs);

  // TODO(victorgomes): Support DoneWithoutValue.
  template <typename CondFn>
  ReduceResult Select(CondFn cond, base::FunctionRef<ReduceResult()> if_true,
                      base::FunctionRef<ReduceResult()> if_false);

  struct PendingSplice {
    BasicBlock* entry;
    BasicBlock* exit;
    ZoneVector<BasicBlock*> all_blocks;
  };
  void RecordPendingSplice(BasicBlock* entry, BasicBlock* exit,
                           ZoneVector<BasicBlock*> all_blocks) {
    DCHECK(!pending_splice_.has_value());
    pending_splice_ = PendingSplice{entry, exit, all_blocks};
  }
  bool HasPendingSplice() const { return pending_splice_.has_value(); }
  const PendingSplice& pending_splice() const {
    DCHECK(pending_splice_.has_value());
    return *pending_splice_;
  }
  PendingSplice TakePendingSplice() {
    DCHECK(pending_splice_.has_value());
    return *std::exchange(pending_splice_, {});
  }
  void FlushNodesToBlock();

  void SetNewNodePosition(BasicBlockPosition position) {
    current_block_position_ = position;
  }

  BasicBlockPosition current_block_position() const {
    return current_block_position_;
  }

  template <UseReprHintRecording hint = UseReprHintRecording::kRecord>
  ReduceResult ConvertInputTo(ValueNode* input, ValueRepresentation expected);

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

  // Tracks whether any node that can throw was emitted since the last
  // ResetPeriodThrowingNode(). Used by the optimizer to decide whether a
  // reduced node's exception handler is still reachable: a reduction can lower
  // a throwing node into several nodes where an intermediate throws but the
  // returned value does not.
  void ResetPeriodThrowingNode() { period_added_throwing_node_ = false; }
  bool period_added_throwing_node() const {
    return period_added_throwing_node_;
  }

  void PushInlineCandidate(MaglevCallSiteInfo* call_site) {
    graph()->inlineable_calls().push(call_site);
  }

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
  HoleyFloat64Constant* GetHoleyFloat64Constant(Float64 constant) {
    return graph()->GetHoleyFloat64Constant(constant);
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

  bool IsTheHoleConstant(ValueNode* node);
  ReduceResult GetConvertReceiver(compiler::SharedFunctionInfoRef shared,
                                  ValueNode* receiver,
                                  ConvertReceiverMode mode);
  bool CanInlineCall(const MaglevCompilationUnit* current_unit,
                     compiler::SharedFunctionInfoRef shared,
                     float call_frequency);

  ReduceResult BuildCheckedSmiSizedInt32(ValueNode* input);

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

  std::optional<bool> TryFoldInt32CompareOperation(Operation op,
                                                   ValueNode* left,
                                                   ValueNode* right);
  std::optional<bool> TryFoldInt32CompareOperation(Operation op,
                                                   ValueNode* left,
                                                   int32_t cst_right);
  bool TryFoldInt32CompareOperation(Operation op, int32_t left, int32_t right);

  std::optional<bool> TryFoldUint32CompareOperation(Operation op,
                                                    ValueNode* left,
                                                    ValueNode* right);
  bool TryFoldUint32CompareOperation(Operation op, uint32_t left,
                                     uint32_t right);

  std::optional<bool> TryFoldFloat64CompareOperation(Operation op,
                                                     ValueNode* left,
                                                     ValueNode* right);
  std::optional<bool> TryFoldFloat64CompareOperation(Operation op,
                                                     ValueNode* left,
                                                     double cst_right);
  bool TryFoldFloat64CompareOperation(Operation op, double left, double right);

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

  MaybeReduceResult TryFoldFloat64Min(ValueNode* left, ValueNode* right);
  MaybeReduceResult TryFoldFloat64Max(ValueNode* left, ValueNode* right);

  MaybeReduceResult TryFoldFloat64Ieee754Unary(
      Float64Ieee754Unary::Ieee754Function ieee_function, ValueNode* input);
  MaybeReduceResult TryFoldFloat64Ieee754Binary(
      Float64Ieee754Binary::Ieee754Function ieee_function, ValueNode* left,
      ValueNode* right);
  MaybeReduceResult TryFoldInt32CountLeadingZeros(ValueNode* input);
  MaybeReduceResult TryFoldFloat64CountLeadingZeros(ValueNode* input);

  MaybeReduceResult TryFoldLogicalNot(ValueNode* input);

  MaybeReduceResult TryFoldTestTypeOf(
      ValueNode* input, interpreter::TestTypeOfFlags::LiteralFlag literal);

  bool CheckType(ValueNode* node, NodeType type, NodeType* old = nullptr) {
    return known_node_aspects().CheckType(broker(), node, type, old);
  }
  NodeType CheckTypes(ValueNode* node, std::initializer_list<NodeType> types) {
    return known_node_aspects().CheckTypes(broker(), node, types);
  }

  V8_NODISCARD bool EnsureType(ValueNode* node, NodeType type,
                               NodeType* old = nullptr) {
    return known_node_aspects().EnsureType(broker(), node, type, old);
  }

  void RecordType(ValueNode* node, NodeType type, NodeType* old = nullptr) {
    // For Turbolev, we insert an AssumeType node when recording a previously
    // not-known type so that the GraphOptimizer (and in particular the KNA
    // processor) can also be aware of this type when non-eagerly reoptimizing
    // the graph later.
    if (const bool already_known = EnsureType(node, type, old);
        already_known || !is_turbolev()) {
      return;
    }
    if (const auto* virtual_object = node->TryCast<VirtualObject>()) {
      if (!virtual_object->allocation()) {
        return;
      }
      node = virtual_object->allocation();
    }

    switch (node->value_representation()) {
      case ValueRepresentation::kTagged:
        AddNewNodeNoInputConversion<AssumeTaggedType>({node}, type);
        break;
      case ValueRepresentation::kInt32:
        AddNewNodeNoInputConversion<AssumeInt32Type>({node}, type);
        break;
      case ValueRepresentation::kUint32:
        AddNewNodeNoInputConversion<AssumeUint32Type>({node}, type);
        break;
      case ValueRepresentation::kFloat64:
        AddNewNodeNoInputConversion<AssumeFloat64Type>({node}, type);
        break;
      default:
        // If you ever run into the UNREACHABLE below, you might need to add a
        // new `Assume` operation.
        UNREACHABLE();
    }
  }

  bool IsEmptyNodeType(NodeType type) {
    return v8::internal::maglev::IsEmptyNodeType(type);
  }
  bool IsEmptyNodeType(ValueNode* node) {
    return known_node_aspects().IsEmptyNodeType(broker(), node);
  }
  NodeType GetType(ValueNode* node) {
    NodeType type = known_node_aspects().GetTypeUnchecked(broker(), node);
    if (v8_flags.maglev_assert_types && type != NodeType::kUnknown)
        [[unlikely]] {
      if (type == NodeType::kNone) {
        // We're generating code which should never be executed.
        ReduceResult result = AddNewNode<Trap>({});
        CHECK(result.IsDoneWithPayload());
      } else {
        // TODO(marja): Consider adding different CheckMaglevType variants
        // based on node->value_representation(). Then we wouldn't need to
        // convert the value to tagged.
        ReduceResult result = AddNewNode<CheckMaglevType>({node}, type);
        CHECK(result.IsDoneWithPayload());
      }
    }
    return type;
  }
  NodeInfo* GetOrCreateInfoFor(ValueNode* node) {
    return known_node_aspects().GetOrCreateInfoFor(broker(), node);
  }
  // Returns true if we statically know that {lhs} and {rhs} have disjoint
  // types.
  bool HaveDisjointTypes(ValueNode* lhs, ValueNode* rhs) {
    NodeType rhs_type = GetType(rhs);
    return HasDisjointType(lhs, rhs_type);
  }

  bool HasDisjointType(ValueNode* lhs, NodeType rhs_type) {
    NodeType lhs_type = GetType(lhs);
    return IsEmptyNodeType(IntersectType(lhs_type, rhs_type));
  }

  void SetKnownValue(ValueNode* node, compiler::ObjectRef constant,
                     NodeType new_node_type);

  Zone* zone() const { return zone_; }
  Graph* graph() const { return graph_; }
  compiler::JSHeapBroker* broker() const { return broker_; }
  LocalIsolate* local_isolate() const { return broker()->local_isolate(); }
  bool is_turbolev() const {
    return graph()->compilation_info()->is_turbolev();
  }

  KnownNodeAspects& known_node_aspects() {
    static_assert(ReducerBaseWithKNA<BaseT>);
    return base_->known_node_aspects();
  }
  void set_known_node_aspects(KnownNodeAspects* kna) {
    static_assert(ReducerBaseWithKNASetter<BaseT>);
    base_->set_known_node_aspects(kna);
  }

  friend class Subgraph<BaseT>;

 protected:
  class LazyDeoptResultLocationScope;

  enum class AddNewNodeMode {
    kBuffered,
    kUnbuffered,  // Only support insertion at block end.
  };

  template <typename NodeT, typename... Args>
  ReduceResult AddNewNodeOrGetEquivalent(
      bool convert_inputs, std::initializer_list<ValueNode*> raw_inputs,
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
  void MarkForInt32Truncation(NodeT* node);
  template <typename NodeT>
  void UpdateRange(NodeT* node);

  std::optional<ValueNode*> TryGetConstantAlternative(ValueNode* node);

  InlinedAllocation* ExtendOrReallocateCurrentAllocationBlock(
      AllocationType allocation_type, VirtualObject* value);
  ReduceResult ConvertForField(ValueNode* value, const vobj::Field& desc,
                               AllocationType allocation_type);
  void BuildInitializeStore(vobj::Field desc, InlinedAllocation* alloc,
                            AllocationType allocation_type, ValueNode* value,
                            StoreTaggedMode store_mode,
                            MaybeAssignedFlag maybe_assigned = kMaybeAssigned);
  void BuildInitializeStore_Tagged(vobj::Field desc, InlinedAllocation* alloc,
                                   AllocationType allocation_type,
                                   ValueNode* value, StoreTaggedMode store_mode,
                                   MaybeAssignedFlag maybe_assigned);
  void BuildInitializeStore_TrustedPointer(vobj::Field desc,
                                           InlinedAllocation* alloc,
                                           AllocationType allocation_type,
                                           ValueNode* value);

  template <typename T>
  friend class MapInference;

 private:
  template <typename NodeT, typename... Args>
  ReduceResult EmitAbruptBlockEnd(std::initializer_list<ValueNode*> inputs,
                                  Args&&... args);

  // Use TryGetConstant instead.
  compiler::OptionalHeapObjectRef TryGetHeapObjectConstant(
      ValueNode* node, ValueNode** constant_node);
  BaseT* base_;

  Graph* graph_;
  Zone* zone_;
  compiler::JSHeapBroker* broker_;

  MaglevGraphLabeller::Provenance current_provenance_;
  BasicBlock* current_block_ = nullptr;
  BasicBlockPosition current_block_position_ = BasicBlockPosition::End();
  AddNewNodeMode add_new_node_mode_ = AddNewNodeMode::kBuffered;

  AllocationBlock* current_allocation_block_ = nullptr;

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

  bool period_added_throwing_node_ = false;

  compiler::FeedbackSource current_speculation_feedback_ = {};
  SpeculationMode current_speculation_mode_ =
      SpeculationMode::kDisallowSpeculation;
  std::optional<PendingSplice> pending_splice_;

  // The innermost optimizer-side Subgraph currently being built, or nullptr.
  // Lets a nested Subgraph splice directly into its enclosing subgraph instead
  // of recording a separate top-level pending splice.
  Subgraph<BaseT>* active_subgraph_ = nullptr;
};

template <typename DerivedT, typename BaseT>
class SubgraphBase {
 public:
  class Variable {
   public:
    explicit Variable(int index) : pseudo_register_(index) {}

   private:
    friend class SubgraphBase;
    friend DerivedT;
    interpreter::Register pseudo_register_;
  };

  class Label;

  class LabelForTrackingInterpreterFrameState {
   public:
    LabelForTrackingInterpreterFrameState(SubgraphBase* sg,
                                          int predecessor_count,
                                          int future_bind_offset)
        : sg_(sg),
          predecessor_count_(predecessor_count),
          future_bind_offset_(future_bind_offset) {}
    LabelForTrackingInterpreterFrameState(SubgraphBase* sg,
                                          int predecessor_count,
                                          std::initializer_list<Variable*> vars,
                                          int future_bind_offset)
        : sg_(sg),
          predecessor_count_(predecessor_count),
          vars_(vars),
          future_bind_offset_(future_bind_offset) {}

   private:
    friend class Label;
    SubgraphBase* sg_;
    int predecessor_count_;
    std::vector<Variable*> vars_;
    int future_bind_offset_;
  };

  class Label {
   public:
    Label(SubgraphBase* sg, int predecessor_count);
    Label(SubgraphBase* sg, int predecessor_count,
          std::initializer_list<Variable*> vars);

    // NOLINTNEXTLINE(runtime/explicit)
    Label(const LabelForTrackingInterpreterFrameState& label);

    BasicBlockRef* ref() { return &ref_; }
    int predecessor_count() const { return predecessor_count_; }
    bool ShouldTrackInterpreterFrameState() const {
      return future_bind_offset_.has_value();
    }

   private:
    friend class SubgraphBase;
    friend DerivedT;

    int predecessor_count_ = -1;

    // These are for tracking the values of Variables and merging them into
    // variable_frame_.
    MergePointInterpreterFrameState* variable_merge_state_ = nullptr;
    compiler::BytecodeLivenessState* variable_liveness_ = nullptr;
    BasicBlockRef ref_;

    // Used only by MaglevSubGraphBuilder when the label is bound at a
    // future bytecode offset; the optimizer never sets either of these.
    // These are for tracking the values of registers and merging into the
    // interpreter frame. Setting the future_bind_offset to the bytecode offset
    // where the "bind" for this label will be enables tracking.
    // TODO(marja): Unify merge_state_ and variable_merge_state_ and only
    // have one.
    std::optional<int> future_bind_offset_;
    MergePointInterpreterFrameState* merge_state_ = nullptr;
  };

  void set(Variable& v, ValueNode* value) {
    variable_frame_.set(v.pseudo_register_, value);
  }
  ValueNode* get(const Variable& v) const {
    return variable_frame_.get(v.pseudo_register_);
  }

  void GotoOrTrim(Label* label) {
    if (reducer_->current_block() == nullptr) {
      ReducePredecessorCount(label);
      return;
    }
    static_cast<DerivedT*>(this)->Goto(label);
  }

  void ReducePredecessorCount(Label* label, unsigned num = 1);

  V8_NODISCARD ReduceResult TrimPredecessorsAndBind(Label* label);

 protected:
  SubgraphBase(MaglevReducer<BaseT>* reducer, MaglevCompilationUnit* dummy_unit)
      : reducer_(reducer),
        dummy_unit_(dummy_unit),
        variable_frame_(*dummy_unit, /*known_node_aspects=*/nullptr) {}

  MaglevReducer<BaseT>* reducer() { return reducer_; }
  MaglevCompilationUnit* dummy_unit() { return dummy_unit_; }
  InterpreterFrameState& variable_frame() { return variable_frame_; }

  // Default CRTP hook; the builder side overrides to also MergeDead its
  // separate interpreter-frame-state tracking. The optimizer never
  // triggers it (its labels always have future_bind_offset_ = nullopt).
  void MergeDeadInterpreterFrameState(Label*, unsigned) { UNREACHABLE(); }

  MaglevReducer<BaseT>* reducer_;
  MaglevCompilationUnit* dummy_unit_;
  InterpreterFrameState variable_frame_;
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_REDUCER_H_
