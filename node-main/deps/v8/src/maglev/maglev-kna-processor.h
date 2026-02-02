// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_KNA_PROCESSOR_H_
#define V8_MAGLEV_MAGLEV_KNA_PROCESSOR_H_

#include <type_traits>

#include "src/base/base-export.h"
#include "src/base/logging.h"
#include "src/compiler/js-heap-broker.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

template <typename T>
concept IsNodeT = std::is_base_of_v<Node, T>;

// Recomputes the Known Node Aspects (KNA) for the entire graph. KNA tracks
// information about nodes that can be used for optimizations, such as
// eliminating redundant checks or loads.
//
// It performs a forward data-flow analysis over the graph. Starting with
// empty KNA, it iterates through nodes in each basic block. When it
// encounters a node with potential side effects (e.g., writing to an array or
// field), it updates the KNA to reflect that some previously known information
// may no longer be valid. This updated information is then merged into
// successor basic blocks.
class RecomputeKnownNodeAspectsProcessor {
 public:
  explicit RecomputeKnownNodeAspectsProcessor(Graph* graph)
      : graph_(graph), known_node_aspects_(nullptr) {}

  void PreProcessGraph(Graph* graph) {
    known_node_aspects_ = zone()->New<KnownNodeAspects>(zone());
    for (BasicBlock* block : graph->blocks()) {
      if (block->has_state()) {
        block->state()->ClearKnownNodeAspects();
      }
      if (block->is_loop() && block->state()->IsUnreachableByForwardEdge()) {
        DCHECK(block->state()->is_resumable_loop());
        block->state()->MergeNodeAspects(zone(), *known_node_aspects_);
      }
    }
  }
  void PostProcessGraph(Graph* graph) {}
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    if (V8_UNLIKELY(block->IsUnreachable())) {
      // The block is unreachable, we probably never set the KNA to this
      // block. Just use an empty one.
      // TODO(victorgomes): Maybe we shouldn't visit unreachable blocks.
      known_node_aspects_ = zone()->New<KnownNodeAspects>(zone());
    } else if (block->has_state()) {
      known_node_aspects_ = block->state()->TakeKnownNodeAspects();
    } else if (block->is_edge_split_block()) {
      // Clone the next available KNA.
      BasicBlock* next_block = block;
      while (next_block->is_edge_split_block()) {
        next_block = next_block->control_node()->Cast<Jump>()->target();
      }
      known_node_aspects_ = next_block->state()->CloneKnownNodeAspects(zone());
    }
    DCHECK_NOT_NULL(known_node_aspects_);
    return BlockProcessResult::kContinue;
  }
  void PostProcessBasicBlock(BasicBlock* block) {}
  void PostPhiProcessing() {}

  template <IsNodeT NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState& state) {
    if constexpr (NodeT::kProperties.can_throw()) {
      ExceptionHandlerInfo* info = node->exception_handler_info();
      if (info->HasExceptionHandler() && !info->ShouldLazyDeopt()) {
        Merge(node->exception_handler_info()->catch_block());
      }
    }
    MarkPossibleSideEffect(node);
    return ProcessNode(node);
  }

  ProcessResult Process(Switch* node, const ProcessingState& state) {
    for (int i = 0; i < node->size(); i++) {
      Merge(node->targets()[i].block_ptr());
    }
    if (node->has_fallthrough()) {
      Merge(node->fallthrough());
    }
    return ProcessResult::kContinue;
  }

  ProcessResult Process(BranchControlNode* node, const ProcessingState& state) {
    Merge(node->if_true());
    Merge(node->if_false());
    return ProcessResult::kContinue;
  }

  ProcessResult Process(Jump* node, const ProcessingState& state) {
    Merge(node->target());
    return ProcessResult::kContinue;
  }

  ProcessResult Process(CheckpointedJump* node, const ProcessingState& state) {
    Merge(node->target());
    return ProcessResult::kContinue;
  }

  ProcessResult Process(JumpLoop* node, const ProcessingState& state) {
#ifdef DEBUG
    known_node_aspects_ = nullptr;
#endif  // DEBUG
    return ProcessResult::kContinue;
  }

  ProcessResult Process(TerminalControlNode* node,
                        const ProcessingState& state) {
#ifdef DEBUG
    known_node_aspects_ = nullptr;
#endif  // DEBUG
    return ProcessResult::kContinue;
  }

  ProcessResult Process(ControlNode* node, const ProcessingState& state) {
    UNREACHABLE();
  }

  KnownNodeAspects& known_node_aspects() {
    DCHECK_NOT_NULL(known_node_aspects_);
    return *known_node_aspects_;
  }

 private:
  Graph* graph_;
  KnownNodeAspects* known_node_aspects_;

  Zone* zone() { return graph_->zone(); }
  compiler::JSHeapBroker* broker() { return graph_->broker(); }

  NodeInfo* GetOrCreateInfoFor(ValueNode* node) {
    return known_node_aspects().GetOrCreateInfoFor(broker(), node);
  }
  bool EnsureType(ValueNode* node, NodeType type) {
    return known_node_aspects().EnsureType(broker(), node, type);
  }
  NodeType GetType(ValueNode* node) {
    return known_node_aspects().GetType(broker(), node);
  }

  void Merge(BasicBlock* block) {
    while (block->is_edge_split_block()) {
      block = block->control_node()->Cast<Jump>()->target();
    }
    // If we don't have state, this must be a fallthrough basic block.
    if (!block->has_state()) return;
    block->state()->MergeNodeAspects(zone(), *known_node_aspects_);
  }

  template <typename NodeT>
  void MarkPossibleSideEffect(NodeT* node) {
    // Don't do anything for nodes without side effects.
    if constexpr (!NodeT::kProperties.can_write()) return;

    if constexpr (IsElementsArrayWrite(Node::opcode_of<NodeT>)) {
      node->ClearElementsProperties(graph_->is_tracing_enabled(),
                                    known_node_aspects());
    } else if constexpr (!IsSimpleFieldStore(Node::opcode_of<NodeT>) &&
                         !IsTypedArrayStore(Node::opcode_of<NodeT>)) {
      // Don't change known node aspects for simple field stores. The only
      // relevant side effect on these is writes to objects which invalidate
      // loaded properties and context slots, and we invalidate these already as
      // part of emitting the store.
      node->ClearUnstableNodeAspects(graph_->is_tracing_enabled(),
                                     known_node_aspects());
    }
  }

#define PROCESS_CHECK(Type)                             \
  ProcessResult ProcessNode(Check##Type* node) {        \
    EnsureType(node->input_node(0), NodeType::k##Type); \
    return ProcessResult::kContinue;                    \
  }
  PROCESS_CHECK(Smi)
  PROCESS_CHECK(Number)
  PROCESS_CHECK(String)
  PROCESS_CHECK(SeqOneByteString)
  PROCESS_CHECK(StringOrStringWrapper)
  PROCESS_CHECK(StringOrOddball)
  PROCESS_CHECK(Symbol)
#undef PROCESS_CHECK

#define PROCESS_SAFE_CONV(Node, Alt, Type)                                     \
  ProcessResult ProcessNode(Node* node) {                                      \
    NodeInfo* info = GetOrCreateInfoFor(node->input_node(0));                  \
    if (!info->alternative().Alt()) {                                          \
      /* TODO(victorgomes): What happens if we we have an alternative already? \
       * Should we remove this one as well? */                                 \
      info->alternative().set_##Alt(node);                                     \
    }                                                                          \
    info->IntersectType(NodeType::k##Type);                                    \
    return ProcessResult::kContinue;                                           \
  }
// TODO(victorgomes): Ideally we would like to check we already know the type,
// but currently we cannot. The issue is that if the GraphBuilder emits a
// node A and then Ensure(A, kSmi), we are not able to recover that A is an Smi.
// This happens for instance for LoadProperty.
#define PROCESS_UNSAFE_CONV(Node, Alt, Type)                                   \
  ProcessResult ProcessNode(Node* node) {                                      \
    NodeInfo* info = GetOrCreateInfoFor(node->input_node(0));                  \
    if (!info->alternative().Alt()) {                                          \
      /* TODO(victorgomes): What happens if we we have an alternative already? \
       * Should we remove this one as well? */                                 \
      info->alternative().set_##Alt(node);                                     \
    }                                                                          \
    /* CHECK(NodeTypeIs(GetType(node->input_node(0)), NodeType::k##Type)); */  \
    return ProcessResult::kContinue;                                           \
  }
  PROCESS_SAFE_CONV(CheckedSmiUntag, int32, Smi)
  PROCESS_UNSAFE_CONV(UnsafeSmiUntag, int32, Smi)
  PROCESS_SAFE_CONV(CheckedSmiTagInt32, tagged, Smi)
  PROCESS_UNSAFE_CONV(UnsafeSmiTagInt32, tagged, Smi)
  PROCESS_SAFE_CONV(CheckedSmiTagUint32, tagged, Smi)
  PROCESS_UNSAFE_CONV(UnsafeSmiTagUint32, tagged, Smi)
  PROCESS_SAFE_CONV(CheckedSmiTagIntPtr, tagged, Smi)
  PROCESS_UNSAFE_CONV(UnsafeSmiTagIntPtr, tagged, Smi)
  PROCESS_SAFE_CONV(TruncateCheckedNumberOrOddballToInt32,
                    truncated_int32_to_number, NumberOrOddball)
  PROCESS_UNSAFE_CONV(TruncateUnsafeNumberOrOddballToInt32,
                      truncated_int32_to_number, NumberOrOddball)
  PROCESS_SAFE_CONV(CheckedUint32ToInt32, int32, Number)
  PROCESS_UNSAFE_CONV(UnsafeInt32ToUint32, int32, Number)
  PROCESS_SAFE_CONV(CheckedIntPtrToInt32, int32, Number)
  PROCESS_SAFE_CONV(CheckedHoleyFloat64ToInt32, int32, Number)
  PROCESS_UNSAFE_CONV(UnsafeHoleyFloat64ToInt32, int32, Number)
  PROCESS_SAFE_CONV(CheckedNumberToInt32, int32, Number)
  PROCESS_UNSAFE_CONV(ChangeIntPtrToFloat64, float64, Number)
  PROCESS_SAFE_CONV(CheckedSmiTagFloat64, float64, Smi)
  PROCESS_SAFE_CONV(CheckedNumberOrOddballToFloat64, float64, NumberOrOddball)
  PROCESS_UNSAFE_CONV(UncheckedNumberOrOddballToFloat64, float64,
                      NumberOrOddball)
  PROCESS_SAFE_CONV(CheckedHoleyFloat64ToFloat64, float64, Number)
  PROCESS_UNSAFE_CONV(HoleyFloat64ToMaybeNanFloat64, float64, Number)
#undef PROCESS_SAFE_CONV
#undef PROCESS_UNSAFE_CONV

  ProcessResult ProcessNode(Node* node) { return ProcessResult::kContinue; }
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_KNA_PROCESSOR_H_
