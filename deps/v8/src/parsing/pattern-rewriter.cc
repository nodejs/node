// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/ast.h"
#include "src/messages.h"
#include "src/objects-inl.h"
#include "src/parsing/expression-scope-reparenter.h"
#include "src/parsing/parser.h"

namespace v8 {

namespace internal {

class PatternRewriter final : public AstVisitor<PatternRewriter> {
 public:
  // Limit the allowed number of local variables in a function. The hard limit
  // is that offsets computed by FullCodeGenerator::StackOperand and similar
  // functions are ints, and they should not overflow. In addition, accessing
  // local variables creates user-controlled constants in the generated code,
  // and we don't want too much user-controlled memory inside the code (this was
  // the reason why this limit was introduced in the first place; see
  // https://codereview.chromium.org/7003030/ ).
  static const int kMaxNumFunctionLocals = 4194303;  // 2^22-1

  typedef Parser::DeclarationDescriptor DeclarationDescriptor;

  static void DeclareAndInitializeVariables(
      Parser* parser, Block* block,
      const DeclarationDescriptor* declaration_descriptor,
      const Parser::DeclarationParsingResult::Declaration* declaration,
      ZoneList<const AstRawString*>* names, bool* ok);

  static void RewriteDestructuringAssignment(Parser* parser,
                                             RewritableExpression* to_rewrite,
                                             Scope* scope);

 private:
  enum PatternContext { BINDING, ASSIGNMENT, ASSIGNMENT_ELEMENT };

  class AssignmentElementScope {
   public:
    explicit AssignmentElementScope(PatternRewriter* rewriter)
        : rewriter_(rewriter), context_(rewriter->context()) {
      if (context_ == ASSIGNMENT) rewriter->context_ = ASSIGNMENT_ELEMENT;
    }
    ~AssignmentElementScope() { rewriter_->context_ = context_; }

   private:
    PatternRewriter* const rewriter_;
    const PatternContext context_;
  };

  PatternRewriter(Scope* scope, Parser* parser, PatternContext context)
      : scope_(scope),
        parser_(parser),
        context_(context),
        initializer_position_(kNoSourcePosition),
        value_beg_position_(kNoSourcePosition),
        block_(nullptr),
        descriptor_(nullptr),
        names_(nullptr),
        current_value_(nullptr),
        recursion_level_(0),
        ok_(nullptr) {}

#define DECLARE_VISIT(type) void Visit##type(v8::internal::type* node);
  // Visiting functions for AST nodes make this an AstVisitor.
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  PatternContext context() const { return context_; }

  void RecurseIntoSubpattern(AstNode* pattern, Expression* value) {
    Expression* old_value = current_value_;
    current_value_ = value;
    recursion_level_++;
    Visit(pattern);
    recursion_level_--;
    current_value_ = old_value;
  }

  void VisitObjectLiteral(ObjectLiteral* node, Variable** temp_var);
  void VisitArrayLiteral(ArrayLiteral* node, Variable** temp_var);

  bool IsBindingContext() const { return context_ == BINDING; }
  bool IsAssignmentContext() const {
    return context_ == ASSIGNMENT || context_ == ASSIGNMENT_ELEMENT;
  }
  bool IsSubPattern() const { return recursion_level_ > 1; }

  bool DeclaresParameterContainingSloppyEval() const;
  void RewriteParameterScopes(Expression* expr);

  Variable* CreateTempVar(Expression* value = nullptr);

  AstNodeFactory* factory() const { return parser_->factory(); }
  AstValueFactory* ast_value_factory() const {
    return parser_->ast_value_factory();
  }
  Zone* zone() const { return parser_->zone(); }
  Scope* scope() const { return scope_; }

  Scope* const scope_;
  Parser* const parser_;
  PatternContext context_;
  int initializer_position_;
  int value_beg_position_;
  Block* block_;
  const DeclarationDescriptor* descriptor_;
  ZoneList<const AstRawString*>* names_;
  Expression* current_value_;
  int recursion_level_;
  bool* ok_;

  DEFINE_AST_VISITOR_MEMBERS_WITHOUT_STACKOVERFLOW()
};

void Parser::DeclareAndInitializeVariables(
    Block* block, const DeclarationDescriptor* declaration_descriptor,
    const DeclarationParsingResult::Declaration* declaration,
    ZoneList<const AstRawString*>* names, bool* ok) {
  PatternRewriter::DeclareAndInitializeVariables(
      this, block, declaration_descriptor, declaration, names, ok);
}

void Parser::RewriteDestructuringAssignment(RewritableExpression* to_rewrite) {
  PatternRewriter::RewriteDestructuringAssignment(this, to_rewrite, scope());
}

Expression* Parser::RewriteDestructuringAssignment(Assignment* assignment) {
  DCHECK_NOT_NULL(assignment);
  DCHECK_EQ(Token::ASSIGN, assignment->op());
  auto to_rewrite = factory()->NewRewritableExpression(assignment, scope());
  RewriteDestructuringAssignment(to_rewrite);
  return to_rewrite->expression();
}

void PatternRewriter::DeclareAndInitializeVariables(
    Parser* parser, Block* block,
    const DeclarationDescriptor* declaration_descriptor,
    const Parser::DeclarationParsingResult::Declaration* declaration,
    ZoneList<const AstRawString*>* names, bool* ok) {
  DCHECK(block->ignore_completion_value());

  PatternRewriter rewriter(declaration_descriptor->scope, parser, BINDING);
  rewriter.initializer_position_ = declaration->initializer_position;
  rewriter.value_beg_position_ = declaration->value_beg_position;
  rewriter.block_ = block;
  rewriter.descriptor_ = declaration_descriptor;
  rewriter.names_ = names;
  rewriter.ok_ = ok;

  rewriter.RecurseIntoSubpattern(declaration->pattern,
                                 declaration->initializer);
}

void PatternRewriter::RewriteDestructuringAssignment(
    Parser* parser, RewritableExpression* to_rewrite, Scope* scope) {
  DCHECK(!scope->HasBeenRemoved());
  DCHECK(!to_rewrite->is_rewritten());

  PatternRewriter rewriter(scope, parser, ASSIGNMENT);
  rewriter.RecurseIntoSubpattern(to_rewrite, nullptr);
}

void PatternRewriter::VisitVariableProxy(VariableProxy* pattern) {
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

  DCHECK_NOT_NULL(block_);
  DCHECK_NOT_NULL(descriptor_);
  DCHECK_NOT_NULL(ok_);

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
  Declaration* declaration;
  if (descriptor_->mode == VAR && !descriptor_->scope->is_declaration_scope()) {
    DCHECK(descriptor_->scope->is_block_scope() ||
           descriptor_->scope->is_with_scope());
    declaration = factory()->NewNestedVariableDeclaration(
        proxy, descriptor_->scope, descriptor_->declaration_pos);
  } else {
    declaration =
        factory()->NewVariableDeclaration(proxy, descriptor_->declaration_pos);
  }

  // When an extra declaration scope needs to be inserted to account for
  // a sloppy eval in a default parameter or function body, the parameter
  // needs to be declared in the function's scope, not in the varblock
  // scope which will be used for the initializer expression.
  Scope* outer_function_scope = nullptr;
  if (DeclaresParameterContainingSloppyEval()) {
    outer_function_scope = descriptor_->scope->outer_scope();
  }
  Variable* var = parser_->Declare(
      declaration, descriptor_->declaration_kind, descriptor_->mode,
      Variable::DefaultInitializationFlag(descriptor_->mode), ok_,
      outer_function_scope);
  if (!*ok_) return;
  DCHECK_NOT_NULL(var);
  DCHECK(proxy->is_resolved());
  DCHECK_NE(initializer_position_, kNoSourcePosition);
  var->set_initializer_position(initializer_position_);

  Scope* declaration_scope =
      outer_function_scope != nullptr
          ? outer_function_scope
          : (IsLexicalVariableMode(descriptor_->mode)
                 ? descriptor_->scope
                 : descriptor_->scope->GetDeclarationScope());
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

  Scope* var_init_scope = descriptor_->scope;
  Parser::MarkLoopVariableAsAssigned(var_init_scope, proxy->var(),
                                     descriptor_->declaration_kind);

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

  // For 'let' and 'const' declared variables the initialization always
  // assigns to the declared variable.
  // But for var declarations we need to do a new lookup.
  if (descriptor_->mode == VAR) {
    proxy = var_init_scope->NewUnresolved(factory(), name);
  } else {
    DCHECK_NOT_NULL(proxy);
    DCHECK_NOT_NULL(proxy->var());
  }
  // Add break location for destructured sub-pattern.
  int pos = value_beg_position_;
  if (pos == kNoSourcePosition) {
    pos = IsSubPattern() ? pattern->position() : value->position();
  }
  Assignment* assignment =
      factory()->NewAssignment(Token::INIT, proxy, value, pos);
  block_->statements()->Add(factory()->NewExpressionStatement(assignment, pos),
                            zone());
}

Variable* PatternRewriter::CreateTempVar(Expression* value) {
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

void PatternRewriter::VisitRewritableExpression(RewritableExpression* node) {
  if (!node->expression()->IsAssignment()) {
    // RewritableExpressions are also used for desugaring Spread, which is
    // orthogonal to PatternRewriter; just visit the underlying expression.
    DCHECK_EQ(AstNode::kArrayLiteral, node->expression()->node_type());
    return Visit(node->expression());
  } else if (context() != ASSIGNMENT) {
    // This is not a destructuring assignment. Mark the node as rewritten to
    // prevent redundant rewriting and visit the underlying expression.
    DCHECK(!node->is_rewritten());
    node->set_rewritten();
    return Visit(node->expression());
  }

  DCHECK(!node->is_rewritten());
  DCHECK_EQ(ASSIGNMENT, context());
  Assignment* assign = node->expression()->AsAssignment();
  DCHECK_NOT_NULL(assign);
  DCHECK_EQ(Token::ASSIGN, assign->op());

  int pos = assign->position();
  Block* old_block = block_;
  block_ = factory()->NewBlock(8, true);
  Variable* temp = nullptr;
  Expression* pattern = assign->target();
  Expression* old_value = current_value_;
  current_value_ = assign->value();
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
}

bool PatternRewriter::DeclaresParameterContainingSloppyEval() const {
  // Need to check for a binding context to make sure we have a descriptor.
  if (IsBindingContext() &&
      // Only relevant for parameters.
      descriptor_->declaration_kind == DeclarationDescriptor::PARAMETER &&
      // And only when scope is a block scope;
      // without eval, it is a function scope.
      scope()->is_block_scope()) {
    DCHECK(scope()->is_declaration_scope());
    DCHECK(scope()->AsDeclarationScope()->calls_sloppy_eval());
    DCHECK(scope()->outer_scope()->is_function_scope());
    return true;
  }

  return false;
}

// When an extra declaration scope needs to be inserted to account for
// a sloppy eval in a default parameter or function body, the expressions
// needs to be in that new inner scope which was added after initial
// parsing.
void PatternRewriter::RewriteParameterScopes(Expression* expr) {
  if (DeclaresParameterContainingSloppyEval()) {
    ReparentExpressionScope(parser_->stack_limit(), expr, scope());
  }
}

void PatternRewriter::VisitObjectLiteral(ObjectLiteral* pattern,
                                         Variable** temp_var) {
  auto temp = *temp_var = CreateTempVar(current_value_);

  ZoneList<Expression*>* rest_runtime_callargs = nullptr;
  if (pattern->has_rest_property()) {
    // non_rest_properties_count = pattern->properties()->length - 1;
    // args_length = 1 + non_rest_properties_count because we need to
    // pass temp as well to the runtime function.
    int args_length = pattern->properties()->length();
    rest_runtime_callargs =
        new (zone()) ZoneList<Expression*>(args_length, zone());
    rest_runtime_callargs->Add(factory()->NewVariableProxy(temp), zone());
  }

  block_->statements()->Add(parser_->BuildAssertIsCoercible(temp, pattern),
                            zone());

  for (ObjectLiteralProperty* property : *pattern->properties()) {
    Expression* value;

    if (property->kind() == ObjectLiteralProperty::Kind::SPREAD) {
      // var { y, [x++]: a, ...c } = temp
      //     becomes
      // var y = temp.y;
      // var temp1 = %ToName(x++);
      // var a = temp[temp1];
      // var c;
      // c = %CopyDataPropertiesWithExcludedProperties(temp, "y", temp1);
      value = factory()->NewCallRuntime(
          Runtime::kCopyDataPropertiesWithExcludedProperties,
          rest_runtime_callargs, kNoSourcePosition);
    } else {
      Expression* key = property->key();

      if (!key->IsLiteral()) {
        // Computed property names contain expressions which might require
        // scope rewriting.
        RewriteParameterScopes(key);
      }

      if (pattern->has_rest_property()) {
        Expression* excluded_property = key;

        if (property->is_computed_name()) {
          DCHECK(!key->IsPropertyName() || !key->IsNumberLiteral());
          auto args = new (zone()) ZoneList<Expression*>(1, zone());
          args->Add(key, zone());
          auto to_name_key = CreateTempVar(factory()->NewCallRuntime(
              Runtime::kToName, args, kNoSourcePosition));
          key = factory()->NewVariableProxy(to_name_key);
          excluded_property = factory()->NewVariableProxy(to_name_key);
        } else {
          DCHECK(key->IsPropertyName() || key->IsNumberLiteral());
        }

        DCHECK_NOT_NULL(rest_runtime_callargs);
        rest_runtime_callargs->Add(excluded_property, zone());
      }

      value = factory()->NewProperty(factory()->NewVariableProxy(temp), key,
                                     kNoSourcePosition);
    }

    AssignmentElementScope element_scope(this);
    RecurseIntoSubpattern(property->value(), value);
  }
}

void PatternRewriter::VisitObjectLiteral(ObjectLiteral* node) {
  Variable* temp_var = nullptr;
  VisitObjectLiteral(node, &temp_var);
}

void PatternRewriter::VisitArrayLiteral(ArrayLiteral* node,
                                        Variable** temp_var) {
  DCHECK(block_->ignore_completion_value());

  auto temp = *temp_var = CreateTempVar(current_value_);
  auto iterator = CreateTempVar(factory()->NewGetIterator(
      factory()->NewVariableProxy(temp), current_value_, IteratorType::kNormal,
      current_value_->position()));
  auto next = CreateTempVar(factory()->NewProperty(
      factory()->NewVariableProxy(iterator),
      factory()->NewStringLiteral(ast_value_factory()->next_string(),
                                  kNoSourcePosition),
      kNoSourcePosition));
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
  block_ = factory()->NewBlock(8, true);

  Spread* spread = nullptr;
  for (Expression* value : *node->values()) {
    if (value->IsSpread()) {
      spread = value->AsSpread();
      break;
    }

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

      auto inner_else = factory()->NewBlock(2, true);
      inner_else->statements()->Add(
          factory()->NewExpressionStatement(assign_value, nopos), zone());
      inner_else->statements()->Add(
          factory()->NewExpressionStatement(unset_done, nopos), zone());

      auto inner_if = factory()->NewIfStatement(
          result_done,
          factory()->NewExpressionStatement(assign_undefined, nopos),
          inner_else, nopos);

      auto next_block = factory()->NewBlock(3, true);
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
                  factory()->NewVariableProxy(iterator),
                  factory()->NewVariableProxy(next), result,
                  IteratorType::kNormal, kNoSourcePosition),
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

    if (!value->IsTheHoleLiteral()) {
      {
        // completion = kAbruptCompletion;
        Expression* proxy = factory()->NewVariableProxy(completion);
        Expression* assignment = factory()->NewAssignment(
            Token::ASSIGN, proxy,
            factory()->NewSmiLiteral(Parser::kAbruptCompletion, nopos), nopos);
        block_->statements()->Add(
            factory()->NewExpressionStatement(assignment, nopos), zone());
      }

      {
        AssignmentElementScope element_scope(this);
        RecurseIntoSubpattern(value, factory()->NewVariableProxy(v));
      }

      {
        // completion = kNormalCompletion;
        Expression* proxy = factory()->NewVariableProxy(completion);
        Expression* assignment = factory()->NewAssignment(
            Token::ASSIGN, proxy,
            factory()->NewSmiLiteral(Parser::kNormalCompletion, nopos), nopos);
        block_->statements()->Add(
            factory()->NewExpressionStatement(assignment, nopos), zone());
      }
    }
  }

  if (spread != nullptr) {
    // A spread can only occur as the last component.  It is not handled by
    // RecurseIntoSubpattern above.

    // let array = [];
    // let index = 0;
    // while (!done) {
    //   done = true;  // If .next, .done or .value throws, don't close.
    //   result = IteratorNext(iterator);
    //   if (!result.done) {
    //     StoreInArrayLiteral(array, index, result.value);
    //     done = false;
    //   }
    //   index++;
    // }

    // let array = [];
    Variable* array;
    {
      auto empty_exprs = new (zone()) ZoneList<Expression*>(0, zone());
      array = CreateTempVar(
          factory()->NewArrayLiteral(empty_exprs, kNoSourcePosition));
    }

    // let index = 0;
    Variable* index =
        CreateTempVar(factory()->NewSmiLiteral(0, kNoSourcePosition));

    // done = true;
    Statement* set_done = factory()->NewExpressionStatement(
        factory()->NewAssignment(
            Token::ASSIGN, factory()->NewVariableProxy(done),
            factory()->NewBooleanLiteral(true, nopos), nopos),
        nopos);

    // result = IteratorNext(iterator);
    Statement* get_next = factory()->NewExpressionStatement(
        parser_->BuildIteratorNextResult(factory()->NewVariableProxy(iterator),
                                         factory()->NewVariableProxy(next),
                                         result, IteratorType::kNormal, nopos),
        nopos);

    // StoreInArrayLiteral(array, index, result.value);
    Statement* store;
    {
      auto value = factory()->NewProperty(
          factory()->NewVariableProxy(result),
          factory()->NewStringLiteral(ast_value_factory()->value_string(),
                                      nopos),
          nopos);
      store = factory()->NewExpressionStatement(
          factory()->NewStoreInArrayLiteral(factory()->NewVariableProxy(array),
                                            factory()->NewVariableProxy(index),
                                            value, nopos),
          nopos);
    }

    // done = false;
    Statement* unset_done = factory()->NewExpressionStatement(
        factory()->NewAssignment(
            Token::ASSIGN, factory()->NewVariableProxy(done),
            factory()->NewBooleanLiteral(false, nopos), nopos),
        nopos);

    // if (!result.done) { #store; #unset_done }
    Statement* maybe_store_and_unset_done;
    {
      Expression* result_done =
          factory()->NewProperty(factory()->NewVariableProxy(result),
                                 factory()->NewStringLiteral(
                                     ast_value_factory()->done_string(), nopos),
                                 nopos);

      Block* then = factory()->NewBlock(2, true);
      then->statements()->Add(store, zone());
      then->statements()->Add(unset_done, zone());

      maybe_store_and_unset_done = factory()->NewIfStatement(
          factory()->NewUnaryOperation(Token::NOT, result_done, nopos), then,
          factory()->NewEmptyStatement(nopos), nopos);
    }

    // index++;
    Statement* increment_index;
    {
      increment_index = factory()->NewExpressionStatement(
          factory()->NewCountOperation(
              Token::INC, false, factory()->NewVariableProxy(index), nopos),
          nopos);
    }

    // while (!done) {
    //   #set_done;
    //   #get_next;
    //   #maybe_store_and_unset_done;
    //   #increment_index;
    // }
    WhileStatement* loop = factory()->NewWhileStatement(nullptr, nopos);
    {
      Expression* condition = factory()->NewUnaryOperation(
          Token::NOT, factory()->NewVariableProxy(done), nopos);
      Block* body = factory()->NewBlock(4, true);
      body->statements()->Add(set_done, zone());
      body->statements()->Add(get_next, zone());
      body->statements()->Add(maybe_store_and_unset_done, zone());
      body->statements()->Add(increment_index, zone());
      loop->Initialize(condition, body);
    }

    block_->statements()->Add(loop, zone());
    RecurseIntoSubpattern(spread->expression(),
                          factory()->NewVariableProxy(array));
  }

  Expression* closing_condition = factory()->NewUnaryOperation(
      Token::NOT, factory()->NewVariableProxy(done), nopos);

  parser_->FinalizeIteratorUse(completion, closing_condition, iterator, block_,
                               target, IteratorType::kNormal);
  block_ = target;
}

void PatternRewriter::VisitArrayLiteral(ArrayLiteral* node) {
  Variable* temp_var = nullptr;
  VisitArrayLiteral(node, &temp_var);
}

void PatternRewriter::VisitAssignment(Assignment* node) {
  // let {<pattern> = <init>} = <value>
  //   becomes
  // temp = <value>;
  // <pattern> = temp === undefined ? <init> : temp;
  DCHECK_EQ(Token::ASSIGN, node->op());

  // Rewriting of Assignment nodes for destructuring assignment
  // is handled in VisitRewritableExpression().
  DCHECK_NE(ASSIGNMENT, context());

  auto initializer = node->value();
  auto value = initializer;
  auto temp = CreateTempVar(current_value_);

  Expression* is_undefined = factory()->NewCompareOperation(
      Token::EQ_STRICT, factory()->NewVariableProxy(temp),
      factory()->NewUndefinedLiteral(kNoSourcePosition), kNoSourcePosition);
  value = factory()->NewConditional(is_undefined, initializer,
                                    factory()->NewVariableProxy(temp),
                                    kNoSourcePosition);

  // Initializer may have been parsed in the wrong scope.
  RewriteParameterScopes(initializer);

  RecurseIntoSubpattern(node->target(), value);
}


// =============== AssignmentPattern only ==================

void PatternRewriter::VisitProperty(v8::internal::Property* node) {
  DCHECK(IsAssignmentContext());
  auto value = current_value_;

  Assignment* assignment =
      factory()->NewAssignment(Token::ASSIGN, node, value, node->position());

  block_->statements()->Add(
      factory()->NewExpressionStatement(assignment, kNoSourcePosition), zone());
}


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
NOT_A_PATTERN(GetIterator)
NOT_A_PATTERN(GetTemplateObject)
NOT_A_PATTERN(IfStatement)
NOT_A_PATTERN(ImportCallExpression)
NOT_A_PATTERN(Literal)
NOT_A_PATTERN(NativeFunctionLiteral)
NOT_A_PATTERN(RegExpLiteral)
NOT_A_PATTERN(ResolvedProperty)
NOT_A_PATTERN(ReturnStatement)
NOT_A_PATTERN(SloppyBlockFunctionStatement)
NOT_A_PATTERN(Spread)
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
NOT_A_PATTERN(InitializeClassFieldsStatement)

#undef NOT_A_PATTERN
}  // namespace internal
}  // namespace v8
