// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_PROCESSOR_H_
#define V8_MAGLEV_MAGLEV_GRAPH_PROCESSOR_H_

#include "src/compiler/bytecode-analysis.h"
#include "src/maglev/maglev-basic-block.h"
#include "src/maglev/maglev-compilation-info.h"
#include "src/maglev/maglev-graph.h"
#include "src/maglev/maglev-interpreter-frame-state.h"
#include "src/maglev/maglev-ir.h"

namespace v8 {
namespace internal {
namespace maglev {

// The GraphProcessor takes a NodeProcessor, and applies it to each Node in the
// Graph by calling NodeProcessor::Process on each Node.
//
// The GraphProcessor also keeps track of the current ProcessingState, including
// the inferred corresponding InterpreterFrameState and (optionally) the state
// at the most recent Checkpoint, and passes this to the Process method.
//
// It expects a NodeProcessor class with:
//
//   // A function that processes the graph before the nodes are walked.
//   void PreProcessGraph(MaglevCompilationInfo*, Graph* graph);
//
//   // A function that processes the graph after the nodes are walked.
//   void PostProcessGraph(MaglevCompilationInfo*, Graph* graph);
//
//   // A function that processes each basic block before its nodes are walked.
//   void PreProcessBasicBlock(MaglevCompilationInfo*, BasicBlock* block);
//
//   // Process methods for each Node type. The GraphProcessor switches over
//   // the Node's opcode, casts it to the appropriate FooNode, and dispatches
//   // to NodeProcessor::Process. It's then up to the NodeProcessor to provide
//   // either distinct Process methods per Node type, or using templates or
//   // overloading as appropriate to group node processing.
//   void Process(FooNode* node, const ProcessingState& state) {}
//
template <typename NodeProcessor>
class GraphProcessor;

class ProcessingState {
 public:
  explicit ProcessingState(MaglevCompilationInfo* compilation_info,
                           BlockConstIterator block_it)
      : compilation_info_(compilation_info), block_it_(block_it) {}

  // Disallow copies, since the underlying frame states stay mutable.
  ProcessingState(const ProcessingState&) = delete;
  ProcessingState& operator=(const ProcessingState&) = delete;

  BasicBlock* block() const { return *block_it_; }
  BasicBlock* next_block() const { return *(block_it_ + 1); }

  MaglevCompilationInfo* compilation_info() const { return compilation_info_; }

  MaglevGraphLabeller* graph_labeller() const {
    return compilation_info_->graph_labeller();
  }

 private:
  MaglevCompilationInfo* compilation_info_;
  BlockConstIterator block_it_;
};

template <typename NodeProcessor>
class GraphProcessor {
 public:
  template <typename... Args>
  explicit GraphProcessor(MaglevCompilationInfo* compilation_info,
                          Args&&... args)
      : compilation_info_(compilation_info),
        node_processor_(std::forward<Args>(args)...) {}

  void ProcessGraph(Graph* graph) {
    graph_ = graph;

    node_processor_.PreProcessGraph(compilation_info_, graph);

    for (block_it_ = graph->begin(); block_it_ != graph->end(); ++block_it_) {
      BasicBlock* block = *block_it_;

      node_processor_.PreProcessBasicBlock(compilation_info_, block);

      if (block->has_phi()) {
        for (Phi* phi : *block->phis()) {
          node_processor_.Process(phi, GetCurrentState());
        }
      }

      for (node_it_ = block->nodes().begin(); node_it_ != block->nodes().end();
           ++node_it_) {
        Node* node = *node_it_;
        ProcessNodeBase(node, GetCurrentState());
      }

      ProcessNodeBase(block->control_node(), GetCurrentState());
    }

    node_processor_.PostProcessGraph(compilation_info_, graph);
  }

  NodeProcessor& node_processor() { return node_processor_; }
  const NodeProcessor& node_processor() const { return node_processor_; }

 private:
  ProcessingState GetCurrentState() {
    return ProcessingState(compilation_info_, block_it_);
  }

  void ProcessNodeBase(NodeBase* node, const ProcessingState& state) {
    switch (node->opcode()) {
#define CASE(OPCODE)                                      \
  case Opcode::k##OPCODE:                                 \
    PreProcess(node->Cast<OPCODE>(), state);              \
    node_processor_.Process(node->Cast<OPCODE>(), state); \
    break;
      NODE_BASE_LIST(CASE)
#undef CASE
    }
  }

  void PreProcess(NodeBase* node, const ProcessingState& state) {}

  MaglevCompilationInfo* const compilation_info_;
  NodeProcessor node_processor_;
  Graph* graph_;
  BlockConstIterator block_it_;
  NodeConstIterator node_it_;
};

// A NodeProcessor that wraps multiple NodeProcessors, and forwards to each of
// them iteratively.
template <typename... Processors>
class NodeMultiProcessor;

template <>
class NodeMultiProcessor<> {
 public:
  void PreProcessGraph(MaglevCompilationInfo*, Graph* graph) {}
  void PostProcessGraph(MaglevCompilationInfo*, Graph* graph) {}
  void PreProcessBasicBlock(MaglevCompilationInfo*, BasicBlock* block) {}
  void Process(NodeBase* node, const ProcessingState& state) {}
};

template <typename Processor, typename... Processors>
class NodeMultiProcessor<Processor, Processors...>
    : NodeMultiProcessor<Processors...> {
  using Base = NodeMultiProcessor<Processors...>;

 public:
  template <typename Node>
  void Process(Node* node, const ProcessingState& state) {
    processor_.Process(node, state);
    Base::Process(node, state);
  }
  void PreProcessGraph(MaglevCompilationInfo* info, Graph* graph) {
    processor_.PreProcessGraph(info, graph);
    Base::PreProcessGraph(info, graph);
  }
  void PostProcessGraph(MaglevCompilationInfo* info, Graph* graph) {
    // Post process in reverse order because that kind of makes sense.
    Base::PostProcessGraph(info, graph);
    processor_.PostProcessGraph(info, graph);
  }
  void PreProcessBasicBlock(MaglevCompilationInfo* info, BasicBlock* block) {
    processor_.PreProcessBasicBlock(info, block);
    Base::PreProcessBasicBlock(info, block);
  }

 private:
  Processor processor_;
};

template <typename... Processors>
using GraphMultiProcessor = GraphProcessor<NodeMultiProcessor<Processors...>>;

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_PROCESSOR_H_
