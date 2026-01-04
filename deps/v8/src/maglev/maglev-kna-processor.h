// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_KNA_PROCESSOR_H_
#define V8_MAGLEV_MAGLEV_KNA_PROCESSOR_H_

#include <type_traits>

#include "src/base/base-export.h"
#include "src/base/logging.h"
#include "src/codegen/bailout-reason.h"
#include "src/compiler/js-heap-broker.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-known-node-aspects.h"

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
      : graph_(graph),
        known_node_aspects_(nullptr),
        reachable_exception_handlers_(zone()) {}

  void PreProcessGraph(Graph* graph) {
    known_node_aspects_ = zone()->New<KnownNodeAspects>(zone());
    for (BasicBlock* block : graph->blocks()) {
      if (block->has_state()) {
        block->state()->ClearKnownNodeAspects();
      }
    }
  }
  void PostProcessGraph(Graph* graph) {}
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    // TODO(victorgomes): Support removing the unreachable blocks instead of
    // just skipping it.
    if (V8_UNLIKELY(block->IsUnreachable())) {
      // Ensure successors can also be unreachable.
      return AbortBlock(block);
    }

    if (block->is_exception_handler_block()) {
      if (!reachable_exception_handlers_.contains(block)) {
        // This is an unreachable exception handler block.
        // Ensure successors can also be unreachable.
        return AbortBlock(block);
      }
    }

    if (block->is_loop() && block->state()->is_resumable_loop()) {
      // TODO(victorgomes): Ideally, we should use the loop backedge KNA cache
      // for all loops.
      known_node_aspects_ = zone()->New<KnownNodeAspects>(zone());
    } else if (block->is_loop()) {
      known_node_aspects_ =
          block->state()->TakeKnownNodeAspects()->CloneForLoopHeader(
              false, nullptr, zone());
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
        BasicBlock* exception_handler =
            node->exception_handler_info()->catch_block();
        reachable_exception_handlers_.insert(exception_handler);
        Merge(exception_handler);
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
  ZoneAbslFlatHashSet<BasicBlock*> reachable_exception_handlers_;

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

  BlockProcessResult AbortBlock(BasicBlock* block) {
    ControlNode* control = block->reset_control_node();
    block->RemovePredecessorFollowing(control);
    control->OverwriteWith<Abort>()->set_reason(AbortReason::kUnreachable);
    block->set_deferred(true);
    block->set_control_node(control);
    block->mark_dead();
    graph_->set_may_have_unreachable_blocks();
    return BlockProcessResult::kSkip;
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
    known_node_aspects().MarkPossibleSideEffect(node, broker(),
                                                graph_->is_tracing_enabled());
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
  PROCESS_SAFE_CONV(CheckedSmiTagFloat64, tagged, Smi)
  PROCESS_SAFE_CONV(TruncateCheckedNumberOrOddballToInt32,
                    truncated_int32_to_number, NumberOrOddball)
  PROCESS_UNSAFE_CONV(TruncateUnsafeNumberOrOddballToInt32,
                      truncated_int32_to_number, NumberOrOddball)
  PROCESS_SAFE_CONV(CheckedUint32ToInt32, int32, Number)
  PROCESS_SAFE_CONV(CheckedIntPtrToInt32, int32, Number)
  PROCESS_SAFE_CONV(CheckedFloat64ToInt32, int32, Number)
  PROCESS_SAFE_CONV(CheckedHoleyFloat64ToInt32, int32, Number)
  PROCESS_UNSAFE_CONV(UnsafeFloat64ToInt32, int32, Number)
  PROCESS_UNSAFE_CONV(UnsafeHoleyFloat64ToInt32, int32, Number)
  PROCESS_SAFE_CONV(CheckedNumberToInt32, int32, Number)
  PROCESS_UNSAFE_CONV(ChangeIntPtrToFloat64, float64, Number)
  // TODO(victorgomes): pass node->conversion_type() rather than always
  // NumberOrOddball for CheckedNumberOrOddballToFloat64.
  PROCESS_SAFE_CONV(CheckedNumberOrOddballToFloat64, float64, NumberOrOddball)
  PROCESS_SAFE_CONV(CheckedNumberToFloat64, float64, Number)
  PROCESS_UNSAFE_CONV(UnsafeNumberOrOddballToFloat64, float64, NumberOrOddball)
  PROCESS_UNSAFE_CONV(UnsafeNumberToFloat64, float64, Number)
  PROCESS_SAFE_CONV(CheckedHoleyFloat64ToFloat64, float64, Number)
  PROCESS_UNSAFE_CONV(HoleyFloat64ToSilencedFloat64, float64, Number)
  PROCESS_SAFE_CONV(ChangeInt32ToFloat64, float64, Number)
  PROCESS_SAFE_CONV(ChangeInt32ToHoleyFloat64, holey_float64, Number)
#undef PROCESS_SAFE_CONV
#undef PROCESS_UNSAFE_CONV

  ProcessResult ProcessNode(LoadTaggedField* node) {
    if (!node->property_key().is_none()) {
      auto& props_for_key = known_node_aspects().GetLoadedPropertiesForKey(
          zone(), node->is_const(), node->property_key());
      props_for_key[node->ValueInput().node()] = node;
    }
    return ProcessResult::kContinue;
  }

  ProcessResult ProcessNode(LoadDataViewByteLength* node) {
    auto& props_for_key = known_node_aspects().GetLoadedPropertiesForKey(
        zone(), true, PropertyKey::ArrayBufferViewByteLength());
    props_for_key[node->ValueInput().node()] = node;
    return ProcessResult::kContinue;
  }

  void ProcessStoreContextSlot(ValueNode* context, ValueNode* value,
                               int offset) {
    known_node_aspects().ClearAliasedContextSlotsFor(graph_, context, offset,
                                                     value);
    known_node_aspects().SetContextCachedValue(context, offset, value);
  }

  template <typename NodeT>
  void ProcessStoreTaggedField(NodeT* node) {
    // If a store to a context, we use the specialized context slot cache.
    if (node->is_store_to_context()) {
      return ProcessStoreContextSlot(node->ObjectInput().node(),
                                     node->ValueInput().node(), node->offset());
    }
    // ... otherwise we try the properties cache.
    if (node->property_key().is_none()) return;
    auto& props_for_key = known_node_aspects().GetLoadedPropertiesForKey(
        zone(), false, node->property_key());
    // We don't do any aliasing analysis, so stores clobber all other cached
    // loads of a property with that key. We only need to do this for
    // non-constant properties, since constant properties are known not to
    // change and therefore can't be clobbered.
    // TODO(leszeks): Do some light aliasing analysis here, e.g. checking
    // whether there's an intersection of known maps.
    props_for_key.clear();
    props_for_key[node->ObjectInput().node()] = node->ValueInput().node();
  }

  ProcessResult ProcessNode(StoreTaggedFieldNoWriteBarrier* node) {
    ProcessStoreTaggedField(node);
    return ProcessResult::kContinue;
  }

  ProcessResult ProcessNode(StoreTaggedFieldWithWriteBarrier* node) {
    ProcessStoreTaggedField(node);
    return ProcessResult::kContinue;
  }

  template <typename NodeT>
  void ProcessLoadContextSlot(NodeT* node) {
    ValueNode* context = node->input_node(0);
    ValueNode*& cached_value = known_node_aspects().GetContextCachedValue(
        context, node->offset(),
        node->is_const() ? ContextSlotMutability::kImmutable
                         : ContextSlotMutability::kMutable);
    if (!cached_value) cached_value = node;
    if (!node->is_const()) {
      known_node_aspects().UpdateMayHaveAliasingContexts(
          broker(), broker()->local_isolate(), context);
    }
  }

  ProcessResult ProcessNode(LoadContextSlot* node) {
    ProcessLoadContextSlot(node);
    return ProcessResult::kContinue;
  }

  ProcessResult ProcessNode(LoadContextSlotNoCells* node) {
    ProcessLoadContextSlot(node);
    return ProcessResult::kContinue;
  }

  ProcessResult ProcessNode(StoreContextSlotWithWriteBarrier* node) {
    ProcessStoreContextSlot(node->ContextInput().node(),
                            node->NewValueInput().node(), node->offset());
    return ProcessResult::kContinue;
  }

  ProcessResult ProcessNode(StoreSmiContextCell* node) {
    ProcessStoreContextSlot(graph_->GetConstant(node->context()),
                            node->ValueInput().node(), node->slot_offset());
    return ProcessResult::kContinue;
  }

  ProcessResult ProcessNode(StoreInt32ContextCell* node) {
    ProcessStoreContextSlot(graph_->GetConstant(node->context()),
                            node->ValueInput().node(), node->slot_offset());
    return ProcessResult::kContinue;
  }

  ProcessResult ProcessNode(StoreFloat64ContextCell* node) {
    ProcessStoreContextSlot(graph_->GetConstant(node->context()),
                            node->ValueInput().node(), node->slot_offset());
    return ProcessResult::kContinue;
  }

  ProcessResult ProcessNode(Node* node) { return ProcessResult::kContinue; }
};

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_KNA_PROCESSOR_H_
