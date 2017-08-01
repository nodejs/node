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
                      Compiler::EagerInnerFunctionLiterals* eager_literals,
                      bool collect_type_profile = false)
      : zone_(zone),
        eager_literals_(eager_literals),
        next_id_(BailoutId::FirstUsable().ToInt()),
        suspend_count_(0),
        properties_(zone),
        language_mode_(SLOPPY),
        slot_cache_(zone),
        disable_crankshaft_reason_(kNoReason),
        dont_optimize_reason_(kNoReason),
        catch_prediction_(HandlerTable::UNCAUGHT),
        collect_type_profile_(collect_type_profile) {
    InitializeAstVisitor(stack_limit);
  }

  bool Renumber(FunctionLiteral* node);

 private:
// AST node visitor interface.
#define DEFINE_VISIT(type) void Visit##type(type* node);
  AST_NODE_LIST(DEFINE_VISIT)
#undef DEFINE_VISIT

  void VisitVariableProxy(VariableProxy* node, TypeofMode typeof_mode);
  void VisitVariableProxyReference(VariableProxy* node);
  void VisitPropertyReference(Property* node);
  void VisitReference(Expression* expr);

  void VisitStatementsAndDeclarations(Block* node);
  void VisitStatements(ZoneList<Statement*>* statements);
  void VisitDeclarations(Declaration::List* declarations);
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
  void DisableFullCodegenAndCrankshaft(BailoutReason reason) {
    disable_crankshaft_reason_ = reason;
    properties_.flags() |= AstProperties::kMustUseIgnitionTurbo;
  }

  template <typename Node>
  void ReserveFeedbackSlots(Node* node) {
    node->AssignFeedbackSlots(properties_.get_spec(), language_mode_,
                              &slot_cache_);
  }

  class LanguageModeScope {
   public:
    LanguageModeScope(AstNumberingVisitor* visitor, LanguageMode language_mode)
        : visitor_(visitor), outer_language_mode_(visitor->language_mode_) {
      visitor_->language_mode_ = language_mode;
    }
    ~LanguageModeScope() { visitor_->language_mode_ = outer_language_mode_; }

   private:
    AstNumberingVisitor* visitor_;
    LanguageMode outer_language_mode_;
  };

  BailoutReason dont_optimize_reason() const { return dont_optimize_reason_; }

  Zone* zone() const { return zone_; }

  Zone* zone_;
  Compiler::EagerInnerFunctionLiterals* eager_literals_;
  int next_id_;
  int suspend_count_;
  AstProperties properties_;
  LanguageMode language_mode_;
  // The slot cache allows us to reuse certain feedback slots.
  FeedbackSlotCache slot_cache_;
  BailoutReason disable_crankshaft_reason_;
  BailoutReason dont_optimize_reason_;
  HandlerTable::CatchPrediction catch_prediction_;
  bool collect_type_profile_;

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
  DisableFullCodegenAndCrankshaft(kDebuggerStatement);
}


void AstNumberingVisitor::VisitNativeFunctionLiteral(
    NativeFunctionLiteral* node) {
  IncrementNodeCount();
  DisableOptimization(kNativeFunctionLiteral);
  node->set_base_id(ReserveIdRange(NativeFunctionLiteral::num_ids()));
  ReserveFeedbackSlots(node);
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
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitVariableProxyReference(VariableProxy* node) {
  IncrementNodeCount();
  switch (node->var()->location()) {
    case VariableLocation::LOOKUP:
      DisableFullCodegenAndCrankshaft(
          kReferenceToAVariableWhichRequiresDynamicLookup);
      break;
    case VariableLocation::MODULE:
      DisableFullCodegenAndCrankshaft(kReferenceToModuleVariable);
      break;
    default:
      break;
  }
  node->set_base_id(ReserveIdRange(VariableProxy::num_ids()));
}

void AstNumberingVisitor::VisitVariableProxy(VariableProxy* node,
                                             TypeofMode typeof_mode) {
  VisitVariableProxyReference(node);
  node->AssignFeedbackSlots(properties_.get_spec(), typeof_mode, &slot_cache_);
}

void AstNumberingVisitor::VisitVariableProxy(VariableProxy* node) {
  VisitVariableProxy(node, NOT_INSIDE_TYPEOF);
}


void AstNumberingVisitor::VisitThisFunction(ThisFunction* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(ThisFunction::num_ids()));
}


void AstNumberingVisitor::VisitSuperPropertyReference(
    SuperPropertyReference* node) {
  IncrementNodeCount();
  DisableFullCodegenAndCrankshaft(kSuperReference);
  node->set_base_id(ReserveIdRange(SuperPropertyReference::num_ids()));
  Visit(node->this_var());
  Visit(node->home_object());
}


void AstNumberingVisitor::VisitSuperCallReference(SuperCallReference* node) {
  IncrementNodeCount();
  DisableFullCodegenAndCrankshaft(kSuperReference);
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

  DCHECK(!node->is_async_return() ||
         properties_.flags() & AstProperties::kMustUseIgnitionTurbo);
}

void AstNumberingVisitor::VisitSuspend(Suspend* node) {
  node->set_suspend_id(suspend_count_);
  suspend_count_++;
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(Suspend::num_ids()));
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
  if ((node->op() == Token::TYPEOF) && node->expression()->IsVariableProxy()) {
    VariableProxy* proxy = node->expression()->AsVariableProxy();
    VisitVariableProxy(proxy, INSIDE_TYPEOF);
  } else {
    Visit(node->expression());
  }
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
  Scope* scope = node->scope();
  if (scope != nullptr) {
    LanguageModeScope language_mode_scope(this, scope->language_mode());
    VisitStatementsAndDeclarations(node);
  } else {
    VisitStatementsAndDeclarations(node);
  }
}

void AstNumberingVisitor::VisitStatementsAndDeclarations(Block* node) {
  Scope* scope = node->scope();
  DCHECK(scope == nullptr || !scope->HasBeenRemoved());
  if (scope) VisitDeclarations(scope->declarations());
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
  if (node->is_jsruntime() && catch_prediction_ == HandlerTable::ASYNC_AWAIT) {
    switch (node->context_index()) {
      case Context::ASYNC_FUNCTION_AWAIT_CAUGHT_INDEX:
        node->set_context_index(Context::ASYNC_FUNCTION_AWAIT_UNCAUGHT_INDEX);
        break;
      case Context::ASYNC_GENERATOR_AWAIT_CAUGHT:
        node->set_context_index(Context::ASYNC_GENERATOR_AWAIT_UNCAUGHT);
        break;
      default:
        break;
    }
  }
}


void AstNumberingVisitor::VisitWithStatement(WithStatement* node) {
  IncrementNodeCount();
  DisableFullCodegenAndCrankshaft(kWithStatement);
  Visit(node->expression());
  Visit(node->statement());
}


void AstNumberingVisitor::VisitDoWhileStatement(DoWhileStatement* node) {
  IncrementNodeCount();
  DisableSelfOptimization();
  node->set_base_id(ReserveIdRange(DoWhileStatement::num_ids()));
  node->set_first_suspend_id(suspend_count_);
  Visit(node->body());
  Visit(node->cond());
  node->set_suspend_count(suspend_count_ - node->first_suspend_id());
}


void AstNumberingVisitor::VisitWhileStatement(WhileStatement* node) {
  IncrementNodeCount();
  DisableSelfOptimization();
  node->set_base_id(ReserveIdRange(WhileStatement::num_ids()));
  node->set_first_suspend_id(suspend_count_);
  Visit(node->cond());
  Visit(node->body());
  node->set_suspend_count(suspend_count_ - node->first_suspend_id());
}


void AstNumberingVisitor::VisitTryCatchStatement(TryCatchStatement* node) {
  DCHECK(node->scope() == nullptr || !node->scope()->HasBeenRemoved());
  IncrementNodeCount();
  DisableFullCodegenAndCrankshaft(kTryCatchStatement);
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
  DisableFullCodegenAndCrankshaft(kTryFinallyStatement);
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

void AstNumberingVisitor::VisitSpread(Spread* node) {
  IncrementNodeCount();
  // We can only get here from spread calls currently.
  DisableFullCodegenAndCrankshaft(kSpreadCall);
  node->set_base_id(ReserveIdRange(Spread::num_ids()));
  Visit(node->expression());
}

void AstNumberingVisitor::VisitEmptyParentheses(EmptyParentheses* node) {
  UNREACHABLE();
}

void AstNumberingVisitor::VisitGetIterator(GetIterator* node) {
  IncrementNodeCount();
  DisableFullCodegenAndCrankshaft(kGetIterator);
  node->set_base_id(ReserveIdRange(GetIterator::num_ids()));
  Visit(node->iterable());
  ReserveFeedbackSlots(node);
}

void AstNumberingVisitor::VisitImportCallExpression(
    ImportCallExpression* node) {
  IncrementNodeCount();
  DisableFullCodegenAndCrankshaft(kDynamicImport);
  Visit(node->argument());
}

void AstNumberingVisitor::VisitForInStatement(ForInStatement* node) {
  IncrementNodeCount();
  DisableSelfOptimization();
  node->set_base_id(ReserveIdRange(ForInStatement::num_ids()));
  Visit(node->enumerable());  // Not part of loop.
  node->set_first_suspend_id(suspend_count_);
  Visit(node->each());
  Visit(node->body());
  node->set_suspend_count(suspend_count_ - node->first_suspend_id());
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitForOfStatement(ForOfStatement* node) {
  IncrementNodeCount();
  DisableFullCodegenAndCrankshaft(kForOfStatement);
  node->set_base_id(ReserveIdRange(ForOfStatement::num_ids()));
  Visit(node->assign_iterator());  // Not part of loop.
  node->set_first_suspend_id(suspend_count_);
  Visit(node->next_result());
  Visit(node->result_done());
  Visit(node->assign_each());
  Visit(node->body());
  node->set_suspend_count(suspend_count_ - node->first_suspend_id());
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
  node->set_first_suspend_id(suspend_count_);
  if (node->cond() != NULL) Visit(node->cond());
  if (node->next() != NULL) Visit(node->next());
  Visit(node->body());
  node->set_suspend_count(suspend_count_ - node->first_suspend_id());
}


void AstNumberingVisitor::VisitClassLiteral(ClassLiteral* node) {
  IncrementNodeCount();
  DisableFullCodegenAndCrankshaft(kClassLiteral);
  node->set_base_id(ReserveIdRange(ClassLiteral::num_ids()));
  LanguageModeScope language_mode_scope(this, STRICT);
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
  node->InitDepthAndFlags();
  // Mark all computed expressions that are bound to a key that
  // is shadowed by a later occurrence of the same key. For the
  // marked expressions, no store code will be is emitted.
  node->CalculateEmitStore(zone_);
  ReserveFeedbackSlots(node);
}

void AstNumberingVisitor::VisitLiteralProperty(LiteralProperty* node) {
  if (node->is_computed_name())
    DisableFullCodegenAndCrankshaft(kComputedPropertyName);
  Visit(node->key());
  Visit(node->value());
}

void AstNumberingVisitor::VisitArrayLiteral(ArrayLiteral* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(node->num_ids()));
  for (int i = 0; i < node->values()->length(); i++) {
    Visit(node->values()->at(i));
  }
  node->InitDepthAndFlags();
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitCall(Call* node) {
  if (node->is_possibly_eval()) {
    DisableFullCodegenAndCrankshaft(kFunctionCallsEval);
  }
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
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(FunctionLiteral::num_ids()));
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
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitRewritableExpression(
    RewritableExpression* node) {
  IncrementNodeCount();
  node->set_base_id(ReserveIdRange(RewritableExpression::num_ids()));
  Visit(node->expression());
}


bool AstNumberingVisitor::Renumber(FunctionLiteral* node) {
  DeclarationScope* scope = node->scope();
  DCHECK(!scope->HasBeenRemoved());

  if (scope->new_target_var() != nullptr ||
      scope->this_function_var() != nullptr) {
    DisableFullCodegenAndCrankshaft(kSuperReference);
  }

  if (scope->arguments() != nullptr &&
      !scope->arguments()->IsStackAllocated()) {
    DisableFullCodegenAndCrankshaft(kContextAllocatedArguments);
  }

  if (scope->rest_parameter() != nullptr) {
    DisableFullCodegenAndCrankshaft(kRestParameter);
  }

  if (IsResumableFunction(node->kind())) {
    DisableFullCodegenAndCrankshaft(kGenerator);
  }

  if (IsClassConstructor(node->kind())) {
    DisableFullCodegenAndCrankshaft(kClassConstructorFunction);
  }

  LanguageModeScope language_mode_scope(this, node->language_mode());

  if (collect_type_profile_) {
    properties_.get_spec()->AddTypeProfileSlot();
  }

  VisitDeclarations(scope->declarations());
  VisitStatements(node->body());

  node->set_ast_properties(&properties_);
  node->set_dont_optimize_reason(dont_optimize_reason());
  node->set_suspend_count(suspend_count_);

  if (FLAG_trace_opt && !FLAG_turbo) {
    if (disable_crankshaft_reason_ != kNoReason) {
      // TODO(leszeks): This is a quick'n'dirty fix to allow the debug name of
      // the function to be accessed in the below print. This DCHECK will fail
      // if we move ast numbering off the main thread, but that won't be before
      // we remove FCG, in which case this entire check isn't necessary anyway.
      AllowHandleDereference allow_deref;
      DCHECK(!node->debug_name().is_null());

      PrintF("[enforcing Ignition and TurboFan for %s because: %s\n",
             node->debug_name()->ToCString().get(),
             GetBailoutReason(disable_crankshaft_reason_));
    }
  }

  return !HasStackOverflow();
}

bool AstNumbering::Renumber(
    uintptr_t stack_limit, Zone* zone, FunctionLiteral* function,
    Compiler::EagerInnerFunctionLiterals* eager_literals,
    bool collect_type_profile) {
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  AstNumberingVisitor visitor(stack_limit, zone, eager_literals,
                              collect_type_profile);
  return visitor.Renumber(function);
}
}  // namespace internal
}  // namespace v8
