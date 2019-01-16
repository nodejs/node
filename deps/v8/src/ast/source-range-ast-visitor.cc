// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/source-range-ast-visitor.h"

#include "src/ast/ast-source-ranges.h"

namespace v8 {
namespace internal {

SourceRangeAstVisitor::SourceRangeAstVisitor(uintptr_t stack_limit,
                                             Expression* root,
                                             SourceRangeMap* source_range_map)
    : AstTraversalVisitor(stack_limit, root),
      source_range_map_(source_range_map) {}

void SourceRangeAstVisitor::VisitBlock(Block* stmt) {
  AstTraversalVisitor::VisitBlock(stmt);
  ZonePtrList<Statement>* stmts = stmt->statements();
  AstNodeSourceRanges* enclosingSourceRanges = source_range_map_->Find(stmt);
  if (enclosingSourceRanges != nullptr) {
    CHECK(enclosingSourceRanges->HasRange(SourceRangeKind::kContinuation));
    MaybeRemoveLastContinuationRange(stmts);
  }
}

void SourceRangeAstVisitor::VisitFunctionLiteral(FunctionLiteral* expr) {
  AstTraversalVisitor::VisitFunctionLiteral(expr);
  ZonePtrList<Statement>* stmts = expr->body();
  MaybeRemoveLastContinuationRange(stmts);
}

bool SourceRangeAstVisitor::VisitNode(AstNode* node) {
  AstNodeSourceRanges* range = source_range_map_->Find(node);

  if (range == nullptr) return true;
  if (!range->HasRange(SourceRangeKind::kContinuation)) return true;

  // Called in pre-order. In case of conflicting continuation ranges, only the
  // outermost range may survive.

  SourceRange continuation = range->GetRange(SourceRangeKind::kContinuation);
  if (continuation_positions_.find(continuation.start) !=
      continuation_positions_.end()) {
    range->RemoveContinuationRange();
  } else {
    continuation_positions_.emplace(continuation.start);
  }

  return true;
}

void SourceRangeAstVisitor::MaybeRemoveLastContinuationRange(
    ZonePtrList<Statement>* statements) {
  if (statements == nullptr || statements->is_empty()) return;

  Statement* last_statement = statements->last();
  AstNodeSourceRanges* last_range = source_range_map_->Find(last_statement);
  if (last_range == nullptr) return;

  if (last_range->HasRange(SourceRangeKind::kContinuation)) {
    last_range->RemoveContinuationRange();
  }
}

}  // namespace internal
}  // namespace v8
