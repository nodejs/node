// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_AST_FUNCTION_LITERAL_ID_REINDEXER
#define V8_AST_AST_FUNCTION_LITERAL_ID_REINDEXER

#include "src/ast/ast-traversal-visitor.h"
#include "src/base/macros.h"

namespace v8 {
namespace internal {

// Changes the ID of all FunctionLiterals in the given Expression by adding the
// given delta.
class AstFunctionLiteralIdReindexer final
    : public AstTraversalVisitor<AstFunctionLiteralIdReindexer> {
 public:
  AstFunctionLiteralIdReindexer(size_t stack_limit, int delta);
  ~AstFunctionLiteralIdReindexer();

  void Reindex(Expression* pattern);

  // AstTraversalVisitor implementation.
  void VisitFunctionLiteral(FunctionLiteral* lit);

 private:
  int delta_;

  DISALLOW_COPY_AND_ASSIGN(AstFunctionLiteralIdReindexer);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_AST_FUNCTION_LITERAL_ID_REINDEXER
