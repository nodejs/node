// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_AST_PRINTER_H_
#define V8_REGEXP_REGEXP_AST_PRINTER_H_

#ifdef V8_ENABLE_REGEXP_DIAGNOSTICS
#include "src/regexp/regexp-ast.h"
#include "src/regexp/regexp-printer.h"

namespace v8 {
namespace internal {
namespace regexp {

class Diagnostics;

template <typename T>
class NodePrinter;

template <>
class V8_EXPORT_PRIVATE NodePrinter<Tree> : public NodePrinterBase<Tree>,
                                            public Visitor {
 public:
  NodePrinter(std::ostream& os, GraphLabeller<Tree>* labeller, Zone* zone)
      : NodePrinterBase(os, labeller, zone, 't') {}
  NodePrinter(const PrinterBase& other, GraphLabeller<Tree>* labeller)
      : NodePrinterBase(other, labeller, 't') {}

  void VisitCharacterRange(CharacterRange that);
#define DECLARE_VISIT(Name) void* Visit##Name(Name*, void* data) override;
  FOR_EACH_REG_EXP_TREE_TYPE(DECLARE_VISIT)
#undef DECLARE_VISIT
  void Print(Tree* tree);
};

using AstNodePrinter = NodePrinter<Tree>;

class TraceTreeScope {
 public:
  explicit TraceTreeScope(Diagnostics* diagnostics);
  ~TraceTreeScope();
  void PrintTree(Tree* tree);
  std::ostream& os();

 private:
  TraceTreeScope* const parent_;
  const int depth_;
  Diagnostics* diagnostics_;
};

}  // namespace regexp
}  // namespace internal
}  // namespace v8
#endif  // V8_ENABLE_REGEXP_DIAGNOSTICS

#endif  // V8_REGEXP_REGEXP_AST_PRINTER_H_
