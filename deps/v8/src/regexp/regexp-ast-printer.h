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

class RegExpDiagnostics;

template <typename T>
class RegExpNodePrinter;

template <>
class V8_EXPORT_PRIVATE RegExpNodePrinter<RegExpTree>
    : public RegExpNodePrinterBase<RegExpTree>, public RegExpVisitor {
 public:
  RegExpNodePrinter(std::ostream& os, RegExpGraphLabeller<RegExpTree>* labeller,
                    Zone* zone)
      : RegExpNodePrinterBase(os, labeller, zone, 't') {}
  RegExpNodePrinter(const RegExpPrinterBase& other,
                    RegExpGraphLabeller<RegExpTree>* labeller)
      : RegExpNodePrinterBase(other, labeller, 't') {}

  void VisitCharacterRange(CharacterRange that);
#define DECLARE_VISIT(Name) \
  void* Visit##Name(RegExp##Name*, void* data) override;
  FOR_EACH_REG_EXP_TREE_TYPE(DECLARE_VISIT)
#undef DECLARE_VISIT
  void Print(RegExpTree* tree);
};

using RegExpAstNodePrinter = RegExpNodePrinter<RegExpTree>;

class TraceRegExpTreeScope {
 public:
  explicit TraceRegExpTreeScope(RegExpDiagnostics* diagnostics);
  ~TraceRegExpTreeScope();
  void PrintTree(RegExpTree* tree);
  std::ostream& os();

 private:
  TraceRegExpTreeScope* const parent_;
  const int depth_;
  RegExpDiagnostics* diagnostics_;
};

}  // namespace internal
}  // namespace v8
#endif  // V8_ENABLE_REGEXP_DIAGNOSTICS

#endif  // V8_REGEXP_REGEXP_AST_PRINTER_H_
