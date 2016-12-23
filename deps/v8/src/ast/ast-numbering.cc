// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ast/ast-numbering.h"

#include "src/ast/ast.h"
#include "src/ast/scopes.h"

namespace v8 {
namespace internal {

class AstNumberingVisitor final : public AstVisitor<AstNumberingVisitor> {
 public:
  AstNumberingVisitor(Isolate* isolate, Zone* zone)
      : isolate_(isolate),
        zone_(zone),
        next_id_(BailoutId::FirstUsable().ToInt()),
        yield_count_(0),
        properties_(zone),
        slot_cache_(zone),
        dont_optimize_reason_(kNoReason),
        catch_prediction_(HandlerTable::UNCAUGHT) {
    InitializeAstVisitor(isolate);
  }

  bool Renumber(FunctionLiteral* node);

 private:
// AST node visitor interface.
#define DEFINE_VISIT(type) void Visit##type(type* node);
  AST_NODE_LIST(DEFINE_VISIT)
#undef DEFINE_VISIT

  void VisitVariableProxyReference(VariableProxy* node);
  void VisitPropertyReference(Property* node);
  void VisitReference(Expression* expr);

  void VisitStatements(ZoneList<Statement*>* statements);
  void VisitDeclarations(ZoneList<Declaration*>* declarations);
  void VisitArguments(ZoneList<Expression*>* arguments);
  void VisitLiteralProperty(LiteralProperty* property);

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
    properties_.flags() |= AstProperties::kDontCrankshaft;
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
  int yield_count_;
  AstProperties properties_;
  // The slot cache allows us to reuse certain feedback vector slots.
  FeedbackVectorSlotCache slot_cache_;
  BailoutReason dont_optimize_reason_;
  HandlerTable::CatchPrediction catch_prediction_;

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();
  DISALLOW_COPY_AND_ASSIGN(AstNumberingVisitor);
};


void AstNumberingVisitor::VisitVariableDeclaration(VariableDeclaration* node) {
  IncrementNodeCount();
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


void AstNumberingVisitor::VisitExpressionStatement(ExpressionStatement* node) {
  IncrementNodeCount();
  Visit(node->expression());
}


void AstNumberingVisitor::VisitReturnStatement(ReturnStatement* node) {
  IncrementNodeCount();
  Visit(node->expression());
}


void AstNumberingVisitor::VisitYield(Yield* node) {
  node->set_yield_id(yield_count_);
  yield_count_++;
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
  node->set_base_id(ReserveIdRange(CallRuntime::num_ids()));
  VisitArguments(node->arguments());
  // To support catch prediction within async/await:
  //
  // The AstNumberingVisitor is when catch prediction currently occurs, and it
  // is the only common point that has access to this information. The parser
  // just doesn't know yet. Take the following two cases of catch prediction:
  //
  // try { await fn(); } catch (e) { }
  // try { await fn(); } finally { }
  //
  // When parsing the await that we want to mark as caught or uncaught, it's
  // not yet known whether it will be followed by a 'finally' or a 'catch.
  // The AstNumberingVisitor is what learns whether it is caught. To make
  // the information available later to the runtime, the AstNumberingVisitor
  // has to stash it somewhere. Changing the runtime function into another
  // one in ast-numbering seemed like a simple and straightforward solution to
  // that problem.
  if (node->is_jsruntime() &&
      node->context_index() == Context::ASYNC_FUNCTION_AWAIT_CAUGHT_INDEX &&
      catch_prediction_ == HandlerTable::ASYNC_AWAIT) {
    node->set_context_index(Context::ASYNC_FUNCTION_AWAIT_UNCAUGHT_INDEX);
  }
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
  node->set_first_yield_id(yield_count_);
  Visit(node->body());
  Visit(node->cond());
  node->set_yield_count(yield_count_ - node->first_yield_id());
}


void AstNumberingVisitor::VisitWhileStatement(WhileStatement* node) {
  IncrementNodeCount();
  DisableSelfOptimization();
  node->set_base_id(ReserveIdRange(WhileStatement::num_ids()));
  node->set_first_yield_id(yield_count_);
  Visit(node->cond());
  Visit(node->body());
  node->set_yield_count(yield_count_ - node->first_yield_id());
}


void AstNumberingVisitor::VisitTryCatchStatement(TryCatchStatement* node) {
  IncrementNodeCount();
  DisableCrankshaft(kTryCatchStatement);
  {
    const HandlerTable::CatchPrediction old_prediction = catch_prediction_;
    // This node uses its own prediction, unless it's "uncaught", in which case
    // we adopt the prediction of the outer try-block.
    HandlerTable::CatchPrediction catch_prediction = node->catch_prediction();
    if (catch_prediction != HandlerTable::UNCAUGHT) {
      catch_prediction_ = catch_prediction;
    }
    node->set_catch_prediction(catch_prediction_);
    Visit(node->try_block());
    catch_prediction_ = old_prediction;
  }
  Visit(node->catch_block());
}


void AstNumberingVisitor::VisitTryFinallyStatement(TryFinallyStatement* node) {
  IncrementNodeCount();
  DisableCrankshaft(kTryFinallyStatement);
  // We can't know whether the finally block will override ("catch") an
  // exception thrown in the try block, so we just adopt the outer prediction.
  node->set_catch_prediction(catch_prediction_);
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
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitCompareOperation(CompareOperation* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(CompareOperation::num_ids()));
  Visit(node->left());
  Visit(node->right());
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitSpread(Spread* node) { UNREACHABLE(); }


void AstNumberingVisitor::VisitEmptyParentheses(EmptyParentheses* node) {
  UNREACHABLE();
}


void AstNumberingVisitor::VisitForInStatement(ForInStatement* node) {
  IncrementNodeCount();
  DisableSelfOptimization();
  node->set_base_id(ReserveIdRange(ForInStatement::num_ids()));
  Visit(node->enumerable());  // Not part of loop.
  node->set_first_yield_id(yield_count_);
  Visit(node->each());
  Visit(node->body());
  node->set_yield_count(yield_count_ - node->first_yield_id());
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitForOfStatement(ForOfStatement* node) {
  IncrementNodeCount();
  DisableCrankshaft(kForOfStatement);
  node->set_base_id(ReserveIdRange(ForOfStatement::num_ids()));
  Visit(node->assign_iterator());  // Not part of loop.
  node->set_first_yield_id(yield_count_);
  Visit(node->next_result());
  Visit(node->result_done());
  Visit(node->assign_each());
  Visit(node->body());
  node->set_yield_count(yield_count_ - node->first_yield_id());
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
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitForStatement(ForStatement* node) {
  IncrementNodeCount();
  DisableSelfOptimization();
  node->set_base_id(ReserveIdRange(ForStatement::num_ids()));
  if (node->init() != NULL) Visit(node->init());  // Not part of loop.
  node->set_first_yield_id(yield_count_);
  if (node->cond() != NULL) Visit(node->cond());
  if (node->next() != NULL) Visit(node->next());
  Visit(node->body());
  node->set_yield_count(yield_count_ - node->first_yield_id());
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
    VisitLiteralProperty(node->properties()->at(i));
  }
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitObjectLiteral(ObjectLiteral* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(node->num_ids()));
  for (int i = 0; i < node->properties()->length(); i++) {
    VisitLiteralProperty(node->properties()->at(i));
  }
  node->BuildConstantProperties(isolate_);
  // Mark all computed expressions that are bound to a key that
  // is shadowed by a later occurrence of the same key. For the
  // marked expressions, no store code will be is emitted.
  node->CalculateEmitStore(zone_);
  ReserveFeedbackSlots(node);
}

void AstNumberingVisitor::VisitLiteralProperty(LiteralProperty* node) {
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


bool AstNumberingVisitor::Renumber(FunctionLiteral* node) {
  DeclarationScope* scope = node->scope();
  if (scope->new_target_var()) DisableCrankshaft(kSuperReference);
  if (scope->calls_eval()) DisableCrankshaft(kFunctionCallsEval);
  if (scope->arguments() != NULL && !scope->arguments()->IsStackAllocated()) {
    DisableCrankshaft(kContextAllocatedArguments);
  }

  if (scope->rest_parameter() != nullptr) {
    DisableCrankshaft(kRestParameter);
  }

  if (IsGeneratorFunction(node->kind()) || IsAsyncFunction(node->kind())) {
    // Generators can be optimized if --turbo-from-bytecode is set.
    if (FLAG_turbo_from_bytecode) {
      DisableCrankshaft(kGenerator);
    } else {
      DisableOptimization(kGenerator);
    }
  }

  VisitDeclarations(scope->declarations());
  VisitStatements(node->body());

  node->set_ast_properties(&properties_);
  node->set_dont_optimize_reason(dont_optimize_reason());
  node->set_yield_count(yield_count_);
  return !HasStackOverflow();
}


bool AstNumbering::Renumber(Isolate* isolate, Zone* zone,
                            FunctionLiteral* function) {
  AstNumberingVisitor visitor(isolate, zone);
  return visitor.Renumber(function);
}
}  // namespace internal
}  // namespace v8
