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
        disable_fullcodegen_reason_(kNoReason),
        dont_optimize_reason_(kNoReason),
        dont_self_optimize_(false),
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
  void VisitSuspend(Suspend* node);

  void VisitStatementsAndDeclarations(Block* node);
  void VisitStatements(ZoneList<Statement*>* statements);
  void VisitDeclarations(Declaration::List* declarations);
  void VisitArguments(ZoneList<Expression*>* arguments);
  void VisitLiteralProperty(LiteralProperty* property);

  int ReserveId() {
    int tmp = next_id_;
    next_id_ += 1;
    return tmp;
  }

  void IncrementNodeCount() { properties_.add_node_count(1); }
  void DisableSelfOptimization() { dont_self_optimize_ = true; }
  void DisableOptimization(BailoutReason reason) {
    dont_optimize_reason_ = reason;
    DisableSelfOptimization();
  }
  void DisableFullCodegen(BailoutReason reason) {
    disable_fullcodegen_reason_ = reason;
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
  BailoutReason disable_fullcodegen_reason_;
  BailoutReason dont_optimize_reason_;
  bool dont_self_optimize_;
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
  DisableFullCodegen(kDebuggerStatement);
}


void AstNumberingVisitor::VisitNativeFunctionLiteral(
    NativeFunctionLiteral* node) {
  IncrementNodeCount();
  DisableOptimization(kNativeFunctionLiteral);
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitDoExpression(DoExpression* node) {
  IncrementNodeCount();
  Visit(node->block());
  Visit(node->result());
}


void AstNumberingVisitor::VisitLiteral(Literal* node) {
  IncrementNodeCount();
}


void AstNumberingVisitor::VisitRegExpLiteral(RegExpLiteral* node) {
  IncrementNodeCount();
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitVariableProxyReference(VariableProxy* node) {
  IncrementNodeCount();
  switch (node->var()->location()) {
    case VariableLocation::LOOKUP:
      DisableFullCodegen(kReferenceToAVariableWhichRequiresDynamicLookup);
      break;
    case VariableLocation::MODULE:
      DisableFullCodegen(kReferenceToModuleVariable);
      break;
    default:
      break;
  }
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
}


void AstNumberingVisitor::VisitSuperPropertyReference(
    SuperPropertyReference* node) {
  IncrementNodeCount();
  DisableFullCodegen(kSuperReference);
  Visit(node->this_var());
  Visit(node->home_object());
}


void AstNumberingVisitor::VisitSuperCallReference(SuperCallReference* node) {
  IncrementNodeCount();
  DisableFullCodegen(kSuperReference);
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

  DCHECK(!node->is_async_return() || disable_fullcodegen_reason_ != kNoReason);
}

void AstNumberingVisitor::VisitSuspend(Suspend* node) {
  node->set_suspend_id(suspend_count_);
  suspend_count_++;
  IncrementNodeCount();
  Visit(node->expression());
}

void AstNumberingVisitor::VisitYield(Yield* node) { VisitSuspend(node); }

void AstNumberingVisitor::VisitYieldStar(YieldStar* node) {
  VisitSuspend(node);
  ReserveFeedbackSlots(node);
}

void AstNumberingVisitor::VisitAwait(Await* node) { VisitSuspend(node); }

void AstNumberingVisitor::VisitThrow(Throw* node) {
  IncrementNodeCount();
  Visit(node->exception());
}


void AstNumberingVisitor::VisitUnaryOperation(UnaryOperation* node) {
  IncrementNodeCount();
  if ((node->op() == Token::TYPEOF) && node->expression()->IsVariableProxy()) {
    VariableProxy* proxy = node->expression()->AsVariableProxy();
    VisitVariableProxy(proxy, INSIDE_TYPEOF);
  } else {
    Visit(node->expression());
  }
}


void AstNumberingVisitor::VisitCountOperation(CountOperation* node) {
  IncrementNodeCount();
  Visit(node->expression());
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitBlock(Block* node) {
  IncrementNodeCount();
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
  VisitArguments(node->arguments());
}


void AstNumberingVisitor::VisitWithStatement(WithStatement* node) {
  IncrementNodeCount();
  DisableFullCodegen(kWithStatement);
  Visit(node->expression());
  Visit(node->statement());
}


void AstNumberingVisitor::VisitDoWhileStatement(DoWhileStatement* node) {
  IncrementNodeCount();
  DisableSelfOptimization();
  node->set_osr_id(ReserveId());
  node->set_first_suspend_id(suspend_count_);
  Visit(node->body());
  Visit(node->cond());
  node->set_suspend_count(suspend_count_ - node->first_suspend_id());
}


void AstNumberingVisitor::VisitWhileStatement(WhileStatement* node) {
  IncrementNodeCount();
  DisableSelfOptimization();
  node->set_osr_id(ReserveId());
  node->set_first_suspend_id(suspend_count_);
  Visit(node->cond());
  Visit(node->body());
  node->set_suspend_count(suspend_count_ - node->first_suspend_id());
}


void AstNumberingVisitor::VisitTryCatchStatement(TryCatchStatement* node) {
  DCHECK(node->scope() == nullptr || !node->scope()->HasBeenRemoved());
  IncrementNodeCount();
  DisableFullCodegen(kTryCatchStatement);
  Visit(node->try_block());
  Visit(node->catch_block());
}


void AstNumberingVisitor::VisitTryFinallyStatement(TryFinallyStatement* node) {
  IncrementNodeCount();
  DisableFullCodegen(kTryFinallyStatement);
  Visit(node->try_block());
  Visit(node->finally_block());
}


void AstNumberingVisitor::VisitPropertyReference(Property* node) {
  IncrementNodeCount();
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

  if (node->is_compound()) VisitBinaryOperation(node->binary_operation());
  VisitReference(node->target());
  Visit(node->value());
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitBinaryOperation(BinaryOperation* node) {
  IncrementNodeCount();
  Visit(node->left());
  Visit(node->right());
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitCompareOperation(CompareOperation* node) {
  IncrementNodeCount();
  Visit(node->left());
  Visit(node->right());
  ReserveFeedbackSlots(node);
}

void AstNumberingVisitor::VisitSpread(Spread* node) {
  IncrementNodeCount();
  // We can only get here from spread calls currently.
  DisableFullCodegen(kSpreadCall);
  Visit(node->expression());
}

void AstNumberingVisitor::VisitEmptyParentheses(EmptyParentheses* node) {
  UNREACHABLE();
}

void AstNumberingVisitor::VisitGetIterator(GetIterator* node) {
  IncrementNodeCount();
  DisableFullCodegen(kGetIterator);
  Visit(node->iterable());
  ReserveFeedbackSlots(node);
}

void AstNumberingVisitor::VisitImportCallExpression(
    ImportCallExpression* node) {
  IncrementNodeCount();
  DisableFullCodegen(kDynamicImport);
  Visit(node->argument());
}

void AstNumberingVisitor::VisitForInStatement(ForInStatement* node) {
  IncrementNodeCount();
  DisableSelfOptimization();
  node->set_osr_id(ReserveId());
  Visit(node->enumerable());  // Not part of loop.
  node->set_first_suspend_id(suspend_count_);
  Visit(node->each());
  Visit(node->body());
  node->set_suspend_count(suspend_count_ - node->first_suspend_id());
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitForOfStatement(ForOfStatement* node) {
  IncrementNodeCount();
  DisableFullCodegen(kForOfStatement);
  node->set_osr_id(ReserveId());
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
  Visit(node->condition());
  Visit(node->then_expression());
  Visit(node->else_expression());
}


void AstNumberingVisitor::VisitIfStatement(IfStatement* node) {
  IncrementNodeCount();
  Visit(node->condition());
  Visit(node->then_statement());
  if (node->HasElseStatement()) {
    Visit(node->else_statement());
  }
}


void AstNumberingVisitor::VisitSwitchStatement(SwitchStatement* node) {
  IncrementNodeCount();
  Visit(node->tag());
  ZoneList<CaseClause*>* cases = node->cases();
  for (int i = 0; i < cases->length(); i++) {
    VisitCaseClause(cases->at(i));
  }
}


void AstNumberingVisitor::VisitCaseClause(CaseClause* node) {
  IncrementNodeCount();
  if (!node->is_default()) Visit(node->label());
  VisitStatements(node->statements());
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitForStatement(ForStatement* node) {
  IncrementNodeCount();
  DisableSelfOptimization();
  node->set_osr_id(ReserveId());
  if (node->init() != NULL) Visit(node->init());  // Not part of loop.
  node->set_first_suspend_id(suspend_count_);
  if (node->cond() != NULL) Visit(node->cond());
  if (node->next() != NULL) Visit(node->next());
  Visit(node->body());
  node->set_suspend_count(suspend_count_ - node->first_suspend_id());
}


void AstNumberingVisitor::VisitClassLiteral(ClassLiteral* node) {
  IncrementNodeCount();
  DisableFullCodegen(kClassLiteral);
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
  if (node->is_computed_name()) DisableFullCodegen(kComputedPropertyName);
  Visit(node->key());
  Visit(node->value());
}

void AstNumberingVisitor::VisitArrayLiteral(ArrayLiteral* node) {
  IncrementNodeCount();
  for (int i = 0; i < node->values()->length(); i++) {
    Visit(node->values()->at(i));
  }
  node->InitDepthAndFlags();
  ReserveFeedbackSlots(node);
}


void AstNumberingVisitor::VisitCall(Call* node) {
  if (node->is_possibly_eval()) {
    DisableFullCodegen(kFunctionCallsEval);
  }
  IncrementNodeCount();
  ReserveFeedbackSlots(node);
  Visit(node->expression());
  VisitArguments(node->arguments());
}


void AstNumberingVisitor::VisitCallNew(CallNew* node) {
  IncrementNodeCount();
  ReserveFeedbackSlots(node);
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
  Visit(node->expression());
}


bool AstNumberingVisitor::Renumber(FunctionLiteral* node) {
  DeclarationScope* scope = node->scope();
  DCHECK(!scope->HasBeenRemoved());

  if (scope->new_target_var() != nullptr ||
      scope->this_function_var() != nullptr) {
    DisableFullCodegen(kSuperReference);
  }

  if (scope->arguments() != nullptr &&
      !scope->arguments()->IsStackAllocated()) {
    DisableFullCodegen(kContextAllocatedArguments);
  }

  if (scope->rest_parameter() != nullptr) {
    DisableFullCodegen(kRestParameter);
  }

  if (IsResumableFunction(node->kind())) {
    DisableFullCodegen(kGenerator);
  }

  if (IsClassConstructor(node->kind())) {
    DisableFullCodegen(kClassConstructorFunction);
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

  if (dont_self_optimize_) {
    node->set_dont_self_optimize();
  }
  if (disable_fullcodegen_reason_ != kNoReason) {
    node->set_must_use_ignition();
    if (FLAG_trace_opt && FLAG_stress_fullcodegen) {
      // TODO(leszeks): This is a quick'n'dirty fix to allow the debug name of
      // the function to be accessed in the below print. This DCHECK will fail
      // if we move ast numbering off the main thread, but that won't be before
      // we remove FCG, in which case this entire check isn't necessary anyway.
      AllowHandleDereference allow_deref;
      DCHECK(!node->debug_name().is_null());

      PrintF("[enforcing Ignition for %s because: %s\n",
             node->debug_name()->ToCString().get(),
             GetBailoutReason(disable_fullcodegen_reason_));
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
