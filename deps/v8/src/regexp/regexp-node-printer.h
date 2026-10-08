// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_NODE_PRINTER_H_
#define V8_REGEXP_REGEXP_NODE_PRINTER_H_

#ifdef V8_ENABLE_REGEXP_DIAGNOSTICS
#include "src/regexp/regexp-nodes.h"
#include "src/regexp/regexp-printer.h"

namespace v8 {
namespace internal {
namespace regexp {

template <typename T>
class NodePrinter;

template <>
class NodePrinter<Node> : public NodePrinterBase<Node>, public NodeVisitor {
 public:
  NodePrinter(std::ostream& os, GraphLabeller<Node>* labeller, Zone* zone)
      : NodePrinterBase(os, labeller, zone, 'n') {}
  NodePrinter(const PrinterBase& other, GraphLabeller<Node>* labeller)
      : NodePrinterBase(other, labeller, 'n') {}

  using NodePrinterBase<Node>::PrintNodeLabel;

#define DECLARE_VISIT(Type) void Visit##Type(Type##Node* that) override;
  FOR_EACH_NODE_TYPE(DECLARE_VISIT)
#undef DECLARE_VISIT
  void PrintGuard(const Guard* guard);
  void PrintNodeLabel(const Node* node, const char* name);
  void PrintSuccess(const SeqNode* node);
};

using RegExpGraphNodePrinter = NodePrinter<Node>;

}  // namespace regexp
}  // namespace internal
}  // namespace v8
#endif  // V8_ENABLE_REGEXP_DIAGNOSTICS

#endif  // V8_REGEXP_REGEXP_NODE_PRINTER_H_
