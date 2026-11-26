// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/engine/salvage.h"

#include <cstddef>
#include <limits>
#include <string>
#include <utility>

#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/core/tunable.h"

namespace redline {

using namespace fast;  // NOLINT(build/namespaces)

REDLINE_TUNABLE(kSalvageChance, "salvage.chance", 1.0, 0, 1,
                "probability to wrap a throwing statement and rerun a failed "
                "mutant once");
REDLINE_TUNABLE(kSalvageToplevelChance, "salvage.toplevel_chance", 0.5, 0, 1,
                "probability to wrap the outermost top-level statement instead "
                "of the innermost throw site");

namespace {

// Finds the first non-whitespace byte offset of 1-based `target_line` in
// `source`. Returns false if the line is out of range or contains only
// whitespace.
bool LineStartOffset(std::string_view source, int target_line,
                     int32_t* line_start) {
  if (target_line < 1) {
    return false;
  }
  int current_line = 1;
  size_t pos = 0;
  while (current_line < target_line && pos < source.size()) {
    size_t nl = source.find('\n', pos);
    if (nl == std::string_view::npos) {
      return false;
    }
    pos = nl + 1;
    current_line++;
  }
  if (current_line != target_line || pos >= source.size()) {
    return false;
  }
  size_t nl = source.find('\n', pos);
  size_t end = nl == std::string_view::npos ? source.size() : nl;
  while (pos < end &&
         (source[pos] == ' ' || source[pos] == '\t' || source[pos] == '\r')) {
    pos++;
  }
  if (pos >= end) {
    return false;
  }
  *line_start = static_cast<int32_t>(pos);
  return true;
}

// Visits every statement list in `root` in pre-order.
template <typename F>
void ForEachStatementList(Node* root, F&& f) {
  Walk(root, [&](Node* node) {
    switch (node->kind) {
      case NodeKind::kProgram:
        f(&node->Cast<Program>()->body, node);
        break;
      case NodeKind::kBlockStatement:
        f(&node->Cast<BlockStatement>()->body, node);
        break;
      case NodeKind::kSwitchCase:
        f(&node->Cast<SwitchCase>()->consequent, node);
        break;
      case NodeKind::kStaticBlock:
        f(&node->Cast<StaticBlock>()->body, node);
        break;
      default:
        break;
    }
    return true;
  });
}

// True when wrapping `stmt` directly in `try/catch` is unsafe or unnecessary.
bool IsUnsalvageableStatement(const Statement* stmt) {
  if (stmt == nullptr) {
    return true;
  }
  switch (stmt->kind) {
    case NodeKind::kExpressionStatement:
      return stmt->Cast<ExpressionStatement>()->is_directive;
    case NodeKind::kFunctionDeclaration:
    case NodeKind::kClassDeclaration:
    case NodeKind::kTryStatement:
    case NodeKind::kBreakStatement:
    case NodeKind::kContinueStatement:
    case NodeKind::kReturnStatement:
    case NodeKind::kEmptyStatement:
    case NodeKind::kDebuggerStatement:
      return true;
    default:
      return false;
  }
}

// Splits a simple `let` or `const` declaration with at least 1 initializer
// into a bare `let` declaration followed by a guarded assignment block.
bool SplitLexicalDeclaration(Tree* tree, const StatementLocation& loc,
                             VariableDeclaration* decl) {
  if (decl->declarations.empty()) {
    return false;
  }
  bool has_init = false;
  for (const VariableDeclarator* d : decl->declarations) {
    if (d == nullptr || d->target == nullptr || !d->target->Is<Identifier>()) {
      return false;
    }
    if (d->init != nullptr) {
      has_init = true;
    }
  }
  if (!has_init) {
    return false;
  }
  auto* bare = tree->New<VariableDeclaration>();
  bare->declaration_kind = VariableKind::kLet;
  std::vector<Statement*> assignments;
  for (VariableDeclarator* d : decl->declarations) {
    std::string_view name = d->target->Cast<Identifier>()->name;
    auto* declarator = tree->New<VariableDeclarator>();
    declarator->target = mk::Id(tree, name);
    declarator->init = nullptr;
    bare->declarations.push_back(declarator);
    if (d->init != nullptr) {
      assignments.push_back(
          mk::ExprStmt(tree, mk::Assign(tree, mk::Id(tree, name), d->init)));
    }
  }
  TryStatement* guarded = mk::Try(tree, std::move(assignments), "", {});
  (*loc.list)[loc.index] = bare;
  loc.list->insert(loc.list->begin() + static_cast<ptrdiff_t>(loc.index + 1),
                   guarded);
  return true;
}

}  // namespace

double SalvageChance() { return kSalvageChance(); }
double SalvageToplevelChance() { return kSalvageToplevelChance(); }

StatementLocation FindCoveringStatement(Tree* tree, std::string_view source,
                                        const std::vector<NodeSpan>& spans,
                                        int target_line, bool top_level) {
  if (tree == nullptr || tree->root() == nullptr) {
    return {};
  }
  int32_t line_start = 0;
  if (!LineStartOffset(source, target_line, &line_start)) {
    return {};
  }
  std::vector<std::pair<int32_t, int32_t>> span_by_id(tree->id_limit(),
                                                      {-1, -1});
  for (const NodeSpan& span : spans) {
    if (span.node_id < span_by_id.size()) {
      span_by_id[span.node_id] = {span.start, span.end};
    }
  }
  auto covers = [&](const Statement* stmt, int32_t* length) {
    if (stmt == nullptr || stmt->id >= span_by_id.size()) {
      return false;
    }
    const auto [s, e] = span_by_id[stmt->id];
    if (s < 0 || e <= s) {
      return false;
    }
    if (s <= line_start && line_start < e) {
      *length = e - s;
      return true;
    }
    return false;
  };
  if (top_level) {
    std::vector<Statement*>& body = tree->root()->body;
    for (size_t i = 0; i < body.size(); i++) {
      int32_t length = 0;
      if (covers(body[i], &length)) {
        return {&body, i, tree->root()};
      }
    }
    return {};
  }
  StatementLocation best;
  int32_t best_length = std::numeric_limits<int32_t>::max();
  ForEachStatementList(
      tree->root(), [&](std::vector<Statement*>* list, Node* owner) {
        for (size_t i = 0; i < list->size(); i++) {
          int32_t length = 0;
          if (covers((*list)[i], &length) && length <= best_length) {
            best_length = length;
            best = {list, i, owner};
          }
        }
      });
  return best;
}

bool WrapStatementForSalvage(Tree* tree, const StatementLocation& loc) {
  if (tree == nullptr || !loc.valid()) {
    return false;
  }
  Statement* stmt = loc.statement();
  if (auto* decl = stmt->As<VariableDeclaration>()) {
    switch (decl->declaration_kind) {
      case VariableKind::kVar:
        break;
      case VariableKind::kLet:
      case VariableKind::kConst:
        return SplitLexicalDeclaration(tree, loc, decl);
      case VariableKind::kUsing:
      case VariableKind::kAwaitUsing:
        return false;
    }
  } else if (stmt->Is<LabeledStatement>()) {
    Statement* inner = stmt;
    while (auto* labeled = inner->As<LabeledStatement>()) {
      inner = labeled->body;
      if (inner == nullptr) {
        return false;
      }
    }
    if (IsUnsalvageableStatement(inner)) {
      return false;
    }
    if (auto* inner_decl = inner->As<VariableDeclaration>();
        inner_decl != nullptr &&
        inner_decl->declaration_kind != VariableKind::kVar) {
      return false;
    }
  } else if (IsUnsalvageableStatement(stmt)) {
    return false;
  }
  (*loc.list)[loc.index] = mk::Try(tree, {stmt}, "", {});
  return true;
}

bool SalvageException(Tree* tree, const ExceptionLines& lines,
                      bool prefer_top_level) {
  if (tree == nullptr || tree->root() == nullptr || lines.empty()) {
    return false;
  }
  std::vector<NodeSpan> spans;
  std::string source = fast::Print(tree->root(), fast::PrintOptions(), &spans);
  const bool multi_frame =
      lines.outermost > 0 && lines.outermost != lines.innermost;
  auto try_wrap = [&](int line, bool top_level, bool require_nested) {
    if (line <= 0) {
      return false;
    }
    StatementLocation loc =
        FindCoveringStatement(tree, source, spans, line, top_level);
    if (!loc.valid()) {
      return false;
    }
    if (require_nested && loc.owner == tree->root()) {
      return false;
    }
    return WrapStatementForSalvage(tree, loc);
  };
  if (multi_frame) {
    if (prefer_top_level) {
      return try_wrap(lines.outermost, /*top_level=*/true, false) ||
             try_wrap(lines.outermost, /*top_level=*/false, false) ||
             try_wrap(lines.innermost, /*top_level=*/false, true);
    }
    return try_wrap(lines.innermost, /*top_level=*/false, true) ||
           try_wrap(lines.outermost, /*top_level=*/false, false) ||
           try_wrap(lines.outermost, /*top_level=*/true, false);
  }
  if (prefer_top_level) {
    return try_wrap(lines.outermost, /*top_level=*/true, false) ||
           try_wrap(lines.innermost, /*top_level=*/false, false);
  }
  return try_wrap(lines.innermost, /*top_level=*/false, false) ||
         try_wrap(lines.outermost, /*top_level=*/true, false);
}

bool ThrowsInLastTopLevelStatement(Tree* tree, const ExceptionLines& lines) {
  if (tree == nullptr || tree->root() == nullptr || lines.empty()) {
    return false;
  }
  std::vector<NodeSpan> spans;
  std::string source = fast::Print(tree->root(), fast::PrintOptions(), &spans);
  // The smallest statement that covers the throw site must be the last
  // top-level statement. A throw in a nested list or in a function body can
  // have statements after it.
  const StatementLocation inner = FindCoveringStatement(
      tree, source, spans, lines.innermost, /*top_level=*/false);
  if (!inner.valid() || inner.owner != tree->root() ||
      inner.index + 1 != inner.list->size()) {
    return false;
  }
  if (lines.outermost <= 0 || lines.outermost == lines.innermost) {
    return true;
  }
  // The outermost frame (a call) must be in the same statement.
  const StatementLocation outer = FindCoveringStatement(
      tree, source, spans, lines.outermost, /*top_level=*/true);
  return outer.valid() && outer.statement() == inner.statement();
}

}  // namespace redline
