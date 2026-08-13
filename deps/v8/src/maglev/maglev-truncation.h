// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_TRUNCATION_H_
#define V8_MAGLEV_MAGLEV_TRUNCATION_H_

#include <algorithm>
#include <cmath>
#include <type_traits>

#include "src/base/logging.h"
#include "src/base/small-vector.h"
#include "src/common/scoped-modification.h"
#include "src/flags/flags.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-reducer.h"

#define TRACE_TRUNCATION(msg)                                                  \
  if (V8_UNLIKELY(v8_flags.trace_maglev_truncation && is_tracing_enabled())) { \
    StdoutStream{} << "[maglev-truncation] " << msg << std::endl;              \
  }

namespace v8 {
namespace internal {
namespace maglev {

template <typename T>
concept IsValueNodeT = std::is_base_of_v<ValueNode, T>;

// Truncating an operation to Int32 replaces a rounded Number result by an
// exact wrapping one, so it is only value equivalent while every intermediate
// it is computed from stays exactly representable as a double. One past
// Number.MAX_SAFE_INTEGER: 2^53 is still exactly representable.
constexpr int64_t kMaxExactlyRepresentableValue =
    static_cast<int64_t>(kMaxSafeIntegerUint64) + 1;
constexpr int64_t kMaxSaturatedValue = kMaxExactlyRepresentableValue + 1;

inline Range RefinedInputRange(ValueNode* node) {
  node = node->UnwrapIdentities();
  Range refined = Range::All();
  switch (node->opcode()) {
    case Opcode::kInt32BitwiseAnd:
      for (int i = 0; i < 2; ++i) {
        if (auto mask = node->TryGetInt32ConstantInput(i); mask && *mask >= 0) {
          refined = Range(0, *mask);
          break;
        }
      }
      break;
    case Opcode::kInt32ShiftRight:
      if (auto shift = node->TryGetInt32ConstantInput(1)) {
        int32_t s = *shift & 0x1f;
        refined = Range(int64_t{INT32_MIN} >> s, int64_t{INT32_MAX} >> s);
      }
      break;
    case Opcode::kInt32ShiftRightLogical:
      if (auto shift = node->TryGetInt32ConstantInput(1)) {
        int32_t s = *shift & 0x1f;
        refined = Range(0, int64_t{UINT32_MAX} >> s);
      }
      break;
    default:
      break;
  }
  return Range::Intersect(refined, node->GetStaticRange());
}

inline int64_t GetMaxExactValueOfRange(Range range) {
  if (range.is_empty()) return kMaxSaturatedValue;
  std::optional<int64_t> min = range.min();
  std::optional<int64_t> max = range.max();
  if (!min.has_value() || !max.has_value()) {
    return kMaxSaturatedValue;
  }
  if (*min < -kMaxExactlyRepresentableValue ||
      *max > kMaxExactlyRepresentableValue) {
    return kMaxSaturatedValue;
  }
  return std::max(-*min, *max);
}

constexpr bool IsAdditiveOperation(Opcode opcode) {
  switch (opcode) {
    case Opcode::kFloat64Add:
    case Opcode::kFloat64Subtract:
    case Opcode::kFloat64SpeculateSafeAdd:
    case Opcode::kInt32Add:
    case Opcode::kInt32Subtract:
    case Opcode::kInt32AddWithOverflow:
    case Opcode::kInt32SubtractWithOverflow:
      return true;
    default:
      return false;
  }
}

// This pass propagates updates for the `CanTruncateToInt32` flag.
// At the end of the pass, if a node has `CanTruncateToInt32` then all its uses
// can handle the node's output being truncated to an int32. IMPORTANT: This is
// a necessary, but not sufficient, condition. The actual truncation will only
// occur if all of the node's inputs can be truncated.
class PropagateTruncationProcessor {
 public:
  explicit PropagateTruncationProcessor(Graph* graph)
      : max_exact_value_(graph->zone()) {}

  void PreProcessGraph(Graph* graph) {}
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }

  template <IsValueNodeT NodeT>
  ProcessResult Process(NodeT* node) {
    if constexpr (NodeT::kProperties.has_eager_deopt_info()) {
      // Note that we don't know yet if the framestates of CheckpointedJumps
      // will eventually be used or not. We have to assume that it might
      // eventually be used, in which case it must prevent truncations of its
      // inputs (which is why we check `has_eager_deopt_info` rather than
      // `can_eager_deopt` in the condition above).
      node->eager_deopt_info()->ForEachInput([&](ValueNode* node) {
        UnsetCanTruncateToInt32ForDeoptFrameInput(node);
      });
    }
    if constexpr (NodeT::kProperties.can_lazy_deopt()) {
      node->lazy_deopt_info()->ForEachInput([&](ValueNode* node) {
        UnsetCanTruncateToInt32ForDeoptFrameInput(node);
      });
    }

    if (IsInt32BitwiseBinaryOperationNode(node->opcode())) {
      return ProcessResult::kContinue;
    }

    // An addition composes linearly with its operands, so a truncatable result
    // makes them truncatable too, as long as the value it stands for stays
    // exactly representable. This covers the Int32 forms as well, including the
    // WithOverflow variants, since TruncationProcessor overwrites truncatable
    // ones with the plain operation.
    if constexpr (IsAdditiveOperation(Node::opcode_of<NodeT>)) {
      if (node->can_truncate_to_int32()) {
        if (GetMaxExactValue(node) > kMaxExactlyRepresentableValue) {
          node->set_can_truncate_to_int32(false);
          UnsetCanTruncateToInt32Inputs(node);
          return ProcessResult::kContinue;
        }
        CheckOperandsFit(node);
        // Otherwise don't unset truncation...
        return ProcessResult::kContinue;
      }
      UnsetCanTruncateToInt32Inputs(node);
      return ProcessResult::kContinue;
    }

    // TODO(marja): We can add a limited version of that, to support cases where
    // one of the operands is a constant and thus we can be sure the result
    // stays in the safe range.

    UnsetCanTruncateToInt32Inputs(node);
    return ProcessResult::kContinue;
  }

  ProcessResult Process(ChangeInt32ToFloat64* node) {
    if (!node->can_truncate_to_int32()) {
      UnsetCanTruncateToInt32Inputs(node);
    }
    return ProcessResult::kContinue;
  }

  // Non-value nodes.
  template <typename NodeT>
  ProcessResult Process(NodeT* node) {
    // Non value nodes does not need to be truncated, but we should
    // propagate that we do not want to truncate its inputs.
    if constexpr (NodeT::kProperties.has_eager_deopt_info()) {
      node->eager_deopt_info()->ForEachInput([&](ValueNode* node) {
        UnsetCanTruncateToInt32ForDeoptFrameInput(node);
      });
    }
    if constexpr (NodeT::kProperties.can_lazy_deopt()) {
      node->lazy_deopt_info()->ForEachInput([&](ValueNode* node) {
        UnsetCanTruncateToInt32ForDeoptFrameInput(node);
      });
    }
    UnsetCanTruncateToInt32Inputs(node);
    return ProcessResult::kContinue;
  }

  ProcessResult Process(Identity* node) {
    return ProcessSingleInputSpecialNode(node);
  }
  ProcessResult Process(ReturnedValue* node) {
    return ProcessSingleInputSpecialNode(node);
  }

  ProcessResult Process(Dead* node) { return ProcessResult::kContinue; }

  // TODO(victorgomes): We can only truncate CheckedHoleyFloat64ToInt32
  // inputs if we statically know they are in Int32 range.

  ProcessResult Process(TruncateFloat64ToInt32* node) {
    // We can always truncate the input of this node.
    return ProcessResult::kContinue;
  }
  ProcessResult Process(TruncateHoleyFloat64ToInt32* node) {
    // We can always truncate the input of this node.
    return ProcessResult::kContinue;
  }
  ProcessResult Process(UnsafeFloat64ToInt32* node) {
    // We can always truncate the input of this node.
    return ProcessResult::kContinue;
  }
  ProcessResult Process(UnsafeHoleyFloat64ToInt32* node) {
    // We can always truncate the input of this node.
    return ProcessResult::kContinue;
  }

  void PostProcessGraph(Graph* graph) {}

 private:
  // Memoised GetMaxExactValue of every node it has been asked about.
  ZoneAbslFlatHashMap<ValueNode*, int64_t> max_exact_value_;

  static ValueNode* UnwrapForTruncation(ValueNode* node) {
    node = node->UnwrapIdentities();
    if (node->Is<ChangeInt32ToFloat64>()) {
      return node->input_node(0)->UnwrapIdentities();
    }
    return node;
  }

  static bool IsTruncatableAdditiveOperation(ValueNode* node) {
    return node->can_truncate_to_int32() && IsAdditiveOperation(node->opcode());
  }

  // Largest absolute value an input can hold, as an operand of `user_opcode`.
  static int64_t GetMaxExactInputValue(Opcode user_opcode, ValueNode* node) {
    node = UnwrapForTruncation(node);
    if (auto constant = node->TryCast<Float64Constant>()) {
      double value = std::abs(constant->value().get_scalar());
      if (!(value <= static_cast<double>(kMaxExactlyRepresentableValue))) {
        return kMaxSaturatedValue;
      }
      return static_cast<int64_t>(std::ceil(value));
    }
    // A multiply this pass truncates keeps only the low 32 bits of a product
    // that its own safe integer check bounds by 2^53, so charge the product.
    if ((node->opcode() == Opcode::kInt32MultiplyWithOverflow ||
         node->opcode() == Opcode::kFloat64Multiply) &&
        node->can_truncate_to_int32()) {
      return GetMaxExactValueOfRange(
          Range::Mul(RefinedInputRange(node->input_node(0)),
                     RefinedInputRange(node->input_node(1))));
    }
    if (node->is_int32() || node->is_uint32()) {
      return int64_t{kMaxUInt32};
    }
    // An operand the pass leaves untruncated has nothing truncated below it
    // either, since refusing a node also refuses everything it is computed
    // from, so its static range still describes the value it holds.
    int64_t max_abs_value = GetMaxExactValueOfRange(node->GetStaticRange());
    if (user_opcode == Opcode::kFloat64SpeculateSafeAdd) {
      // Speculative truncation reaches a Float64 operand through a conversion
      // that range checks it against the additive safe integer feedback range,
      // so that check bounds it too.
      max_abs_value = std::min(max_abs_value, -kMinAdditiveSafeIntegerFeedback);
    }
    return max_abs_value;
  }

  // Largest absolute value this node can reach before truncation wraps it.
  // Addition composes linearly, so looking through every truncatable
  // addition down to the operands they are built from, and summing those,
  // bounds the whole subgraph and with it every intermediate the pass never
  // checks.
  int64_t GetMaxExactValue(ValueNode* root) {
    // Anything above the largest exactly representable value is
    // indistinguishable for our purposes, so saturate there rather than risk
    // overflowing the sum.
    auto add_saturated = [](int64_t a, int64_t b) {
      if (a > kMaxExactlyRepresentableValue - b) {
        return kMaxSaturatedValue;
      }
      return a + b;
    };
    // The additions walked here are acyclic: a value cycle passes through a
    // Phi, and a Phi is not an additive operation, so it ends the walk as an
    // operand.
    base::SmallVector<ValueNode*, 16> stack{root};
    while (!stack.empty()) {
      ValueNode* node = stack.back();
      if (max_exact_value_.contains(node)) {
        stack.pop_back();
        continue;
      }
      // Resolve first: queue up every operand still missing a bound, and come
      // back to this node once they all have one.
      bool has_pending_input = false;
      for (int i = 0; i < node->input_count(); i++) {
        ValueNode* input = UnwrapForTruncation(node->input_node(i));
        if (IsTruncatableAdditiveOperation(input) &&
            !max_exact_value_.contains(input)) {
          stack.push_back(input);
          has_pending_input = true;
        }
      }
      if (has_pending_input) continue;
      // Every operand is bounded now, so this node's bound is their sum.
      int64_t max_abs_value = 0;
      for (int i = 0; i < node->input_count(); i++) {
        ValueNode* input = UnwrapForTruncation(node->input_node(i));
        max_abs_value = add_saturated(
            max_abs_value, IsTruncatableAdditiveOperation(input)
                               ? max_exact_value_.at(input)
                               : GetMaxExactInputValue(node->opcode(), input));
      }
      stack.pop_back();
      max_exact_value_.emplace(node, max_abs_value);
    }
    return max_exact_value_.at(root);
  }

  // Accepting a node commits to accepting every truncatable addition below it,
  // since refusing one later would strand the rest already truncated. Only
  // reads the memo, which GetMaxExactValue has just filled in for them all.
  void CheckOperandsFit(ValueNode* node) {
#ifdef DEBUG
    for (int i = 0; i < node->input_count(); i++) {
      ValueNode* input = UnwrapForTruncation(node->input_node(i));
      if (!IsTruncatableAdditiveOperation(input)) continue;
      DCHECK_LE(max_exact_value_.at(input), kMaxExactlyRepresentableValue);
    }
#endif  // DEBUG
  }

  template <typename NodeT, int I>
  void UnsetCanTruncateToInt32ForFixedInputNodes(NodeT* node) {
    if constexpr (I < static_cast<int>(NodeT::kInputCount)) {
      if constexpr (NodeT::kInputRepresentations[I] !=
                    ValueRepresentation::kTagged) {
        node->NodeBase::input(I).node()->set_can_truncate_to_int32(false);
      }
      UnsetCanTruncateToInt32ForFixedInputNodes<NodeT, I + 1>(node);
    }
  }

  template <typename NodeT>
  void UnsetCanTruncateToInt32Inputs(NodeT* node) {
    if constexpr (IsFixedInputNode<NodeT>()) {
      return UnsetCanTruncateToInt32ForFixedInputNodes<NodeT, 0>(node);
    }
#ifdef DEBUG
    for (Input input : node->inputs()) {
      DCHECK(!input.node()->can_truncate_to_int32());
    }
#endif  // DEBUG
  }

  ProcessResult ProcessSingleInputSpecialNode(ValueNode* node) {
    if (!node->can_truncate_to_int32()) {
      ValueNode* input_node = node->input_node(0);
      if (input_node->value_representation() != ValueRepresentation::kTagged) {
        input_node->set_can_truncate_to_int32(false);
      }
    }
    return ProcessResult::kContinue;
  }

  void UnsetCanTruncateToInt32ForDeoptFrameInput(ValueNode* node) {
    // TODO(victorgomes): Technically if node is in the int32 range, this use
    // would still allow truncation.
    if (!node->is_tagged()) {
      node->set_can_truncate_to_int32(false);
    }
  }
};

// This pass performs the truncation optimization by replacing floating-point
// operations with their more efficient integer-based equivalents.
//
// A node is truncated if, and only if, both of these conditions are met:
//  1. It is marked with the `CanTruncateToInt32` flag.
//  2. All of its inputs have already been converted/truncated to int32.
class TruncationProcessor {
 public:
  explicit TruncationProcessor(Graph* graph) : reducer_(this, graph) {}

  void PreProcessGraph(Graph* graph) {}
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block);
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block);
  void PostPhiProcessing() {}
  void PostProcessGraph(Graph* graph) {}

  template <typename NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState& state) {
    PreProcessNode(node, state);
    PostProcessNode(node);
    return ProcessResult::kContinue;
  }

#define PROCESS_BINOP(Op)                                                  \
  ProcessResult Process(Float64##Op* node, const ProcessingState& state) { \
    PreProcessNode(node, state);                                           \
    ProcessFloat64BinaryOp<Int32##Op>(node);                               \
    PostProcessNode(node);                                                 \
    return ProcessResult::kContinue;                                       \
  }
  PROCESS_BINOP(Add)
  PROCESS_BINOP(Subtract)
  PROCESS_BINOP(Multiply)
  PROCESS_BINOP(Divide)
#undef PROCESS_BINOP

#define PROCESS_TRUNC_CONV(Node)                                    \
  ProcessResult Process(Node* node, const ProcessingState& state) { \
    PreProcessNode(node, state);                                    \
    ProcessResult result = ProcessTruncatedConversion(node);        \
    PostProcessNode(node);                                          \
    return result;                                                  \
  }
  PROCESS_TRUNC_CONV(TruncateFloat64ToInt32)
  PROCESS_TRUNC_CONV(TruncateHoleyFloat64ToInt32)
  PROCESS_TRUNC_CONV(UnsafeFloat64ToInt32)
  PROCESS_TRUNC_CONV(UnsafeHoleyFloat64ToInt32)
#undef PROCESS_TRUNC_CONV

#define PROCESS_INT32_ARITHMETIC_OPERATION_WITH_OVERFLOW(Op)      \
  ProcessResult Process(Op* node, const ProcessingState& state) { \
    PreProcessNode(node, state);                                  \
    ProcessInt32ArithmeticOperationWithOverflow<Op>(node);        \
    PostProcessNode(node);                                        \
    return ProcessResult::kContinue;                              \
  }
  PROCESS_INT32_ARITHMETIC_OPERATION_WITH_OVERFLOW(Int32AddWithOverflow)
  PROCESS_INT32_ARITHMETIC_OPERATION_WITH_OVERFLOW(Int32SubtractWithOverflow)
#undef PROCESS_INT32_ARITHMETIC_OPERATION_WITH_OVERFLOW

  ProcessResult Process(Int32MultiplyWithOverflow* node,
                        const ProcessingState& state) {
    PreProcessNode(node, state);
    ProcessInt32MultiplyWithOverflow(node);
    PostProcessNode(node);
    return ProcessResult::kContinue;
  }

#define PROCESS_INT32_BITWISE_BINARY_OPERATION(Op)                       \
  ProcessResult Process(Op* node, const ProcessingState& state) {        \
    PreProcessNode(node, state);                                         \
    ProcessResult result = ProcessInt32BitwiseBinaryOperation<Op>(node); \
    PostProcessNode(node);                                               \
    return result;                                                       \
  }

  INT32_BITWISE_BINARY_OPERATIONS_NODE_LIST(
      PROCESS_INT32_BITWISE_BINARY_OPERATION)
#undef PROCESS_INT32_BITWISE_BINARY_OPERATION

  ProcessResult Process(Float64SpeculateSafeAdd* node,
                        const ProcessingState& state) {
    ScopedModification<NodeBase*> current_node(&current_node_, node);
    PreProcessNode(node, state);
    ProcessFloat64SpeculateSafeAdd(node);
    PostProcessNode(node);
    return ProcessResult::kContinue;
  }

  DeoptFrame* GetDeoptFrameForEagerDeopt() {
    DCHECK(current_node()->properties().can_eager_deopt() ||
           current_node()->properties().is_deopt_checkpoint());
    return &current_node()->eager_deopt_info()->top_frame();
  }

 private:
  MaglevReducer<TruncationProcessor> reducer_;
  int current_node_index_ = 0;
  NodeBase* current_node_;

  NodeBase* current_node() const {
    CHECK_NOT_NULL(current_node_);
    return current_node_;
  }

  void PreProcessNode(Node*, const ProcessingState& state);
  void PostProcessNode(Node*);

  void UnwrapInputs(NodeBase*);

  // Phis are treated differently since they are not stored directly in the
  // basic block.
  void PreProcessNode(Phi*, const ProcessingState& state);
  void PostProcessNode(Phi*);

  // Control nodes are singletons in the basic block.
  void PreProcessNode(ControlNode*, const ProcessingState& state);
  void PostProcessNode(ControlNode*);

  int NonInt32InputCount(ValueNode* node);
  ValueNode* GetTruncatedInt32Input(ValueNode* node, int index);
  ValueNode* GetSpeculatedTruncatedInt32Input(ValueNode* node, int index);
  void EnsureTruncatedInt32Inputs(ValueNode* node);
  void ConvertInputsToFloat64(ValueNode* node);

  ProcessResult ProcessTruncatedConversion(ValueNode* node);

  bool IsUnsafeIntConstant(ValueNode* node, int index) {
    ValueNode* input = node->input_node(index);
    return IsConstantNode(input->opcode()) &&
           !input->GetStaticRange().IsSafeInt();
  }

  bool IsSafeIntInputOrPhi(ValueNode* node, int index) {
    ValueNode* input = node->input_node(index);
    if (input->Is<Phi>()) return true;
    if (input->is_conversion()) return IsSafeIntInputOrPhi(input, 0);
    return input->GetStaticRange().IsSafeInt();
  }

  void ProcessFloat64SpeculateSafeAdd(Float64SpeculateSafeAdd* node) {
    if (!node->can_truncate_to_int32() || IsUnsafeIntConstant(node, 0) ||
        IsUnsafeIntConstant(node, 1)) {
      // Don't truncate this node.
      node->OverwriteWith<Float64Add>();
      return;
    }
    if (node->range().IsSafeInt()) {
      // Non-speculating truncation.
      ProcessFloat64BinaryOp<Int32Add>(node);
      return;
    }
    // Checking both inputs are usually expensive, so we only speculate if one
    // of the inputs is already a safe int. However since truncation happens
    // before phi representation selector, we also speculate if we see a phi.
    if (!IsSafeIntInputOrPhi(node, 0) && !IsSafeIntInputOrPhi(node, 1)) {
      // Don't truncate this node.
      node->OverwriteWith<Float64Add>();
      return;
    }
    // Speculating truncation.
    for (int i = 0; i < node->input_count(); i++) {
      node->change_input(i, GetSpeculatedTruncatedInt32Input(node, i));
    }
    node->OverwriteWith<Int32Add>();
  }

  template <typename NodeT>

  void ProcessFloat64BinaryOp(ValueNode* node) {
    if (!node->can_truncate_to_int32()) return;

    switch (NonInt32InputCount(node)) {
      case 0:
        // All inputs are Int32, truncate node.
        EnsureTruncatedInt32Inputs(node);
        TRACE_TRUNCATION("truncating "
                         << PrintNodeBrief{node} << " to "
                         << OpcodeToString(Node::opcode_of<NodeT>));
        node->OverwriteWith<NodeT>();
        break;
      case 1:
        // Convert nodes back to float64 and don't truncate.
        ConvertInputsToFloat64(node);
        break;
      case 2:
        // Don't truncate.
        break;
      default:
        UNREACHABLE();
    }
  }

  template <typename NodeT>

  void ProcessInt32ArithmeticOperationWithOverflow(NodeT* node) {
    if (!node->can_truncate_to_int32()) return;

    if (node->opcode() == Opcode::kInt32AddWithOverflow) {
      TRACE_TRUNCATION("truncating " << PrintNodeBrief{node} << " to Int32Add");
      node->OverwriteWith(Opcode::kInt32Add);

    } else {
      DCHECK_EQ(node->opcode(), Opcode::kInt32SubtractWithOverflow);
      TRACE_TRUNCATION("truncating " << PrintNodeBrief{node}
                                     << " to Int32Subtract");
      node->OverwriteWith(Opcode::kInt32Subtract);
    }
    // TODO(marja): To support Int32DivideWithOverflow, we need to be able to
    // reason about ranges.
  }

  // The product provably fits the safe-integer range, so the wrapping Int32
  // multiply matches Number multiplication followed by ToInt32 (which is how a
  // truncatable result is consumed, also erasing the minus-zero distinction).
  // Above 2^53 the two diverge, since the Number product would round.
  void ProcessInt32MultiplyWithOverflow(Int32MultiplyWithOverflow* node) {
    if (!node->can_truncate_to_int32()) return;
    Range product = Range::Mul(RefinedInputRange(node->input_node(0)),
                               RefinedInputRange(node->input_node(1)));
    if (!product.IsSafeInt()) return;
    TRACE_TRUNCATION("truncating " << PrintNodeBrief{node}
                                   << " to Int32Multiply");
    node->OverwriteWith(Opcode::kInt32Multiply);
  }

  template <typename NodeT>
  ProcessResult ProcessInt32BitwiseBinaryOperation(NodeT* node) {
    if (IsCommutativeNode(Node::opcode_of<NodeT>)) {
      std::optional<int32_t> left = node->TryGetInt32ConstantInput(0);
      if (left && left == Int32Identity(Node::opcode_of<NodeT>)) {
        TRACE_TRUNCATION("eliding identity " << PrintNodeBrief{node}
                                             << " with left input");
        node->OverwriteWithIdentityTo(node->input_node(1));
        return ProcessResult::kRemove;
      }
    }
    std::optional<int32_t> right = node->TryGetInt32ConstantInput(1);
    if (right && right == Int32Identity(Node::opcode_of<NodeT>)) {
      TRACE_TRUNCATION("eliding identity " << PrintNodeBrief{node}
                                           << " with right input");
      node->OverwriteWithIdentityTo(node->input_node(0));
      return ProcessResult::kRemove;
    }
    return ProcessResult::kContinue;
  }

  ValueNode* GetTruncatedInt32Constant(double constant);

  bool is_tracing_enabled();
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_TRUNCATION_H_
