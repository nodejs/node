// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_PRINTER_H_
#define V8_MAGLEV_MAGLEV_GRAPH_PRINTER_H_

#include <memory>
#include <ostream>
#include <set>
#include <vector>

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

class MaglevPrintingVisitor {
 public:
  explicit MaglevPrintingVisitor(MaglevGraphLabeller* graph_labeller,
                                 std::ostream& os);

  void PreProcessGraph(Graph* graph);
  void PostProcessGraph(Graph* graph) {}
  void PreProcessBasicBlock(BasicBlock* block);
  void Process(Phi* phi, const ProcessingState& state);
  void Process(Node* node, const ProcessingState& state);
  void Process(ControlNode* node, const ProcessingState& state);

  std::ostream& os() { return *os_for_additional_info_; }

 private:
  MaglevGraphLabeller* graph_labeller_;
  std::ostream& os_;
  std::unique_ptr<std::ostream> os_for_additional_info_;
  std::set<BasicBlock*> loop_headers_;
  std::vector<BasicBlock*> targets_;
  NodeIdT max_node_id_ = kInvalidNodeId;
};

void PrintGraph(std::ostream& os, MaglevCompilationInfo* compilation_info,
                Graph* const graph);

class PrintNode {
 public:
  PrintNode(MaglevGraphLabeller* graph_labeller, const NodeBase* node,
            bool skip_targets = false)
      : graph_labeller_(graph_labeller),
        node_(node),
        skip_targets_(skip_targets) {}

  void Print(std::ostream& os) const;

 private:
  MaglevGraphLabeller* graph_labeller_;
  const NodeBase* node_;
  // This is used when tracing graph building, since targets might not exist
  // yet.
  const bool skip_targets_;
};

std::ostream& operator<<(std::ostream& os, const PrintNode& printer);

class PrintNodeLabel {
 public:
  PrintNodeLabel(MaglevGraphLabeller* graph_labeller, const NodeBase* node)
      : graph_labeller_(graph_labeller), node_(node) {}

  void Print(std::ostream& os) const;

 private:
  MaglevGraphLabeller* graph_labeller_;
  const NodeBase* node_;
};

std::ostream& operator<<(std::ostream& os, const PrintNodeLabel& printer);

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_PRINTER_H_
