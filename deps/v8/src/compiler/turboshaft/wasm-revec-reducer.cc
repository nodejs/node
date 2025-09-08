// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/wasm-revec-reducer.h"

#include <optional>

#include "src/base/logging.h"
#include "src/compiler/turboshaft/opmasks.h"
#include "src/wasm/simd-shuffle.h"

#define TRACE(...)                                  \
  do {                                              \
    if (v8_flags.trace_wasm_revectorize) {          \
      PrintF("Revec: %s %d: ", __func__, __LINE__); \
      PrintF(__VA_ARGS__);                          \
    }                                               \
  } while (false)

namespace v8::internal::compiler::turboshaft {

// Compares the {kind} of two ops with the same SIMD opcode.
bool IsSameSimd128OpKind(const Operation& op0, const Operation& op1) {
  DCHECK_EQ(op0.opcode, op1.opcode);
#define CASE(operation)                                \
  case Opcode::k##operation: {                         \
    using Op = operation##Op;                          \
    return op0.Cast<Op>().kind == op1.Cast<Op>().kind; \
  }
  switch (op0.opcode) {
    // Nodes with kinds to compare:
    CASE(Simd128Unary)
    CASE(Simd128Binop)
    CASE(Simd128Shift)
    CASE(Simd128Ternary)
    CASE(Simd128Splat)
    // Nodes without kinds:
    // TODO(yolanda): Explicitly list nodes we expect to encounter here,
    // and then have "default: UNREACHABLE();" at the end.
    default:
      return true;
  }
#undef CASE
}

bool IsSameOpAndKind(const Operation& op0, const Operation& op1) {
  return op0.opcode == op1.opcode && IsSameSimd128OpKind(op0, op1);
}

std::string GetSimdOpcodeName(Operation const& op) {
  std::ostringstream oss;
  oss << OpcodeName(op.opcode);
  if (op.Is<Simd128BinopOp>() || op.Is<Simd128UnaryOp>() ||
      op.Is<Simd128ShiftOp>() || op.Is<Simd128TestOp>() ||
      op.Is<Simd128TernaryOp>()) {
    op.PrintOptions(oss);
  }
  return oss.str();
}

// Save the result of a uint64_t subtraction.
class OffsetDiff {
 public:
  OffsetDiff(uint64_t offset1, uint64_t offset2) {
    if (offset1 >= offset2) {
      abs_diff_ = offset1 - offset2;
      negative_ = false;
    } else {
      abs_diff_ = offset2 - offset1;
      negative_ = true;
    }
  }

  bool operator==(int64_t other) const {
    return (negative_ == (other < 0)) &&
           (abs_diff_ == static_cast<uint64_t>(abs(other)));
  }
  bool negative() const { return negative_; }

 private:
  uint64_t abs_diff_;  // abs(a-b)
  bool negative_;      // sign(a-b)
};

// This class is the wrapper for StoreOp/LoadOp, which is helpful for
// calculating the relative offset between two StoreOp/LoadOp.
template <typename Op>
  requires(std::is_same_v<Op, StoreOp> || std::is_same_v<Op, LoadOp> ||
           std::is_same_v<Op, Simd128LoadTransformOp>)
class StoreLoadInfo {
 public:
  StoreLoadInfo(const Graph* graph, const Op* op)
      : op_(op), offset_(op->offset) {
    base_ = &graph->Get(op->base());
    if constexpr (std::is_same_v<Op, Simd128LoadTransformOp>) {
      DCHECK_EQ(offset_, 0);
      const WordBinopOp* add_op = base_->TryCast<WordBinopOp>();
      if (!add_op || add_op->kind != WordBinopOp::Kind::kAdd ||
          add_op->rep != WordRepresentation::Word64()) {
        set_invalid();
        return;
      }
      base_ = &graph->Get(add_op->left());
      const ConstantOp* const_op =
          graph->Get(add_op->right()).TryCast<ConstantOp>();
      if (!const_op) {
        set_invalid();
        return;
      }
      offset_ = const_op->word64();
      index_ = &(graph->Get(op->index()));
    } else {
      if (!op->index().has_value()) {
        set_invalid();
        return;
      }
      index_ = &(graph->Get(op->index().value()));
    }

    // Explicit bounds check is introduced in WasmGraphBuilder, the type of
    // bounds-checked index is uintptr_t, index is converted to uintptr_t for
    // memory32.
    // Try to match memory32 with constant index: ChangeUint32ToUintPtr(const).
    if (const ChangeOp* change_op = index_->TryCast<ChangeOp>()) {
      if (change_op->kind != ChangeOp::Kind::kZeroExtend) {
        TRACE("ChangeOp kind not supported for revectorization\n");
        set_invalid();
        return;
      }

      index_ = &graph->Get(change_op->input());
      // If index_ is constant, fold it into offset_.
      if (const ConstantOp* const_op = index_->TryCast<ConstantOp>()) {
        DCHECK_EQ(const_op->kind, ConstantOp::Kind::kWord32);
        // Exceed uint32 limits.
        if (offset_ >
            std::numeric_limits<uint32_t>::max() - const_op->word32()) {
          set_invalid();
          return;
        }
        offset_ += const_op->word32();
        index_ = nullptr;
      }
      // Try to match memory64 with constant index.
    } else if (const ConstantOp* const_op = index_->TryCast<ConstantOp>()) {
      DCHECK_EQ(const_op->kind, ConstantOp::Kind::kWord64);
      // Exceed uint64 limits.
      if (offset_ > std::numeric_limits<uint64_t>::max() - const_op->word64()) {
        set_invalid();
        return;
      }
      offset_ += const_op->word64();
      index_ = nullptr;
    }
  }

  std::optional<OffsetDiff> operator-(const StoreLoadInfo<Op>& rhs) const {
    DCHECK(is_valid() && rhs.is_valid());
    if (base_ != rhs.base_) return {};
    if (index_ != rhs.index_) return {};

    if constexpr (std::is_same_v<Op, Simd128LoadTransformOp>) {
      if (op_->load_kind != rhs.op_->load_kind) return {};
      if (op_->transform_kind != rhs.op_->transform_kind) return {};
    } else {
      if (op_->kind != rhs.op_->kind) return {};
    }

    if constexpr (std::is_same_v<Op, StoreOp>) {
      // TODO(v8:12716) If one store has a full write barrier and the other has
      // no write barrier, consider combining them with a full write barrier.
      if (op_->write_barrier != rhs.op_->write_barrier) return {};
    }

    return OffsetDiff(offset_, rhs.offset_);
  }

  bool is_valid() const { return op_ != nullptr; }

  const Operation* index() const { return index_; }
  uint64_t offset() const { return offset_; }
  const Op* op() const { return op_; }

 private:
  void set_invalid() { op_ = nullptr; }

  const Op* op_;
  const Operation* base_;
  const Operation* index_;
  uint64_t offset_;
};

struct StoreInfoCompare {
  bool operator()(const StoreLoadInfo<StoreOp>& lhs,
                  const StoreLoadInfo<StoreOp>& rhs) const {
    if (lhs.index() != rhs.index()) {
      return lhs.index() < rhs.index();
    }
    return lhs.offset() < rhs.offset();
  }
};

using StoreInfoSet = ZoneSet<StoreLoadInfo<StoreOp>, StoreInfoCompare>;

// Returns true if all of the nodes in node_group are identical.
// Splat opcode in WASM SIMD is used to create a vector with identical lanes.
template <typename T>
bool IsSplat(const T& node_group) {
  DCHECK_EQ(node_group.size(), 2);
  return node_group[1] == node_group[0];
}

void PackNode::Print(Graph* graph) const {
  Operation& op = graph->Get(nodes_[0]);
  TRACE("%s(#%d, #%d)\n", GetSimdOpcodeName(op).c_str(), nodes_[0].id(),
        nodes_[1].id());
}

PackNode* SLPTree::GetPackNode(OpIndex node) {
  auto it = node_to_packnode_.find(node);
  if (it != node_to_packnode_.end()) {
    return it->second;
  }
  return analyzer_->GetPackNode(node);
}

ZoneVector<PackNode*>* SLPTree::GetIntersectPackNodes(OpIndex node) {
  auto it = node_to_intersect_packnodes_.find(node);
  if (it != node_to_intersect_packnodes_.end()) {
    return &(it->second);
  }
  return nullptr;
}

template <typename FunctionType>
void ForEach(FunctionType callback,
             const ZoneUnorderedMap<OpIndex, PackNode*>& node_map) {
  absl::flat_hash_set<PackNode const*> visited;

  for (const auto& entry : node_map) {
    PackNode const* pnode = entry.second;
    if (!pnode || visited.contains(pnode)) {
      continue;
    }
    visited.insert(pnode);

    callback(pnode);
  }
}

template <typename FunctionType>
void ForEach(FunctionType callback,
             const ZoneUnorderedMap<OpIndex, ZoneVector<PackNode*>>& node_map) {
  absl::flat_hash_set<PackNode const*> visited;

  for (const auto& entry : node_map) {
    for (const auto* pnode : entry.second) {
      if (visited.contains(pnode)) {
        continue;
      }
      visited.insert(pnode);
      callback(pnode);
    }
  }
}

void SLPTree::Print(const char* info) {
  TRACE("%s, %zu Packed node:\n", info, node_to_packnode_.size());
  if (!v8_flags.trace_wasm_revectorize) {
    return;
  }

  ForEach([this](PackNode const* pnode) { pnode->Print(&graph_); },
          node_to_packnode_);
  ForEach([this](PackNode const* pnode) { pnode->Print(&graph_); },
          node_to_intersect_packnodes_);
}

bool SLPTree::HasReorderInput(const NodeGroup& node_group) {
  DCHECK_EQ(node_group.size(), 2);
  return (reorder_inputs_.contains(node_group[0]) ||
          reorder_inputs_.contains(node_group[1]) ||
          analyzer_->HasReorderInput(node_group[0]) ||
          analyzer_->HasReorderInput(node_group[1]));
}

bool SLPTree::HasInputDependencies(const NodeGroup& node_group) {
  // When reducing force-packing nodes, some of the inputs need to be reduced
  // earlier. It's possible that these inputs are force-packing nodes themselves
  // which breaks the assumption that nodes with smaller index in ForcePackNode
  // will be visited earlier. Since this pattern is rare in real-life, we will
  // not allow the reordering inputs to be force-packed.
  if (HasReorderInput(node_group)) {
    TRACE("NodeGroup has force-pack inputs.\n");
    return true;
  }

  DCHECK_EQ(node_group.size(), 2);
  if (node_group[0] == node_group[1]) return false;
  OpIndex start, end;
  if (node_group[0] < node_group[1]) {
    start = node_group[0];
    end = node_group[1];
  } else {
    start = node_group[1];
    end = node_group[0];
  }
  // Do BFS from the end node and see if there is a path to the start node.
  // Use a shared deque, as queue is an adaptor and does not support a clear()
  // function.
  ZoneDeque<OpIndex>& to_visit = analyzer_->GetSharedOpIndexDeque();
  ZoneUnorderedSet<OpIndex>& inputs_set = analyzer_->GetSharedOpIndexSet();
  DCHECK(to_visit.empty() && inputs_set.empty());
  to_visit.push_back(end);

  bool result = false;
  while (!to_visit.empty()) {
    OpIndex to_visit_node = to_visit.front();
    Operation& op = graph_.Get(to_visit_node);
    to_visit.pop_front();
    for (OpIndex input : op.inputs()) {
      if (input == start) {
        result = true;
        break;
      } else if (input > start) {
        // Check side effect from input to start's previous node to simplify
        // reducing of force-packing nodes.
        OpIndex start_prev = graph().PreviousIndex(start);
        DCHECK(start_prev.valid());
        if (!IsSideEffectFreeRange(start_prev, input)) {
          result = true;
          break;
        }

        // We should ensure that there is no back edge.
        DCHECK_LT(input, to_visit_node);
        to_visit.push_back(input);
        inputs_set.insert(input);
      }
    }

    if (result) break;
  }

  if (result) {
    to_visit.clear();
  } else {
    reorder_inputs_.merge(inputs_set);
  }

  inputs_set.clear();
  return result;
}

PackNode* SLPTree::NewPackNode(const NodeGroup& node_group) {
  TRACE("PackNode %s(#%d, #%d)\n",
        GetSimdOpcodeName(graph_.Get(node_group[0])).c_str(),
        node_group[0].id(), node_group[1].id());
  PackNode* pnode =
      phase_zone_->New<PackNode>(phase_zone_, node_group, PackNode::kDefault);
  for (OpIndex node : node_group) {
    node_to_packnode_[node] = pnode;
  }
  return pnode;
}

ForcePackNode* SLPTree::NewForcePackNode(const NodeGroup& node_group,
                                         ForcePackNode::ForcePackType type,
                                         unsigned recursion_depth) {
  // Currently we only support force packing two nodes.
  DCHECK_EQ(node_group.size(), 2);

  if (recursion_depth < 1) {
    TRACE("Do not force pack at root #%d:%s\n", node_group[0].id(),
          GetSimdOpcodeName(graph_.Get(node_group[0])).c_str());
    return nullptr;
  }

  // We should guarantee that the one node in the NodeGroup does not rely on the
  // result of the other. Because it is costly to force pack such candidates.
  // For example, we have four nodes {A, B, C, D} which are connected by input
  // edges: A <-- B <-- C <-- D. If {B} and {D} are already packed into a
  // PackNode and we want to force pack {A} and {C}, we need to duplicate {B}
  // and the result will be {A, B, C}, {B, D}. This increases the cost of
  // ForcePack so currently we do not support it.
  if (HasInputDependencies(node_group)) {
    TRACE("ForcePackNode %s(#%d, #%d) failed due to input dependencies.\n",
          GetSimdOpcodeName(graph_.Get(node_group[0])).c_str(),
          node_group[0].id(), node_group[1].id());
    return nullptr;
  }

  TRACE("ForcePackNode %s(#%d, #%d)\n",
        GetSimdOpcodeName(graph_.Get(node_group[0])).c_str(),
        node_group[0].id(), node_group[1].id());
  ForcePackNode* pnode =
      phase_zone_->New<ForcePackNode>(phase_zone_, node_group, type);
  for (OpIndex node : node_group) {
    node_to_packnode_[node] = pnode;
  }

  return pnode;
}

BundlePackNode* SLPTree::NewBundlePackNode(const NodeGroup& node_group,
                                           OpIndex base, int8_t offset,
                                           uint8_t lane_size,
                                           bool is_sign_extract,
                                           bool is_sign_convert) {
  Operation& op = graph_.Get(node_group[0]);
  TRACE("PackNode %s(#%d:, #%d)\n", GetSimdOpcodeName(op).c_str(),
        node_group[0].id(), node_group[1].id());
  BundlePackNode* pnode = phase_zone_->New<BundlePackNode>(
      phase_zone_, node_group, base, offset, lane_size, is_sign_extract,
      is_sign_convert);
  for (OpIndex node : node_group) {
    node_to_packnode_[node] = pnode;
  }
  return pnode;
}

PackNode* SLPTree::NewIntersectPackNode(const NodeGroup& node_group) {
  // Similar to ForcePackNode, dependent inputs are not supported.
  if (HasInputDependencies(node_group)) {
    TRACE("IntersectPackNode %s(#%d, #%d) failed due to input dependencies.\n",
          GetSimdOpcodeName(graph_.Get(node_group[0])).c_str(),
          node_group[0].id(), node_group[1].id());
    return nullptr;
  }

  TRACE("IntersectPackNode %s(#%d, #%d)\n",
        GetSimdOpcodeName(graph_.Get(node_group[0])).c_str(),
        node_group[0].id(), node_group[1].id());
  PackNode* intersect_pnode = phase_zone_->New<PackNode>(
      phase_zone_, node_group, PackNode::kIntersectPackNode);

  for (size_t i = 0; i < node_group.size(); i++) {
    OpIndex op_idx = node_group[i];
    if (i > 0 && op_idx == node_group[0]) continue;
    auto it = node_to_intersect_packnodes_.find(op_idx);
    if (it == node_to_intersect_packnodes_.end()) {
      bool result;
      std::tie(it, result) = node_to_intersect_packnodes_.emplace(
          op_idx, ZoneVector<PackNode*>(phase_zone_));
      DCHECK(result);
    }
    it->second.push_back(intersect_pnode);
  }

  return intersect_pnode;
}

PackNode* SLPTree::NewCommutativePackNodeAndRecurse(const NodeGroup& node_group,
                                                    unsigned depth) {
  PackNode* pnode = NewPackNode(node_group);

  const Simd128BinopOp& op0 = graph_.Get(node_group[0]).Cast<Simd128BinopOp>();
  const Simd128BinopOp& op1 = graph_.Get(node_group[1]).Cast<Simd128BinopOp>();

  bool same_kind =
      (op0.left() == op1.left()) ||
      IsSameOpAndKind(graph_.Get(op0.left()), graph_.Get(op1.left()));
  // If op0.left() and op1.left() don't have the same kind, they'll definitely
  // fail in {BuildTreeRec}, so swap op1's inputs if possible for another
  // chance at success.
  bool swap_inputs = Simd128BinopOp::IsCommutative(op1.kind) && !same_kind;
  if (swap_inputs) {
    TRACE("Change the order of binop operands\n");
  }
  for (size_t i = 0; i < 2; ++i) {
    // Swap the left and right input if necessary.
    size_t node1_input_index = swap_inputs ? 1 - i : i;
    NodeGroup operands(op0.input(i), op1.input(node1_input_index));

    PackNode* child = BuildTreeRec(operands, depth + 1);
    if (!child) return nullptr;
    pnode->SetOperand(i, child);
  }
  return pnode;
}

PackNode* SLPTree::NewPackNodeAndRecurse(const NodeGroup& node_group,
                                         size_t start_index, size_t count,
                                         unsigned depth) {
  PackNode* pnode = NewPackNode(node_group);
  for (size_t i = 0; i < count; ++i) {
    size_t input_index = i + start_index;
    NodeGroup operands(graph_.Get(node_group[0]).input(input_index),
                       graph_.Get(node_group[1]).input(input_index));

    PackNode* child = BuildTreeRec(operands, depth + 1);
    if (!child) return nullptr;
    pnode->SetOperand(i, child);
  }
  return pnode;
}

ShufflePackNode* SLPTree::NewShufflePackNode(
    const NodeGroup& node_group, ShufflePackNode::SpecificInfo::Kind kind) {
  TRACE("PackNode %s(#%d:, #%d)\n",
        GetSimdOpcodeName(graph_.Get(node_group[0])).c_str(),
        node_group[0].id(), node_group[1].id());
  ShufflePackNode* pnode =
      phase_zone_->New<ShufflePackNode>(phase_zone_, node_group, kind);
  for (OpIndex node : node_group) {
    node_to_packnode_[node] = pnode;
  }
  return pnode;
}

ShufflePackNode* SLPTree::Try256ShuffleMatchLoad8x8U(
    const NodeGroup& node_group, const uint8_t* shuffle0,
    const uint8_t* shuffle1) {
  uint8_t shuffle_copy0[kSimd128Size];
  uint8_t shuffle_copy1[kSimd128Size];

  V<Simd128> op_idx0 = node_group[0];
  V<Simd128> op_idx1 = node_group[1];
  const Simd128ShuffleOp& op0 = graph_.Get(op_idx0).Cast<Simd128ShuffleOp>();
  const Simd128ShuffleOp& op1 = graph_.Get(op_idx1).Cast<Simd128ShuffleOp>();

  if (op0.kind != Simd128ShuffleOp::Kind::kI8x16 ||
      op1.kind != Simd128ShuffleOp::Kind::kI8x16) {
    return nullptr;
  }

  if (op0.left() == op0.right() || op1.left() == op1.right()) {
    // Here shuffle couldn't be swizzle.
    return nullptr;
  }

  CopyChars(shuffle_copy0, shuffle0, kSimd128Size);
  CopyChars(shuffle_copy1, shuffle1, kSimd128Size);

  bool need_swap, is_swizzle;

#define CANONICALIZE_SHUFFLE(n)                                                \
  wasm::SimdShuffle::CanonicalizeShuffle(false, shuffle_copy##n, &need_swap,   \
                                         &is_swizzle);                         \
  if (is_swizzle) {                                                            \
    /* Here shuffle couldn't be swizzle. */                                    \
    return nullptr;                                                            \
  }                                                                            \
  V<Simd128> shuffle##n##_left_idx = need_swap ? op##n.right() : op##n.left(); \
  V<Simd128> shuffle##n##_right_idx = need_swap ? op##n.left() : op##n.right();

  CANONICALIZE_SHUFFLE(0);
  CANONICALIZE_SHUFFLE(1);

#undef CANONICALIZE_SHUFFLE
  if (shuffle0_left_idx != shuffle1_left_idx) {
    // Not the same left.
    return nullptr;
  }

  const Simd128LoadTransformOp* load_transform =
      graph_.Get(shuffle0_left_idx).TryCast<Simd128LoadTransformOp>();

  if (!load_transform) {
    // Shuffle left is not Simd128LoadTransformOp.
    return nullptr;
  }

  Simd128ConstantOp* shuffle0_const =
      graph_.Get(shuffle0_right_idx).TryCast<Simd128ConstantOp>();
  Simd128ConstantOp* shuffle1_const =
      graph_.Get(shuffle1_right_idx).TryCast<Simd128ConstantOp>();

  if (!shuffle0_const || !shuffle1_const || !shuffle0_const->IsZero() ||
      !shuffle1_const->IsZero()) {
    // Shuffle right is not zero.
    return nullptr;
  }

  if (load_transform->transform_kind ==
      Simd128LoadTransformOp::TransformKind::k64Zero) {
    /*
      should look like this:
      shuffle0 = 0,x,x,x,  1,x,x,x  2,x,x,x  3,x,x,x
      shuffle1 = 4,x,x,x,  5,x,x,x  6,x,x,x  7,x,x,x
      x >= 16
    */

    for (int i = 0; i < kSimd128Size / 4; ++i) {
      if (shuffle_copy0[i * 4] != i || shuffle_copy1[i * 4] != i + 4) {
        return nullptr;
      }

      if (shuffle_copy0[i * 4 + 1] < kSimd128Size ||
          shuffle_copy0[i * 4 + 2] < kSimd128Size ||
          shuffle_copy0[i * 4 + 3] < kSimd128Size ||
          shuffle_copy1[i * 4 + 1] < kSimd128Size ||
          shuffle_copy1[i * 4 + 2] < kSimd128Size ||
          shuffle_copy1[i * 4 + 3] < kSimd128Size) {
        return nullptr;
      }
    }
    TRACE("match load extend 8x8->32x8\n");
    return NewShufflePackNode(
        node_group, ShufflePackNode::SpecificInfo::Kind::kS256Load8x8U);
  }
  return nullptr;
}

#ifdef V8_TARGET_ARCH_X64
ShufflePackNode* SLPTree::X64TryMatch256Shuffle(const NodeGroup& node_group,
                                                const uint8_t* shuffle0,
                                                const uint8_t* shuffle1) {
  DCHECK_EQ(node_group.size(), 2);
  OpIndex op_idx0 = node_group[0];
  OpIndex op_idx1 = node_group[1];
  const Simd128ShuffleOp& op0 = graph_.Get(op_idx0).Cast<Simd128ShuffleOp>();
  const Simd128ShuffleOp& op1 = graph_.Get(op_idx1).Cast<Simd128ShuffleOp>();
  if (op0.kind != Simd128ShuffleOp::Kind::kI8x16 ||
      op1.kind != Simd128ShuffleOp::Kind::kI8x16) {
    return nullptr;
  }

  uint8_t shuffle8x32[32];

  if (op0.left() == op0.right() && op1.left() == op1.right()) {
    // Shuffles are swizzles.
    for (int i = 0; i < 16; ++i) {
      shuffle8x32[i] = shuffle0[i] % 16;
      shuffle8x32[i + 16] = 16 + shuffle1[i] % 16;
    }

    if (uint8_t shuffle32x8[8];
        wasm::SimdShuffle::TryMatch32x8Shuffle(shuffle8x32, shuffle32x8)) {
      uint8_t control;
      if (wasm::SimdShuffle::TryMatchVpshufd(shuffle32x8, &control)) {
        ShufflePackNode* pnode = NewShufflePackNode(
            node_group, ShufflePackNode::SpecificInfo::Kind::kShufd);
        pnode->info().set_shufd_control(control);
        return pnode;
      }
    }
  } else if (op0.left() != op0.right() && op1.left() != op1.right()) {
    // Shuffles are not swizzles.
    for (int i = 0; i < 16; ++i) {
      if (shuffle0[i] < 16) {
        shuffle8x32[i] = shuffle0[i];
      } else {
        shuffle8x32[i] = 16 + shuffle0[i];
      }

      if (shuffle1[i] < 16) {
        shuffle8x32[i + 16] = 16 + shuffle1[i];
      } else {
        shuffle8x32[i + 16] = 32 + shuffle1[i];
      }
    }

    if (const wasm::ShuffleEntry<kSimd256Size>* arch_shuffle;
        wasm::SimdShuffle::TryMatchArchShuffle(shuffle8x32, false,
                                               &arch_shuffle)) {
      ShufflePackNode::SpecificInfo::Kind kind;
      switch (arch_shuffle->opcode) {
        case kX64S32x8UnpackHigh:
          kind = ShufflePackNode::SpecificInfo::Kind::kS32x8UnpackHigh;
          break;
        case kX64S32x8UnpackLow:
          kind = ShufflePackNode::SpecificInfo::Kind::kS32x8UnpackLow;
          break;
        default:
          UNREACHABLE();
      }
      ShufflePackNode* pnode = NewShufflePackNode(node_group, kind);
      return pnode;
    } else if (uint8_t shuffle32x8[8]; wasm::SimdShuffle::TryMatch32x8Shuffle(
                   shuffle8x32, shuffle32x8)) {
      uint8_t control;
      if (wasm::SimdShuffle::TryMatchShufps256(shuffle32x8, &control)) {
        ShufflePackNode* pnode = NewShufflePackNode(
            node_group, ShufflePackNode::SpecificInfo::Kind::kShufps);
        pnode->info().set_shufps_control(control);
        return pnode;
      }
    }
  }

  return nullptr;
}
#endif  // V8_TARGET_ARCH_X64

// Try to match i8x16/i16x8 to f32x4 conversion pattern.
// The following wasm snippet is an example for load i8x16,
// extend to i32x4 and convert to f32x4:
//  (f32x4.replace_lane 3
//     (f32x4.replace_lane 2
//       (f32x4.replace_lane 1
//         (f32x4.splat
//           (f32.convert_i32_u
//             (i8x16.extract_lane_u 0
//               (local.tee 7
//                 (v128.load align=1
//                   (local.get 0))))))
//         (f32.convert_i32_u
//           (i8x16.extract_lane_u 1
//             (local.get 7))))
//       (f32.convert_i32_u
//         (i8x16.extract_lane_u 2
//           (local.get 7))))
//     (f32.convert_i32_u
//       (i8x16.extract_lane_u 3
//         (local.get 7))))
std::optional<SLPTree::ExtendIntToF32x4Info>
SLPTree::TryGetExtendIntToF32x4Info(OpIndex index) {
  OpIndex current = index;
  LaneExtendInfo lane_extend_info[4];

  // Get information for lane 1 to lane 3.
  for (int lane_index = 3; lane_index > 0; lane_index--) {
    const Simd128ReplaceLaneOp* replace_lane =
        graph_.Get(current)
            .TryCast<turboshaft::Opmask::kSimd128ReplaceLaneF32x4>();
    if (!replace_lane || replace_lane->lane != lane_index) {
      TRACE("Mismatch in replace lane\n");
      return {};
    }
    const ChangeOp* change =
        graph_.Get(replace_lane->new_lane()).TryCast<ChangeOp>();
    if (!change) {
      TRACE("Mismatch in type convert\n");
      return {};
    }
    const Simd128ExtractLaneOp* extract_lane =
        graph_.Get(change->input()).TryCast<Simd128ExtractLaneOp>();
    if (!extract_lane) {
      TRACE("Mismatch in extract lane\n");
      return {};
    }
    lane_extend_info[lane_index].replace_lane_index = replace_lane->lane;
    lane_extend_info[lane_index].change_kind = change->kind;
    lane_extend_info[lane_index].extract_from = extract_lane->input();
    lane_extend_info[lane_index].extract_kind = extract_lane->kind;
    lane_extend_info[lane_index].extract_lane_index = extract_lane->lane;

    current = replace_lane->into();
  }

  // Get information for lane 0 (splat).
  const Simd128SplatOp* splat = graph_.Get(current).TryCast<Simd128SplatOp>();
  if (!splat) {
    TRACE("Mismatch in splat\n");
    return {};
  }
  const ChangeOp* change = graph_.Get(splat->input()).TryCast<ChangeOp>();
  if (!change || (change->kind != ChangeOp::Kind::kSignedToFloat &&
                  change->kind != ChangeOp::Kind::kUnsignedToFloat)) {
    TRACE("Mismatch in splat type convert\n");
    return {};
  }
  const Simd128ExtractLaneOp* extract_lane =
      graph_.Get(change->input()).TryCast<Simd128ExtractLaneOp>();
  if (!extract_lane ||
      (extract_lane->kind != Simd128ExtractLaneOp::Kind::kI8x16S &&
       extract_lane->kind != Simd128ExtractLaneOp::Kind::kI8x16U &&
       extract_lane->kind != Simd128ExtractLaneOp::Kind::kI16x8S &&
       extract_lane->kind != Simd128ExtractLaneOp::Kind::kI16x8U)) {
    TRACE("Mismatch in splat extract lane\n");
    return {};
  }
  lane_extend_info[0].replace_lane_index = 0;
  lane_extend_info[0].change_kind = change->kind;
  lane_extend_info[0].extract_from = extract_lane->input();
  lane_extend_info[0].extract_kind = extract_lane->kind;
  lane_extend_info[0].extract_lane_index = extract_lane->lane;

  // Pattern matching for f32x4.convert_i32x4(i32x4.extract_lane): check if
  // lanes 1-3 match lane 0.
  for (int i = 1; i < 4; i++) {
    if (lane_extend_info[i].change_kind != lane_extend_info[0].change_kind) {
      return {};
    }
    if (lane_extend_info[i].extract_from != lane_extend_info[0].extract_from) {
      return {};
    }
    if (lane_extend_info[i].extract_kind != lane_extend_info[0].extract_kind) {
      return {};
    }
    if (lane_extend_info[i].extract_lane_index !=
        lane_extend_info[0].extract_lane_index + i) {
      return {};
    }
  }

  ExtendIntToF32x4Info info;
  info.extend_from = lane_extend_info[0].extract_from;
  info.start_lane = lane_extend_info[0].extract_lane_index;
  if (lane_extend_info[0].extract_kind == Simd128ExtractLaneOp::Kind::kI8x16S ||
      lane_extend_info[0].extract_kind == Simd128ExtractLaneOp::Kind::kI8x16U) {
    info.lane_size = 1;
  } else {
    info.lane_size = 2;
  }
  info.is_sign_extract =
      lane_extend_info[0].extract_kind == Simd128ExtractLaneOp::Kind::kI8x16S ||
      lane_extend_info[0].extract_kind == Simd128ExtractLaneOp::Kind::kI16x8S;
  info.is_sign_convert =
      lane_extend_info[0].change_kind == ChangeOp::Kind::kSignedToFloat;

  return info;
}

bool SLPTree::TryMatchExtendIntToF32x4(const NodeGroup& node_group,
                                       ExtendIntToF32x4Info* info) {
  OpIndex node0 = node_group[0];
  OpIndex node1 = node_group[1];
  std::optional<ExtendIntToF32x4Info> info0 = TryGetExtendIntToF32x4Info(node0);
  std::optional<ExtendIntToF32x4Info> info1 = TryGetExtendIntToF32x4Info(node1);
  if (!info0.has_value() || !info1.has_value()) {
    return false;
  }

  if (info0.value().extend_from != info1.value().extend_from ||
      info0.value().is_sign_extract != info1.value().is_sign_extract ||
      info0.value().lane_size != info1.value().lane_size ||
      info0.value().is_sign_convert != info1.value().is_sign_convert) {
    return false;
  }

  uint32_t min_lane_index =
      std::min(info0.value().start_lane, info1.value().start_lane);
  if (std::abs(info0.value().start_lane - info1.value().start_lane) != 4) {
    return false;
  }
  if (info0.value().lane_size == 1) {
    if (min_lane_index != 0 && min_lane_index != 8) {
      return false;
    }
  } else {
    DCHECK_EQ(info0.value().lane_size, 2);
    if (min_lane_index != 0) {
      return false;
    }
  }

  *info = info0.value();
  info->start_lane = min_lane_index;
  return true;
}

OpEffects RefineEffects(Operation& op) {
  OpEffects effects = op.Effects();
  const WordBinopOp* word_binop = op.template TryCast<WordBinopOp>();
  if (word_binop &&
      (word_binop->kind == WordBinopOp::Kind::kAdd ||
       word_binop->kind == WordBinopOp::Kind::kMul ||
       word_binop->kind == WordBinopOp::Kind::kSignedMulOverflownBits ||
       word_binop->kind == WordBinopOp::Kind::kUnsignedMulOverflownBits ||
       word_binop->kind == WordBinopOp::Kind::kBitwiseAnd ||
       word_binop->kind == WordBinopOp::Kind::kBitwiseOr ||
       word_binop->kind == WordBinopOp::Kind::kBitwiseXor ||
       word_binop->kind == WordBinopOp::Kind::kSub)) {
    // WordBinop consumes control flow effect to avoid division by 0.
    effects.consumes.control_flow = false;
  }
  return effects;
}

bool SLPTree::IsSideEffectFreeRange(OpIndex from, OpIndex to) {
  DCHECK_LE(from.offset(), to.offset());
  if (from == to) return true;
  const OpEffects effects = RefineEffects(graph().Get(to));
  bool to_is_protected_load = graph().Get(to).IsProtectedLoad();
  for (OpIndex prev_node = graph().PreviousIndex(to); prev_node != from;
       prev_node = graph().PreviousIndex(prev_node)) {
    const OpEffects prev_effects = RefineEffects(graph().Get(prev_node));
    if ((to_is_protected_load && graph().Get(prev_node).IsProtectedLoad())
            ? CannotSwapProtectedLoads(prev_effects, effects)
            : CannotSwapOperations(prev_effects, effects)) {
      TRACE("break side effect %d, %d\n", prev_node.id(), to.id());
      return false;
    }
  }
  return true;
}

bool IsSignExtensionOp(Operation& op) {
  if (const Simd128UnaryOp* unop = op.TryCast<Simd128UnaryOp>()) {
    return unop->kind >= Simd128UnaryOp::Kind::kFirstSignExtensionOp &&
           unop->kind <= Simd128UnaryOp::Kind::kLastSignExtensionOp;
  } else if (const Simd128BinopOp* binop = op.TryCast<Simd128BinopOp>()) {
    return binop->kind >= Simd128BinopOp::Kind::kFirstSignExtensionOp &&
           binop->kind <= Simd128BinopOp::Kind::kLastSignExtensionOp;
  }
  return false;
}

bool SLPTree::IsEqual(const OpIndex node0, const OpIndex node1) {
  if (node0 == node1) return true;
  if (const ConstantOp* const0 = graph_.Get(node0).TryCast<ConstantOp>()) {
    if (const ConstantOp* const1 = graph_.Get(node1).TryCast<ConstantOp>()) {
      return *const0 == *const1;
    }
  }
  return false;
}

PackNode* SLPTree::BuildTree(const NodeGroup& roots) {
  root_ = BuildTreeRec(roots, 0);
  return root_;
}

PackNode* SLPTree::BuildTreeRec(const NodeGroup& node_group,
                                unsigned recursion_depth) {
  DCHECK_EQ(node_group.size(), 2);

  OpIndex node0 = node_group[0];
  OpIndex node1 = node_group[1];
  Operation& op0 = graph_.Get(node0);
  Operation& op1 = graph_.Get(node1);

  if (recursion_depth == kMaxRecursionDepth) {
    TRACE("Failed due to max recursion depth!\n");
    return nullptr;
  }

  // Check if this node group can be packed.
  if (op0.opcode != op1.opcode) {
    TRACE("Can't pack different opcodes\n");
    return nullptr;
  }

  if (graph_.BlockIndexOf(node0) != graph_.BlockIndexOf(node1)) {
    TRACE("Can't pack operations in different basic blocks\n");
    return nullptr;
  }

  auto is_sign_ext = IsSignExtensionOp(op0) && IsSignExtensionOp(op1);

  if (!is_sign_ext && !IsSameSimd128OpKind(op0, op1)) {
    TRACE("(%s, %s) have different kind\n", GetSimdOpcodeName(op0).c_str(),
          GetSimdOpcodeName(op1).c_str());
    return nullptr;
  }

  if (node0.offset() <= node1.offset() ? !IsSideEffectFreeRange(node0, node1)
                                       : !IsSideEffectFreeRange(node1, node0)) {
    TRACE("Can't pack nodes with side effects between them\n");
    return nullptr;
  }

  // Check if this is a duplicate of another entry.
  bool is_intersected = false;
  // For revisited node_group, we only need to match from node0.
  if (PackNode* pnode = GetPackNode(node0)) {
    if (pnode->IsSame(node_group)) {
      TRACE("Perfect diamond merge at #%d,%s\n", node0.id(),
            GetSimdOpcodeName(op0).c_str());
      return pnode;
    }

    // TODO(yolanda): Support other intersect PackNode e.g. overlapped loads.
    if (!pnode->IsForcePackNode()) {
      TRACE("Unsupported partial overlap at #%d,%s!\n", node0.id(),
            GetSimdOpcodeName(op0).c_str());
      return nullptr;
    }

    is_intersected = true;
  }

  // Match intersect packnodes from current tree.
  if (ZoneVector<PackNode*>* intersect_packnodes =
          GetIntersectPackNodes(node0)) {
    for (PackNode* intersect_pnode : *intersect_packnodes) {
      if (intersect_pnode->IsSame(node_group)) {
        TRACE("Perfect diamond merge at intersect pack node #%d,%s, #%d\n",
              node0.id(), GetSimdOpcodeName(op0).c_str(), node1.id());
        return intersect_pnode;
      }
    }

    is_intersected = true;
  }

  // Match intersect packnodes from analyzer.
  if (ZoneVector<PackNode*>* intersect_packnodes =
          analyzer_->GetIntersectPackNodes(node0)) {
    for (PackNode* intersect_pnode : *intersect_packnodes) {
      if (intersect_pnode->IsSame(node_group)) {
        TRACE("Perfect diamond merge at intersect pack node #%d,%s, #%d\n",
              node0.id(), GetSimdOpcodeName(op0).c_str(), node1.id());
        return intersect_pnode;
      }
    }

    is_intersected = true;
  }

  if (is_intersected) {
    TRACE("Partial overlap at #%d,%s!\n", node0.id(),
          GetSimdOpcodeName(op0).c_str());
  } else {
    // Match overlapped PackNode on the second node.
    if (PackNode* pnode = GetPackNode(node1)) {
      if (!pnode->IsForcePackNode()) {
        TRACE("Unsupported partial overlap at #%d,%s!\n", node1.id(),
              GetSimdOpcodeName(op1).c_str());
        return nullptr;
      }
      is_intersected = true;
      // Check elsewise as we've verified no matched IntersectPackNode
      // from node0 for early return.
    } else if (GetIntersectPackNodes(node1) ||
               analyzer_->GetIntersectPackNodes(node1)) {
      is_intersected = true;
    }

    if (is_intersected) {
      TRACE("Partial overlap at #%d,%s!\n", node1.id(),
            GetSimdOpcodeName(op1).c_str());
    }
  }

  if (is_intersected) {
    if (recursion_depth < 1) {
      TRACE("Do not intersect at root #%d:%s\n", node0.id(),
            GetSimdOpcodeName(graph_.Get(node0)).c_str());
      return nullptr;
    }

    TRACE("Create IntersectPackNode due to partial overlap!\n");
    PackNode* pnode = NewIntersectPackNode(node_group);
    return pnode;
  }

  size_t value_in_count = op0.input_count;

  switch (op0.opcode) {
    case Opcode::kSimd128Constant: {
      PackNode* p = NewPackNode(node_group);
      return p;
    }

    case Opcode::kSimd128LoadTransform: {
      const Simd128LoadTransformOp& transform_op0 =
          op0.Cast<Simd128LoadTransformOp>();
      const Simd128LoadTransformOp& transform_op1 =
          op1.Cast<Simd128LoadTransformOp>();
      StoreLoadInfo<Simd128LoadTransformOp> info0(&graph_, &transform_op0);
      StoreLoadInfo<Simd128LoadTransformOp> info1(&graph_, &transform_op1);
      if (!info0.is_valid() || !info1.is_valid()) {
        return NewForcePackNode(node_group, ForcePackNode::kGeneral,
                                recursion_depth);
      }
      std::optional<OffsetDiff> stride = info1 - info0;
      switch (transform_op0.transform_kind) {
        case Simd128LoadTransformOp::TransformKind::k8x8S:
        case Simd128LoadTransformOp::TransformKind::k8x8U:
        case Simd128LoadTransformOp::TransformKind::k16x4S:
        case Simd128LoadTransformOp::TransformKind::k16x4U:
        case Simd128LoadTransformOp::TransformKind::k32x2S:
        case Simd128LoadTransformOp::TransformKind::k32x2U: {
          TRACE("Simd128LoadTransform: LoadExtend\n");
          if (stride.has_value()) {
            const OffsetDiff value = stride.value();
            if (value == kSimd128Size / 2) {
              return NewPackNode(node_group);
            } else if (value == 0) {
              return NewForcePackNode(node_group, ForcePackNode::kSplat,
                                      recursion_depth);
            }
          }
          return NewForcePackNode(node_group, ForcePackNode::kGeneral,
                                  recursion_depth);
        }
        case Simd128LoadTransformOp::TransformKind::k8Splat:
        case Simd128LoadTransformOp::TransformKind::k16Splat:
        case Simd128LoadTransformOp::TransformKind::k32Splat:
        case Simd128LoadTransformOp::TransformKind::k64Splat: {
          TRACE("Simd128LoadTransform: LoadSplat\n");
          if (IsSplat(node_group) ||
              (stride.has_value() && stride.value() == 0)) {
            return NewPackNode(node_group);
          }
          return NewForcePackNode(node_group, ForcePackNode::kGeneral,
                                  recursion_depth);
        }
        case Simd128LoadTransformOp::TransformKind::k32Zero:
        case Simd128LoadTransformOp::TransformKind::k64Zero: {
          TRACE("Simd128LoadTransform: k64Zero/k32Zero\n");
          if (stride.has_value() && stride.value() == 0) {
            return NewForcePackNode(node_group, ForcePackNode::kSplat,
                                    recursion_depth);
          }
          return NewForcePackNode(node_group, ForcePackNode::kGeneral,
                                  recursion_depth);
        }
      }
      UNREACHABLE();  // Exhaustive switch.
    }

    case Opcode::kLoad: {
      TRACE("Load leaf node\n");
      const LoadOp& load0 = op0.Cast<LoadOp>();
      const LoadOp& load1 = op1.Cast<LoadOp>();
      if (load0.loaded_rep != MemoryRepresentation::Simd128() ||
          load1.loaded_rep != MemoryRepresentation::Simd128()) {
        TRACE("Failed due to non-simd load representation!\n");
        return nullptr;
      }
      StoreLoadInfo<LoadOp> info0(&graph_, &load0);
      StoreLoadInfo<LoadOp> info1(&graph_, &load1);
      if (info0.is_valid() && info1.is_valid()) {
        std::optional<OffsetDiff> stride = info1 - info0;
        if (stride.has_value()) {
          const OffsetDiff value = stride.value();
          if (value == kSimd128Size) {
            // TODO(jiepan): Sort load. We can support {value == -kSimd128Size}
            // by swapping the two ops in {node_group}.
            return NewPackNode(node_group);
          } else if (value == 0) {
            return NewForcePackNode(node_group, ForcePackNode::kSplat,
                                    recursion_depth);
          }
        }
      }
      return NewForcePackNode(node_group, ForcePackNode::kGeneral,
                              recursion_depth);
    }

    case Opcode::kStore: {
      TRACE("Added a vector of stores.\n");
      // input: base, value, [index]
      PackNode* pnode =
          NewPackNodeAndRecurse(node_group, 1, 1, recursion_depth);
      return pnode;
    }

    case Opcode::kPhi: {
      TRACE("Added a vector of phi nodes.\n");
      const PhiOp& phi = op0.Cast<PhiOp>();
      if (phi.rep != RegisterRepresentation::Simd128() ||
          op0.input_count != op1.input_count) {
        TRACE("Failed due to invalid phi\n");
        return nullptr;
      }
      PackNode* pnode =
          NewPackNodeAndRecurse(node_group, 0, value_in_count, recursion_depth);
      return pnode;
    }

    case Opcode::kSimd128Unary: {
#define UNARY_CASE(op_128, not_used) case Simd128UnaryOp::Kind::k##op_128:

#define UNARY_SIGN_EXTENSION_CASE(op_low, not_used1, op_high)                 \
  case Simd128UnaryOp::Kind::k##op_low: {                                     \
    if (const Simd128UnaryOp* unop1 =                                         \
            op1.TryCast<Opmask::kSimd128##op_high>();                         \
        unop1 && op0.Cast<Simd128UnaryOp>().input() == unop1->input()) {      \
      return NewPackNode(node_group);                                         \
    }                                                                         \
    [[fallthrough]];                                                          \
  }                                                                           \
  case Simd128UnaryOp::Kind::k##op_high: {                                    \
    if (op1.Cast<Simd128UnaryOp>().kind == op0.Cast<Simd128UnaryOp>().kind) { \
      auto force_pack_type =                                                  \
          node0 == node1 ? ForcePackNode::kSplat : ForcePackNode::kGeneral;   \
      return NewForcePackNode(node_group, force_pack_type, recursion_depth);  \
    } else {                                                                  \
      return nullptr;                                                         \
    }                                                                         \
  }
      switch (op0.Cast<Simd128UnaryOp>().kind) {
        SIMD256_UNARY_SIGN_EXTENSION_OP(UNARY_SIGN_EXTENSION_CASE)
        SIMD256_UNARY_SIMPLE_OP(UNARY_CASE) {
          TRACE("Added a vector of Unary\n");
          PackNode* pnode = NewPackNodeAndRecurse(node_group, 0, value_in_count,
                                                  recursion_depth);
          return pnode;
        }
        default: {
          TRACE("Unsupported Simd128Unary: %s\n",
                GetSimdOpcodeName(op0).c_str());
          return nullptr;
        }
      }
#undef UNARY_CASE
#undef UNARY_SIGN_EXTENSION_CASE
    }

    case Opcode::kSimd128Binop: {
      const Simd128BinopOp& binop0 = op0.Cast<Simd128BinopOp>();
      switch (binop0.kind) {
#define BINOP_SIGN_EXTENSION_CASE(op_low, _, op_high)                        \
  case Simd128BinopOp::Kind::k##op_low: {                                    \
    if (const Simd128BinopOp* binop1 =                                       \
            op1.TryCast<Opmask::kSimd128##op_high>();                        \
        binop1 && binop0.left() == binop1->left() &&                         \
        binop0.right() == binop1->right()) {                                 \
      return NewPackNode(node_group);                                        \
    }                                                                        \
    [[fallthrough]];                                                         \
  }                                                                          \
  case Simd128BinopOp::Kind::k##op_high: {                                   \
    if (op1.Cast<Simd128BinopOp>().kind == binop0.kind) {                    \
      auto force_pack_type =                                                 \
          node0 == node1 ? ForcePackNode::kSplat : ForcePackNode::kGeneral;  \
      return NewForcePackNode(node_group, force_pack_type, recursion_depth); \
    }                                                                        \
    return nullptr;                                                          \
  }
        SIMD256_BINOP_SIGN_EXTENSION_OP(BINOP_SIGN_EXTENSION_CASE)
#undef BINOP_SIGN_EXTENSION_CASE

#define BINOP_CASE(op_128, _) case Simd128BinopOp::Kind::k##op_128:

        SIMD256_BINOP_SIMPLE_OP(BINOP_CASE) {
          TRACE("Added a vector of Binop\n");
          PackNode* pnode =
              NewCommutativePackNodeAndRecurse(node_group, recursion_depth);
          return pnode;
        }
#undef BINOP_CASE

        default: {
          TRACE("Unsupported Simd128Binop: %s\n",
                GetSimdOpcodeName(op0).c_str());
          return nullptr;
        }
      }
    }

    case Opcode::kSimd128Shift: {
      Simd128ShiftOp& shift_op0 = op0.Cast<Simd128ShiftOp>();
      Simd128ShiftOp& shift_op1 = op1.Cast<Simd128ShiftOp>();
      if (IsEqual(shift_op0.shift(), shift_op1.shift())) {
        switch (shift_op0.kind) {
#define SHIFT_CASE(op_128, _) case Simd128ShiftOp::Kind::k##op_128:
          SIMD256_SHIFT_OP(SHIFT_CASE) {
            TRACE("Added a vector of Shift op.\n");
            // We've already checked that the "shift by" input of both shifts is
            // the same, and we'll only pack the 1st input of the shifts
            // together anyways (since on both Simd128 and Simd256, the "shift
            // by" input of shifts is a Word32). Thus we only need to check the
            // 1st input of the shift when recursing.
            constexpr int kShiftValueInCount = 1;
            PackNode* pnode = NewPackNodeAndRecurse(
                node_group, 0, kShiftValueInCount, recursion_depth);
            return pnode;
          }
#undef SHIFT_CASE
          default: {
            TRACE("Unsupported Simd128ShiftOp: %s\n",
                  GetSimdOpcodeName(op0).c_str());
            return nullptr;
          }
        }
      }
      TRACE("Failed due to different shift amounts!\n");
      return nullptr;
    }
    case Opcode::kSimd128Ternary: {
#define TERNARY_CASE(op_128, _) case Simd128TernaryOp::Kind::k##op_128:
      switch (op0.Cast<Simd128TernaryOp>().kind) {
        SIMD256_TERNARY_OP(TERNARY_CASE) {
          TRACE("Added a vector of Ternary\n");
          PackNode* pnode = NewPackNodeAndRecurse(node_group, 0, value_in_count,
                                                  recursion_depth);
          return pnode;
        }
#undef TERNARY_CASE
        default: {
          TRACE("Unsupported Simd128Ternary: %s\n",
                GetSimdOpcodeName(op0).c_str());
          return nullptr;
        }
      }
    }

    case Opcode::kSimd128Splat: {
      if (!IsEqual(op0.input(0), op1.input(0))) {
        TRACE("Failed due to different splat input!\n");
        return nullptr;
      }
      PackNode* pnode = NewPackNode(node_group);
      return pnode;
    }

    case Opcode::kSimd128Shuffle: {
      const Simd128ShuffleOp& shuffle_op0 = op0.Cast<Simd128ShuffleOp>();
      const Simd128ShuffleOp& shuffle_op1 = op1.Cast<Simd128ShuffleOp>();
      if (shuffle_op0.kind != Simd128ShuffleOp::Kind::kI8x16 ||
          shuffle_op1.kind != Simd128ShuffleOp::Kind::kI8x16) {
        return nullptr;
      }
      // We pack shuffles only if it can match specific patterns. We should
      // avoid packing general shuffles because it will cause regression.
      const auto& shuffle0 = shuffle_op0.shuffle;
      const auto& shuffle1 = shuffle_op1.shuffle;

      if (CompareCharsEqual(shuffle0, shuffle1, kSimd128Size)) {
        if (IsSplat(node_group)) {
          // Check if the shuffle can be replaced by a loadsplat.
          // Take load32splat as an example:
          // 1. Param0  # be used as load base
          // 2. Param1  # be used as load index
          // 3. Param2  # be used as store base
          // 4. Param3  # be used as store index
          // 5. Load128(base, index, offset=0)
          // 6. AnyOp
          // 7. Shuffle32x4 (1,2, [2,2,2,2])
          // 8. Store128(3,4,7, offset=0)
          // 9. Store128(3,4,7, offset=16)
          //
          // We can replace the load128 and shuffle with a loadsplat32:
          // 1. Param0  # be used as load base
          // 2. Param1  # be used as load index
          // 3. Param2  # be used as store base
          // 4. Param3  # be used as store index
          // 5. Load32Splat256(base, index, offset=4)
          // 6. Store256(3,4,7,offset=0)
          int index;
          if (wasm::SimdShuffle::TryMatchSplat<4>(shuffle0, &index) &&
              graph_.Get(op0.input(index >> 2)).opcode == Opcode::kLoad) {
            ShufflePackNode* pnode = NewShufflePackNode(
                node_group,
                ShufflePackNode::SpecificInfo::Kind::kS256Load32Transform);
            pnode->info().set_splat_index(index);
            return pnode;
          } else if (wasm::SimdShuffle::TryMatchSplat<2>(shuffle0, &index) &&
                     graph_.Get(op0.input(index >> 1)).opcode ==
                         Opcode::kLoad) {
            ShufflePackNode* pnode = NewShufflePackNode(
                node_group,
                ShufflePackNode::SpecificInfo::Kind::kS256Load64Transform);
            pnode->info().set_splat_index(index);
            return pnode;
          }
        }

#ifdef V8_TARGET_ARCH_X64
        if (ShufflePackNode* pnode =
                X64TryMatch256Shuffle(node_group, shuffle0, shuffle1)) {
          for (size_t i = 0; i < value_in_count; ++i) {
            NodeGroup operands(graph_.Get(node_group[0]).input(i),
                               graph_.Get(node_group[1]).input(i));
            PackNode* child = BuildTreeRec(operands, recursion_depth + 1);
            if (!child) {
              return nullptr;
            }
            pnode->SetOperand(i, child);
          }
          return pnode;
        }
#endif  // V8_TARGET_ARCH_X64

        TRACE("Unsupported Simd128Shuffle\n");
        return nullptr;
      } else {
        return Try256ShuffleMatchLoad8x8U(node_group, shuffle0, shuffle1);
      }
    }

    case Opcode::kSimd128ReplaceLane: {
      ExtendIntToF32x4Info info;
      if (TryMatchExtendIntToF32x4(node_group, &info)) {
        TRACE("Match extend i8x4/i16x4 to f32x4\n");
        PackNode* p = NewBundlePackNode(
            node_group, info.extend_from, info.start_lane, info.lane_size,
            info.is_sign_extract, info.is_sign_convert);
        return p;
      }
      return NewForcePackNode(
          node_group,
          node0 == node1 ? ForcePackNode::kSplat : ForcePackNode::kGeneral,
          recursion_depth);
    }

    default:
      TRACE("Default branch #%d:%s\n", node0.id(),
            GetSimdOpcodeName(op0).c_str());
      break;
  }
  return nullptr;
}

void WasmRevecAnalyzer::MergeSLPTree(SLPTree& slp_tree) {
  // We ensured the SLP trees are mergable when BuildTreeRec.
  for (const auto& entry : slp_tree.GetIntersectNodeMapping()) {
    auto it = revectorizable_intersect_node_.find(entry.first);
    if (it == revectorizable_intersect_node_.end()) {
      bool result;
      std::tie(it, result) = revectorizable_intersect_node_.emplace(
          entry.first, ZoneVector<PackNode*>(phase_zone_));
      DCHECK(result);
    }
    ZoneVector<PackNode*>& intersect_pnodes = it->second;
    intersect_pnodes.insert(intersect_pnodes.end(), entry.second.begin(),
                            entry.second.end());
    SLOW_DCHECK(std::unique(intersect_pnodes.begin(), intersect_pnodes.end()) ==
                intersect_pnodes.end());
  }

  revectorizable_node_.merge(slp_tree.GetNodeMapping());
  reorder_inputs_.merge(slp_tree.GetReorderInputs());
}

bool WasmRevecAnalyzer::IsSupportedReduceSeed(const Operation& op) {
  if (!op.Is<Simd128BinopOp>()) {
    return false;
  }
  switch (op.Cast<Simd128BinopOp>().kind) {
#define CASE(op_128) case Simd128BinopOp::Kind::k##op_128:
    REDUCE_SEED_KIND(CASE) { return true; }
#undef CASE
    default:
      return false;
  }
}

void WasmRevecAnalyzer::ProcessBlock(const Block& block) {
  StoreInfoSet simd128_stores(phase_zone_);
  for (const Operation& op : base::Reversed(graph_.operations(block))) {
    if (const StoreOp* store_op = op.TryCast<StoreOp>()) {
      if (store_op->stored_rep == MemoryRepresentation::Simd128()) {
        StoreLoadInfo<StoreOp> info(&graph_, store_op);
        if (info.is_valid()) {
          simd128_stores.insert(info);
        }
      }
    }
    // Try to find reduce op which can be used as revec seeds.
    if (IsSupportedReduceSeed(op)) {
      const Simd128BinopOp& binop = op.Cast<Simd128BinopOp>();
      V<Simd128> left_index = binop.left();
      V<Simd128> right_index = binop.right();
      const Operation& left_op = graph_.Get(left_index);
      const Operation& right_op = graph_.Get(right_index);

      if (left_index != right_index && left_op.opcode == right_op.opcode &&
          IsSameSimd128OpKind(left_op, right_op)) {
        reduce_seeds_.push_back({left_index, right_index});
      }
    }
  }

  if (simd128_stores.size() >= 2) {
    for (auto it = std::next(simd128_stores.cbegin()),
              end = simd128_stores.cend();
         it != end;) {
      const StoreLoadInfo<StoreOp>& info0 = *std::prev(it);
      const StoreLoadInfo<StoreOp>& info1 = *it;
      std::optional<OffsetDiff> diff = info1 - info0;

      if (diff.has_value()) {
        const OffsetDiff value = diff.value();
        DCHECK_EQ(value.negative(), false);
        if (value == kSimd128Size) {
          store_seeds_.push_back(
              {graph_.Index(*info0.op()), graph_.Index(*info1.op())});
          if (std::distance(it, end) < 2) {
            break;
          }
          std::advance(it, 2);
          continue;
        }
      }
      it++;
    }
  }
}

void WasmRevecAnalyzer::Run() {
  for (Block& block : base::Reversed(graph_.blocks())) {
    ProcessBlock(block);
  }

  if (store_seeds_.empty() && reduce_seeds_.empty()) {
    TRACE("Empty seed\n");
    return;
  }

  if (v8_flags.trace_wasm_revectorize) {
    PrintF("store seeds:\n");
    for (auto pair : store_seeds_) {
      PrintF("{\n");
      PrintF("#%u ", pair.first.id());
      graph_.Get(pair.first).Print();
      PrintF("#%u ", pair.second.id());
      graph_.Get(pair.second).Print();
      PrintF("}\n");
    }

    PrintF("reduce seeds:\n");
    for (auto pair : reduce_seeds_) {
      PrintF("{ ");
      PrintF("#%u, ", pair.first.id());
      PrintF("#%u ", pair.second.id());
      PrintF("}\n");
    }
  }

  ZoneVector<std::pair<OpIndex, OpIndex>> all_seeds(
      store_seeds_.begin(), store_seeds_.end(), phase_zone_);
  all_seeds.insert(all_seeds.end(), reduce_seeds_.begin(), reduce_seeds_.end());

  for (auto pair : all_seeds) {
    NodeGroup roots(pair.first, pair.second);
    SLPTree slp_tree(graph_, this, phase_zone_);
    PackNode* root = slp_tree.BuildTree(roots);
    if (!root) {
      TRACE("Building SLP tree failed!\n");
      continue;
    }

    slp_tree.Print("After tree building");
    MergeSLPTree(slp_tree);
  }

  if (revectorizable_node_.empty()) return;

  use_map_ = phase_zone_->New<Simd128UseMap>(graph_, phase_zone_);
  if (DecideVectorize()) {
    should_reduce_ = true;
    Print("Decided to vectorize");
  }
}

bool WasmRevecAnalyzer::DecideVectorize() {
  TRACE("Entering\n");
  int save = 0, cost = 0;
  ForEach(
      [&](PackNode const* pnode) {
        const NodeGroup& nodes = pnode->nodes();
        // An additional store is emitted in case of OOB trap at the higher
        // 128-bit address. Thus we don't save anything if the store at lower
        // address is executed first. Return directly as we don't need to check
        // external uses for stores.
        if (graph_.Get(nodes[0]).opcode == Opcode::kStore) {
          if (nodes[0] > nodes[1]) save++;
          return;
        }

        if (pnode->IsForcePackNode()) {
          cost++;
          return;
        }

        // Splat nodes will not cause a saving as it simply extends itself.
        if (!IsSplat(nodes)) {
          save++;
        }

#ifdef V8_TARGET_ARCH_X64
        // On x64 platform, we don't emit extract for lane 0 as the source ymm
        // register is an alias for the corresponding xmm register in its lower
        // 128 bits.
        size_t start = 1;
#else
        size_t start = 0;
#endif  // V8_TARGET_ARCH_X64
        for (size_t i = start; i < nodes.size(); i++) {
          if (i > 0 && nodes[i] == nodes[0]) continue;
          for (OpIndex use : use_map_->uses(nodes[i])) {
            if (!GetPackNode(use) || GetPackNode(use)->is_force_packing()) {
              TRACE("External use edge: (%d:%s) -> (%d:%s)\n", use.id(),
                    OpcodeName(graph_.Get(use).opcode), nodes[i].id(),
                    OpcodeName(graph_.Get(nodes[i]).opcode));
              ++cost;

              // We only need one Extract node and all other uses can share.
              break;
            }
          }
        }
      },
      revectorizable_node_);

  // Note: {cost += revectorizable_intersect_node_.size()} would have
  // different behavior, because the map contains vectors, and {ForEach}
  // iterates over those vectors' elements.
  ForEach(
      [&](PackNode const* pnode) {
        // We always generate SimdPack128To256Op for IntersectPackNode.
        cost++;
        return;
      },
      revectorizable_intersect_node_);

  TRACE("Save: %d, cost: %d\n", save, cost);
  return save > cost;
}

void WasmRevecAnalyzer::Print(const char* info) {
  if (!v8_flags.trace_wasm_revectorize) {
    return;
  }

  TRACE("%s, %zu revectorizable nodes:\n", info, revectorizable_node_.size());
  ForEach([this](PackNode const* pnode) { pnode->Print(&graph_); },
          revectorizable_node_);
  TRACE("%s, %zu revectorizable intersect nodes:\n", info,
        revectorizable_intersect_node_.size());
  ForEach([this](PackNode const* pnode) { pnode->Print(&graph_); },
          revectorizable_intersect_node_);
}

#undef TRACE

}  // namespace v8::internal::compiler::turboshaft
