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

void SourceRangeAstVisitor::VisitSwitchStatement(SwitchStatement* stmt) {
  AstTraversalVisitor::VisitSwitchStatement(stmt);
  ZonePtrList<CaseClause>* clauses = stmt->cases();
  for (CaseClause* clause : *clauses) {
    MaybeRemoveLastContinuationRange(clause->statements());
  }
}

void SourceRangeAstVisitor::VisitFunctionLiteral(FunctionLiteral* expr) {
  AstTraversalVisitor::VisitFunctionLiteral(expr);
  ZonePtrList<Statement>* stmts = expr->body();
  MaybeRemoveLastContinuationRange(stmts);
}

void SourceRangeAstVisitor::VisitTryCatchStatement(TryCatchStatement* stmt) {
  AstTraversalVisitor::VisitTryCatchStatement(stmt);
  MaybeRemoveContinuationRange(stmt->try_block());
  MaybeRemoveContinuationRangeOfAsyncReturn(stmt);
}

void SourceRangeAstVisitor::VisitTryFinallyStatement(
    TryFinallyStatement* stmt) {
  AstTraversalVisitor::VisitTryFinallyStatement(stmt);
  MaybeRemoveContinuationRange(stmt->try_block());
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

void SourceRangeAstVisitor::MaybeRemoveContinuationRange(
    Statement* last_statement) {
  AstNodeSourceRanges* last_range = nullptr;

  if (last_statement->IsExpressionStatement() &&
      last_statement->AsExpressionStatement()->expression()->IsThrow()) {
    // For ThrowStatement, source range is tied to Throw expression not
    // ExpressionStatement.
    last_range = source_range_map_->Find(
        last_statement->AsExpressionStatement()->expression());
  } else {
    last_range = source_range_map_->Find(last_statement);
  }

  if (last_range == nullptr) return;

  if (last_range->HasRange(SourceRangeKind::kContinuation)) {
    last_range->RemoveContinuationRange();
  }
}

void SourceRangeAstVisitor::MaybeRemoveLastContinuationRange(
    ZonePtrList<Statement>* statements) {
  if (statements->is_empty()) return;
  MaybeRemoveContinuationRange(statements->last());
}

namespace {
Statement* FindLastNonSyntheticReturn(ZonePtrList<Statement>* statements) {
  for (int i = statements->length() - 1; i >= 0; --i) {
    Statement* stmt = statements->at(i);
    if (!stmt->IsReturnStatement()) break;
    if (stmt->AsReturnStatement()->is_synthetic_async_return()) continue;
    return stmt;
  }
  return nullptr;
}
}  // namespace

void SourceRangeAstVisitor::MaybeRemoveContinuationRangeOfAsyncReturn(
    TryCatchStatement* try_catch_stmt) {
  // Detect try-catch inserted by NewTryCatchStatementForAsyncAwait in the
  // parser (issued for async functions, including async generators), and
  // remove the continuation ranges of return statements corresponding to
  // returns at function end in the untransformed source.
  if (try_catch_stmt->is_try_catch_for_async()) {
    Statement* last_non_synthetic =
        FindLastNonSyntheticReturn(try_catch_stmt->try_block()->statements());
    if (last_non_synthetic) {
      MaybeRemoveContinuationRange(last_non_synthetic);
    }
  }
}

}  // namespace internal
}  // namespace v8
