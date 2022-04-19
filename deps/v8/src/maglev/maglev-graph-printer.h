// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_GRAPH_PRINTER_H_
#define V8_MAGLEV_MAGLEV_GRAPH_PRINTER_H_

#include <memory>
#include <ostream>
#include <set>
#include <vector>

namespace v8 {
namespace internal {
namespace maglev {

class BasicBlock;
class ControlNode;
class Graph;
class MaglevCompilationUnit;
class MaglevGraphLabeller;
class Node;
class NodeBase;
class Phi;
class ProcessingState;

class MaglevPrintingVisitor {
 public:
  explicit MaglevPrintingVisitor(std::ostream& os);

  void PreProcessGraph(MaglevCompilationUnit*, Graph* graph);
  void PostProcessGraph(MaglevCompilationUnit*, Graph* graph) {}
  void PreProcessBasicBlock(MaglevCompilationUnit*, BasicBlock* block);
  void Process(Phi* phi, const ProcessingState& state);
  void Process(Node* node, const ProcessingState& state);
  void Process(ControlNode* node, const ProcessingState& state);

  std::ostream& os() { return *os_for_additional_info_; }

 private:
  std::ostream& os_;
  std::unique_ptr<std::ostream> os_for_additional_info_;
  std::set<BasicBlock*> loop_headers_;
  std::vector<BasicBlock*> targets_;
};

void PrintGraph(std::ostream& os, MaglevCompilationUnit* compilation_unit,
                Graph* const graph);

class PrintNode {
 public:
  PrintNode(MaglevGraphLabeller* graph_labeller, const NodeBase* node)
      : graph_labeller_(graph_labeller), node_(node) {}

  void Print(std::ostream& os) const;

 private:
  MaglevGraphLabeller* graph_labeller_;
  const NodeBase* node_;
};

std::ostream& operator<<(std::ostream& os, const PrintNode& printer);

class PrintNodeLabel {
 public:
  PrintNodeLabel(MaglevGraphLabeller* graph_labeller, const Node* node)
      : graph_labeller_(graph_labeller), node_(node) {}

  void Print(std::ostream& os) const;

 private:
  MaglevGraphLabeller* graph_labeller_;
  const Node* node_;
};

std::ostream& operator<<(std::ostream& os, const PrintNodeLabel& printer);

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_MAGLEV_MAGLEV_GRAPH_PRINTER_H_
