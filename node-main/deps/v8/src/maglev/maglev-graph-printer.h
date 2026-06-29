// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_PRINTER_H_
#define V8_MAGLEV_MAGLEV_GRAPH_PRINTER_H_

#include <memory>
#include <ostream>
#include <set>
#include <vector>

#include "src/maglev/maglev-compilation-unit.h"
#include "src/maglev/maglev-graph-labeller.h"
#include "src/maglev/maglev-graph-processor.h"
#include "src/maglev/maglev-ir.h"

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

class MaglevPrintingVisitor {
 public:
  explicit MaglevPrintingVisitor(std::ostream& os,
                                 bool has_regalloc_data = false);

  void PreProcessGraph(Graph* graph);
  void PostProcessGraph(Graph* graph) {}
  void PostProcessBasicBlock(BasicBlock* block) {}
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block);
  void PostPhiProcessing() {}
  ProcessResult Process(Phi* phi, const ProcessingState& state);
  ProcessResult Process(Node* node, const ProcessingState& state);
  ProcessResult Process(ControlNode* node, const ProcessingState& state);

  std::ostream& os() { return *os_for_additional_info_; }

 private:
  std::ostream& os_;
  std::unique_ptr<std::ostream> os_for_additional_info_;
  std::set<BasicBlock*> loop_headers_;
  std::vector<BasicBlock*> targets_;
  NodeIdT max_node_id_ = kInvalidNodeId;
  MaglevGraphLabeller::Provenance existing_provenance_;
  bool has_regalloc_data_;
};

void PrintGraph(std::ostream& os, Graph* const graph,
                bool has_regalloc_data = false);

#else

// Dummy inlined definitions of the printer classes/functions.

class MaglevPrintingVisitor {
 public:
  explicit MaglevPrintingVisitor(std::ostream& os) : os_(os) {}

  void PreProcessGraph(Graph* graph) {}
  void PostProcessGraph(Graph* graph) {}
  BlockProcessResult PreProcessBasicBlock(BasicBlock* block) {
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

  std::ostream& os() { return os_; }

 private:
  std::ostream& os_;
};

inline void PrintGraph(std::ostream& os, Graph* const graph,
                       bool has_regalloc_data = false) {}

#endif  // V8_ENABLE_MAGLEV_GRAPH_PRINTER

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_PRINTER_H_
