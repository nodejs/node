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
  // A function or arrow expression with a generated body. If
  // `parameter_types` is not null, the parameters get types (see
  // Parameters()), and their types go to `parameter_types`.
  fast::Expression* FunctionValue(
      int depth = 0, std::vector<JsType>* parameter_types = nullptr);
  // `count` parameters for a function whose body uses `nested`. Declares
  // each fresh name in `nested`. The last parameter can be a rest parameter
  // (an array), and the others can have a default value that uses the
  // earlier parameters. Appends the names to `names` if it is not null.
  // If `types` is not null, each parameter gets a type from
  // ParameterTypes(), the body sees the parameter with this type, and the
  // types go to `types`.
  std::vector<fast::Expression*> Parameters(
      GenContext& nested, int count, std::string_view prefix = "a",
      std::vector<std::string_view>* names = nullptr,
      std::vector<JsType>* types = nullptr);
  // Types for the `count` parameters of a new function: types that enough
  // visible variables have, so that calls can pass these variables.
  std::vector<JsType> ParameterTypes(int count);
  // Arguments for parameters of the given types: most arguments have the
  // type of their parameter, the others have any type.
  std::vector<fast::Expression*> ArgumentsFor(const std::vector<JsType>& types);
  // True if a function with `params` can have a "use strict" directive:
  // no default, rest or pattern parameter.
  static bool SimpleParameters(const std::vector<fast::Expression*>& params);
  // A "use strict" directive for the start of a function body.
  fast::Statement* UseStrict();
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

  // --- typed builtin arguments (Fuzzilli: findOrGenerateArguments and
  // generateTypeInternal, ProgramBuilder.swift:709-1084) ----------------------

  // A value for one typed parameter: a visible variable of the kind, or a
  // new value of the kind. A new value can be an enum string, an options
  // bag, or an instance that its typed constructor makes.
  fast::Expression* ArgumentFor(const ArgSpec& spec, int depth = 1);
  // Values for a typed parameter list. The list can stop at an optional
  // parameter, and a rest parameter takes 0 to 2 values.
  std::vector<fast::Expression*> ArgumentsFor(std::span<const ArgSpec> params,
                                              int depth = 1);
  // A new object of a builtin type of the model (`new DataView(buf)`,
  // `Temporal.PlainDate.from(...)`, `[].values()`), or nullptr.
  fast::Expression* Instance(JsType type, int depth = 1);
  // `Name`, or `A.B` for a dotted global of the model (`Temporal.PlainDate`).
  fast::Expression* GlobalPath(std::string_view path);
  // A visible variable whose type has methods of its own in the model (not
  // a plain object or function), or nullptr.
  fast::Identifier* TypedReceiver();

  // Builtin calls.
  fast::Expression* BuiltinCall();  // Math.max(..) etc.
  fast::Expression* MethodCall(fast::Expression* object, JsType type);
  // `object.name(args)`. Sometimes the callee is `object["name"]`, and
  // sometimes the call appends a spread of an array to the arguments.
  fast::CallExpression* CallMethod(fast::Expression* object,
                                   std::string_view name,
                                   std::vector<fast::Expression*> args);
  // A call of a method that returns a boolean, on a visible variable of a
  // type that has such methods (s.startsWith(x), re.test(s)), or nullptr.
  fast::Expression* BooleanMethodCall();
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
  // `var/let/const x = init;` with a name x of a small shared pool, or in
  // sloppy code `x = init;` for a new global (Fuzzilli's
  // NamedVariableGenerator). Registers x. nullptr if no such statement is
  // valid here.
  fast::Statement* SharedNameStatement(fast::Expression* init, JsType type);
  // Statements for a nested body (function/loop), via the generator set.
  std::vector<fast::Statement*> Body(GenContext& nested, int count);
  // A call of `callee` with random arguments. If `callee` names a function
  // with known parameter types (Builder::InfoOf), the arguments follow them.
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
  struct ExprDepthGuard {
    explicit ExprDepthGuard(GenContext& c) : depth(c.expr_depth) { ++depth; }
    ~ExprDepthGuard() { --depth; }
    int& depth;
  };
  bool TooDeep() const;
  fast::Expression* Leaf(JsType type);

  // Shared names of Declare(). This function returns a name of the shared
  // pool for a declaration of `kind` with `init` at this position. It
  // returns an empty name if such a declaration can be a SyntaxError. It
  // also does so if the declaration can put a reference into a TDZ.
  std::string_view SharedName(fast::VariableKind kind,
                              const fast::Expression* init);
  // True if no variable of this mutation and no statement of this block
  // uses `name`. With `hoists` (a `var`), the statements of the tree block
  // count even inside nested generated code.
  bool NameIsFreeHere(std::string_view name, bool hoists);
  // True if `x = v;` with this name creates a global property: no binding
  // of the name is in scope, and no scope is dynamic.
  bool ImplicitGlobalAllowed(std::string_view name);

  GenContext& ctx_;
};

// True for names usable as `obj.name` without quoting.
bool IsIdentifierName(std::string_view name);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_GENERATORS_CODEGEN_H_
