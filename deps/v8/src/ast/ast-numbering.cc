// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/ast-numbering.h"

#include "src/ast/ast.h"
#include "src/ast/scopes.h"

namespace v8 {
namespace internal {

class AstNumberingVisitor final : public AstVisitor {
 public:
  AstNumberingVisitor(Isolate* isolate, Zone* zone)
      : AstVisitor(),
        isolate_(isolate),
        zone_(zone),
        next_id_(BailoutId::FirstUsable().ToInt()),
        properties_(zone),
        slot_cache_(zone),
        dont_optimize_reason_(kNoReason) {
    InitializeAstVisitor(isolate);
  }

  bool Renumber(FunctionLiteral* node);

 private:
// AST node visitor interface.
#define DEFINE_VISIT(type) void Visit##type(type* node) override;
  AST_NODE_LIST(DEFINE_VISIT)
#undef DEFINE_VISIT

  bool Finish(FunctionLiteral* node);

  void VisitVariableProxyReference(VariableProxy* node);
  void VisitPropertyReference(Property* node);
  void VisitReference(Expression* expr);

  void VisitStatements(ZoneList<Statement*>* statements) override;
  void VisitDeclarations(ZoneList<Declaration*>* declarations) override;
  void VisitArguments(ZoneList<Expression*>* arguments);
  void VisitObjectLiteralProperty(ObjectLiteralProperty* property);

  int ReserveIdRange(int n) {
    int tmp = next_id_;
    next_id_ += n;
    return tmp;
  }

  void IncrementNodeCount() { properties_.add_node_count(1); }
  void DisableSelfOptimization() {
    properties_.flags() |= AstProperties::kDontSelfOptimize;
  }
  void DisableOptimization(BailoutReason reason) {
    dont_optimize_reason_ = reason;
    DisableSelfOptimization();
  }
  void DisableCrankshaft(BailoutReason reason) {
    if (FLAG_turbo_shipping) {
      properties_.flags() |= AstProperties::kDontCrankshaft;
    } else {
      dont_optimize_reason_ = reason;
      DisableSelfOptimization();
    }
  }

  template <typename Node>
  void ReserveFeedbackSlots(Node* node) {
    node->AssignFeedbackVectorSlots(isolate_, properties_.get_spec(),
                                    &slot_cache_);
  }

  BailoutReason dont_optimize_reason() const { return dont_optimize_reason_; }

  Isolate* isolate_;
  Zone* zone_;
  int next_id_;
  AstProperties properties_;
  // The slot cache allows us to reuse certain feedback vector slots.
  FeedbackVectorSlotCache slot_cache_;
  BailoutReason dont_optimize_reason_;

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();
  DISALLOW_COPY_AND_ASSIGN(AstNumberingVisitor);
};


void AstNumberingVisitor::VisitVariableDeclaration(VariableDeclaration* node) {
  IncrementNodeCount();
  VisitVariableProxy(node->proxy());
}


void AstNumberingVisitor::VisitExportDeclaration(ExportDeclaration* node) {
  IncrementNodeCount();
  DisableOptimization(kExportDeclaration);
  VisitVariableProxy(node->proxy());
}


void AstNumberingVisitor::VisitEmptyStatement(EmptyStatement* node) {
  IncrementNodeCount();
}


void AstNumberingVisitor::VisitSloppyBlockFunctionStatement(
    SloppyBlockFunctionStatement* node) {
  IncrementNodeCount();
  Visit(node->statement());
}


void AstNumberingVisitor::VisitContinueStatement(ContinueStatement* node) {
  IncrementNodeCount();
}


void AstNumberingVisitor::VisitBreakStatement(BreakStatement* node) {
  IncrementNodeCount();
}


void AstNumberingVisitor::VisitDebuggerStatement(DebuggerStatement* node) {
  IncrementNodeCount();
  DisableOptimization(kDebuggerStatement);
  node->set_base_id(ReserveIdRange(DebuggerStatement::num_ids()));
}


void AstNumberingVisitor::VisitNativeFunctionLiteral(
    NativeFunctionLiteral* node) {
  IncrementNodeCount();
  DisableOptimization(kNativeFunctionLiteral);
  node->set_base_id(ReserveIdRange(NativeFunctionLiteral::num_ids()));
}


void AstNumberingVisitor::VisitDoExpression(DoExpression* node) {
  IncrementNodeCount();
  DisableCrankshaft(kDoExpression);
  node->set_base_id(ReserveIdRange(DoExpression::num_ids()));
  Visit(node->block());
  Visit(node->result());
}


void AstNumberingVisitor::VisitLiteral(Literal* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(Literal::num_ids()));
}


void AstNumberingVisitor::VisitRegExpLiteral(RegExpLiteral* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(RegExpLiteral::num_ids()));
}


void AstNumberingVisitor::VisitVariableProxyReference(VariableProxy* node) {
  IncrementNodeCount();
  if (node->var()->IsLookupSlot()) {
    DisableCrankshaft(kReferenceToAVariableWhichRequiresDynamicLookup);
  }
  node->set_base_id(ReserveIdRange(VariableProxy::num_ids()));
}


void AstNumberingVisitor::VisitVariableProxy(VariableProxy* node) {
  VisitVariableProxyReference(node);
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitThisFunction(ThisFunction* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(ThisFunction::num_ids()));
}


void AstNumberingVisitor::VisitSuperPropertyReference(
    SuperPropertyReference* node) {
  IncrementNodeCount();
  DisableCrankshaft(kSuperReference);
  node->set_base_id(ReserveIdRange(SuperPropertyReference::num_ids()));
  Visit(node->this_var());
  Visit(node->home_object());
}


void AstNumberingVisitor::VisitSuperCallReference(SuperCallReference* node) {
  IncrementNodeCount();
  DisableCrankshaft(kSuperReference);
  node->set_base_id(ReserveIdRange(SuperCallReference::num_ids()));
  Visit(node->this_var());
  Visit(node->new_target_var());
  Visit(node->this_function_var());
}


void AstNumberingVisitor::VisitImportDeclaration(ImportDeclaration* node) {
  IncrementNodeCount();
  DisableOptimization(kImportDeclaration);
  VisitVariableProxy(node->proxy());
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
  DisableOptimization(kYield);
  ReserveFeedbackSlots(node);
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
  ReserveFeedbackSlots(node);
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


void AstNumberingVisitor::VisitCallRuntime(CallRuntime* node) {
  IncrementNodeCount();
  ReserveFeedbackSlots(node);
  if (node->is_jsruntime()) {
    // Don't try to optimize JS runtime calls because we bailout on them.
    DisableOptimization(kCallToAJavaScriptRuntimeFunction);
  }
  node->set_base_id(ReserveIdRange(CallRuntime::num_ids()));
  VisitArguments(node->arguments());
}


void AstNumberingVisitor::VisitWithStatement(WithStatement* node) {
  IncrementNodeCount();
  DisableCrankshaft(kWithStatement);
  node->set_base_id(ReserveIdRange(WithStatement::num_ids()));
  Visit(node->expression());
  Visit(node->statement());
}


void AstNumberingVisitor::VisitDoWhileStatement(DoWhileStatement* node) {
  IncrementNodeCount();
  DisableSelfOptimization();
  node->set_base_id(ReserveIdRange(DoWhileStatement::num_ids()));
  Visit(node->body());
  Visit(node->cond());
}


void AstNumberingVisitor::VisitWhileStatement(WhileStatement* node) {
  IncrementNodeCount();
  DisableSelfOptimization();
  node->set_base_id(ReserveIdRange(WhileStatement::num_ids()));
  Visit(node->cond());
  Visit(node->body());
}


void AstNumberingVisitor::VisitTryCatchStatement(TryCatchStatement* node) {
  IncrementNodeCount();
  DisableOptimization(kTryCatchStatement);
  Visit(node->try_block());
  Visit(node->catch_block());
}


void AstNumberingVisitor::VisitTryFinallyStatement(TryFinallyStatement* node) {
  IncrementNodeCount();
  DisableOptimization(kTryFinallyStatement);
  Visit(node->try_block());
  Visit(node->finally_block());
}


void AstNumberingVisitor::VisitPropertyReference(Property* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(Property::num_ids()));
  Visit(node->key());
  Visit(node->obj());
}


void AstNumberingVisitor::VisitReference(Expression* expr) {
  DCHECK(expr->IsProperty() || expr->IsVariableProxy());
  if (expr->IsProperty()) {
    VisitPropertyReference(expr->AsProperty());
  } else {
    VisitVariableProxyReference(expr->AsVariableProxy());
  }
}


void AstNumberingVisitor::VisitProperty(Property* node) {
  VisitPropertyReference(node);
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitAssignment(Assignment* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(Assignment::num_ids()));

  if (node->is_compound()) VisitBinaryOperation(node->binary_operation());
  VisitReference(node->target());
  Visit(node->value());
  ReserveFeedbackSlots(node);
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


void AstNumberingVisitor::VisitSpread(Spread* node) { UNREACHABLE(); }


void AstNumberingVisitor::VisitEmptyParentheses(EmptyParentheses* node) {
  UNREACHABLE();
}


void AstNumberingVisitor::VisitForInStatement(ForInStatement* node) {
  IncrementNodeCount();
  DisableSelfOptimization();
  node->set_base_id(ReserveIdRange(ForInStatement::num_ids()));
  Visit(node->each());
  Visit(node->enumerable());
  Visit(node->body());
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitForOfStatement(ForOfStatement* node) {
  IncrementNodeCount();
  DisableCrankshaft(kForOfStatement);
  node->set_base_id(ReserveIdRange(ForOfStatement::num_ids()));
  Visit(node->assign_iterator());
  Visit(node->next_result());
  Visit(node->result_done());
  Visit(node->assign_each());
  Visit(node->body());
  ReserveFeedbackSlots(node);
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
  DisableSelfOptimization();
  node->set_base_id(ReserveIdRange(ForStatement::num_ids()));
  if (node->init() != NULL) Visit(node->init());
  if (node->cond() != NULL) Visit(node->cond());
  if (node->next() != NULL) Visit(node->next());
  Visit(node->body());
}


void AstNumberingVisitor::VisitClassLiteral(ClassLiteral* node) {
  IncrementNodeCount();
  DisableCrankshaft(kClassLiteral);
  node->set_base_id(ReserveIdRange(node->num_ids()));
  if (node->extends()) Visit(node->extends());
  if (node->constructor()) Visit(node->constructor());
  if (node->class_variable_proxy()) {
    VisitVariableProxy(node->class_variable_proxy());
  }
  for (int i = 0; i < node->properties()->length(); i++) {
    VisitObjectLiteralProperty(node->properties()->at(i));
  }
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitObjectLiteral(ObjectLiteral* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(node->num_ids()));
  for (int i = 0; i < node->properties()->length(); i++) {
    VisitObjectLiteralProperty(node->properties()->at(i));
  }
  node->BuildConstantProperties(isolate_);
  // Mark all computed expressions that are bound to a key that
  // is shadowed by a later occurrence of the same key. For the
  // marked expressions, no store code will be is emitted.
  node->CalculateEmitStore(zone_);
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitObjectLiteralProperty(
    ObjectLiteralProperty* node) {
  if (node->is_computed_name()) DisableCrankshaft(kComputedPropertyName);
  Visit(node->key());
  Visit(node->value());
}


void AstNumberingVisitor::VisitArrayLiteral(ArrayLiteral* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(node->num_ids()));
  for (int i = 0; i < node->values()->length(); i++) {
    Visit(node->values()->at(i));
  }
  node->BuildConstantElements(isolate_);
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitCall(Call* node) {
  IncrementNodeCount();
  if (node->tail_call_mode() == TailCallMode::kAllow) {
    DisableOptimization(kTailCall);
  }
  ReserveFeedbackSlots(node);
  node->set_base_id(ReserveIdRange(Call::num_ids()));
  Visit(node->expression());
  VisitArguments(node->arguments());
}


void AstNumberingVisitor::VisitCallNew(CallNew* node) {
  IncrementNodeCount();
  ReserveFeedbackSlots(node);
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


void AstNumberingVisitor::VisitRewritableExpression(
    RewritableExpression* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(RewritableExpression::num_ids()));
  Visit(node->expression());
}


bool AstNumberingVisitor::Finish(FunctionLiteral* node) {
  node->set_ast_properties(&properties_);
  node->set_dont_optimize_reason(dont_optimize_reason());
  return !HasStackOverflow();
}


bool AstNumberingVisitor::Renumber(FunctionLiteral* node) {
  Scope* scope = node->scope();

  if (scope->HasIllegalRedeclaration()) {
    Visit(scope->GetIllegalRedeclaration());
    DisableOptimization(kFunctionWithIllegalRedeclaration);
    return Finish(node);
  }
  if (scope->new_target_var()) DisableCrankshaft(kSuperReference);
  if (scope->calls_eval()) DisableOptimization(kFunctionCallsEval);
  if (scope->arguments() != NULL && !scope->arguments()->IsStackAllocated()) {
    DisableCrankshaft(kContextAllocatedArguments);
  }

  int rest_index;
  if (scope->rest_parameter(&rest_index)) {
    DisableCrankshaft(kRestParameter);
  }

  VisitDeclarations(scope->declarations());
  VisitStatements(node->body());

  return Finish(node);
}


bool AstNumbering::Renumber(Isolate* isolate, Zone* zone,
                            FunctionLiteral* function) {
  AstNumberingVisitor visitor(isolate, zone);
  return visitor.Renumber(function);
}
}  // namespace internal
}  // namespace v8
