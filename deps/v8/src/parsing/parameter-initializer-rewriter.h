// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PARAMETER_EXPRESSION_REWRITER_H_
#define V8_PARSING_PARAMETER_EXPRESSION_REWRITER_H_

#include "src/ast/ast.h"

namespace v8 {
namespace internal {


void RewriteParameterInitializerScope(uintptr_t stack_limit,
                                      Expression* initializer, Scope* old_scope,
                                      Scope* new_scope);


}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PARAMETER_EXPRESSION_REWRITER_H_
