// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/ast.h"
#include "src/ast-numbering.h"
#include "src/compiler.h"
#include "src/scopes.h"

namespace v8 {
namespace internal {


class AstNumberingVisitor FINAL : public AstVisitor {
 public:
  explicit AstNumberingVisitor(Zone* zone)
      : AstVisitor(), next_id_(BailoutId::FirstUsable().ToInt()) {
    InitializeAstVisitor(zone);
  }

  void Renumber(FunctionLiteral* node);

 private:
// AST node visitor interface.
#define DEFINE_VISIT(type) virtual void Visit##type(type* node) OVERRIDE;
  AST_NODE_LIST(DEFINE_VISIT)
#undef DEFINE_VISIT

  void VisitStatements(ZoneList<Statement*>* statements) OVERRIDE;
  void VisitDeclarations(ZoneList<Declaration*>* declarations) OVERRIDE;
  void VisitArguments(ZoneList<Expression*>* arguments);
  void VisitObjectLiteralProperty(ObjectLiteralProperty* property);

  int ReserveIdRange(int n) {
    int tmp = next_id_;
    next_id_ += n;
    return tmp;
  }

  void IncrementNodeCount() { properties_.add_node_count(1); }

  int next_id_;
  AstProperties properties_;

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();
  DISALLOW_COPY_AND_ASSIGN(AstNumberingVisitor);
};


void AstNumberingVisitor::VisitVariableDeclaration(VariableDeclaration* node) {
  IncrementNodeCount();
  VisitVariableProxy(node->proxy());
}


void AstNumberingVisitor::VisitExportDeclaration(ExportDeclaration* node) {
  IncrementNodeCount();
  VisitVariableProxy(node->proxy());
}


void AstNumberingVisitor::VisitModuleUrl(ModuleUrl* node) {
  IncrementNodeCount();
}


void AstNumberingVisitor::VisitEmptyStatement(EmptyStatement* node) {
  IncrementNodeCount();
}


void AstNumberingVisitor::VisitContinueStatement(ContinueStatement* node) {
  IncrementNodeCount();
}


void AstNumberingVisitor::VisitBreakStatement(BreakStatement* node) {
  IncrementNodeCount();
}


void AstNumberingVisitor::VisitDebuggerStatement(DebuggerStatement* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(DebuggerStatement::num_ids()));
}


void AstNumberingVisitor::VisitNativeFunctionLiteral(
    NativeFunctionLiteral* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(NativeFunctionLiteral::num_ids()));
}


void AstNumberingVisitor::VisitLiteral(Literal* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(Literal::num_ids()));
}


void AstNumberingVisitor::VisitRegExpLiteral(RegExpLiteral* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(RegExpLiteral::num_ids()));
}


void AstNumberingVisitor::VisitVariableProxy(VariableProxy* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(VariableProxy::num_ids()));
}


void AstNumberingVisitor::VisitThisFunction(ThisFunction* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(ThisFunction::num_ids()));
}


void AstNumberingVisitor::VisitSuperReference(SuperReference* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(SuperReference::num_ids()));
  Visit(node->this_var());
}


void AstNumberingVisitor::VisitModuleDeclaration(ModuleDeclaration* node) {
  IncrementNodeCount();
  VisitVariableProxy(node->proxy());
  Visit(node->module());
}


void AstNumberingVisitor::VisitImportDeclaration(ImportDeclaration* node) {
  IncrementNodeCount();
  VisitVariableProxy(node->proxy());
  Visit(node->module());
}


void AstNumberingVisitor::VisitModuleVariable(ModuleVariable* node) {
  IncrementNodeCount();
  Visit(node->proxy());
}


void AstNumberingVisitor::VisitModulePath(ModulePath* node) {
  IncrementNodeCount();
  Visit(node->module());
}


void AstNumberingVisitor::VisitModuleStatement(ModuleStatement* node) {
  IncrementNodeCount();
  Visit(node->body());
}


void AstNumberingVisitor::VisitExpressionStatement(ExpressionStatement* node) {
  IncrementNodeCount();
  Visit(node->expression());
}


void AstNumberingVisitor::VisitReturnStatement(ReturnStatement* node) {
  IncrementNodeCount();
  Visit(node->expression());
}


void AstNumberingVisitor::VisitYield(Yield* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(Yield::num_ids()));
  Visit(node->generator_object());
  Visit(node->expression());
}


void AstNumberingVisitor::VisitThrow(Throw* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(Throw::num_ids()));
  Visit(node->exception());
}


void AstNumberingVisitor::VisitUnaryOperation(UnaryOperation* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(UnaryOperation::num_ids()));
  Visit(node->expression());
}


void AstNumberingVisitor::VisitCountOperation(CountOperation* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(CountOperation::num_ids()));
  Visit(node->expression());
}


void AstNumberingVisitor::VisitBlock(Block* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(Block::num_ids()));
  if (node->scope() != NULL) VisitDeclarations(node->scope()->declarations());
  VisitStatements(node->statements());
}


void AstNumberingVisitor::VisitFunctionDeclaration(FunctionDeclaration* node) {
  IncrementNodeCount();
  VisitVariableProxy(node->proxy());
  VisitFunctionLiteral(node->fun());
}


void AstNumberingVisitor::VisitModuleLiteral(ModuleLiteral* node) {
  IncrementNodeCount();
  VisitBlock(node->body());
}


void AstNumberingVisitor::VisitCallRuntime(CallRuntime* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(CallRuntime::num_ids()));
  VisitArguments(node->arguments());
}


void AstNumberingVisitor::VisitWithStatement(WithStatement* node) {
  IncrementNodeCount();
  Visit(node->expression());
  Visit(node->statement());
}


void AstNumberingVisitor::VisitDoWhileStatement(DoWhileStatement* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(DoWhileStatement::num_ids()));
  Visit(node->body());
  Visit(node->cond());
}


void AstNumberingVisitor::VisitWhileStatement(WhileStatement* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(WhileStatement::num_ids()));
  Visit(node->cond());
  Visit(node->body());
}


void AstNumberingVisitor::VisitTryCatchStatement(TryCatchStatement* node) {
  IncrementNodeCount();
  Visit(node->try_block());
  Visit(node->catch_block());
}


void AstNumberingVisitor::VisitTryFinallyStatement(TryFinallyStatement* node) {
  IncrementNodeCount();
  Visit(node->try_block());
  Visit(node->finally_block());
}


void AstNumberingVisitor::VisitProperty(Property* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(Property::num_ids()));
  Visit(node->key());
  Visit(node->obj());
}


void AstNumberingVisitor::VisitAssignment(Assignment* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(Assignment::num_ids()));
  if (node->is_compound()) VisitBinaryOperation(node->binary_operation());
  Visit(node->target());
  Visit(node->value());
}


void AstNumberingVisitor::VisitBinaryOperation(BinaryOperation* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(BinaryOperation::num_ids()));
  Visit(node->left());
  Visit(node->right());
}


void AstNumberingVisitor::VisitCompareOperation(CompareOperation* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(CompareOperation::num_ids()));
  Visit(node->left());
  Visit(node->right());
}


void AstNumberingVisitor::VisitForInStatement(ForInStatement* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(ForInStatement::num_ids()));
  Visit(node->each());
  Visit(node->enumerable());
  Visit(node->body());
}


void AstNumberingVisitor::VisitForOfStatement(ForOfStatement* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(ForOfStatement::num_ids()));
  Visit(node->assign_iterator());
  Visit(node->next_result());
  Visit(node->result_done());
  Visit(node->assign_each());
  Visit(node->body());
}


void AstNumberingVisitor::VisitConditional(Conditional* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(Conditional::num_ids()));
  Visit(node->condition());
  Visit(node->then_expression());
  Visit(node->else_expression());
}


void AstNumberingVisitor::VisitIfStatement(IfStatement* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(IfStatement::num_ids()));
  Visit(node->condition());
  Visit(node->then_statement());
  if (node->HasElseStatement()) {
    Visit(node->else_statement());
  }
}


void AstNumberingVisitor::VisitSwitchStatement(SwitchStatement* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(SwitchStatement::num_ids()));
  Visit(node->tag());
  ZoneList<CaseClause*>* cases = node->cases();
  for (int i = 0; i < cases->length(); i++) {
    VisitCaseClause(cases->at(i));
  }
}


void AstNumberingVisitor::VisitCaseClause(CaseClause* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(CaseClause::num_ids()));
  if (!node->is_default()) Visit(node->label());
  VisitStatements(node->statements());
}


void AstNumberingVisitor::VisitForStatement(ForStatement* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(ForStatement::num_ids()));
  if (node->init() != NULL) Visit(node->init());
  if (node->cond() != NULL) Visit(node->cond());
  if (node->next() != NULL) Visit(node->next());
  Visit(node->body());
}


void AstNumberingVisitor::VisitClassLiteral(ClassLiteral* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(ClassLiteral::num_ids()));
  if (node->extends()) Visit(node->extends());
  if (node->constructor()) Visit(node->constructor());
  for (int i = 0; i < node->properties()->length(); i++) {
    VisitObjectLiteralProperty(node->properties()->at(i));
  }
}


void AstNumberingVisitor::VisitObjectLiteral(ObjectLiteral* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(ObjectLiteral::num_ids()));
  for (int i = 0; i < node->properties()->length(); i++) {
    VisitObjectLiteralProperty(node->properties()->at(i));
  }
}


void AstNumberingVisitor::VisitObjectLiteralProperty(
    ObjectLiteralProperty* node) {
  Visit(node->key());
  Visit(node->value());
}


void AstNumberingVisitor::VisitArrayLiteral(ArrayLiteral* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(node->num_ids()));
  for (int i = 0; i < node->values()->length(); i++) {
    Visit(node->values()->at(i));
  }
}


void AstNumberingVisitor::VisitCall(Call* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(Call::num_ids()));
  Visit(node->expression());
  VisitArguments(node->arguments());
}


void AstNumberingVisitor::VisitCallNew(CallNew* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(CallNew::num_ids()));
  Visit(node->expression());
  VisitArguments(node->arguments());
}


void AstNumberingVisitor::VisitStatements(ZoneList<Statement*>* statements) {
  if (statements == NULL) return;
  for (int i = 0; i < statements->length(); i++) {
    Visit(statements->at(i));
  }
}


void AstNumberingVisitor::VisitDeclarations(
    ZoneList<Declaration*>* declarations) {
  for (int i = 0; i < declarations->length(); i++) {
    Visit(declarations->at(i));
  }
}


void AstNumberingVisitor::VisitArguments(ZoneList<Expression*>* arguments) {
  for (int i = 0; i < arguments->length(); i++) {
    Visit(arguments->at(i));
  }
}


void AstNumberingVisitor::VisitFunctionLiteral(FunctionLiteral* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(FunctionLiteral::num_ids()));
  // We don't recurse into the declarations or body of the function literal:
  // you have to separately Renumber() each FunctionLiteral that you compile.
}


void AstNumberingVisitor::Renumber(FunctionLiteral* node) {
  properties_.flags()->Add(*node->flags());
  properties_.increase_feedback_slots(node->slot_count());
  properties_.increase_ic_feedback_slots(node->ic_slot_count());

  if (node->scope()->HasIllegalRedeclaration()) {
    node->scope()->VisitIllegalRedeclaration(this);
    return;
  }

  Scope* scope = node->scope();
  VisitDeclarations(scope->declarations());
  if (scope->is_function_scope() && scope->function() != NULL) {
    // Visit the name of the named function expression.
    Visit(scope->function());
  }
  VisitStatements(node->body());

  node->set_ast_properties(&properties_);
}


bool AstNumbering::Renumber(FunctionLiteral* function, Zone* zone) {
  AstNumberingVisitor visitor(zone);
  visitor.Renumber(function);
  return !visitor.HasStackOverflow();
}
}
}  // namespace v8::internal
