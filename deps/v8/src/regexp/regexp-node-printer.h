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

template <typename T>
class RegExpNodePrinter;

template <>
class RegExpNodePrinter<RegExpNode> : public RegExpNodePrinterBase<RegExpNode>,
                                      public NodeVisitor {
 public:
  RegExpNodePrinter(std::ostream& os, RegExpGraphLabeller<RegExpNode>* labeller,
                    Zone* zone)
      : RegExpNodePrinterBase(os, labeller, zone, 'n') {}
  RegExpNodePrinter(const RegExpPrinterBase& other,
                    RegExpGraphLabeller<RegExpNode>* labeller)
      : RegExpNodePrinterBase(other, labeller, 'n') {}

  using RegExpNodePrinterBase<RegExpNode>::PrintNodeLabel;

#define DECLARE_VISIT(Type) void Visit##Type(Type##Node* that) override;
  FOR_EACH_NODE_TYPE(DECLARE_VISIT)
#undef DECLARE_VISIT
  void PrintGuard(const Guard* guard);
  void PrintNodeLabel(const RegExpNode* node, const char* name);
  void PrintSuccess(const SeqRegExpNode* node);
};

using RegExpGraphNodePrinter = RegExpNodePrinter<RegExpNode>;

}  // namespace internal
}  // namespace v8
#endif  // V8_ENABLE_REGEXP_DIAGNOSTICS

#endif  // V8_REGEXP_REGEXP_NODE_PRINTER_H_
