// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_GENERATORS_CODEGEN_H_
#define V8_EXPERIMENTAL_REDLINE_GENERATORS_CODEGEN_H_

#include <string_view>
#include <vector>

#include "experimental/redline/core/ast_builder.h"
#include "experimental/redline/generators/generator.h"

namespace redline {

// Expression and small-statement synthesis shared by all generators and by
// the expression-level mutators. Everything is typed only by the coarse
// JsType guesses; the V8 parser remains the validity oracle.
class CodeGen {
 public:
  explicit CodeGen(GenContext& ctx) : ctx_(ctx) {}

  GenContext& ctx() { return ctx_; }
  fast::Tree* tree() { return ctx_.tree(); }
  Random& rng() { return ctx_.rng(); }
  const Environment& env() { return ctx_.env; }

  // --- leaves ----------------------------------------------------------------

  fast::Expression* Literal(JsType type = JsType::kUnknown);
  fast::Expression* Integer();  // small or interesting integer
  fast::Expression* Number();
  fast::Expression* String();
  fast::Expression* Boolean();
  fast::Expression* BigInt();
  fast::Expression* RegExp();
  fast::Expression* Nullish();

  // A visible variable of `type` (kUnknown = any), or nullptr.
  fast::Identifier* Variable(JsType type = JsType::kUnknown);
  // A visible variable that may be assigned to, or nullptr.
  fast::Identifier* MutableVariable(JsType type = JsType::kUnknown);
  bool HasVariable(JsType type = JsType::kUnknown);

  // --- composite values ------------------------------------------------------

  // A value of (roughly) `type`: variable, literal or a small expression.
  fast::Expression* Value(JsType type = JsType::kUnknown, int depth = 0);
  // Any expression, possibly nested (depth-limited).
  fast::Expression* Expression(int depth = 0);
  fast::Expression* ObjectLiteral(int depth = 0);
  fast::Expression* ArrayLiteral(int depth = 0);
  // A function or arrow expression with a generated body.
  fast::Expression* FunctionValue(int depth = 0);
  // Something callable: a function variable, a builtin function or a new
  // arrow function.
  fast::Expression* Callable();
  // Something with properties: an object-ish variable or a literal.
  fast::Expression* ObjectLike();
  // An assignment target: a mutable variable or a member of an object.
  fast::Expression* Target();
  // `object.name` or `object[expr]` depending on the name.
  fast::Expression* Property(fast::Expression* object, std::string_view name);
  fast::Expression* Property(fast::Expression* object);  // random name
  std::string_view PropertyName();
  // Random key expression for computed access: string, number, symbol.
  fast::Expression* Key();

  // Arguments for a builtin function (respecting its arity and hints).
  std::vector<fast::Expression*> Arguments(const BuiltinFunction& function);
  std::vector<fast::Expression*> Arguments(int min_args, int max_args);

  // Builtin calls.
  fast::Expression* BuiltinCall();  // Math.max(..) etc.
  fast::Expression* MethodCall(fast::Expression* object, JsType type);
  fast::Expression* Construct(JsType type = JsType::kUnknown);  // new X(..)

  // Operators.
  fast::Expression* BinaryOperation(int depth = 0);
  fast::Expression* Comparison(int depth = 0);
  fast::Expression* UnaryOperation(int depth = 0);
  fast::Expression* Condition(int depth = 0);  // boolean-ish expression

  // --- statements ------------------------------------------------------------

  // `let/const/var name = init;` registering the new variable.
  fast::Statement* Declare(fast::Expression* init, JsType type,
                           std::string_view prefix = "v");
  // `let name; try { name = init; } catch (e) {}` registering the new
  // variable: for calls whose signature is unknown (discovered APIs,
  // methods applied to foreign receivers), where an exception would
  // otherwise end the program. `name` is undefined if `init` threw.
  std::vector<fast::Statement*> DeclareGuarded(fast::Expression* init,
                                               JsType type,
                                               std::string_view prefix = "v");
  // Declare() or DeclareGuarded() with the given probability of guarding.
  void DeclareMaybeGuarded(std::vector<fast::Statement*>* out,
                           fast::Expression* init, JsType type,
                           std::string_view prefix, double guard_probability);
  // Statements for a nested body (function/loop), via the generator set.
  std::vector<fast::Statement*> Body(GenContext& nested, int count);
  // A call of `callee` with random arguments.
  fast::CallExpression* CallWithArguments(fast::Expression* callee,
                                          int max_args = 3);
  // Wraps an expression in a statement.
  fast::Statement* Stmt(fast::Expression* e) { return mk::ExprStmt(tree(), e); }

  // Operator tables.
  static fast::BinaryOp RandomArithmeticOp(Random& rng);
  static fast::BinaryOp RandomComparisonOp(Random& rng);
  static fast::BinaryOp RandomBinaryOp(Random& rng);
  static fast::UnaryOp RandomUnaryOp(Random& rng);
  static fast::AssignOp RandomCompoundAssignOp(Random& rng);
  static fast::LogicalOp RandomLogicalOp(Random& rng);

 private:
  GenContext& ctx_;
};

// True for names usable as `obj.name` without quoting.
bool IsIdentifierName(std::string_view name);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_GENERATORS_CODEGEN_H_
