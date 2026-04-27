// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef V8_ENABLE_REGEXP_DIAGNOSTICS

#include "src/regexp/regexp-printer.h"

#include "src/regexp/regexp-ast-printer.h"
#include "src/regexp/regexp-graph-printer.h"

namespace v8 {
namespace internal {
namespace regexp {

#ifdef V8_ENABLE_REGEXP_DIAGNOSTICS
Diagnostics::Diagnostics(std::ostream& os, Zone* zone) : os_(os), zone_(zone) {}
Diagnostics::~Diagnostics() = default;

void Diagnostics::set_graph_labeller(
    std::unique_ptr<GraphLabeller<Node>> graph_labeller) {
  graph_labeller_ = std::move(graph_labeller);
}

void Diagnostics::set_tree_labeller(
    std::unique_ptr<GraphLabeller<Tree>> tree_labeller) {
  tree_labeller_ = std::move(tree_labeller);
}

void Diagnostics::set_graph_printer(
    std::unique_ptr<GraphPrinter> graph_printer) {
  graph_printer_ = std::move(graph_printer);
}

void Diagnostics::set_ast_printer(std::unique_ptr<AstNodePrinter> ast_printer) {
  ast_printer_ = std::move(ast_printer);
}
#endif

}  // namespace regexp
}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_REGEXP_DIAGNOSTICS
