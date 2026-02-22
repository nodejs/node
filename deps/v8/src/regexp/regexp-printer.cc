// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef V8_ENABLE_REGEXP_DIAGNOSTICS

#include "src/regexp/regexp-printer.h"

#include "src/regexp/regexp-ast-printer.h"
#include "src/regexp/regexp-graph-printer.h"

namespace v8 {
namespace internal {

#ifdef V8_ENABLE_REGEXP_DIAGNOSTICS
RegExpDiagnostics::RegExpDiagnostics(std::ostream& os, Zone* zone)
    : os_(os), zone_(zone) {}
RegExpDiagnostics::~RegExpDiagnostics() = default;

void RegExpDiagnostics::set_graph_labeller(
    std::unique_ptr<RegExpGraphLabeller<RegExpNode>> graph_labeller) {
  graph_labeller_ = std::move(graph_labeller);
}

void RegExpDiagnostics::set_tree_labeller(
    std::unique_ptr<RegExpGraphLabeller<RegExpTree>> tree_labeller) {
  tree_labeller_ = std::move(tree_labeller);
}

void RegExpDiagnostics::set_graph_printer(
    std::unique_ptr<RegExpGraphPrinter> graph_printer) {
  graph_printer_ = std::move(graph_printer);
}

void RegExpDiagnostics::set_ast_printer(
    std::unique_ptr<RegExpAstNodePrinter> ast_printer) {
  ast_printer_ = std::move(ast_printer);
}
#endif

}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_REGEXP_DIAGNOSTICS
