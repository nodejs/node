// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TYPING_RESET_H_
#define V8_TYPING_RESET_H_

#include "src/ast/ast-expression-visitor.h"

namespace v8 {
namespace internal {

// A Visitor over a CompilationInfo's AST that resets
// typing bounds back to their default.

class TypingReseter : public AstExpressionVisitor {
 public:
  TypingReseter(Isolate* isolate, FunctionLiteral* root);

 protected:
  void VisitExpression(Expression* expression) override;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_TYPING_RESET_H_
