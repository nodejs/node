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

class BoyerMoorePositionInfo;
class BoyerMooreLookahead;
class RegExpNode;
class RegExpTree;
class Zone;

class RegExpGraphPrinter {
 public:
  explicit RegExpGraphPrinter(
      std::unique_ptr<RegExpNodePrinter<RegExpNode>> printer);
  ~RegExpGraphPrinter();
  void PrintGraph(RegExpNode* root);
  void PrintNode(RegExpNode* node);
  void PrintNodeNoNewline(RegExpNode* node);
  void PrintTrace(Trace* trace);
  void PrintBoyerMoorePositionInfo(const BoyerMoorePositionInfo* pi);
  void PrintBoyerMooreLookahead(const BoyerMooreLookahead* bm);

 private:
  class Edge {
   public:
    Edge(RegExpNode* from, RegExpNode* to) : from_(from), to_(to) {}

    const RegExpNode* from() const { return from_; }
    const RegExpNode* to() const { return to_; }
    bool IsValid() const { return from_ != nullptr && to_ != nullptr; }
    void Invalidate() {
      from_ = nullptr;
      to_ = nullptr;
    }

   private:
    RegExpNode* from_;
    RegExpNode* to_;
  };

  class Scheduler;

  void PrintNodeLabel(RegExpNode* node) { printer_->PrintNodeLabel(node); }
  void PreSizeTargets();
  void PrintVerticalArrows(RegExpNode* current = nullptr);
  void PrintVerticalArrowsBelow(RegExpNode* current,
                                bool has_fallthrough = false);

  size_t AddEdge(RegExpNode* from, RegExpNode* to);
  bool AddEdgeIfNotNext(RegExpNode* from, RegExpNode* to, RegExpNode* next,
                        std::set<size_t>* arrows_starting_here = nullptr);
  bool IsBackEdge(const Edge& edge) const;
  bool EdgeStartsAt(const Edge& edge, const RegExpNode* node) const;
  bool EdgeEndsAt(const Edge& edge, const RegExpNode* node) const;
  bool IsTarget(const RegExpNode* node) const;
  bool IsStart(const RegExpNode* node) const;

  std::ostream& os() { return printer_->os(); }
  void set_color(RegExpPrinterBase::Color color) { printer_->set_color(color); }
  RegExpNodePrinter<RegExpNode>* printer() { return printer_.get(); }
  RegExpGraphLabeller<RegExpNode>* labeller() { return printer_->labeller(); }

  std::unique_ptr<RegExpNodePrinter<RegExpNode>> printer_;
  std::unique_ptr<Scheduler> schedule_;
  std::vector<RegExpNode*> targets_;
  std::vector<Edge> edges_;
  int max_node_id_ = 0;
};

}  // namespace internal
}  // namespace v8
#endif  // V8_ENABLE_REGEXP_DIAGNOSTICS

#endif  // V8_REGEXP_REGEXP_GRAPH_PRINTER_H_
