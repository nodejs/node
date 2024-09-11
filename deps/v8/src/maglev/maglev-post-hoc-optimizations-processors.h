// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_POST_HOC_OPTIMIZATIONS_PROCESSORS_H_
#define V8_MAGLEV_MAGLEV_POST_HOC_OPTIMIZATIONS_PROCESSORS_H_

#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph-printer.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-ir.h"

namespace v8::internal::maglev {

template <typename NodeT>
constexpr bool CanBeStoreToNonEscapedObject() {
  return std::is_same_v<NodeT, StoreMap> ||
         std::is_same_v<NodeT, StoreTaggedFieldWithWriteBarrier> ||
         std::is_same_v<NodeT, StoreTaggedFieldNoWriteBarrier> ||
         std::is_same_v<NodeT, StoreFloat64>;
}

class AnyUseMarkingProcessor {
 public:
  void PreProcessGraph(Graph* graph) {}
  void PreProcessBasicBlock(BasicBlock* block) {}

  template <typename NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState& state) {
    if constexpr (IsValueNode(Node::opcode_of<NodeT>) &&
                  (!NodeT::kProperties.is_required_when_unused() ||
                   std::is_same_v<ArgumentsElements, NodeT>)) {
      if (!node->is_used()) {
        if (!node->unused_inputs_were_visited()) {
          DropInputUses(node);
        }
        return ProcessResult::kRemove;
      }
    }

    if constexpr (CanBeStoreToNonEscapedObject<NodeT>()) {
      if (node->input(0).node()->template Is<InlinedAllocation>()) {
        stores_to_allocations_.push_back(node);
      }
    }

    return ProcessResult::kContinue;
  }

  void PostProcessGraph(Graph* graph) {
    RunEscapeAnalysis(graph);
    DropUseOfValueInStoresToCapturedAllocations();
  }

 private:
  std::vector<Node*> stores_to_allocations_;

  void EscapeAllocation(Graph* graph, InlinedAllocation* alloc,
                        Graph::AllocationDependencies& deps) {
    if (alloc->HasBeenAnalysed() && alloc->HasEscaped()) return;
    alloc->SetEscaped();
    for (auto dep : deps) {
      EscapeAllocation(graph, dep, graph->allocations().find(dep)->second);
    }
  }

  void VerifyEscapeAnalysis(Graph* graph) {
#ifdef DEBUG
    for (auto it : graph->allocations()) {
      auto alloc = it.first;
      DCHECK(alloc->HasBeenAnalysed());
      if (alloc->HasEscaped()) {
        for (auto dep : it.second) {
          DCHECK(dep->HasEscaped());
        }
      }
    }
#endif  // DEBUG
  }

  void RunEscapeAnalysis(Graph* graph) {
    for (auto it : graph->allocations()) {
      auto alloc = it.first;
      if (alloc->HasBeenAnalysed()) continue;
      // Check if all its uses are non escaping.
      if (alloc->IsEscaping()) {
        // Escape this allocation and all its dependencies.
        EscapeAllocation(graph, alloc, it.second);
      } else {
        // Try to capture the allocation. This can still change if a escaped
        // allocation has this value as one of its dependencies.
        alloc->SetElided();
      }
    }
    // Check that we've reached a fixpoint.
    VerifyEscapeAnalysis(graph);
  }

  void DropUseOfValueInStoresToCapturedAllocations() {
    for (Node* node : stores_to_allocations_) {
      InlinedAllocation* alloc =
          node->input(0).node()->Cast<InlinedAllocation>();
      // Since we don't analyze if allocations will escape until a fixpoint,
      // this could drop an use of an allocation and turn it non-escaping.
      if (alloc->HasBeenElided()) {
        // Skip first input.
        for (int i = 1; i < node->input_count(); i++) {
          DropInputUses(node->input(i));
        }
      }
    }
  }

  void DropInputUses(Input& input) {
    ValueNode* input_node = input.node();
    if (input_node->properties().is_required_when_unused() &&
        !input_node->Is<ArgumentsElements>())
      return;
    input_node->remove_use();
    if (!input_node->is_used() && !input_node->unused_inputs_were_visited()) {
      DropInputUses(input_node);
    }
  }

  void DropInputUses(ValueNode* node) {
    for (Input& input : *node) {
      DropInputUses(input);
    }
    DCHECK(!node->properties().can_eager_deopt());
    DCHECK(!node->properties().can_lazy_deopt());
    node->mark_unused_inputs_visited();
  }
};

class DeadNodeSweepingProcessor {
 public:
  explicit DeadNodeSweepingProcessor(MaglevCompilationInfo* compilation_info) {
    if (V8_UNLIKELY(compilation_info->has_graph_labeller())) {
      labeller_ = compilation_info->graph_labeller();
    }
  }

  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  void PreProcessBasicBlock(BasicBlock* block) {}

  ProcessResult Process(AllocationBlock* node, const ProcessingState& state) {
    // Note: this need to be done before ValueLocationConstraintProcessor, since
    // it access the allocation offsets.
    int size = 0;
    for (auto alloc : node->allocation_list()) {
      if (alloc->HasEscaped()) {
        alloc->set_offset(size);
        size += alloc->size();
      }
    }
    // ... and update its size.
    node->set_size(size);
    // If size is zero, then none of the inlined allocations have escaped, we
    // can remove the allocation block.
    if (size == 0) return ProcessResult::kRemove;
    return ProcessResult::kContinue;
  }

  ProcessResult Process(InlinedAllocation* node, const ProcessingState& state) {
    // Remove inlined allocation that became non-escaping.
    if (!node->HasEscaped()) {
      if (v8_flags.trace_maglev_escape_analysis) {
        std::cout << "* Removing allocation node "
                  << PrintNodeLabel(labeller_, node) << std::endl;
      }
      return ProcessResult::kRemove;
    }
    return ProcessResult::kContinue;
  }

  template <typename NodeT>
  ProcessResult Process(NodeT* node, const ProcessingState& state) {
    if constexpr (IsValueNode(Node::opcode_of<NodeT>) &&
                  (!NodeT::kProperties.is_required_when_unused() ||
                   std::is_same_v<ArgumentsElements, NodeT>)) {
      if (!node->is_used()) {
        return ProcessResult::kRemove;
      }
      return ProcessResult::kContinue;
    }

    if constexpr (CanBeStoreToNonEscapedObject<NodeT>()) {
      if (InlinedAllocation* object =
              node->input(0).node()->template TryCast<InlinedAllocation>()) {
        if (!object->HasEscaped()) {
          if (v8_flags.trace_maglev_escape_analysis) {
            std::cout << "* Removing store node "
                      << PrintNodeLabel(labeller_, node) << " to allocation "
                      << PrintNodeLabel(labeller_, object) << std::endl;
          }
          return ProcessResult::kRemove;
        }
      }
    }
    return ProcessResult::kContinue;
  }

 private:
  MaglevGraphLabeller* labeller_ = nullptr;
};

}  // namespace v8::internal::maglev

#endif  // V8_MAGLEV_MAGLEV_POST_HOC_OPTIMIZATIONS_PROCESSORS_H_
