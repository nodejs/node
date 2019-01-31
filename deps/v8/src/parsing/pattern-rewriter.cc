// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/ast.h"
#include "src/message-template.h"
#include "src/objects-inl.h"
#include "src/parsing/expression-scope-reparenter.h"
#include "src/parsing/parser.h"

namespace v8 {

namespace internal {

// An AST visitor which performs declaration and assignment related tasks,
// particularly for destructuring patterns:
//
//   1. Declares variables from variable proxies (particularly for destructuring
//      declarations),
//   2. Marks destructuring-assigned variable proxies as assigned, and
//   3. Rewrites scopes for parameters containing a sloppy eval.
//
// Historically this also rewrote destructuring assignments/declarations as a
// block of multiple assignments, hence the named, however this is now done
// during bytecode generation.
//
// TODO(leszeks): Rename or remove this class
class PatternRewriter final : public AstVisitor<PatternRewriter> {
 public:
  typedef Parser::DeclarationDescriptor DeclarationDescriptor;

  static void InitializeVariables(
      Parser* parser, VariableKind kind,
      const Parser::DeclarationParsingResult::Declaration* declaration);

 private:
  PatternRewriter(Parser* parser, VariableKind kind, int initializer_position,
                  bool declares_parameter_containing_sloppy_eval)
      : parser_(parser),
        initializer_position_(initializer_position),
        declares_parameter_containing_sloppy_eval_(
            declares_parameter_containing_sloppy_eval) {}

#define DECLARE_VISIT(type) void Visit##type(v8::internal::type* node);
  // Visiting functions for AST nodes make this an AstVisitor.
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  Expression* Visit(Assignment* assign) {
    if (parser_->has_error()) return parser_->FailureExpression();
    DCHECK_EQ(Token::ASSIGN, assign->op());

    Expression* pattern = assign->target();
    if (pattern->IsObjectLiteral()) {
      VisitObjectLiteral(pattern->AsObjectLiteral());
    } else {
      DCHECK(pattern->IsArrayLiteral());
      VisitArrayLiteral(pattern->AsArrayLiteral());
    }
    return assign;
  }

  void RewriteParameterScopes(Expression* expr);

  Scope* scope() const { return parser_->scope(); }

  Parser* const parser_;
  const int initializer_position_;
  const bool declares_parameter_containing_sloppy_eval_;

  DEFINE_AST_VISITOR_MEMBERS_WITHOUT_STACKOVERFLOW()
};

void Parser::InitializeVariables(
    ScopedPtrList<Statement>* statements, VariableKind kind,
    const DeclarationParsingResult::Declaration* declaration) {
  if (has_error()) return;

  if (!declaration->initializer) {
    // The parameter scope is only a block scope if the initializer calls sloppy
    // eval. Since there is no initializer, we can't be calling sloppy eval.
    DCHECK_IMPLIES(kind == PARAMETER_VARIABLE, scope()->is_function_scope());
    return;
  }

  PatternRewriter::InitializeVariables(this, kind, declaration);
  int pos = declaration->value_beg_position;
  if (pos == kNoSourcePosition) {
    pos = declaration->initializer_position;
  }
  Assignment* assignment = factory()->NewAssignment(
      Token::INIT, declaration->pattern, declaration->initializer, pos);
  statements->Add(factory()->NewExpressionStatement(assignment, pos));
}

void PatternRewriter::InitializeVariables(
    Parser* parser, VariableKind kind,
    const Parser::DeclarationParsingResult::Declaration* declaration) {
  PatternRewriter rewriter(
      parser, kind, declaration->initializer_position,
      kind == PARAMETER_VARIABLE && parser->scope()->is_block_scope());

  rewriter.Visit(declaration->pattern);
}

void PatternRewriter::VisitVariableProxy(VariableProxy* proxy) {
  DCHECK(!parser_->has_error());
  Variable* var =
      proxy->is_resolved()
          ? proxy->var()
          : scope()->GetDeclarationScope()->LookupLocal(proxy->raw_name());

  DCHECK_NOT_NULL(var);

  DCHECK_NE(initializer_position_, kNoSourcePosition);
  var->set_initializer_position(initializer_position_);
}

// When an extra declaration scope needs to be inserted to account for
// a sloppy eval in a default parameter or function body, the expressions
// needs to be in that new inner scope which was added after initial
// parsing.
void PatternRewriter::RewriteParameterScopes(Expression* expr) {
  if (declares_parameter_containing_sloppy_eval_) {
    ReparentExpressionScope(parser_->stack_limit(), expr, scope());
  }
}

void PatternRewriter::VisitObjectLiteral(ObjectLiteral* pattern) {
  for (ObjectLiteralProperty* property : *pattern->properties()) {
    Expression* key = property->key();
    if (!key->IsLiteral()) {
      // Computed property names contain expressions which might require
      // scope rewriting.
      RewriteParameterScopes(key);
    }
    Visit(property->value());
  }
}

void PatternRewriter::VisitArrayLiteral(ArrayLiteral* node) {
  for (Expression* value : *node->values()) {
    if (value->IsTheHoleLiteral()) continue;
    Visit(value);
  }
}

void PatternRewriter::VisitAssignment(Assignment* node) {
  DCHECK_EQ(Token::ASSIGN, node->op());

  // Initializer may have been parsed in the wrong scope.
  RewriteParameterScopes(node->value());

  Visit(node->target());
}

void PatternRewriter::VisitSpread(Spread* node) { Visit(node->expression()); }

// =============== UNREACHABLE =============================

#define NOT_A_PATTERN(Node) \
  void PatternRewriter::Visit##Node(v8::internal::Node*) { UNREACHABLE(); }

NOT_A_PATTERN(BinaryOperation)
NOT_A_PATTERN(NaryOperation)
NOT_A_PATTERN(Block)
NOT_A_PATTERN(BreakStatement)
NOT_A_PATTERN(Call)
NOT_A_PATTERN(CallNew)
NOT_A_PATTERN(CallRuntime)
NOT_A_PATTERN(ClassLiteral)
NOT_A_PATTERN(CompareOperation)
NOT_A_PATTERN(CompoundAssignment)
NOT_A_PATTERN(Conditional)
NOT_A_PATTERN(ContinueStatement)
NOT_A_PATTERN(CountOperation)
NOT_A_PATTERN(DebuggerStatement)
NOT_A_PATTERN(DoExpression)
NOT_A_PATTERN(DoWhileStatement)
NOT_A_PATTERN(EmptyStatement)
NOT_A_PATTERN(EmptyParentheses)
NOT_A_PATTERN(ExpressionStatement)
NOT_A_PATTERN(ForInStatement)
NOT_A_PATTERN(ForOfStatement)
NOT_A_PATTERN(ForStatement)
NOT_A_PATTERN(FunctionDeclaration)
NOT_A_PATTERN(FunctionLiteral)
NOT_A_PATTERN(GetTemplateObject)
NOT_A_PATTERN(IfStatement)
NOT_A_PATTERN(ImportCallExpression)
NOT_A_PATTERN(Literal)
NOT_A_PATTERN(NativeFunctionLiteral)
NOT_A_PATTERN(Property)
NOT_A_PATTERN(RegExpLiteral)
NOT_A_PATTERN(ResolvedProperty)
NOT_A_PATTERN(ReturnStatement)
NOT_A_PATTERN(SloppyBlockFunctionStatement)
NOT_A_PATTERN(StoreInArrayLiteral)
NOT_A_PATTERN(SuperPropertyReference)
NOT_A_PATTERN(SuperCallReference)
NOT_A_PATTERN(SwitchStatement)
NOT_A_PATTERN(TemplateLiteral)
NOT_A_PATTERN(ThisFunction)
NOT_A_PATTERN(Throw)
NOT_A_PATTERN(TryCatchStatement)
NOT_A_PATTERN(TryFinallyStatement)
NOT_A_PATTERN(UnaryOperation)
NOT_A_PATTERN(VariableDeclaration)
NOT_A_PATTERN(WhileStatement)
NOT_A_PATTERN(WithStatement)
NOT_A_PATTERN(Yield)
NOT_A_PATTERN(YieldStar)
NOT_A_PATTERN(Await)
NOT_A_PATTERN(InitializeClassMembersStatement)

#undef NOT_A_PATTERN
}  // namespace internal
}  // namespace v8
