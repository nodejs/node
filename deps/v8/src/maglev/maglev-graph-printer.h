// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_PRINTER_H_
#define V8_MAGLEV_MAGLEV_GRAPH_PRINTER_H_

#include <memory>
#include <optional>
#include <ostream>
#include <set>
#include <vector>

#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-ir.h"
#include "src/maglev/maglev-phase.h"

namespace v8 {
namespace internal {
namespace maglev {

class BasicBlock;
class ControlNode;
class Graph;
class MaglevCompilationInfo;
class MaglevGraphLabeller;
class Node;
class NodeBase;
class Phi;
class ProcessingState;

#ifdef V8_ENABLE_MAGLEV_GRAPH_PRINTER

class LineCountingStream;

class MaglevPrintingVisitor {
 public:
  explicit MaglevPrintingVisitor(std::ostream& os, Graph* graph,
                                 MaglevPhase phase);

  void PreProcessGraph(Graph* graph);
  void PostProcessGraph(Graph* graph) {}
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block);
  void PostPhiProcessing() {}
  ProcessResult Process(Phi* phi, const ProcessingState& state);
  ProcessResult Process(Node* node, const ProcessingState& state);
  ProcessResult Process(ControlNode* node, const ProcessingState& state);
  ProcessResult Process(NodeBase* node, const ProcessingState& state);

  std::ostream& os() { return *os_for_additional_info_; }

 private:
  bool has_regalloc_data() const {
    return is_maglev_ && phase_ >= MaglevPhase::kAnyUseMarking;
  }
  bool print_sweepable_dead_phis() const {
    // The AnyUseMarking processor can keep phis in the graph but remove their
    // backedge, which leads to DCHECK failures when trying to print regalloc
    // data for the removed backedge when displaying phi gap moves. We thus use
    // the avoid printing regalloc data for dead phis' inputs after this phase.
    if (!is_maglev_) {
      // Turbolev never has Maglev regalloc, so no issues there.
      return true;
    }
    // The regalloc data can be incomplete only after AnyUseMarking.
    return phase_ != MaglevPhase::kAnyUseMarking;
  }

  std::ostream& os_;
  LineCountingStream* counting_stream_;
  std::unique_ptr<std::ostream> os_for_additional_info_;
  std::set<BasicBlock*> loop_headers_;
  std::vector<BasicBlock*> targets_;
  NodeIdT max_node_id_ = kInvalidNodeId;
  MaglevGraphLabeller::Provenance existing_provenance_;
  MaglevPhase phase_;
  bool is_maglev_;
};

void PrintGraph(std::ostream& os, Graph* const graph, MaglevPhase phase);
void PrintGraphToFile(Graph* const graph, MaglevPhase phase);

#else

// Dummy inlined definitions of the printer classes/functions.

class MaglevPrintingVisitor {
 public:
  explicit MaglevPrintingVisitor(std::ostream& os, Graph* graph,
                                 MaglevPhase phase)
      : os_(os) {}

  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  BlockProcessResult PostProcessBasicBlock(BasicBlock* block) {
    return BlockProcessResult::kContinue;
  }
  void PostPhiProcessing() {}
  ProcessResult Process(Phi* phi, const ProcessingState& state) {
    return ProcessResult::kContinue;
  }
  ProcessResult Process(Node* node, const ProcessingState& state) {
    return ProcessResult::kContinue;
  }
  ProcessResult Process(ControlNode* node, const ProcessingState& state) {
    return ProcessResult::kContinue;
  }
  ProcessResult Process(NodeBase* node, const ProcessingState& state) {
    return ProcessResult::kContinue;
  }

  std::ostream& os() { return os_; }

 private:
  std::ostream& os_;
};

inline void PrintGraph(std::ostream& os, Graph* const graph,
                       MaglevPhase phase) {}
inline void PrintGraphToFile(Graph* const graph, MaglevPhase phase) {}

#endif  // V8_ENABLE_MAGLEV_GRAPH_PRINTER

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_PRINTER_H_
