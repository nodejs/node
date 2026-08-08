// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_GRAPH_PRINTER_H_
#define V8_REGEXP_REGEXP_GRAPH_PRINTER_H_

#ifdef V8_ENABLE_REGEXP_DIAGNOSTICS
#include <iostream>
#include <memory>
#include <vector>

#include "src/base/logging.h"
#include "src/flags/flags.h"
#include "src/regexp/regexp-ast.h"
#include "src/regexp/regexp-node-printer.h"
#include "src/regexp/regexp-nodes.h"
#include "src/regexp/regexp-printer.h"

namespace v8 {
namespace internal {

class Zone;

namespace regexp {

class BoyerMoorePositionInfo;
class BoyerMooreLookahead;
class Node;
class Tree;

class GraphPrinter {
 public:
  explicit GraphPrinter(std::unique_ptr<NodePrinter<Node>> printer);
  ~GraphPrinter();
  void PrintGraph(Node* root);
  void PrintNode(Node* node);
  void PrintNodeNoNewline(Node* node);
  void PrintTrace(Trace* trace);
  void PrintBoyerMoorePositionInfo(const BoyerMoorePositionInfo* pi);
  void PrintBoyerMooreLookahead(const BoyerMooreLookahead* bm);

 private:
  class Edge {
   public:
    Edge(Node* from, Node* to) : from_(from), to_(to) {}

    const Node* from() const { return from_; }
    const Node* to() const { return to_; }
    bool IsValid() const { return from_ != nullptr && to_ != nullptr; }
    void Invalidate() {
      from_ = nullptr;
      to_ = nullptr;
    }

   private:
    Node* from_;
    Node* to_;
  };

  class Scheduler;

  void PrintNodeLabel(Node* node) { printer_->PrintNodeLabel(node); }
  void PreSizeTargets();
  void PrintVerticalArrows(Node* current = nullptr);
  void PrintVerticalArrowsBelow(Node* current, bool has_fallthrough = false);

  size_t AddEdge(Node* from, Node* to);
  bool AddEdgeIfNotNext(Node* from, Node* to, Node* next,
                        std::set<size_t>* arrows_starting_here = nullptr);
  bool IsBackEdge(const Edge& edge) const;
  bool EdgeStartsAt(const Edge& edge, const Node* node) const;
  bool EdgeEndsAt(const Edge& edge, const Node* node) const;
  bool IsTarget(const Node* node) const;
  bool IsStart(const Node* node) const;

  std::ostream& os() { return printer_->os(); }
  void set_color(PrinterBase::Color color) { printer_->set_color(color); }
  NodePrinter<Node>* printer() { return printer_.get(); }
  GraphLabeller<Node>* labeller() { return printer_->labeller(); }

  std::unique_ptr<NodePrinter<Node>> printer_;
  std::unique_ptr<Scheduler> schedule_;
  std::vector<Node*> targets_;
  std::vector<Edge> edges_;
  int max_node_id_ = 0;
};

}  // namespace regexp
}  // namespace internal
}  // namespace v8
#endif  // V8_ENABLE_REGEXP_DIAGNOSTICS

#endif  // V8_REGEXP_REGEXP_GRAPH_PRINTER_H_
