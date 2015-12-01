// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/typing-reset.h"

#include "src/ast.h"
#include "src/codegen.h"
#include "src/scopes.h"

namespace v8 {
namespace internal {


TypingReseter::TypingReseter(Isolate* isolate, Zone* zone,
                             FunctionLiteral* root)
    : AstExpressionVisitor(isolate, zone, root) {}


void TypingReseter::VisitExpression(Expression* expression) {
  expression->set_bounds(Bounds::Unbounded());
}
}
}  // namespace v8::internal
