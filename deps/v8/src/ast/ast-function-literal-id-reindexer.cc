// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/ast-function-literal-id-reindexer.h"
#include "src/objects-inl.h"

#include "src/ast/ast.h"

namespace v8 {
namespace internal {

AstFunctionLiteralIdReindexer::AstFunctionLiteralIdReindexer(size_t stack_limit,
                                                             int delta)
    : AstTraversalVisitor(stack_limit), delta_(delta) {}

AstFunctionLiteralIdReindexer::~AstFunctionLiteralIdReindexer() = default;

void AstFunctionLiteralIdReindexer::Reindex(Expression* pattern) {
  Visit(pattern);
}

void AstFunctionLiteralIdReindexer::VisitFunctionLiteral(FunctionLiteral* lit) {
  AstTraversalVisitor::VisitFunctionLiteral(lit);
  lit->set_function_literal_id(lit->function_literal_id() + delta_);
}

}  // namespace internal
}  // namespace v8
