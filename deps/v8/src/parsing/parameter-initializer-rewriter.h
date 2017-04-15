// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PARAMETER_EXPRESSION_REWRITER_H_
#define V8_PARSING_PARAMETER_EXPRESSION_REWRITER_H_

#include "src/ast/ast-types.h"

namespace v8 {
namespace internal {

class Expression;
class Scope;

// When an extra declaration scope needs to be inserted to account for
// a sloppy eval in a default parameter or function body, the expressions
// needs to be in that new inner scope which was added after initial
// parsing.
//
// param_scope is the new inner scope, and its outer_scope() is assumed
// to be the function scope which was used during the initial parse.
void ReparentParameterExpressionScope(uintptr_t stack_limit, Expression* expr,
                                      Scope* param_scope);

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PARAMETER_EXPRESSION_REWRITER_H_
