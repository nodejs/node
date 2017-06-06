// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_REWRITER_H_
#define V8_PARSING_REWRITER_H_

namespace v8 {
namespace internal {

class AstValueFactory;
class DoExpression;
class Isolate;
class ParseInfo;
class Parser;
class DeclarationScope;
class Scope;

class Rewriter {
 public:
  // Rewrite top-level code (ECMA 262 "programs") so as to conservatively
  // include an assignment of the value of the last statement in the code to
  // a compiler-generated temporary variable wherever needed.
  //
  // Assumes code has been parsed and scopes have been analyzed.  Mutates the
  // AST, so the AST should not continue to be used in the case of failure.
  static bool Rewrite(ParseInfo* info, Isolate* isolate);

  // Rewrite a list of statements, using the same rules as a top-level program,
  // to ensure identical behaviour of completion result.  The temporary is added
  // to the closure scope of the do-expression, which matches the closure scope
  // of the outer scope (the do-expression itself runs in a block scope, not a
  // closure scope). This closure scope needs to be passed in since the
  // do-expression could have dropped its own block scope.
  static bool Rewrite(Parser* parser, DeclarationScope* closure_scope,
                      DoExpression* expr, AstValueFactory* factory);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_REWRITER_H_
