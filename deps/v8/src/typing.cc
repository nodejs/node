// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/typing.h"

#include "src/frames.h"
#include "src/frames-inl.h"
#include "src/ostreams.h"
#include "src/parser.h"  // for CompileTimeValue; TODO(rossberg): should move
#include "src/scopes.h"

namespace v8 {
namespace internal {


AstTyper::AstTyper(CompilationInfo* info)
    : info_(info),
      oracle_(
          handle(info->closure()->shared()->code()),
          handle(info->closure()->shared()->feedback_vector()),
          handle(info->closure()->context()->native_context()),
          info->zone()),
      store_(info->zone()) {
  InitializeAstVisitor(info->zone());
}


#define RECURSE(call)                         \
  do {                                        \
    DCHECK(!visitor->HasStackOverflow());     \
    call;                                     \
    if (visitor->HasStackOverflow()) return;  \
  } while (false)

void AstTyper::Run(CompilationInfo* info) {
  AstTyper* visitor = new(info->zone()) AstTyper(info);
  Scope* scope = info->scope();

  // Handle implicit declaration of the function name in named function
  // expressions before other declarations.
  if (scope->is_function_scope() && scope->function() != NULL) {
    RECURSE(visitor->VisitVariableDeclaration(scope->function()));
  }
  RECURSE(visitor->VisitDeclarations(scope->declarations()));
  RECURSE(visitor->VisitStatements(info->function()->body()));
}

#undef RECURSE


#ifdef OBJECT_PRINT
  static void PrintObserved(Variable* var, Object* value, Type* type) {
    OFStream os(stdout);
    os << "  observed " << (var->IsParameter() ? "param" : "local") << "  ";
    var->name()->Print(os);
    os << " : " << Brief(value) << " -> ";
    type->PrintTo(os);
    os << std::endl;
  }
#endif  // OBJECT_PRINT


Effect AstTyper::ObservedOnStack(Object* value) {
  Type* lower = Type::NowOf(value, zone());
  return Effect(Bounds(lower, Type::Any(zone())));
}


void AstTyper::ObserveTypesAtOsrEntry(IterationStatement* stmt) {
  if (stmt->OsrEntryId() != info_->osr_ast_id()) return;

  DisallowHeapAllocation no_gc;
  JavaScriptFrameIterator it(isolate());
  JavaScriptFrame* frame = it.frame();
  Scope* scope = info_->scope();

  // Assert that the frame on the stack belongs to the function we want to OSR.
  DCHECK_EQ(*info_->closure(), frame->function());

  int params = scope->num_parameters();
  int locals = scope->StackLocalCount();

  // Use sequential composition to achieve desired narrowing.
  // The receiver is a parameter with index -1.
  store_.Seq(parameter_index(-1), ObservedOnStack(frame->receiver()));
  for (int i = 0; i < params; i++) {
    store_.Seq(parameter_index(i), ObservedOnStack(frame->GetParameter(i)));
  }

  for (int i = 0; i < locals; i++) {
    store_.Seq(stack_local_index(i), ObservedOnStack(frame->GetExpression(i)));
  }

#ifdef OBJECT_PRINT
  if (FLAG_trace_osr && FLAG_print_scopes) {
    PrintObserved(scope->receiver(),
                  frame->receiver(),
                  store_.LookupBounds(parameter_index(-1)).lower);

    for (int i = 0; i < params; i++) {
      PrintObserved(scope->parameter(i),
                    frame->GetParameter(i),
                    store_.LookupBounds(parameter_index(i)).lower);
    }

    ZoneList<Variable*> local_vars(locals, zone());
    ZoneList<Variable*> context_vars(scope->ContextLocalCount(), zone());
    scope->CollectStackAndContextLocals(&local_vars, &context_vars);
    for (int i = 0; i < locals; i++) {
      PrintObserved(local_vars.at(i),
                    frame->GetExpression(i),
                    store_.LookupBounds(stack_local_index(i)).lower);
    }
  }
#endif  // OBJECT_PRINT
}


#define RECURSE(call)                \
  do {                               \
    DCHECK(!HasStackOverflow());     \
    call;                            \
    if (HasStackOverflow()) return;  \
  } while (false)


void AstTyper::VisitStatements(ZoneList<Statement*>* stmts) {
  for (int i = 0; i < stmts->length(); ++i) {
    Statement* stmt = stmts->at(i);
    RECURSE(Visit(stmt));
    if (stmt->IsJump()) break;
  }
}


void AstTyper::VisitBlock(Block* stmt) {
  RECURSE(VisitStatements(stmt->statements()));
  if (stmt->labels() != NULL) {
    store_.Forget();  // Control may transfer here via 'break l'.
  }
}


void AstTyper::VisitExpressionStatement(ExpressionStatement* stmt) {
  RECURSE(Visit(stmt->expression()));
}


void AstTyper::VisitEmptyStatement(EmptyStatement* stmt) {
}


void AstTyper::VisitIfStatement(IfStatement* stmt) {
  // Collect type feedback.
  if (!stmt->condition()->ToBooleanIsTrue() &&
      !stmt->condition()->ToBooleanIsFalse()) {
    stmt->condition()->RecordToBooleanTypeFeedback(oracle());
  }

  RECURSE(Visit(stmt->condition()));
  Effects then_effects = EnterEffects();
  RECURSE(Visit(stmt->then_statement()));
  ExitEffects();
  Effects else_effects = EnterEffects();
  RECURSE(Visit(stmt->else_statement()));
  ExitEffects();
  then_effects.Alt(else_effects);
  store_.Seq(then_effects);
}


void AstTyper::VisitContinueStatement(ContinueStatement* stmt) {
  // TODO(rossberg): is it worth having a non-termination effect?
}


void AstTyper::VisitBreakStatement(BreakStatement* stmt) {
  // TODO(rossberg): is it worth having a non-termination effect?
}


void AstTyper::VisitReturnStatement(ReturnStatement* stmt) {
  // Collect type feedback.
  // TODO(rossberg): we only need this for inlining into test contexts...
  stmt->expression()->RecordToBooleanTypeFeedback(oracle());

  RECURSE(Visit(stmt->expression()));
  // TODO(rossberg): is it worth having a non-termination effect?
}


void AstTyper::VisitWithStatement(WithStatement* stmt) {
  RECURSE(stmt->expression());
  RECURSE(stmt->statement());
}


void AstTyper::VisitSwitchStatement(SwitchStatement* stmt) {
  RECURSE(Visit(stmt->tag()));

  ZoneList<CaseClause*>* clauses = stmt->cases();
  Effects local_effects(zone());
  bool complex_effects = false;  // True for label effects or fall-through.

  for (int i = 0; i < clauses->length(); ++i) {
    CaseClause* clause = clauses->at(i);

    Effects clause_effects = EnterEffects();

    if (!clause->is_default()) {
      Expression* label = clause->label();
      // Collect type feedback.
      Type* tag_type;
      Type* label_type;
      Type* combined_type;
      oracle()->CompareType(clause->CompareId(),
                            &tag_type, &label_type, &combined_type);
      NarrowLowerType(stmt->tag(), tag_type);
      NarrowLowerType(label, label_type);
      clause->set_compare_type(combined_type);

      RECURSE(Visit(label));
      if (!clause_effects.IsEmpty()) complex_effects = true;
    }

    ZoneList<Statement*>* stmts = clause->statements();
    RECURSE(VisitStatements(stmts));
    ExitEffects();
    if (stmts->is_empty() || stmts->last()->IsJump()) {
      local_effects.Alt(clause_effects);
    } else {
      complex_effects = true;
    }
  }

  if (complex_effects) {
    store_.Forget();  // Reached this in unknown state.
  } else {
    store_.Seq(local_effects);
  }
}


void AstTyper::VisitCaseClause(CaseClause* clause) {
  UNREACHABLE();
}


void AstTyper::VisitDoWhileStatement(DoWhileStatement* stmt) {
  // Collect type feedback.
  if (!stmt->cond()->ToBooleanIsTrue()) {
    stmt->cond()->RecordToBooleanTypeFeedback(oracle());
  }

  // TODO(rossberg): refine the unconditional Forget (here and elsewhere) by
  // computing the set of variables assigned in only some of the origins of the
  // control transfer (such as the loop body here).
  store_.Forget();  // Control may transfer here via looping or 'continue'.
  ObserveTypesAtOsrEntry(stmt);
  RECURSE(Visit(stmt->body()));
  RECURSE(Visit(stmt->cond()));
  store_.Forget();  // Control may transfer here via 'break'.
}


void AstTyper::VisitWhileStatement(WhileStatement* stmt) {
  // Collect type feedback.
  if (!stmt->cond()->ToBooleanIsTrue()) {
    stmt->cond()->RecordToBooleanTypeFeedback(oracle());
  }

  store_.Forget();  // Control may transfer here via looping or 'continue'.
  RECURSE(Visit(stmt->cond()));
  ObserveTypesAtOsrEntry(stmt);
  RECURSE(Visit(stmt->body()));
  store_.Forget();  // Control may transfer here via termination or 'break'.
}


void AstTyper::VisitForStatement(ForStatement* stmt) {
  if (stmt->init() != NULL) {
    RECURSE(Visit(stmt->init()));
  }
  store_.Forget();  // Control may transfer here via looping.
  if (stmt->cond() != NULL) {
    // Collect type feedback.
    stmt->cond()->RecordToBooleanTypeFeedback(oracle());

    RECURSE(Visit(stmt->cond()));
  }
  ObserveTypesAtOsrEntry(stmt);
  RECURSE(Visit(stmt->body()));
  if (stmt->next() != NULL) {
    store_.Forget();  // Control may transfer here via 'continue'.
    RECURSE(Visit(stmt->next()));
  }
  store_.Forget();  // Control may transfer here via termination or 'break'.
}


void AstTyper::VisitForInStatement(ForInStatement* stmt) {
  // Collect type feedback.
  stmt->set_for_in_type(static_cast<ForInStatement::ForInType>(
      oracle()->ForInType(stmt->ForInFeedbackSlot())));

  RECURSE(Visit(stmt->enumerable()));
  store_.Forget();  // Control may transfer here via looping or 'continue'.
  ObserveTypesAtOsrEntry(stmt);
  RECURSE(Visit(stmt->body()));
  store_.Forget();  // Control may transfer here via 'break'.
}


void AstTyper::VisitForOfStatement(ForOfStatement* stmt) {
  RECURSE(Visit(stmt->iterable()));
  store_.Forget();  // Control may transfer here via looping or 'continue'.
  RECURSE(Visit(stmt->body()));
  store_.Forget();  // Control may transfer here via 'break'.
}


void AstTyper::VisitTryCatchStatement(TryCatchStatement* stmt) {
  Effects try_effects = EnterEffects();
  RECURSE(Visit(stmt->try_block()));
  ExitEffects();
  Effects catch_effects = EnterEffects();
  store_.Forget();  // Control may transfer here via 'throw'.
  RECURSE(Visit(stmt->catch_block()));
  ExitEffects();
  try_effects.Alt(catch_effects);
  store_.Seq(try_effects);
  // At this point, only variables that were reassigned in the catch block are
  // still remembered.
}


void AstTyper::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  RECURSE(Visit(stmt->try_block()));
  store_.Forget();  // Control may transfer here via 'throw'.
  RECURSE(Visit(stmt->finally_block()));
}


void AstTyper::VisitDebuggerStatement(DebuggerStatement* stmt) {
  store_.Forget();  // May do whatever.
}


void AstTyper::VisitFunctionLiteral(FunctionLiteral* expr) {
  expr->InitializeSharedInfo(Handle<Code>(info_->closure()->shared()->code()));
}


void AstTyper::VisitClassLiteral(ClassLiteral* expr) {}


void AstTyper::VisitNativeFunctionLiteral(NativeFunctionLiteral* expr) {
}


void AstTyper::VisitConditional(Conditional* expr) {
  // Collect type feedback.
  expr->condition()->RecordToBooleanTypeFeedback(oracle());

  RECURSE(Visit(expr->condition()));
  Effects then_effects = EnterEffects();
  RECURSE(Visit(expr->then_expression()));
  ExitEffects();
  Effects else_effects = EnterEffects();
  RECURSE(Visit(expr->else_expression()));
  ExitEffects();
  then_effects.Alt(else_effects);
  store_.Seq(then_effects);

  NarrowType(expr, Bounds::Either(
      expr->then_expression()->bounds(),
      expr->else_expression()->bounds(), zone()));
}


void AstTyper::VisitVariableProxy(VariableProxy* expr) {
  Variable* var = expr->var();
  if (var->IsStackAllocated()) {
    NarrowType(expr, store_.LookupBounds(variable_index(var)));
  }
}


void AstTyper::VisitLiteral(Literal* expr) {
  Type* type = Type::Constant(expr->value(), zone());
  NarrowType(expr, Bounds(type));
}


void AstTyper::VisitRegExpLiteral(RegExpLiteral* expr) {
  NarrowType(expr, Bounds(Type::RegExp(zone())));
}


void AstTyper::VisitObjectLiteral(ObjectLiteral* expr) {
  ZoneList<ObjectLiteral::Property*>* properties = expr->properties();
  for (int i = 0; i < properties->length(); ++i) {
    ObjectLiteral::Property* prop = properties->at(i);

    // Collect type feedback.
    if ((prop->kind() == ObjectLiteral::Property::MATERIALIZED_LITERAL &&
        !CompileTimeValue::IsCompileTimeValue(prop->value())) ||
        prop->kind() == ObjectLiteral::Property::COMPUTED) {
      if (prop->key()->value()->IsInternalizedString() && prop->emit_store()) {
        prop->RecordTypeFeedback(oracle());
      }
    }

    RECURSE(Visit(prop->value()));
  }

  NarrowType(expr, Bounds(Type::Object(zone())));
}


void AstTyper::VisitArrayLiteral(ArrayLiteral* expr) {
  ZoneList<Expression*>* values = expr->values();
  for (int i = 0; i < values->length(); ++i) {
    Expression* value = values->at(i);
    RECURSE(Visit(value));
  }

  NarrowType(expr, Bounds(Type::Array(zone())));
}


void AstTyper::VisitAssignment(Assignment* expr) {
  // Collect type feedback.
  Property* prop = expr->target()->AsProperty();
  if (prop != NULL) {
    TypeFeedbackId id = expr->AssignmentFeedbackId();
    expr->set_is_uninitialized(oracle()->StoreIsUninitialized(id));
    if (!expr->IsUninitialized()) {
      if (prop->key()->IsPropertyName()) {
        Literal* lit_key = prop->key()->AsLiteral();
        DCHECK(lit_key != NULL && lit_key->value()->IsString());
        Handle<String> name = Handle<String>::cast(lit_key->value());
        oracle()->AssignmentReceiverTypes(id, name, expr->GetReceiverTypes());
      } else {
        KeyedAccessStoreMode store_mode;
        IcCheckType key_type;
        oracle()->KeyedAssignmentReceiverTypes(id, expr->GetReceiverTypes(),
                                               &store_mode, &key_type);
        expr->set_store_mode(store_mode);
        expr->set_key_type(key_type);
      }
    }
  }

  Expression* rhs =
      expr->is_compound() ? expr->binary_operation() : expr->value();
  RECURSE(Visit(expr->target()));
  RECURSE(Visit(rhs));
  NarrowType(expr, rhs->bounds());

  VariableProxy* proxy = expr->target()->AsVariableProxy();
  if (proxy != NULL && proxy->var()->IsStackAllocated()) {
    store_.Seq(variable_index(proxy->var()), Effect(expr->bounds()));
  }
}


void AstTyper::VisitYield(Yield* expr) {
  RECURSE(Visit(expr->generator_object()));
  RECURSE(Visit(expr->expression()));

  // We don't know anything about the result type.
}


void AstTyper::VisitThrow(Throw* expr) {
  RECURSE(Visit(expr->exception()));
  // TODO(rossberg): is it worth having a non-termination effect?

  NarrowType(expr, Bounds(Type::None(zone())));
}


void AstTyper::VisitProperty(Property* expr) {
  // Collect type feedback.
  TypeFeedbackId id = expr->PropertyFeedbackId();
  expr->set_is_uninitialized(oracle()->LoadIsUninitialized(id));
  if (!expr->IsUninitialized()) {
    if (expr->key()->IsPropertyName()) {
      Literal* lit_key = expr->key()->AsLiteral();
      DCHECK(lit_key != NULL && lit_key->value()->IsString());
      Handle<String> name = Handle<String>::cast(lit_key->value());
      oracle()->PropertyReceiverTypes(id, name, expr->GetReceiverTypes());
    } else {
      bool is_string;
      oracle()->KeyedPropertyReceiverTypes(
          id, expr->GetReceiverTypes(), &is_string);
      expr->set_is_string_access(is_string);
    }
  }

  RECURSE(Visit(expr->obj()));
  RECURSE(Visit(expr->key()));

  // We don't know anything about the result type.
}


void AstTyper::VisitCall(Call* expr) {
  // Collect type feedback.
  RECURSE(Visit(expr->expression()));
  if (!expr->expression()->IsProperty() &&
      expr->IsUsingCallFeedbackSlot(isolate()) &&
      oracle()->CallIsMonomorphic(expr->CallFeedbackSlot())) {
    expr->set_target(oracle()->GetCallTarget(expr->CallFeedbackSlot()));
    Handle<AllocationSite> site =
        oracle()->GetCallAllocationSite(expr->CallFeedbackSlot());
    expr->set_allocation_site(site);
  }

  ZoneList<Expression*>* args = expr->arguments();
  for (int i = 0; i < args->length(); ++i) {
    Expression* arg = args->at(i);
    RECURSE(Visit(arg));
  }

  VariableProxy* proxy = expr->expression()->AsVariableProxy();
  if (proxy != NULL && proxy->var()->is_possibly_eval(isolate())) {
    store_.Forget();  // Eval could do whatever to local variables.
  }

  // We don't know anything about the result type.
}


void AstTyper::VisitCallNew(CallNew* expr) {
  // Collect type feedback.
  expr->RecordTypeFeedback(oracle());

  RECURSE(Visit(expr->expression()));
  ZoneList<Expression*>* args = expr->arguments();
  for (int i = 0; i < args->length(); ++i) {
    Expression* arg = args->at(i);
    RECURSE(Visit(arg));
  }

  NarrowType(expr, Bounds(Type::None(zone()), Type::Receiver(zone())));
}


void AstTyper::VisitCallRuntime(CallRuntime* expr) {
  ZoneList<Expression*>* args = expr->arguments();
  for (int i = 0; i < args->length(); ++i) {
    Expression* arg = args->at(i);
    RECURSE(Visit(arg));
  }

  // We don't know anything about the result type.
}


void AstTyper::VisitUnaryOperation(UnaryOperation* expr) {
  // Collect type feedback.
  if (expr->op() == Token::NOT) {
    // TODO(rossberg): only do in test or value context.
    expr->expression()->RecordToBooleanTypeFeedback(oracle());
  }

  RECURSE(Visit(expr->expression()));

  switch (expr->op()) {
    case Token::NOT:
    case Token::DELETE:
      NarrowType(expr, Bounds(Type::Boolean(zone())));
      break;
    case Token::VOID:
      NarrowType(expr, Bounds(Type::Undefined(zone())));
      break;
    case Token::TYPEOF:
      NarrowType(expr, Bounds(Type::InternalizedString(zone())));
      break;
    default:
      UNREACHABLE();
  }
}


void AstTyper::VisitCountOperation(CountOperation* expr) {
  // Collect type feedback.
  TypeFeedbackId store_id = expr->CountStoreFeedbackId();
  KeyedAccessStoreMode store_mode;
  IcCheckType key_type;
  oracle()->GetStoreModeAndKeyType(store_id, &store_mode, &key_type);
  expr->set_store_mode(store_mode);
  expr->set_key_type(key_type);
  oracle()->CountReceiverTypes(store_id, expr->GetReceiverTypes());
  expr->set_type(oracle()->CountType(expr->CountBinOpFeedbackId()));
  // TODO(rossberg): merge the count type with the generic expression type.

  RECURSE(Visit(expr->expression()));

  NarrowType(expr, Bounds(Type::SignedSmall(zone()), Type::Number(zone())));

  VariableProxy* proxy = expr->expression()->AsVariableProxy();
  if (proxy != NULL && proxy->var()->IsStackAllocated()) {
    store_.Seq(variable_index(proxy->var()), Effect(expr->bounds()));
  }
}


void AstTyper::VisitBinaryOperation(BinaryOperation* expr) {
  // Collect type feedback.
  Type* type;
  Type* left_type;
  Type* right_type;
  Maybe<int> fixed_right_arg;
  Handle<AllocationSite> allocation_site;
  oracle()->BinaryType(expr->BinaryOperationFeedbackId(),
      &left_type, &right_type, &type, &fixed_right_arg,
      &allocation_site, expr->op());
  NarrowLowerType(expr, type);
  NarrowLowerType(expr->left(), left_type);
  NarrowLowerType(expr->right(), right_type);
  expr->set_allocation_site(allocation_site);
  expr->set_fixed_right_arg(fixed_right_arg);
  if (expr->op() == Token::OR || expr->op() == Token::AND) {
    expr->left()->RecordToBooleanTypeFeedback(oracle());
  }

  switch (expr->op()) {
    case Token::COMMA:
      RECURSE(Visit(expr->left()));
      RECURSE(Visit(expr->right()));
      NarrowType(expr, expr->right()->bounds());
      break;
    case Token::OR:
    case Token::AND: {
      Effects left_effects = EnterEffects();
      RECURSE(Visit(expr->left()));
      ExitEffects();
      Effects right_effects = EnterEffects();
      RECURSE(Visit(expr->right()));
      ExitEffects();
      left_effects.Alt(right_effects);
      store_.Seq(left_effects);

      NarrowType(expr, Bounds::Either(
          expr->left()->bounds(), expr->right()->bounds(), zone()));
      break;
    }
    case Token::BIT_OR:
    case Token::BIT_AND: {
      RECURSE(Visit(expr->left()));
      RECURSE(Visit(expr->right()));
      Type* upper = Type::Union(
          expr->left()->bounds().upper, expr->right()->bounds().upper, zone());
      if (!upper->Is(Type::Signed32())) upper = Type::Signed32(zone());
      Type* lower = Type::Intersect(Type::SignedSmall(zone()), upper, zone());
      NarrowType(expr, Bounds(lower, upper));
      break;
    }
    case Token::BIT_XOR:
    case Token::SHL:
    case Token::SAR:
      RECURSE(Visit(expr->left()));
      RECURSE(Visit(expr->right()));
      NarrowType(expr,
          Bounds(Type::SignedSmall(zone()), Type::Signed32(zone())));
      break;
    case Token::SHR:
      RECURSE(Visit(expr->left()));
      RECURSE(Visit(expr->right()));
      // TODO(rossberg): The upper bound would be Unsigned32, but since there
      // is no 'positive Smi' type for the lower bound, we use the smallest
      // union of Smi and Unsigned32 as upper bound instead.
      NarrowType(expr, Bounds(Type::SignedSmall(zone()), Type::Number(zone())));
      break;
    case Token::ADD: {
      RECURSE(Visit(expr->left()));
      RECURSE(Visit(expr->right()));
      Bounds l = expr->left()->bounds();
      Bounds r = expr->right()->bounds();
      Type* lower =
          !l.lower->IsInhabited() || !r.lower->IsInhabited() ?
              Type::None(zone()) :
          l.lower->Is(Type::String()) || r.lower->Is(Type::String()) ?
              Type::String(zone()) :
          l.lower->Is(Type::Number()) && r.lower->Is(Type::Number()) ?
              Type::SignedSmall(zone()) : Type::None(zone());
      Type* upper =
          l.upper->Is(Type::String()) || r.upper->Is(Type::String()) ?
              Type::String(zone()) :
          l.upper->Is(Type::Number()) && r.upper->Is(Type::Number()) ?
              Type::Number(zone()) : Type::NumberOrString(zone());
      NarrowType(expr, Bounds(lower, upper));
      break;
    }
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD:
      RECURSE(Visit(expr->left()));
      RECURSE(Visit(expr->right()));
      NarrowType(expr, Bounds(Type::SignedSmall(zone()), Type::Number(zone())));
      break;
    default:
      UNREACHABLE();
  }
}


void AstTyper::VisitCompareOperation(CompareOperation* expr) {
  // Collect type feedback.
  Type* left_type;
  Type* right_type;
  Type* combined_type;
  oracle()->CompareType(expr->CompareOperationFeedbackId(),
      &left_type, &right_type, &combined_type);
  NarrowLowerType(expr->left(), left_type);
  NarrowLowerType(expr->right(), right_type);
  expr->set_combined_type(combined_type);

  RECURSE(Visit(expr->left()));
  RECURSE(Visit(expr->right()));

  NarrowType(expr, Bounds(Type::Boolean(zone())));
}


void AstTyper::VisitThisFunction(ThisFunction* expr) {
}


void AstTyper::VisitSuperReference(SuperReference* expr) {}


void AstTyper::VisitDeclarations(ZoneList<Declaration*>* decls) {
  for (int i = 0; i < decls->length(); ++i) {
    Declaration* decl = decls->at(i);
    RECURSE(Visit(decl));
  }
}


void AstTyper::VisitVariableDeclaration(VariableDeclaration* declaration) {
}


void AstTyper::VisitFunctionDeclaration(FunctionDeclaration* declaration) {
  RECURSE(Visit(declaration->fun()));
}


void AstTyper::VisitModuleDeclaration(ModuleDeclaration* declaration) {
  RECURSE(Visit(declaration->module()));
}


void AstTyper::VisitImportDeclaration(ImportDeclaration* declaration) {
  RECURSE(Visit(declaration->module()));
}


void AstTyper::VisitExportDeclaration(ExportDeclaration* declaration) {
}


void AstTyper::VisitModuleLiteral(ModuleLiteral* module) {
  RECURSE(Visit(module->body()));
}


void AstTyper::VisitModuleVariable(ModuleVariable* module) {
}


void AstTyper::VisitModulePath(ModulePath* module) {
  RECURSE(Visit(module->module()));
}


void AstTyper::VisitModuleUrl(ModuleUrl* module) {
}


void AstTyper::VisitModuleStatement(ModuleStatement* stmt) {
  RECURSE(Visit(stmt->body()));
}


} }  // namespace v8::internal
