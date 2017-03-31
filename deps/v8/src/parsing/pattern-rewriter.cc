// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/ast.h"
#include "src/messages.h"
#include "src/objects-inl.h"
#include "src/parsing/parameter-initializer-rewriter.h"
#include "src/parsing/parser.h"

namespace v8 {

namespace internal {

void Parser::PatternRewriter::DeclareAndInitializeVariables(
    Parser* parser, Block* block,
    const DeclarationDescriptor* declaration_descriptor,
    const DeclarationParsingResult::Declaration* declaration,
    ZoneList<const AstRawString*>* names, bool* ok) {
  PatternRewriter rewriter;

  DCHECK(block->ignore_completion_value());

  rewriter.scope_ = declaration_descriptor->scope;
  rewriter.parser_ = parser;
  rewriter.context_ = BINDING;
  rewriter.pattern_ = declaration->pattern;
  rewriter.initializer_position_ = declaration->initializer_position;
  rewriter.block_ = block;
  rewriter.descriptor_ = declaration_descriptor;
  rewriter.names_ = names;
  rewriter.ok_ = ok;
  rewriter.recursion_level_ = 0;

  rewriter.RecurseIntoSubpattern(rewriter.pattern_, declaration->initializer);
}


void Parser::PatternRewriter::RewriteDestructuringAssignment(
    Parser* parser, RewritableExpression* to_rewrite, Scope* scope) {
  DCHECK(!scope->HasBeenRemoved());
  DCHECK(!to_rewrite->is_rewritten());

  bool ok = true;

  PatternRewriter rewriter;
  rewriter.scope_ = scope;
  rewriter.parser_ = parser;
  rewriter.context_ = ASSIGNMENT;
  rewriter.pattern_ = to_rewrite;
  rewriter.block_ = nullptr;
  rewriter.descriptor_ = nullptr;
  rewriter.names_ = nullptr;
  rewriter.ok_ = &ok;
  rewriter.recursion_level_ = 0;

  rewriter.RecurseIntoSubpattern(rewriter.pattern_, nullptr);
  DCHECK(ok);
}


Expression* Parser::PatternRewriter::RewriteDestructuringAssignment(
    Parser* parser, Assignment* assignment, Scope* scope) {
  DCHECK_NOT_NULL(assignment);
  DCHECK_EQ(Token::ASSIGN, assignment->op());
  auto to_rewrite = parser->factory()->NewRewritableExpression(assignment);
  RewriteDestructuringAssignment(parser, to_rewrite, scope);
  return to_rewrite->expression();
}


Parser::PatternRewriter::PatternContext
Parser::PatternRewriter::SetAssignmentContextIfNeeded(Expression* node) {
  PatternContext old_context = context();
  // AssignmentExpressions may occur in the Initializer position of a
  // SingleNameBinding. Such expressions should not prompt a change in the
  // pattern's context.
  if (node->IsAssignment() && node->AsAssignment()->op() == Token::ASSIGN &&
      !IsInitializerContext()) {
    set_context(ASSIGNMENT);
  }
  return old_context;
}


Parser::PatternRewriter::PatternContext
Parser::PatternRewriter::SetInitializerContextIfNeeded(Expression* node) {
  // Set appropriate initializer context for BindingElement and
  // AssignmentElement nodes
  PatternContext old_context = context();
  bool is_destructuring_assignment =
      node->IsRewritableExpression() &&
      !node->AsRewritableExpression()->is_rewritten();
  bool is_assignment =
      node->IsAssignment() && node->AsAssignment()->op() == Token::ASSIGN;
  if (is_destructuring_assignment || is_assignment) {
    switch (old_context) {
      case BINDING:
        set_context(INITIALIZER);
        break;
      case ASSIGNMENT:
        set_context(ASSIGNMENT_INITIALIZER);
        break;
      default:
        break;
    }
  }
  return old_context;
}


void Parser::PatternRewriter::VisitVariableProxy(VariableProxy* pattern) {
  Expression* value = current_value_;

  if (IsAssignmentContext()) {
    // In an assignment context, simply perform the assignment
    Assignment* assignment = factory()->NewAssignment(
        Token::ASSIGN, pattern, value, pattern->position());
    block_->statements()->Add(
        factory()->NewExpressionStatement(assignment, pattern->position()),
        zone());
    return;
  }

  descriptor_->scope->RemoveUnresolved(pattern);

  // Declare variable.
  // Note that we *always* must treat the initial value via a separate init
  // assignment for variables and constants because the value must be assigned
  // when the variable is encountered in the source. But the variable/constant
  // is declared (and set to 'undefined') upon entering the function within
  // which the variable or constant is declared. Only function variables have
  // an initial value in the declaration (because they are initialized upon
  // entering the function).
  const AstRawString* name = pattern->raw_name();
  VariableProxy* proxy =
      factory()->NewVariableProxy(name, NORMAL_VARIABLE, pattern->position());
  Declaration* declaration = factory()->NewVariableDeclaration(
      proxy, descriptor_->scope, descriptor_->declaration_pos);
  Variable* var = parser_->Declare(
      declaration, descriptor_->declaration_kind, descriptor_->mode,
      Variable::DefaultInitializationFlag(descriptor_->mode), ok_,
      descriptor_->hoist_scope);
  if (!*ok_) return;
  DCHECK_NOT_NULL(var);
  DCHECK(proxy->is_resolved());
  DCHECK(initializer_position_ != kNoSourcePosition);
  var->set_initializer_position(initializer_position_);

  // TODO(adamk): This should probably be checking hoist_scope.
  // Move it to Parser::Declare() to make it easier to test
  // the right scope.
  Scope* declaration_scope = IsLexicalVariableMode(descriptor_->mode)
                                 ? descriptor_->scope
                                 : descriptor_->scope->GetDeclarationScope();
  if (declaration_scope->num_var() > kMaxNumFunctionLocals) {
    parser_->ReportMessage(MessageTemplate::kTooManyVariables);
    *ok_ = false;
    return;
  }
  if (names_) {
    names_->Add(name, zone());
  }

  // If there's no initializer, we're done.
  if (value == nullptr) return;

  // A declaration of the form:
  //
  //    var v = x;
  //
  // is syntactic sugar for:
  //
  //    var v; v = x;
  //
  // In particular, we need to re-lookup 'v' as it may be a different
  // 'v' than the 'v' in the declaration (e.g., if we are inside a
  // 'with' statement or 'catch' block). Global var declarations
  // also need special treatment.
  Scope* var_init_scope = descriptor_->scope;

  if (descriptor_->mode == VAR && var_init_scope->is_script_scope()) {
    // Global variable declarations must be compiled in a specific
    // way. When the script containing the global variable declaration
    // is entered, the global variable must be declared, so that if it
    // doesn't exist (on the global object itself, see ES5 errata) it
    // gets created with an initial undefined value. This is handled
    // by the declarations part of the function representing the
    // top-level global code; see Runtime::DeclareGlobalVariable. If
    // it already exists (in the object or in a prototype), it is
    // *not* touched until the variable declaration statement is
    // executed.
    //
    // Executing the variable declaration statement will always
    // guarantee to give the global object an own property.
    // This way, global variable declarations can shadow
    // properties in the prototype chain, but only after the variable
    // declaration statement has been executed. This is important in
    // browsers where the global object (window) has lots of
    // properties defined in prototype objects.

    ZoneList<Expression*>* arguments =
        new (zone()) ZoneList<Expression*>(3, zone());
    arguments->Add(
        factory()->NewStringLiteral(name, descriptor_->declaration_pos),
        zone());
    arguments->Add(factory()->NewNumberLiteral(var_init_scope->language_mode(),
                                               kNoSourcePosition),
                   zone());
    arguments->Add(value, zone());

    CallRuntime* initialize = factory()->NewCallRuntime(
        Runtime::kInitializeVarGlobal, arguments, value->position());
    block_->statements()->Add(
        factory()->NewExpressionStatement(initialize, initialize->position()),
        zone());
  } else {
    // For 'let' and 'const' declared variables the initialization always
    // assigns to the declared variable.
    // But for var declarations we need to do a new lookup.
    if (descriptor_->mode == VAR) {
      proxy = var_init_scope->NewUnresolved(factory(), name);
      // TODO(neis): Set is_assigned on proxy.
    } else {
      DCHECK_NOT_NULL(proxy);
      DCHECK_NOT_NULL(proxy->var());
      if (var_init_scope->is_script_scope() ||
          var_init_scope->is_module_scope()) {
        // We have to pessimistically assume that top-level variables will be
        // assigned.  This is because there may be lazily parsed top-level
        // functions, which, for efficiency, we preparse without variable
        // tracking.
        proxy->set_is_assigned();
      }
    }
    // Add break location for destructured sub-pattern.
    int pos = IsSubPattern() ? pattern->position() : value->position();
    Assignment* assignment =
        factory()->NewAssignment(Token::INIT, proxy, value, pos);
    block_->statements()->Add(
        factory()->NewExpressionStatement(assignment, pos), zone());
  }
}


Variable* Parser::PatternRewriter::CreateTempVar(Expression* value) {
  auto temp = scope()->NewTemporary(ast_value_factory()->empty_string());
  if (value != nullptr) {
    auto assignment = factory()->NewAssignment(
        Token::ASSIGN, factory()->NewVariableProxy(temp), value,
        kNoSourcePosition);

    block_->statements()->Add(
        factory()->NewExpressionStatement(assignment, kNoSourcePosition),
        zone());
  }
  return temp;
}


void Parser::PatternRewriter::VisitRewritableExpression(
    RewritableExpression* node) {
  // If this is not a destructuring assignment...
  if (!IsAssignmentContext()) {
    // Mark the node as rewritten to prevent redundant rewriting, and
    // perform BindingPattern rewriting
    DCHECK(!node->is_rewritten());
    node->Rewrite(node->expression());
    return Visit(node->expression());
  } else if (!node->expression()->IsAssignment()) {
    return Visit(node->expression());
  }

  if (node->is_rewritten()) return;
  DCHECK(IsAssignmentContext());
  Assignment* assign = node->expression()->AsAssignment();
  DCHECK_NOT_NULL(assign);
  DCHECK_EQ(Token::ASSIGN, assign->op());

  auto initializer = assign->value();
  auto value = initializer;

  if (IsInitializerContext()) {
    // let {<pattern> = <init>} = <value>
    //   becomes
    // temp = <value>;
    // <pattern> = temp === undefined ? <init> : temp;
    auto temp_var = CreateTempVar(current_value_);
    Expression* is_undefined = factory()->NewCompareOperation(
        Token::EQ_STRICT, factory()->NewVariableProxy(temp_var),
        factory()->NewUndefinedLiteral(kNoSourcePosition), kNoSourcePosition);
    value = factory()->NewConditional(is_undefined, initializer,
                                      factory()->NewVariableProxy(temp_var),
                                      kNoSourcePosition);
  }

  PatternContext old_context = SetAssignmentContextIfNeeded(initializer);
  int pos = assign->position();
  Block* old_block = block_;
  block_ = factory()->NewBlock(nullptr, 8, true, pos);
  Variable* temp = nullptr;
  Expression* pattern = assign->target();
  Expression* old_value = current_value_;
  current_value_ = value;
  if (pattern->IsObjectLiteral()) {
    VisitObjectLiteral(pattern->AsObjectLiteral(), &temp);
  } else {
    DCHECK(pattern->IsArrayLiteral());
    VisitArrayLiteral(pattern->AsArrayLiteral(), &temp);
  }
  DCHECK_NOT_NULL(temp);
  current_value_ = old_value;
  Expression* expr = factory()->NewDoExpression(block_, temp, pos);
  node->Rewrite(expr);
  block_ = old_block;
  if (block_) {
    block_->statements()->Add(factory()->NewExpressionStatement(expr, pos),
                              zone());
  }
  set_context(old_context);
}

// When an extra declaration scope needs to be inserted to account for
// a sloppy eval in a default parameter or function body, the expressions
// needs to be in that new inner scope which was added after initial
// parsing.
void Parser::PatternRewriter::RewriteParameterScopes(Expression* expr) {
  if (!IsBindingContext()) return;
  if (descriptor_->declaration_kind != DeclarationDescriptor::PARAMETER) return;
  if (!scope()->is_block_scope()) return;

  DCHECK(scope()->is_declaration_scope());
  DCHECK(scope()->outer_scope()->is_function_scope());
  DCHECK(scope()->calls_sloppy_eval());

  ReparentParameterExpressionScope(parser_->stack_limit(), expr, scope());
}

void Parser::PatternRewriter::VisitObjectLiteral(ObjectLiteral* pattern,
                                                 Variable** temp_var) {
  auto temp = *temp_var = CreateTempVar(current_value_);

  block_->statements()->Add(parser_->BuildAssertIsCoercible(temp), zone());

  for (ObjectLiteralProperty* property : *pattern->properties()) {
    PatternContext context = SetInitializerContextIfNeeded(property->value());

    // Computed property names contain expressions which might require
    // scope rewriting.
    if (!property->key()->IsLiteral()) RewriteParameterScopes(property->key());

    RecurseIntoSubpattern(
        property->value(),
        factory()->NewProperty(factory()->NewVariableProxy(temp),
                               property->key(), kNoSourcePosition));
    set_context(context);
  }
}


void Parser::PatternRewriter::VisitObjectLiteral(ObjectLiteral* node) {
  Variable* temp_var = nullptr;
  VisitObjectLiteral(node, &temp_var);
}


void Parser::PatternRewriter::VisitArrayLiteral(ArrayLiteral* node,
                                                Variable** temp_var) {
  DCHECK(block_->ignore_completion_value());

  auto temp = *temp_var = CreateTempVar(current_value_);
  auto iterator = CreateTempVar(factory()->NewGetIterator(
      factory()->NewVariableProxy(temp), kNoSourcePosition));
  auto done =
      CreateTempVar(factory()->NewBooleanLiteral(false, kNoSourcePosition));
  auto result = CreateTempVar();
  auto v = CreateTempVar();
  auto completion = CreateTempVar();
  auto nopos = kNoSourcePosition;

  // For the purpose of iterator finalization, we temporarily set block_ to a
  // new block.  In the main body of this function, we write to block_ (both
  // explicitly and implicitly via recursion).  At the end of the function, we
  // wrap this new block in a try-finally statement, restore block_ to its
  // original value, and add the try-finally statement to block_.
  auto target = block_;
  block_ = factory()->NewBlock(nullptr, 8, true, nopos);

  Spread* spread = nullptr;
  for (Expression* value : *node->values()) {
    if (value->IsSpread()) {
      spread = value->AsSpread();
      break;
    }

    PatternContext context = SetInitializerContextIfNeeded(value);

    // if (!done) {
    //   done = true;  // If .next, .done or .value throws, don't close.
    //   result = IteratorNext(iterator);
    //   if (result.done) {
    //     v = undefined;
    //   } else {
    //     v = result.value;
    //     done = false;
    //   }
    // }
    Statement* if_not_done;
    {
      auto result_done = factory()->NewProperty(
          factory()->NewVariableProxy(result),
          factory()->NewStringLiteral(ast_value_factory()->done_string(),
                                      kNoSourcePosition),
          kNoSourcePosition);

      auto assign_undefined = factory()->NewAssignment(
          Token::ASSIGN, factory()->NewVariableProxy(v),
          factory()->NewUndefinedLiteral(kNoSourcePosition), kNoSourcePosition);

      auto assign_value = factory()->NewAssignment(
          Token::ASSIGN, factory()->NewVariableProxy(v),
          factory()->NewProperty(
              factory()->NewVariableProxy(result),
              factory()->NewStringLiteral(ast_value_factory()->value_string(),
                                          kNoSourcePosition),
              kNoSourcePosition),
          kNoSourcePosition);

      auto unset_done = factory()->NewAssignment(
          Token::ASSIGN, factory()->NewVariableProxy(done),
          factory()->NewBooleanLiteral(false, kNoSourcePosition),
          kNoSourcePosition);

      auto inner_else =
          factory()->NewBlock(nullptr, 2, true, kNoSourcePosition);
      inner_else->statements()->Add(
          factory()->NewExpressionStatement(assign_value, nopos), zone());
      inner_else->statements()->Add(
          factory()->NewExpressionStatement(unset_done, nopos), zone());

      auto inner_if = factory()->NewIfStatement(
          result_done,
          factory()->NewExpressionStatement(assign_undefined, nopos),
          inner_else, nopos);

      auto next_block =
          factory()->NewBlock(nullptr, 3, true, kNoSourcePosition);
      next_block->statements()->Add(
          factory()->NewExpressionStatement(
              factory()->NewAssignment(
                  Token::ASSIGN, factory()->NewVariableProxy(done),
                  factory()->NewBooleanLiteral(true, nopos), nopos),
              nopos),
          zone());
      next_block->statements()->Add(
          factory()->NewExpressionStatement(
              parser_->BuildIteratorNextResult(
                  factory()->NewVariableProxy(iterator), result,
                  kNoSourcePosition),
              kNoSourcePosition),
          zone());
      next_block->statements()->Add(inner_if, zone());

      if_not_done = factory()->NewIfStatement(
          factory()->NewUnaryOperation(
              Token::NOT, factory()->NewVariableProxy(done), kNoSourcePosition),
          next_block, factory()->NewEmptyStatement(kNoSourcePosition),
          kNoSourcePosition);
    }
    block_->statements()->Add(if_not_done, zone());

    if (!(value->IsLiteral() && value->AsLiteral()->raw_value()->IsTheHole())) {
      {
        // completion = kAbruptCompletion;
        Expression* proxy = factory()->NewVariableProxy(completion);
        Expression* assignment = factory()->NewAssignment(
            Token::ASSIGN, proxy,
            factory()->NewSmiLiteral(kAbruptCompletion, nopos), nopos);
        block_->statements()->Add(
            factory()->NewExpressionStatement(assignment, nopos), zone());
      }

      RecurseIntoSubpattern(value, factory()->NewVariableProxy(v));

      {
        // completion = kNormalCompletion;
        Expression* proxy = factory()->NewVariableProxy(completion);
        Expression* assignment = factory()->NewAssignment(
            Token::ASSIGN, proxy,
            factory()->NewSmiLiteral(kNormalCompletion, nopos), nopos);
        block_->statements()->Add(
            factory()->NewExpressionStatement(assignment, nopos), zone());
      }
    }
    set_context(context);
  }

  if (spread != nullptr) {
    // A spread can only occur as the last component.  It is not handled by
    // RecurseIntoSubpattern above.

    // let array = [];
    // while (!done) {
    //   done = true;  // If .next, .done or .value throws, don't close.
    //   result = IteratorNext(iterator);
    //   if (!result.done) {
    //     %AppendElement(array, result.value);
    //     done = false;
    //   }
    // }

    // let array = [];
    Variable* array;
    {
      auto empty_exprs = new (zone()) ZoneList<Expression*>(0, zone());
      array = CreateTempVar(factory()->NewArrayLiteral(
          empty_exprs,
          // Reuse pattern's literal index - it is unused since there is no
          // actual literal allocated.
          node->literal_index(), kNoSourcePosition));
    }

    // done = true;
    Statement* set_done = factory()->NewExpressionStatement(
        factory()->NewAssignment(
            Token::ASSIGN, factory()->NewVariableProxy(done),
            factory()->NewBooleanLiteral(true, nopos), nopos),
        nopos);

    // result = IteratorNext(iterator);
    Statement* get_next = factory()->NewExpressionStatement(
        parser_->BuildIteratorNextResult(factory()->NewVariableProxy(iterator),
                                         result, nopos),
        nopos);

    // %AppendElement(array, result.value);
    Statement* append_element;
    {
      auto args = new (zone()) ZoneList<Expression*>(2, zone());
      args->Add(factory()->NewVariableProxy(array), zone());
      args->Add(factory()->NewProperty(
                    factory()->NewVariableProxy(result),
                    factory()->NewStringLiteral(
                        ast_value_factory()->value_string(), nopos),
                    nopos),
                zone());
      append_element = factory()->NewExpressionStatement(
          factory()->NewCallRuntime(Runtime::kAppendElement, args, nopos),
          nopos);
    }

    // done = false;
    Statement* unset_done = factory()->NewExpressionStatement(
        factory()->NewAssignment(
            Token::ASSIGN, factory()->NewVariableProxy(done),
            factory()->NewBooleanLiteral(false, nopos), nopos),
        nopos);

    // if (!result.done) { #append_element; #unset_done }
    Statement* maybe_append_and_unset_done;
    {
      Expression* result_done =
          factory()->NewProperty(factory()->NewVariableProxy(result),
                                 factory()->NewStringLiteral(
                                     ast_value_factory()->done_string(), nopos),
                                 nopos);

      Block* then = factory()->NewBlock(nullptr, 2, true, nopos);
      then->statements()->Add(append_element, zone());
      then->statements()->Add(unset_done, zone());

      maybe_append_and_unset_done = factory()->NewIfStatement(
          factory()->NewUnaryOperation(Token::NOT, result_done, nopos), then,
          factory()->NewEmptyStatement(nopos), nopos);
    }

    // while (!done) {
    //   #set_done;
    //   #get_next;
    //   #maybe_append_and_unset_done;
    // }
    WhileStatement* loop = factory()->NewWhileStatement(nullptr, nopos);
    {
      Expression* condition = factory()->NewUnaryOperation(
          Token::NOT, factory()->NewVariableProxy(done), nopos);
      Block* body = factory()->NewBlock(nullptr, 3, true, nopos);
      body->statements()->Add(set_done, zone());
      body->statements()->Add(get_next, zone());
      body->statements()->Add(maybe_append_and_unset_done, zone());
      loop->Initialize(condition, body);
    }

    block_->statements()->Add(loop, zone());
    RecurseIntoSubpattern(spread->expression(),
                          factory()->NewVariableProxy(array));
  }

  Expression* closing_condition = factory()->NewUnaryOperation(
      Token::NOT, factory()->NewVariableProxy(done), nopos);

  parser_->FinalizeIteratorUse(scope(), completion, closing_condition, iterator,
                               block_, target);
  block_ = target;
}


void Parser::PatternRewriter::VisitArrayLiteral(ArrayLiteral* node) {
  Variable* temp_var = nullptr;
  VisitArrayLiteral(node, &temp_var);
}


void Parser::PatternRewriter::VisitAssignment(Assignment* node) {
  // let {<pattern> = <init>} = <value>
  //   becomes
  // temp = <value>;
  // <pattern> = temp === undefined ? <init> : temp;
  DCHECK_EQ(Token::ASSIGN, node->op());

  auto initializer = node->value();
  auto value = initializer;
  auto temp = CreateTempVar(current_value_);

  if (IsInitializerContext()) {
    Expression* is_undefined = factory()->NewCompareOperation(
        Token::EQ_STRICT, factory()->NewVariableProxy(temp),
        factory()->NewUndefinedLiteral(kNoSourcePosition), kNoSourcePosition);
    value = factory()->NewConditional(is_undefined, initializer,
                                      factory()->NewVariableProxy(temp),
                                      kNoSourcePosition);
  }

  // Initializer may have been parsed in the wrong scope.
  RewriteParameterScopes(initializer);

  PatternContext old_context = SetAssignmentContextIfNeeded(initializer);
  RecurseIntoSubpattern(node->target(), value);
  set_context(old_context);
}


// =============== AssignmentPattern only ==================

void Parser::PatternRewriter::VisitProperty(v8::internal::Property* node) {
  DCHECK(IsAssignmentContext());
  auto value = current_value_;

  Assignment* assignment =
      factory()->NewAssignment(Token::ASSIGN, node, value, node->position());

  block_->statements()->Add(
      factory()->NewExpressionStatement(assignment, kNoSourcePosition), zone());
}


// =============== UNREACHABLE =============================

#define NOT_A_PATTERN(Node)                                        \
  void Parser::PatternRewriter::Visit##Node(v8::internal::Node*) { \
    UNREACHABLE();                                                 \
  }

NOT_A_PATTERN(BinaryOperation)
NOT_A_PATTERN(Block)
NOT_A_PATTERN(BreakStatement)
NOT_A_PATTERN(Call)
NOT_A_PATTERN(CallNew)
NOT_A_PATTERN(CallRuntime)
NOT_A_PATTERN(CaseClause)
NOT_A_PATTERN(ClassLiteral)
NOT_A_PATTERN(CompareOperation)
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
NOT_A_PATTERN(GetIterator)
NOT_A_PATTERN(IfStatement)
NOT_A_PATTERN(Literal)
NOT_A_PATTERN(NativeFunctionLiteral)
NOT_A_PATTERN(RegExpLiteral)
NOT_A_PATTERN(ReturnStatement)
NOT_A_PATTERN(SloppyBlockFunctionStatement)
NOT_A_PATTERN(Spread)
NOT_A_PATTERN(SuperPropertyReference)
NOT_A_PATTERN(SuperCallReference)
NOT_A_PATTERN(SwitchStatement)
NOT_A_PATTERN(ThisFunction)
NOT_A_PATTERN(Throw)
NOT_A_PATTERN(TryCatchStatement)
NOT_A_PATTERN(TryFinallyStatement)
NOT_A_PATTERN(UnaryOperation)
NOT_A_PATTERN(VariableDeclaration)
NOT_A_PATTERN(WhileStatement)
NOT_A_PATTERN(WithStatement)
NOT_A_PATTERN(Yield)

#undef NOT_A_PATTERN
}  // namespace internal
}  // namespace v8
