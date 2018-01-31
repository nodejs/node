// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/ast-numbering.h"

#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/compiler.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

class AstNumberingVisitor final : public AstVisitor<AstNumberingVisitor> {
 public:
  AstNumberingVisitor(uintptr_t stack_limit, Zone* zone,
                      Compiler::EagerInnerFunctionLiterals* eager_literals)
      : zone_(zone),
        eager_literals_(eager_literals),
        suspend_count_(0),
        dont_optimize_reason_(kNoReason) {
    InitializeAstVisitor(stack_limit);
  }

  bool Renumber(FunctionLiteral* node);

 private:
// AST node visitor interface.
#define DEFINE_VISIT(type) void Visit##type(type* node);
  AST_NODE_LIST(DEFINE_VISIT)
#undef DEFINE_VISIT

  void VisitSuspend(Suspend* node);

  void VisitStatementsAndDeclarations(Block* node);
  void VisitStatements(ZoneList<Statement*>* statements);
  void VisitDeclarations(Declaration::List* declarations);
  void VisitArguments(ZoneList<Expression*>* arguments);
  void VisitLiteralProperty(LiteralProperty* property);

  void DisableOptimization(BailoutReason reason) {
    dont_optimize_reason_ = reason;
  }

  BailoutReason dont_optimize_reason() const { return dont_optimize_reason_; }

  Zone* zone() const { return zone_; }

  Zone* zone_;
  Compiler::EagerInnerFunctionLiterals* eager_literals_;
  int suspend_count_;
  FunctionKind function_kind_;
  BailoutReason dont_optimize_reason_;

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();
  DISALLOW_COPY_AND_ASSIGN(AstNumberingVisitor);
};

void AstNumberingVisitor::VisitVariableDeclaration(VariableDeclaration* node) {
  VisitVariableProxy(node->proxy());
}

void AstNumberingVisitor::VisitEmptyStatement(EmptyStatement* node) {
}

void AstNumberingVisitor::VisitSloppyBlockFunctionStatement(
    SloppyBlockFunctionStatement* node) {
  Visit(node->statement());
}

void AstNumberingVisitor::VisitContinueStatement(ContinueStatement* node) {
}

void AstNumberingVisitor::VisitBreakStatement(BreakStatement* node) {
}

void AstNumberingVisitor::VisitDebuggerStatement(DebuggerStatement* node) {
}

void AstNumberingVisitor::VisitNativeFunctionLiteral(
    NativeFunctionLiteral* node) {
  DisableOptimization(kNativeFunctionLiteral);
}

void AstNumberingVisitor::VisitDoExpression(DoExpression* node) {
  Visit(node->block());
  Visit(node->result());
}

void AstNumberingVisitor::VisitLiteral(Literal* node) {
}

void AstNumberingVisitor::VisitRegExpLiteral(RegExpLiteral* node) {
}

void AstNumberingVisitor::VisitVariableProxy(VariableProxy* node) {
}

void AstNumberingVisitor::VisitThisFunction(ThisFunction* node) {
}

void AstNumberingVisitor::VisitSuperPropertyReference(
    SuperPropertyReference* node) {
  Visit(node->this_var());
  Visit(node->home_object());
}

void AstNumberingVisitor::VisitSuperCallReference(SuperCallReference* node) {
  Visit(node->this_var());
  Visit(node->new_target_var());
  Visit(node->this_function_var());
}

void AstNumberingVisitor::VisitExpressionStatement(ExpressionStatement* node) {
  Visit(node->expression());
}

void AstNumberingVisitor::VisitReturnStatement(ReturnStatement* node) {
  Visit(node->expression());
}

void AstNumberingVisitor::VisitSuspend(Suspend* node) {
  node->set_suspend_id(suspend_count_);
  suspend_count_++;
  Visit(node->expression());
}

void AstNumberingVisitor::VisitYield(Yield* node) { VisitSuspend(node); }

void AstNumberingVisitor::VisitYieldStar(YieldStar* node) {
  node->set_suspend_id(suspend_count_++);
  if (IsAsyncGeneratorFunction(function_kind_)) {
    node->set_await_iterator_close_suspend_id(suspend_count_++);
    node->set_await_delegated_iterator_output_suspend_id(suspend_count_++);
  }
  Visit(node->expression());
}

void AstNumberingVisitor::VisitAwait(Await* node) { VisitSuspend(node); }

void AstNumberingVisitor::VisitThrow(Throw* node) {
  Visit(node->exception());
}

void AstNumberingVisitor::VisitUnaryOperation(UnaryOperation* node) {
  Visit(node->expression());
}

void AstNumberingVisitor::VisitCountOperation(CountOperation* node) {
  Visit(node->expression());
}

void AstNumberingVisitor::VisitBlock(Block* node) {
  VisitStatementsAndDeclarations(node);
}

void AstNumberingVisitor::VisitStatementsAndDeclarations(Block* node) {
  Scope* scope = node->scope();
  DCHECK(scope == nullptr || !scope->HasBeenRemoved());
  if (scope) VisitDeclarations(scope->declarations());
  VisitStatements(node->statements());
}

void AstNumberingVisitor::VisitFunctionDeclaration(FunctionDeclaration* node) {
  VisitVariableProxy(node->proxy());
  VisitFunctionLiteral(node->fun());
}

void AstNumberingVisitor::VisitCallRuntime(CallRuntime* node) {
  VisitArguments(node->arguments());
}

void AstNumberingVisitor::VisitWithStatement(WithStatement* node) {
  Visit(node->expression());
  Visit(node->statement());
}

void AstNumberingVisitor::VisitDoWhileStatement(DoWhileStatement* node) {
  node->set_first_suspend_id(suspend_count_);
  Visit(node->body());
  Visit(node->cond());
  node->set_suspend_count(suspend_count_ - node->first_suspend_id());
}

void AstNumberingVisitor::VisitWhileStatement(WhileStatement* node) {
  node->set_first_suspend_id(suspend_count_);
  Visit(node->cond());
  Visit(node->body());
  node->set_suspend_count(suspend_count_ - node->first_suspend_id());
}

void AstNumberingVisitor::VisitTryCatchStatement(TryCatchStatement* node) {
  DCHECK(node->scope() == nullptr || !node->scope()->HasBeenRemoved());
  Visit(node->try_block());
  Visit(node->catch_block());
}

void AstNumberingVisitor::VisitTryFinallyStatement(TryFinallyStatement* node) {
  Visit(node->try_block());
  Visit(node->finally_block());
}

void AstNumberingVisitor::VisitProperty(Property* node) {
  Visit(node->key());
  Visit(node->obj());
}

void AstNumberingVisitor::VisitAssignment(Assignment* node) {
  Visit(node->target());
  Visit(node->value());
}

void AstNumberingVisitor::VisitCompoundAssignment(CompoundAssignment* node) {
  VisitBinaryOperation(node->binary_operation());
  VisitAssignment(node);
}

void AstNumberingVisitor::VisitBinaryOperation(BinaryOperation* node) {
  Visit(node->left());
  Visit(node->right());
}

void AstNumberingVisitor::VisitNaryOperation(NaryOperation* node) {
  Visit(node->first());
  for (size_t i = 0; i < node->subsequent_length(); ++i) {
    Visit(node->subsequent(i));
  }
}

void AstNumberingVisitor::VisitCompareOperation(CompareOperation* node) {
  Visit(node->left());
  Visit(node->right());
}

void AstNumberingVisitor::VisitSpread(Spread* node) {
  Visit(node->expression());
}

void AstNumberingVisitor::VisitEmptyParentheses(EmptyParentheses* node) {
  UNREACHABLE();
}

void AstNumberingVisitor::VisitGetIterator(GetIterator* node) {
  Visit(node->iterable());
}

void AstNumberingVisitor::VisitGetTemplateObject(GetTemplateObject* node) {}

void AstNumberingVisitor::VisitImportCallExpression(
    ImportCallExpression* node) {
  Visit(node->argument());
}

void AstNumberingVisitor::VisitForInStatement(ForInStatement* node) {
  Visit(node->enumerable());  // Not part of loop.
  node->set_first_suspend_id(suspend_count_);
  Visit(node->each());
  Visit(node->body());
  node->set_suspend_count(suspend_count_ - node->first_suspend_id());
}

void AstNumberingVisitor::VisitForOfStatement(ForOfStatement* node) {
  Visit(node->assign_iterator());  // Not part of loop.
  node->set_first_suspend_id(suspend_count_);
  Visit(node->next_result());
  Visit(node->result_done());
  Visit(node->assign_each());
  Visit(node->body());
  node->set_suspend_count(suspend_count_ - node->first_suspend_id());
}

void AstNumberingVisitor::VisitConditional(Conditional* node) {
  Visit(node->condition());
  Visit(node->then_expression());
  Visit(node->else_expression());
}

void AstNumberingVisitor::VisitIfStatement(IfStatement* node) {
  Visit(node->condition());
  Visit(node->then_statement());
  if (node->HasElseStatement()) {
    Visit(node->else_statement());
  }
}

void AstNumberingVisitor::VisitSwitchStatement(SwitchStatement* node) {
  Visit(node->tag());
  for (CaseClause* clause : *node->cases()) {
    if (!clause->is_default()) Visit(clause->label());
    VisitStatements(clause->statements());
  }
}

void AstNumberingVisitor::VisitForStatement(ForStatement* node) {
  if (node->init() != nullptr) Visit(node->init());  // Not part of loop.
  node->set_first_suspend_id(suspend_count_);
  if (node->cond() != nullptr) Visit(node->cond());
  if (node->next() != nullptr) Visit(node->next());
  Visit(node->body());
  node->set_suspend_count(suspend_count_ - node->first_suspend_id());
}

void AstNumberingVisitor::VisitClassLiteral(ClassLiteral* node) {
  if (node->extends()) Visit(node->extends());
  if (node->constructor()) Visit(node->constructor());
  if (node->static_fields_initializer() != nullptr) {
    Visit(node->static_fields_initializer());
  }
  if (node->instance_fields_initializer_function() != nullptr) {
    Visit(node->instance_fields_initializer_function());
  }
  for (int i = 0; i < node->properties()->length(); i++) {
    VisitLiteralProperty(node->properties()->at(i));
  }
}

void AstNumberingVisitor::VisitInitializeClassFieldsStatement(
    InitializeClassFieldsStatement* node) {
  for (int i = 0; i < node->fields()->length(); i++) {
    VisitLiteralProperty(node->fields()->at(i));
  }
}

void AstNumberingVisitor::VisitObjectLiteral(ObjectLiteral* node) {
  for (int i = 0; i < node->properties()->length(); i++) {
    VisitLiteralProperty(node->properties()->at(i));
  }
  node->InitDepthAndFlags();
  // Mark all computed expressions that are bound to a key that
  // is shadowed by a later occurrence of the same key. For the
  // marked expressions, no store code will be is emitted.
  node->CalculateEmitStore(zone_);
}

void AstNumberingVisitor::VisitLiteralProperty(LiteralProperty* node) {
  Visit(node->key());
  Visit(node->value());
}

void AstNumberingVisitor::VisitArrayLiteral(ArrayLiteral* node) {
  for (int i = 0; i < node->values()->length(); i++) {
    Visit(node->values()->at(i));
  }
  node->InitDepthAndFlags();
}

void AstNumberingVisitor::VisitCall(Call* node) {
  Visit(node->expression());
  VisitArguments(node->arguments());
}

void AstNumberingVisitor::VisitCallNew(CallNew* node) {
  Visit(node->expression());
  VisitArguments(node->arguments());
}

void AstNumberingVisitor::VisitStatements(ZoneList<Statement*>* statements) {
  if (statements == nullptr) return;
  for (int i = 0; i < statements->length(); i++) {
    Visit(statements->at(i));
    if (statements->at(i)->IsJump()) break;
  }
}

void AstNumberingVisitor::VisitDeclarations(Declaration::List* decls) {
  for (Declaration* decl : *decls) Visit(decl);
}

void AstNumberingVisitor::VisitArguments(ZoneList<Expression*>* arguments) {
  for (int i = 0; i < arguments->length(); i++) {
    Visit(arguments->at(i));
  }
}

void AstNumberingVisitor::VisitFunctionLiteral(FunctionLiteral* node) {
  if (node->ShouldEagerCompile()) {
    if (eager_literals_) {
      eager_literals_->Add(new (zone())
                               ThreadedListZoneEntry<FunctionLiteral*>(node));
    }

    // If the function literal is being eagerly compiled, recurse into the
    // declarations and body of the function literal.
    if (!AstNumbering::Renumber(stack_limit_, zone_, node, eager_literals_)) {
      SetStackOverflow();
      return;
    }
  }
}

void AstNumberingVisitor::VisitRewritableExpression(
    RewritableExpression* node) {
  Visit(node->expression());
}

bool AstNumberingVisitor::Renumber(FunctionLiteral* node) {
  DeclarationScope* scope = node->scope();
  DCHECK(!scope->HasBeenRemoved());
  function_kind_ = node->kind();

  VisitDeclarations(scope->declarations());
  VisitStatements(node->body());

  node->set_dont_optimize_reason(dont_optimize_reason());
  node->set_suspend_count(suspend_count_);

  return !HasStackOverflow();
}

bool AstNumbering::Renumber(
    uintptr_t stack_limit, Zone* zone, FunctionLiteral* function,
    Compiler::EagerInnerFunctionLiterals* eager_literals) {
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  AstNumberingVisitor visitor(stack_limit, zone, eager_literals);
  return visitor.Renumber(function);
}
}  // namespace internal
}  // namespace v8
