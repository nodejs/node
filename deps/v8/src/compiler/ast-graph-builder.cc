// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/ast-graph-builder.h"

#include "src/compiler.h"
#include "src/compiler/control-builders.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/node-properties-inl.h"
#include "src/full-codegen.h"
#include "src/parser.h"
#include "src/scopes.h"

namespace v8 {
namespace internal {
namespace compiler {

AstGraphBuilder::AstGraphBuilder(CompilationInfo* info, JSGraph* jsgraph)
    : StructuredGraphBuilder(jsgraph->graph(), jsgraph->common()),
      info_(info),
      jsgraph_(jsgraph),
      globals_(0, info->zone()),
      breakable_(NULL),
      execution_context_(NULL) {
  InitializeAstVisitor(info->zone());
}


Node* AstGraphBuilder::GetFunctionClosure() {
  if (!function_closure_.is_set()) {
    // Parameter -1 is special for the function closure
    const Operator* op = common()->Parameter(-1);
    Node* node = NewNode(op, graph()->start());
    function_closure_.set(node);
  }
  return function_closure_.get();
}


Node* AstGraphBuilder::GetFunctionContext() {
  if (!function_context_.is_set()) {
    // Parameter (arity + 1) is special for the outer context of the function
    const Operator* op = common()->Parameter(info()->num_parameters() + 1);
    Node* node = NewNode(op, graph()->start());
    function_context_.set(node);
  }
  return function_context_.get();
}


bool AstGraphBuilder::CreateGraph() {
  Scope* scope = info()->scope();
  DCHECK(graph() != NULL);

  // Set up the basic structure of the graph.
  int parameter_count = info()->num_parameters();
  graph()->SetStart(graph()->NewNode(common()->Start(parameter_count)));

  // Initialize the top-level environment.
  Environment env(this, scope, graph()->start());
  set_environment(&env);

  // Build node to initialize local function context.
  Node* closure = GetFunctionClosure();
  Node* outer = GetFunctionContext();
  Node* inner = BuildLocalFunctionContext(outer, closure);

  // Push top-level function scope for the function body.
  ContextScope top_context(this, scope, inner);

  // Build the arguments object if it is used.
  BuildArgumentsObject(scope->arguments());

  // Emit tracing call if requested to do so.
  if (FLAG_trace) {
    NewNode(javascript()->Runtime(Runtime::kTraceEnter, 0));
  }

  // Visit implicit declaration of the function name.
  if (scope->is_function_scope() && scope->function() != NULL) {
    VisitVariableDeclaration(scope->function());
  }

  // Visit declarations within the function scope.
  VisitDeclarations(scope->declarations());

  // TODO(mstarzinger): This should do an inlined stack check.
  Node* node = NewNode(javascript()->Runtime(Runtime::kStackGuard, 0));
  PrepareFrameState(node, BailoutId::FunctionEntry());

  // Visit statements in the function body.
  VisitStatements(info()->function()->body());
  if (HasStackOverflow()) return false;

  // Emit tracing call if requested to do so.
  if (FLAG_trace) {
    // TODO(mstarzinger): Only traces implicit return.
    Node* return_value = jsgraph()->UndefinedConstant();
    NewNode(javascript()->Runtime(Runtime::kTraceExit, 1), return_value);
  }

  // Return 'undefined' in case we can fall off the end.
  Node* control = NewNode(common()->Return(), jsgraph()->UndefinedConstant());
  UpdateControlDependencyToLeaveFunction(control);

  // Finish the basic structure of the graph.
  environment()->UpdateControlDependency(exit_control());
  graph()->SetEnd(NewNode(common()->End()));

  return true;
}


// Left-hand side can only be a property, a global or a variable slot.
enum LhsKind { VARIABLE, NAMED_PROPERTY, KEYED_PROPERTY };


// Determine the left-hand side kind of an assignment.
static LhsKind DetermineLhsKind(Expression* expr) {
  Property* property = expr->AsProperty();
  DCHECK(expr->IsValidReferenceExpression());
  LhsKind lhs_kind =
      (property == NULL) ? VARIABLE : (property->key()->IsPropertyName())
                                          ? NAMED_PROPERTY
                                          : KEYED_PROPERTY;
  return lhs_kind;
}


// Helper to find an existing shared function info in the baseline code for the
// given function literal. Used to canonicalize SharedFunctionInfo objects.
static Handle<SharedFunctionInfo> SearchSharedFunctionInfo(
    Code* unoptimized_code, FunctionLiteral* expr) {
  int start_position = expr->start_position();
  for (RelocIterator it(unoptimized_code); !it.done(); it.next()) {
    RelocInfo* rinfo = it.rinfo();
    if (rinfo->rmode() != RelocInfo::EMBEDDED_OBJECT) continue;
    Object* obj = rinfo->target_object();
    if (obj->IsSharedFunctionInfo()) {
      SharedFunctionInfo* shared = SharedFunctionInfo::cast(obj);
      if (shared->start_position() == start_position) {
        return Handle<SharedFunctionInfo>(shared);
      }
    }
  }
  return Handle<SharedFunctionInfo>();
}


StructuredGraphBuilder::Environment* AstGraphBuilder::CopyEnvironment(
    StructuredGraphBuilder::Environment* env) {
  return new (zone()) Environment(*reinterpret_cast<Environment*>(env));
}


AstGraphBuilder::Environment::Environment(AstGraphBuilder* builder,
                                          Scope* scope,
                                          Node* control_dependency)
    : StructuredGraphBuilder::Environment(builder, control_dependency),
      parameters_count_(scope->num_parameters() + 1),
      locals_count_(scope->num_stack_slots()),
      parameters_node_(NULL),
      locals_node_(NULL),
      stack_node_(NULL) {
  DCHECK_EQ(scope->num_parameters() + 1, parameters_count());

  // Bind the receiver variable.
  Node* receiver = builder->graph()->NewNode(common()->Parameter(0),
                                             builder->graph()->start());
  values()->push_back(receiver);

  // Bind all parameter variables. The parameter indices are shifted by 1
  // (receiver is parameter index -1 but environment index 0).
  for (int i = 0; i < scope->num_parameters(); ++i) {
    Node* parameter = builder->graph()->NewNode(common()->Parameter(i + 1),
                                                builder->graph()->start());
    values()->push_back(parameter);
  }

  // Bind all local variables to undefined.
  Node* undefined_constant = builder->jsgraph()->UndefinedConstant();
  values()->insert(values()->end(), locals_count(), undefined_constant);
}


AstGraphBuilder::Environment::Environment(const Environment& copy)
    : StructuredGraphBuilder::Environment(
          static_cast<StructuredGraphBuilder::Environment>(copy)),
      parameters_count_(copy.parameters_count_),
      locals_count_(copy.locals_count_),
      parameters_node_(copy.parameters_node_),
      locals_node_(copy.locals_node_),
      stack_node_(copy.stack_node_) {}


void AstGraphBuilder::Environment::UpdateStateValues(Node** state_values,
                                                     int offset, int count) {
  bool should_update = false;
  Node** env_values = (count == 0) ? NULL : &values()->at(offset);
  if (*state_values == NULL || (*state_values)->InputCount() != count) {
    should_update = true;
  } else {
    DCHECK(static_cast<size_t>(offset + count) <= values()->size());
    for (int i = 0; i < count; i++) {
      if ((*state_values)->InputAt(i) != env_values[i]) {
        should_update = true;
        break;
      }
    }
  }
  if (should_update) {
    const Operator* op = common()->StateValues(count);
    (*state_values) = graph()->NewNode(op, count, env_values);
  }
}


Node* AstGraphBuilder::Environment::Checkpoint(
    BailoutId ast_id, OutputFrameStateCombine combine) {
  UpdateStateValues(&parameters_node_, 0, parameters_count());
  UpdateStateValues(&locals_node_, parameters_count(), locals_count());
  UpdateStateValues(&stack_node_, parameters_count() + locals_count(),
                    stack_height());

  const Operator* op = common()->FrameState(JS_FRAME, ast_id, combine);

  return graph()->NewNode(op, parameters_node_, locals_node_, stack_node_,
                          GetContext(),
                          builder()->jsgraph()->UndefinedConstant());
}


AstGraphBuilder::AstContext::AstContext(AstGraphBuilder* own,
                                        Expression::Context kind)
    : kind_(kind), owner_(own), outer_(own->ast_context()) {
  owner()->set_ast_context(this);  // Push.
#ifdef DEBUG
  original_height_ = environment()->stack_height();
#endif
}


AstGraphBuilder::AstContext::~AstContext() {
  owner()->set_ast_context(outer_);  // Pop.
}


AstGraphBuilder::AstEffectContext::~AstEffectContext() {
  DCHECK(environment()->stack_height() == original_height_);
}


AstGraphBuilder::AstValueContext::~AstValueContext() {
  DCHECK(environment()->stack_height() == original_height_ + 1);
}


AstGraphBuilder::AstTestContext::~AstTestContext() {
  DCHECK(environment()->stack_height() == original_height_ + 1);
}


void AstGraphBuilder::AstEffectContext::ProduceValue(Node* value) {
  // The value is ignored.
}


void AstGraphBuilder::AstValueContext::ProduceValue(Node* value) {
  environment()->Push(value);
}


void AstGraphBuilder::AstTestContext::ProduceValue(Node* value) {
  environment()->Push(owner()->BuildToBoolean(value));
}


Node* AstGraphBuilder::AstEffectContext::ConsumeValue() { return NULL; }


Node* AstGraphBuilder::AstValueContext::ConsumeValue() {
  return environment()->Pop();
}


Node* AstGraphBuilder::AstTestContext::ConsumeValue() {
  return environment()->Pop();
}


AstGraphBuilder::BreakableScope* AstGraphBuilder::BreakableScope::FindBreakable(
    BreakableStatement* target) {
  BreakableScope* current = this;
  while (current != NULL && current->target_ != target) {
    owner_->environment()->Drop(current->drop_extra_);
    current = current->next_;
  }
  DCHECK(current != NULL);  // Always found (unless stack is malformed).
  return current;
}


void AstGraphBuilder::BreakableScope::BreakTarget(BreakableStatement* stmt) {
  FindBreakable(stmt)->control_->Break();
}


void AstGraphBuilder::BreakableScope::ContinueTarget(BreakableStatement* stmt) {
  FindBreakable(stmt)->control_->Continue();
}


void AstGraphBuilder::VisitForValueOrNull(Expression* expr) {
  if (expr == NULL) {
    return environment()->Push(jsgraph()->NullConstant());
  }
  VisitForValue(expr);
}


void AstGraphBuilder::VisitForValues(ZoneList<Expression*>* exprs) {
  for (int i = 0; i < exprs->length(); ++i) {
    VisitForValue(exprs->at(i));
  }
}


void AstGraphBuilder::VisitForValue(Expression* expr) {
  AstValueContext for_value(this);
  if (!HasStackOverflow()) {
    expr->Accept(this);
  }
}


void AstGraphBuilder::VisitForEffect(Expression* expr) {
  AstEffectContext for_effect(this);
  if (!HasStackOverflow()) {
    expr->Accept(this);
  }
}


void AstGraphBuilder::VisitForTest(Expression* expr) {
  AstTestContext for_condition(this);
  if (!HasStackOverflow()) {
    expr->Accept(this);
  }
}


void AstGraphBuilder::VisitVariableDeclaration(VariableDeclaration* decl) {
  Variable* variable = decl->proxy()->var();
  VariableMode mode = decl->mode();
  bool hole_init = mode == CONST || mode == CONST_LEGACY || mode == LET;
  switch (variable->location()) {
    case Variable::UNALLOCATED: {
      Handle<Oddball> value = variable->binding_needs_init()
                                  ? isolate()->factory()->the_hole_value()
                                  : isolate()->factory()->undefined_value();
      globals()->Add(variable->name(), zone());
      globals()->Add(value, zone());
      break;
    }
    case Variable::PARAMETER:
    case Variable::LOCAL:
      if (hole_init) {
        Node* value = jsgraph()->TheHoleConstant();
        environment()->Bind(variable, value);
      }
      break;
    case Variable::CONTEXT:
      if (hole_init) {
        Node* value = jsgraph()->TheHoleConstant();
        const Operator* op = javascript()->StoreContext(0, variable->index());
        NewNode(op, current_context(), value);
      }
      break;
    case Variable::LOOKUP:
      UNIMPLEMENTED();
  }
}


void AstGraphBuilder::VisitFunctionDeclaration(FunctionDeclaration* decl) {
  Variable* variable = decl->proxy()->var();
  switch (variable->location()) {
    case Variable::UNALLOCATED: {
      Handle<SharedFunctionInfo> function =
          Compiler::BuildFunctionInfo(decl->fun(), info()->script(), info());
      // Check for stack-overflow exception.
      if (function.is_null()) return SetStackOverflow();
      globals()->Add(variable->name(), zone());
      globals()->Add(function, zone());
      break;
    }
    case Variable::PARAMETER:
    case Variable::LOCAL: {
      VisitForValue(decl->fun());
      Node* value = environment()->Pop();
      environment()->Bind(variable, value);
      break;
    }
    case Variable::CONTEXT: {
      VisitForValue(decl->fun());
      Node* value = environment()->Pop();
      const Operator* op = javascript()->StoreContext(0, variable->index());
      NewNode(op, current_context(), value);
      break;
    }
    case Variable::LOOKUP:
      UNIMPLEMENTED();
  }
}


void AstGraphBuilder::VisitModuleDeclaration(ModuleDeclaration* decl) {
  UNREACHABLE();
}


void AstGraphBuilder::VisitImportDeclaration(ImportDeclaration* decl) {
  UNREACHABLE();
}


void AstGraphBuilder::VisitExportDeclaration(ExportDeclaration* decl) {
  UNREACHABLE();
}


void AstGraphBuilder::VisitModuleLiteral(ModuleLiteral* modl) { UNREACHABLE(); }


void AstGraphBuilder::VisitModuleVariable(ModuleVariable* modl) {
  UNREACHABLE();
}


void AstGraphBuilder::VisitModulePath(ModulePath* modl) { UNREACHABLE(); }


void AstGraphBuilder::VisitModuleUrl(ModuleUrl* modl) { UNREACHABLE(); }


void AstGraphBuilder::VisitBlock(Block* stmt) {
  BlockBuilder block(this);
  BreakableScope scope(this, stmt, &block, 0);
  if (stmt->labels() != NULL) block.BeginBlock();
  if (stmt->scope() == NULL) {
    // Visit statements in the same scope, no declarations.
    VisitStatements(stmt->statements());
  } else {
    const Operator* op = javascript()->CreateBlockContext();
    Node* scope_info = jsgraph()->Constant(stmt->scope()->GetScopeInfo());
    Node* context = NewNode(op, scope_info, GetFunctionClosure());
    ContextScope scope(this, stmt->scope(), context);

    // Visit declarations and statements in a block scope.
    VisitDeclarations(stmt->scope()->declarations());
    VisitStatements(stmt->statements());
  }
  if (stmt->labels() != NULL) block.EndBlock();
}


void AstGraphBuilder::VisitModuleStatement(ModuleStatement* stmt) {
  UNREACHABLE();
}


void AstGraphBuilder::VisitExpressionStatement(ExpressionStatement* stmt) {
  VisitForEffect(stmt->expression());
}


void AstGraphBuilder::VisitEmptyStatement(EmptyStatement* stmt) {
  // Do nothing.
}


void AstGraphBuilder::VisitIfStatement(IfStatement* stmt) {
  IfBuilder compare_if(this);
  VisitForTest(stmt->condition());
  Node* condition = environment()->Pop();
  compare_if.If(condition);
  compare_if.Then();
  Visit(stmt->then_statement());
  compare_if.Else();
  Visit(stmt->else_statement());
  compare_if.End();
}


void AstGraphBuilder::VisitContinueStatement(ContinueStatement* stmt) {
  StructuredGraphBuilder::Environment* env = environment()->CopyAsUnreachable();
  breakable()->ContinueTarget(stmt->target());
  set_environment(env);
}


void AstGraphBuilder::VisitBreakStatement(BreakStatement* stmt) {
  StructuredGraphBuilder::Environment* env = environment()->CopyAsUnreachable();
  breakable()->BreakTarget(stmt->target());
  set_environment(env);
}


void AstGraphBuilder::VisitReturnStatement(ReturnStatement* stmt) {
  VisitForValue(stmt->expression());
  Node* result = environment()->Pop();
  Node* control = NewNode(common()->Return(), result);
  UpdateControlDependencyToLeaveFunction(control);
}


void AstGraphBuilder::VisitWithStatement(WithStatement* stmt) {
  VisitForValue(stmt->expression());
  Node* value = environment()->Pop();
  const Operator* op = javascript()->CreateWithContext();
  Node* context = NewNode(op, value, GetFunctionClosure());
  ContextScope scope(this, stmt->scope(), context);
  Visit(stmt->statement());
}


void AstGraphBuilder::VisitSwitchStatement(SwitchStatement* stmt) {
  ZoneList<CaseClause*>* clauses = stmt->cases();
  SwitchBuilder compare_switch(this, clauses->length());
  BreakableScope scope(this, stmt, &compare_switch, 0);
  compare_switch.BeginSwitch();
  int default_index = -1;

  // Keep the switch value on the stack until a case matches.
  VisitForValue(stmt->tag());
  Node* tag = environment()->Top();

  // Iterate over all cases and create nodes for label comparison.
  for (int i = 0; i < clauses->length(); i++) {
    CaseClause* clause = clauses->at(i);

    // The default is not a test, remember index.
    if (clause->is_default()) {
      default_index = i;
      continue;
    }

    // Create nodes to perform label comparison as if via '==='. The switch
    // value is still on the operand stack while the label is evaluated.
    VisitForValue(clause->label());
    Node* label = environment()->Pop();
    const Operator* op = javascript()->StrictEqual();
    Node* condition = NewNode(op, tag, label);
    compare_switch.BeginLabel(i, condition);

    // Discard the switch value at label match.
    environment()->Pop();
    compare_switch.EndLabel();
  }

  // Discard the switch value and mark the default case.
  environment()->Pop();
  if (default_index >= 0) {
    compare_switch.DefaultAt(default_index);
  }

  // Iterate over all cases and create nodes for case bodies.
  for (int i = 0; i < clauses->length(); i++) {
    CaseClause* clause = clauses->at(i);
    compare_switch.BeginCase(i);
    VisitStatements(clause->statements());
    compare_switch.EndCase();
  }

  compare_switch.EndSwitch();
}


void AstGraphBuilder::VisitDoWhileStatement(DoWhileStatement* stmt) {
  LoopBuilder while_loop(this);
  while_loop.BeginLoop();
  VisitIterationBody(stmt, &while_loop, 0);
  while_loop.EndBody();
  VisitForTest(stmt->cond());
  Node* condition = environment()->Pop();
  while_loop.BreakUnless(condition);
  while_loop.EndLoop();
}


void AstGraphBuilder::VisitWhileStatement(WhileStatement* stmt) {
  LoopBuilder while_loop(this);
  while_loop.BeginLoop();
  VisitForTest(stmt->cond());
  Node* condition = environment()->Pop();
  while_loop.BreakUnless(condition);
  VisitIterationBody(stmt, &while_loop, 0);
  while_loop.EndBody();
  while_loop.EndLoop();
}


void AstGraphBuilder::VisitForStatement(ForStatement* stmt) {
  LoopBuilder for_loop(this);
  VisitIfNotNull(stmt->init());
  for_loop.BeginLoop();
  if (stmt->cond() != NULL) {
    VisitForTest(stmt->cond());
    Node* condition = environment()->Pop();
    for_loop.BreakUnless(condition);
  }
  VisitIterationBody(stmt, &for_loop, 0);
  for_loop.EndBody();
  VisitIfNotNull(stmt->next());
  for_loop.EndLoop();
}


// TODO(dcarney): this is a big function.  Try to clean up some.
void AstGraphBuilder::VisitForInStatement(ForInStatement* stmt) {
  VisitForValue(stmt->subject());
  Node* obj = environment()->Pop();
  // Check for undefined or null before entering loop.
  IfBuilder is_undefined(this);
  Node* is_undefined_cond =
      NewNode(javascript()->StrictEqual(), obj, jsgraph()->UndefinedConstant());
  is_undefined.If(is_undefined_cond);
  is_undefined.Then();
  is_undefined.Else();
  {
    IfBuilder is_null(this);
    Node* is_null_cond =
        NewNode(javascript()->StrictEqual(), obj, jsgraph()->NullConstant());
    is_null.If(is_null_cond);
    is_null.Then();
    is_null.Else();
    // Convert object to jsobject.
    // PrepareForBailoutForId(stmt->PrepareId(), TOS_REG);
    obj = NewNode(javascript()->ToObject(), obj);
    environment()->Push(obj);
    // TODO(dcarney): should do a fast enum cache check here to skip runtime.
    environment()->Push(obj);
    Node* cache_type = ProcessArguments(
        javascript()->Runtime(Runtime::kGetPropertyNamesFast, 1), 1);
    // TODO(dcarney): these next runtime calls should be removed in favour of
    //                a few simplified instructions.
    environment()->Push(obj);
    environment()->Push(cache_type);
    Node* cache_pair =
        ProcessArguments(javascript()->Runtime(Runtime::kForInInit, 2), 2);
    // cache_type may have been replaced.
    Node* cache_array = NewNode(common()->Projection(0), cache_pair);
    cache_type = NewNode(common()->Projection(1), cache_pair);
    environment()->Push(cache_type);
    environment()->Push(cache_array);
    Node* cache_length = ProcessArguments(
        javascript()->Runtime(Runtime::kForInCacheArrayLength, 2), 2);
    {
      // TODO(dcarney): this check is actually supposed to be for the
      //                empty enum case only.
      IfBuilder have_no_properties(this);
      Node* empty_array_cond = NewNode(javascript()->StrictEqual(),
                                       cache_length, jsgraph()->ZeroConstant());
      have_no_properties.If(empty_array_cond);
      have_no_properties.Then();
      // Pop obj and skip loop.
      environment()->Pop();
      have_no_properties.Else();
      {
        // Construct the rest of the environment.
        environment()->Push(cache_type);
        environment()->Push(cache_array);
        environment()->Push(cache_length);
        environment()->Push(jsgraph()->ZeroConstant());
        // PrepareForBailoutForId(stmt->BodyId(), NO_REGISTERS);
        LoopBuilder for_loop(this);
        for_loop.BeginLoop();
        // Check loop termination condition.
        Node* index = environment()->Peek(0);
        Node* exit_cond =
            NewNode(javascript()->LessThan(), index, cache_length);
        // TODO(jarin): provide real bailout id.
        PrepareFrameState(exit_cond, BailoutId::None());
        for_loop.BreakUnless(exit_cond);
        // TODO(dcarney): this runtime call should be a handful of
        //                simplified instructions that
        //                basically produce
        //                    value = array[index]
        environment()->Push(obj);
        environment()->Push(cache_array);
        environment()->Push(cache_type);
        environment()->Push(index);
        Node* pair =
            ProcessArguments(javascript()->Runtime(Runtime::kForInNext, 4), 4);
        Node* value = NewNode(common()->Projection(0), pair);
        Node* should_filter = NewNode(common()->Projection(1), pair);
        environment()->Push(value);
        {
          // Test if FILTER_KEY needs to be called.
          IfBuilder test_should_filter(this);
          Node* should_filter_cond =
              NewNode(javascript()->StrictEqual(), should_filter,
                      jsgraph()->TrueConstant());
          test_should_filter.If(should_filter_cond);
          test_should_filter.Then();
          value = environment()->Pop();
          Node* builtins = BuildLoadBuiltinsObject();
          Node* function = BuildLoadObjectField(
              builtins,
              JSBuiltinsObject::OffsetOfFunctionWithId(Builtins::FILTER_KEY));
          // Callee.
          environment()->Push(function);
          // Receiver.
          environment()->Push(obj);
          // Args.
          environment()->Push(value);
          // result is either the string key or Smi(0) indicating the property
          // is gone.
          Node* res = ProcessArguments(
              javascript()->Call(3, NO_CALL_FUNCTION_FLAGS), 3);
          // TODO(jarin): provide real bailout id.
          PrepareFrameState(res, BailoutId::None());
          Node* property_missing = NewNode(javascript()->StrictEqual(), res,
                                           jsgraph()->ZeroConstant());
          {
            IfBuilder is_property_missing(this);
            is_property_missing.If(property_missing);
            is_property_missing.Then();
            // Inc counter and continue.
            Node* index_inc =
                NewNode(javascript()->Add(), index, jsgraph()->OneConstant());
            // TODO(jarin): provide real bailout id.
            PrepareFrameState(index_inc, BailoutId::None());
            environment()->Poke(0, index_inc);
            for_loop.Continue();
            is_property_missing.Else();
            is_property_missing.End();
          }
          // Replace 'value' in environment.
          environment()->Push(res);
          test_should_filter.Else();
          test_should_filter.End();
        }
        value = environment()->Pop();
        // Bind value and do loop body.
        VisitForInAssignment(stmt->each(), value);
        VisitIterationBody(stmt, &for_loop, 5);
        for_loop.EndBody();
        // Inc counter and continue.
        Node* index_inc =
            NewNode(javascript()->Add(), index, jsgraph()->OneConstant());
        // TODO(jarin): provide real bailout id.
        PrepareFrameState(index_inc, BailoutId::None());
        environment()->Poke(0, index_inc);
        for_loop.EndLoop();
        environment()->Drop(5);
        // PrepareForBailoutForId(stmt->ExitId(), NO_REGISTERS);
      }
      have_no_properties.End();
    }
    is_null.End();
  }
  is_undefined.End();
}


void AstGraphBuilder::VisitForOfStatement(ForOfStatement* stmt) {
  VisitForValue(stmt->subject());
  environment()->Pop();
  // TODO(turbofan): create and use loop builder.
}


void AstGraphBuilder::VisitTryCatchStatement(TryCatchStatement* stmt) {
  UNREACHABLE();
}


void AstGraphBuilder::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  UNREACHABLE();
}


void AstGraphBuilder::VisitDebuggerStatement(DebuggerStatement* stmt) {
  // TODO(turbofan): Do we really need a separate reloc-info for this?
  Node* node = NewNode(javascript()->Runtime(Runtime::kDebugBreak, 0));
  PrepareFrameState(node, stmt->DebugBreakId());
}


void AstGraphBuilder::VisitFunctionLiteral(FunctionLiteral* expr) {
  Node* context = current_context();

  // Build a new shared function info if we cannot find one in the baseline
  // code. We also have a stack overflow if the recursive compilation did.
  Handle<SharedFunctionInfo> shared_info =
      SearchSharedFunctionInfo(info()->shared_info()->code(), expr);
  if (shared_info.is_null()) {
    shared_info = Compiler::BuildFunctionInfo(expr, info()->script(), info());
    CHECK(!shared_info.is_null());  // TODO(mstarzinger): Set stack overflow?
  }

  // Create node to instantiate a new closure.
  Node* info = jsgraph()->Constant(shared_info);
  Node* pretenure = expr->pretenure() ? jsgraph()->TrueConstant()
                                      : jsgraph()->FalseConstant();
  const Operator* op = javascript()->Runtime(Runtime::kNewClosure, 3);
  Node* value = NewNode(op, context, info, pretenure);
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitClassLiteral(ClassLiteral* expr) {
  // TODO(arv): Implement.
  UNREACHABLE();
}


void AstGraphBuilder::VisitNativeFunctionLiteral(NativeFunctionLiteral* expr) {
  UNREACHABLE();
}


void AstGraphBuilder::VisitConditional(Conditional* expr) {
  IfBuilder compare_if(this);
  VisitForTest(expr->condition());
  Node* condition = environment()->Pop();
  compare_if.If(condition);
  compare_if.Then();
  Visit(expr->then_expression());
  compare_if.Else();
  Visit(expr->else_expression());
  compare_if.End();
  ast_context()->ReplaceValue();
}


void AstGraphBuilder::VisitVariableProxy(VariableProxy* expr) {
  Node* value = BuildVariableLoad(expr->var(), expr->id());
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitLiteral(Literal* expr) {
  Node* value = jsgraph()->Constant(expr->value());
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitRegExpLiteral(RegExpLiteral* expr) {
  Node* closure = GetFunctionClosure();

  // Create node to materialize a regular expression literal.
  Node* literals_array =
      BuildLoadObjectField(closure, JSFunction::kLiteralsOffset);
  Node* literal_index = jsgraph()->Constant(expr->literal_index());
  Node* pattern = jsgraph()->Constant(expr->pattern());
  Node* flags = jsgraph()->Constant(expr->flags());
  const Operator* op =
      javascript()->Runtime(Runtime::kMaterializeRegExpLiteral, 4);
  Node* literal = NewNode(op, literals_array, literal_index, pattern, flags);
  ast_context()->ProduceValue(literal);
}


void AstGraphBuilder::VisitObjectLiteral(ObjectLiteral* expr) {
  Node* closure = GetFunctionClosure();

  // Create node to deep-copy the literal boilerplate.
  expr->BuildConstantProperties(isolate());
  Node* literals_array =
      BuildLoadObjectField(closure, JSFunction::kLiteralsOffset);
  Node* literal_index = jsgraph()->Constant(expr->literal_index());
  Node* constants = jsgraph()->Constant(expr->constant_properties());
  Node* flags = jsgraph()->Constant(expr->ComputeFlags());
  const Operator* op = javascript()->Runtime(Runtime::kCreateObjectLiteral, 4);
  Node* literal = NewNode(op, literals_array, literal_index, constants, flags);

  // The object is expected on the operand stack during computation of the
  // property values and is the value of the entire expression.
  environment()->Push(literal);

  // Mark all computed expressions that are bound to a key that is shadowed by
  // a later occurrence of the same key. For the marked expressions, no store
  // code is emitted.
  expr->CalculateEmitStore(zone());

  // Create nodes to store computed values into the literal.
  AccessorTable accessor_table(zone());
  for (int i = 0; i < expr->properties()->length(); i++) {
    ObjectLiteral::Property* property = expr->properties()->at(i);
    if (property->IsCompileTimeValue()) continue;

    Literal* key = property->key();
    switch (property->kind()) {
      case ObjectLiteral::Property::CONSTANT:
        UNREACHABLE();
      case ObjectLiteral::Property::MATERIALIZED_LITERAL:
        DCHECK(!CompileTimeValue::IsCompileTimeValue(property->value()));
      // Fall through.
      case ObjectLiteral::Property::COMPUTED: {
        // It is safe to use [[Put]] here because the boilerplate already
        // contains computed properties with an uninitialized value.
        if (key->value()->IsInternalizedString()) {
          if (property->emit_store()) {
            VisitForValue(property->value());
            Node* value = environment()->Pop();
            Unique<Name> name = MakeUnique(key->AsPropertyName());
            Node* store = NewNode(javascript()->StoreNamed(strict_mode(), name),
                                  literal, value);
            PrepareFrameState(store, key->id());
          } else {
            VisitForEffect(property->value());
          }
          break;
        }
        environment()->Push(literal);  // Duplicate receiver.
        VisitForValue(property->key());
        VisitForValue(property->value());
        Node* value = environment()->Pop();
        Node* key = environment()->Pop();
        Node* receiver = environment()->Pop();
        if (property->emit_store()) {
          Node* strict = jsgraph()->Constant(SLOPPY);
          const Operator* op = javascript()->Runtime(Runtime::kSetProperty, 4);
          NewNode(op, receiver, key, value, strict);
        }
        break;
      }
      case ObjectLiteral::Property::PROTOTYPE: {
        environment()->Push(literal);  // Duplicate receiver.
        VisitForValue(property->value());
        Node* value = environment()->Pop();
        Node* receiver = environment()->Pop();
        if (property->emit_store()) {
          const Operator* op = javascript()->Runtime(Runtime::kSetPrototype, 2);
          NewNode(op, receiver, value);
        }
        break;
      }
      case ObjectLiteral::Property::GETTER:
        accessor_table.lookup(key)->second->getter = property->value();
        break;
      case ObjectLiteral::Property::SETTER:
        accessor_table.lookup(key)->second->setter = property->value();
        break;
    }
  }

  // Create nodes to define accessors, using only a single call to the runtime
  // for each pair of corresponding getters and setters.
  for (AccessorTable::Iterator it = accessor_table.begin();
       it != accessor_table.end(); ++it) {
    VisitForValue(it->first);
    VisitForValueOrNull(it->second->getter);
    VisitForValueOrNull(it->second->setter);
    Node* setter = environment()->Pop();
    Node* getter = environment()->Pop();
    Node* name = environment()->Pop();
    Node* attr = jsgraph()->Constant(NONE);
    const Operator* op =
        javascript()->Runtime(Runtime::kDefineAccessorPropertyUnchecked, 5);
    Node* call = NewNode(op, literal, name, getter, setter, attr);
    PrepareFrameState(call, it->first->id());
  }

  // Transform literals that contain functions to fast properties.
  if (expr->has_function()) {
    const Operator* op = javascript()->Runtime(Runtime::kToFastProperties, 1);
    NewNode(op, literal);
  }

  ast_context()->ProduceValue(environment()->Pop());
}


void AstGraphBuilder::VisitArrayLiteral(ArrayLiteral* expr) {
  Node* closure = GetFunctionClosure();

  // Create node to deep-copy the literal boilerplate.
  expr->BuildConstantElements(isolate());
  Node* literals_array =
      BuildLoadObjectField(closure, JSFunction::kLiteralsOffset);
  Node* literal_index = jsgraph()->Constant(expr->literal_index());
  Node* constants = jsgraph()->Constant(expr->constant_elements());
  Node* flags = jsgraph()->Constant(expr->ComputeFlags());
  const Operator* op = javascript()->Runtime(Runtime::kCreateArrayLiteral, 4);
  Node* literal = NewNode(op, literals_array, literal_index, constants, flags);

  // The array and the literal index are both expected on the operand stack
  // during computation of the element values.
  environment()->Push(literal);
  environment()->Push(literal_index);

  // Create nodes to evaluate all the non-constant subexpressions and to store
  // them into the newly cloned array.
  for (int i = 0; i < expr->values()->length(); i++) {
    Expression* subexpr = expr->values()->at(i);
    if (CompileTimeValue::IsCompileTimeValue(subexpr)) continue;

    VisitForValue(subexpr);
    Node* value = environment()->Pop();
    Node* index = jsgraph()->Constant(i);
    Node* store = NewNode(javascript()->StoreProperty(strict_mode()), literal,
                          index, value);
    PrepareFrameState(store, expr->GetIdForElement(i));
  }

  environment()->Pop();  // Array literal index.
  ast_context()->ProduceValue(environment()->Pop());
}


void AstGraphBuilder::VisitForInAssignment(Expression* expr, Node* value) {
  DCHECK(expr->IsValidReferenceExpression());

  // Left-hand side can only be a property, a global or a variable slot.
  Property* property = expr->AsProperty();
  LhsKind assign_type = DetermineLhsKind(expr);

  // Evaluate LHS expression and store the value.
  switch (assign_type) {
    case VARIABLE: {
      Variable* var = expr->AsVariableProxy()->var();
      // TODO(jarin) Fill in the correct bailout id.
      BuildVariableAssignment(var, value, Token::ASSIGN, BailoutId::None());
      break;
    }
    case NAMED_PROPERTY: {
      environment()->Push(value);
      VisitForValue(property->obj());
      Node* object = environment()->Pop();
      value = environment()->Pop();
      Unique<Name> name =
          MakeUnique(property->key()->AsLiteral()->AsPropertyName());
      Node* store =
          NewNode(javascript()->StoreNamed(strict_mode(), name), object, value);
      // TODO(jarin) Fill in the correct bailout id.
      PrepareFrameState(store, BailoutId::None());
      break;
    }
    case KEYED_PROPERTY: {
      environment()->Push(value);
      VisitForValue(property->obj());
      VisitForValue(property->key());
      Node* key = environment()->Pop();
      Node* object = environment()->Pop();
      value = environment()->Pop();
      Node* store = NewNode(javascript()->StoreProperty(strict_mode()), object,
                            key, value);
      // TODO(jarin) Fill in the correct bailout id.
      PrepareFrameState(store, BailoutId::None());
      break;
    }
  }
}


void AstGraphBuilder::VisitAssignment(Assignment* expr) {
  DCHECK(expr->target()->IsValidReferenceExpression());

  // Left-hand side can only be a property, a global or a variable slot.
  Property* property = expr->target()->AsProperty();
  LhsKind assign_type = DetermineLhsKind(expr->target());

  // Evaluate LHS expression.
  switch (assign_type) {
    case VARIABLE:
      // Nothing to do here.
      break;
    case NAMED_PROPERTY:
      VisitForValue(property->obj());
      break;
    case KEYED_PROPERTY: {
      VisitForValue(property->obj());
      VisitForValue(property->key());
      break;
    }
  }

  // Evaluate the value and potentially handle compound assignments by loading
  // the left-hand side value and performing a binary operation.
  if (expr->is_compound()) {
    Node* old_value = NULL;
    switch (assign_type) {
      case VARIABLE: {
        Variable* variable = expr->target()->AsVariableProxy()->var();
        old_value = BuildVariableLoad(variable, expr->target()->id());
        break;
      }
      case NAMED_PROPERTY: {
        Node* object = environment()->Top();
        Unique<Name> name =
            MakeUnique(property->key()->AsLiteral()->AsPropertyName());
        old_value = NewNode(javascript()->LoadNamed(name), object);
        PrepareFrameState(old_value, property->LoadId(), kPushOutput);
        break;
      }
      case KEYED_PROPERTY: {
        Node* key = environment()->Top();
        Node* object = environment()->Peek(1);
        old_value = NewNode(javascript()->LoadProperty(), object, key);
        PrepareFrameState(old_value, property->LoadId(), kPushOutput);
        break;
      }
    }
    environment()->Push(old_value);
    VisitForValue(expr->value());
    Node* right = environment()->Pop();
    Node* left = environment()->Pop();
    Node* value = BuildBinaryOp(left, right, expr->binary_op());
    PrepareFrameState(value, expr->binary_operation()->id(), kPushOutput);
    environment()->Push(value);
  } else {
    VisitForValue(expr->value());
  }

  // Store the value.
  Node* value = environment()->Pop();
  switch (assign_type) {
    case VARIABLE: {
      Variable* variable = expr->target()->AsVariableProxy()->var();
      BuildVariableAssignment(variable, value, expr->op(),
                              expr->AssignmentId());
      break;
    }
    case NAMED_PROPERTY: {
      Node* object = environment()->Pop();
      Unique<Name> name =
          MakeUnique(property->key()->AsLiteral()->AsPropertyName());
      Node* store =
          NewNode(javascript()->StoreNamed(strict_mode(), name), object, value);
      PrepareFrameState(store, expr->AssignmentId());
      break;
    }
    case KEYED_PROPERTY: {
      Node* key = environment()->Pop();
      Node* object = environment()->Pop();
      Node* store = NewNode(javascript()->StoreProperty(strict_mode()), object,
                            key, value);
      PrepareFrameState(store, expr->AssignmentId());
      break;
    }
  }

  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitYield(Yield* expr) {
  VisitForValue(expr->generator_object());
  VisitForValue(expr->expression());
  environment()->Pop();
  environment()->Pop();
  // TODO(turbofan): VisitYield
  ast_context()->ProduceValue(jsgraph()->UndefinedConstant());
}


void AstGraphBuilder::VisitThrow(Throw* expr) {
  VisitForValue(expr->exception());
  Node* exception = environment()->Pop();
  const Operator* op = javascript()->Runtime(Runtime::kThrow, 1);
  Node* value = NewNode(op, exception);
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitProperty(Property* expr) {
  Node* value;
  if (expr->key()->IsPropertyName()) {
    VisitForValue(expr->obj());
    Node* object = environment()->Pop();
    Unique<Name> name = MakeUnique(expr->key()->AsLiteral()->AsPropertyName());
    value = NewNode(javascript()->LoadNamed(name), object);
  } else {
    VisitForValue(expr->obj());
    VisitForValue(expr->key());
    Node* key = environment()->Pop();
    Node* object = environment()->Pop();
    value = NewNode(javascript()->LoadProperty(), object, key);
  }
  PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitCall(Call* expr) {
  Expression* callee = expr->expression();
  Call::CallType call_type = expr->GetCallType(isolate());

  // Prepare the callee and the receiver to the function call. This depends on
  // the semantics of the underlying call type.
  CallFunctionFlags flags = NO_CALL_FUNCTION_FLAGS;
  Node* receiver_value = NULL;
  Node* callee_value = NULL;
  bool possibly_eval = false;
  switch (call_type) {
    case Call::GLOBAL_CALL: {
      Variable* variable = callee->AsVariableProxy()->var();
      callee_value = BuildVariableLoad(variable, expr->expression()->id());
      receiver_value = jsgraph()->UndefinedConstant();
      break;
    }
    case Call::LOOKUP_SLOT_CALL: {
      Variable* variable = callee->AsVariableProxy()->var();
      DCHECK(variable->location() == Variable::LOOKUP);
      Node* name = jsgraph()->Constant(variable->name());
      const Operator* op = javascript()->Runtime(Runtime::kLoadLookupSlot, 2);
      Node* pair = NewNode(op, current_context(), name);
      callee_value = NewNode(common()->Projection(0), pair);
      receiver_value = NewNode(common()->Projection(1), pair);
      break;
    }
    case Call::PROPERTY_CALL: {
      Property* property = callee->AsProperty();
      VisitForValue(property->obj());
      Node* object = environment()->Top();
      if (property->key()->IsPropertyName()) {
        Unique<Name> name =
            MakeUnique(property->key()->AsLiteral()->AsPropertyName());
        callee_value = NewNode(javascript()->LoadNamed(name), object);
      } else {
        VisitForValue(property->key());
        Node* key = environment()->Pop();
        callee_value = NewNode(javascript()->LoadProperty(), object, key);
      }
      PrepareFrameState(callee_value, property->LoadId(), kPushOutput);
      receiver_value = environment()->Pop();
      // Note that a PROPERTY_CALL requires the receiver to be wrapped into an
      // object for sloppy callees. This could also be modeled explicitly here,
      // thereby obsoleting the need for a flag to the call operator.
      flags = CALL_AS_METHOD;
      break;
    }
    case Call::POSSIBLY_EVAL_CALL:
      possibly_eval = true;
    // Fall through.
    case Call::OTHER_CALL:
      VisitForValue(callee);
      callee_value = environment()->Pop();
      receiver_value = jsgraph()->UndefinedConstant();
      break;
  }

  // The callee and the receiver both have to be pushed onto the operand stack
  // before arguments are being evaluated.
  environment()->Push(callee_value);
  environment()->Push(receiver_value);

  // Evaluate all arguments to the function call,
  ZoneList<Expression*>* args = expr->arguments();
  VisitForValues(args);

  // Resolve callee and receiver for a potential direct eval call. This block
  // will mutate the callee and receiver values pushed onto the environment.
  if (possibly_eval && args->length() > 0) {
    int arg_count = args->length();

    // Extract callee and source string from the environment.
    Node* callee = environment()->Peek(arg_count + 1);
    Node* source = environment()->Peek(arg_count - 1);

    // Create node to ask for help resolving potential eval call. This will
    // provide a fully resolved callee and the corresponding receiver.
    Node* receiver = environment()->Lookup(info()->scope()->receiver());
    Node* strict = jsgraph()->Constant(strict_mode());
    Node* position = jsgraph()->Constant(info()->scope()->start_position());
    const Operator* op =
        javascript()->Runtime(Runtime::kResolvePossiblyDirectEval, 5);
    Node* pair = NewNode(op, callee, source, receiver, strict, position);
    Node* new_callee = NewNode(common()->Projection(0), pair);
    Node* new_receiver = NewNode(common()->Projection(1), pair);

    // Patch callee and receiver on the environment.
    environment()->Poke(arg_count + 1, new_callee);
    environment()->Poke(arg_count + 0, new_receiver);
  }

  // Create node to perform the function call.
  const Operator* call = javascript()->Call(args->length() + 2, flags);
  Node* value = ProcessArguments(call, args->length() + 2);
  PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitCallNew(CallNew* expr) {
  VisitForValue(expr->expression());

  // Evaluate all arguments to the construct call.
  ZoneList<Expression*>* args = expr->arguments();
  VisitForValues(args);

  // Create node to perform the construct call.
  const Operator* call = javascript()->CallNew(args->length() + 1);
  Node* value = ProcessArguments(call, args->length() + 1);
  PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitCallJSRuntime(CallRuntime* expr) {
  Handle<String> name = expr->name();

  // The callee and the receiver both have to be pushed onto the operand stack
  // before arguments are being evaluated.
  CallFunctionFlags flags = NO_CALL_FUNCTION_FLAGS;
  Node* receiver_value = BuildLoadBuiltinsObject();
  Unique<String> unique = MakeUnique(name);
  Node* callee_value = NewNode(javascript()->LoadNamed(unique), receiver_value);
  // TODO(jarin): Find/create a bailout id to deoptimize to (crankshaft
  // refuses to optimize functions with jsruntime calls).
  PrepareFrameState(callee_value, BailoutId::None(), kPushOutput);
  environment()->Push(callee_value);
  environment()->Push(receiver_value);

  // Evaluate all arguments to the JS runtime call.
  ZoneList<Expression*>* args = expr->arguments();
  VisitForValues(args);

  // Create node to perform the JS runtime call.
  const Operator* call = javascript()->Call(args->length() + 2, flags);
  Node* value = ProcessArguments(call, args->length() + 2);
  PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitCallRuntime(CallRuntime* expr) {
  const Runtime::Function* function = expr->function();

  // Handle calls to runtime functions implemented in JavaScript separately as
  // the call follows JavaScript ABI and the callee is statically unknown.
  if (expr->is_jsruntime()) {
    DCHECK(function == NULL && expr->name()->length() > 0);
    return VisitCallJSRuntime(expr);
  }

  // Evaluate all arguments to the runtime call.
  ZoneList<Expression*>* args = expr->arguments();
  VisitForValues(args);

  // Create node to perform the runtime call.
  Runtime::FunctionId functionId = function->function_id;
  const Operator* call = javascript()->Runtime(functionId, args->length());
  Node* value = ProcessArguments(call, args->length());
  PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitUnaryOperation(UnaryOperation* expr) {
  switch (expr->op()) {
    case Token::DELETE:
      return VisitDelete(expr);
    case Token::VOID:
      return VisitVoid(expr);
    case Token::TYPEOF:
      return VisitTypeof(expr);
    case Token::NOT:
      return VisitNot(expr);
    default:
      UNREACHABLE();
  }
}


void AstGraphBuilder::VisitCountOperation(CountOperation* expr) {
  DCHECK(expr->expression()->IsValidReferenceExpression());

  // Left-hand side can only be a property, a global or a variable slot.
  Property* property = expr->expression()->AsProperty();
  LhsKind assign_type = DetermineLhsKind(expr->expression());

  // Reserve space for result of postfix operation.
  bool is_postfix = expr->is_postfix() && !ast_context()->IsEffect();
  if (is_postfix) environment()->Push(jsgraph()->UndefinedConstant());

  // Evaluate LHS expression and get old value.
  Node* old_value = NULL;
  int stack_depth = -1;
  switch (assign_type) {
    case VARIABLE: {
      Variable* variable = expr->expression()->AsVariableProxy()->var();
      old_value = BuildVariableLoad(variable, expr->expression()->id());
      stack_depth = 0;
      break;
    }
    case NAMED_PROPERTY: {
      VisitForValue(property->obj());
      Node* object = environment()->Top();
      Unique<Name> name =
          MakeUnique(property->key()->AsLiteral()->AsPropertyName());
      old_value = NewNode(javascript()->LoadNamed(name), object);
      PrepareFrameState(old_value, property->LoadId(), kPushOutput);
      stack_depth = 1;
      break;
    }
    case KEYED_PROPERTY: {
      VisitForValue(property->obj());
      VisitForValue(property->key());
      Node* key = environment()->Top();
      Node* object = environment()->Peek(1);
      old_value = NewNode(javascript()->LoadProperty(), object, key);
      PrepareFrameState(old_value, property->LoadId(), kPushOutput);
      stack_depth = 2;
      break;
    }
  }

  // Convert old value into a number.
  old_value = NewNode(javascript()->ToNumber(), old_value);

  // Save result for postfix expressions at correct stack depth.
  if (is_postfix) environment()->Poke(stack_depth, old_value);

  // Create node to perform +1/-1 operation.
  Node* value =
      BuildBinaryOp(old_value, jsgraph()->OneConstant(), expr->binary_op());
  // TODO(jarin) Insert proper bailout id here (will need to change
  // full code generator).
  PrepareFrameState(value, BailoutId::None());

  // Store the value.
  switch (assign_type) {
    case VARIABLE: {
      Variable* variable = expr->expression()->AsVariableProxy()->var();
      environment()->Push(value);
      BuildVariableAssignment(variable, value, expr->op(),
                              expr->AssignmentId());
      environment()->Pop();
      break;
    }
    case NAMED_PROPERTY: {
      Node* object = environment()->Pop();
      Unique<Name> name =
          MakeUnique(property->key()->AsLiteral()->AsPropertyName());
      Node* store =
          NewNode(javascript()->StoreNamed(strict_mode(), name), object, value);
      environment()->Push(value);
      PrepareFrameState(store, expr->AssignmentId());
      environment()->Pop();
      break;
    }
    case KEYED_PROPERTY: {
      Node* key = environment()->Pop();
      Node* object = environment()->Pop();
      Node* store = NewNode(javascript()->StoreProperty(strict_mode()), object,
                            key, value);
      environment()->Push(value);
      PrepareFrameState(store, expr->AssignmentId());
      environment()->Pop();
      break;
    }
  }

  // Restore old value for postfix expressions.
  if (is_postfix) value = environment()->Pop();

  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitBinaryOperation(BinaryOperation* expr) {
  switch (expr->op()) {
    case Token::COMMA:
      return VisitComma(expr);
    case Token::OR:
    case Token::AND:
      return VisitLogicalExpression(expr);
    default: {
      VisitForValue(expr->left());
      VisitForValue(expr->right());
      Node* right = environment()->Pop();
      Node* left = environment()->Pop();
      Node* value = BuildBinaryOp(left, right, expr->op());
      PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
      ast_context()->ProduceValue(value);
    }
  }
}


void AstGraphBuilder::VisitCompareOperation(CompareOperation* expr) {
  const Operator* op;
  switch (expr->op()) {
    case Token::EQ:
      op = javascript()->Equal();
      break;
    case Token::NE:
      op = javascript()->NotEqual();
      break;
    case Token::EQ_STRICT:
      op = javascript()->StrictEqual();
      break;
    case Token::NE_STRICT:
      op = javascript()->StrictNotEqual();
      break;
    case Token::LT:
      op = javascript()->LessThan();
      break;
    case Token::GT:
      op = javascript()->GreaterThan();
      break;
    case Token::LTE:
      op = javascript()->LessThanOrEqual();
      break;
    case Token::GTE:
      op = javascript()->GreaterThanOrEqual();
      break;
    case Token::INSTANCEOF:
      op = javascript()->InstanceOf();
      break;
    case Token::IN:
      op = javascript()->HasProperty();
      break;
    default:
      op = NULL;
      UNREACHABLE();
  }
  VisitForValue(expr->left());
  VisitForValue(expr->right());
  Node* right = environment()->Pop();
  Node* left = environment()->Pop();
  Node* value = NewNode(op, left, right);
  PrepareFrameState(value, expr->id(), ast_context()->GetStateCombine());
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitThisFunction(ThisFunction* expr) {
  Node* value = GetFunctionClosure();
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitSuperReference(SuperReference* expr) {
  UNREACHABLE();
}


void AstGraphBuilder::VisitCaseClause(CaseClause* expr) { UNREACHABLE(); }


void AstGraphBuilder::VisitDeclarations(ZoneList<Declaration*>* declarations) {
  DCHECK(globals()->is_empty());
  AstVisitor::VisitDeclarations(declarations);
  if (globals()->is_empty()) return;
  Handle<FixedArray> data =
      isolate()->factory()->NewFixedArray(globals()->length(), TENURED);
  for (int i = 0; i < globals()->length(); ++i) data->set(i, *globals()->at(i));
  int encoded_flags = DeclareGlobalsEvalFlag::encode(info()->is_eval()) |
                      DeclareGlobalsNativeFlag::encode(info()->is_native()) |
                      DeclareGlobalsStrictMode::encode(strict_mode());
  Node* flags = jsgraph()->Constant(encoded_flags);
  Node* pairs = jsgraph()->Constant(data);
  const Operator* op = javascript()->Runtime(Runtime::kDeclareGlobals, 3);
  NewNode(op, current_context(), pairs, flags);
  globals()->Rewind(0);
}


void AstGraphBuilder::VisitIfNotNull(Statement* stmt) {
  if (stmt == NULL) return;
  Visit(stmt);
}


void AstGraphBuilder::VisitIterationBody(IterationStatement* stmt,
                                         LoopBuilder* loop, int drop_extra) {
  BreakableScope scope(this, stmt, loop, drop_extra);
  Visit(stmt->body());
}


void AstGraphBuilder::VisitDelete(UnaryOperation* expr) {
  Node* value;
  if (expr->expression()->IsVariableProxy()) {
    // Delete of an unqualified identifier is only allowed in classic mode but
    // deleting "this" is allowed in all language modes.
    Variable* variable = expr->expression()->AsVariableProxy()->var();
    DCHECK(strict_mode() == SLOPPY || variable->is_this());
    value = BuildVariableDelete(variable);
  } else if (expr->expression()->IsProperty()) {
    Property* property = expr->expression()->AsProperty();
    VisitForValue(property->obj());
    VisitForValue(property->key());
    Node* key = environment()->Pop();
    Node* object = environment()->Pop();
    value = NewNode(javascript()->DeleteProperty(strict_mode()), object, key);
  } else {
    VisitForEffect(expr->expression());
    value = jsgraph()->TrueConstant();
  }
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitVoid(UnaryOperation* expr) {
  VisitForEffect(expr->expression());
  Node* value = jsgraph()->UndefinedConstant();
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitTypeof(UnaryOperation* expr) {
  Node* operand;
  if (expr->expression()->IsVariableProxy()) {
    // Typeof does not throw a reference error on global variables, hence we
    // perform a non-contextual load in case the operand is a variable proxy.
    Variable* variable = expr->expression()->AsVariableProxy()->var();
    operand =
        BuildVariableLoad(variable, expr->expression()->id(), NOT_CONTEXTUAL);
  } else {
    VisitForValue(expr->expression());
    operand = environment()->Pop();
  }
  Node* value = NewNode(javascript()->TypeOf(), operand);
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitNot(UnaryOperation* expr) {
  VisitForValue(expr->expression());
  Node* operand = environment()->Pop();
  // TODO(mstarzinger): Possible optimization when we are in effect context.
  Node* value = NewNode(javascript()->UnaryNot(), operand);
  ast_context()->ProduceValue(value);
}


void AstGraphBuilder::VisitComma(BinaryOperation* expr) {
  VisitForEffect(expr->left());
  Visit(expr->right());
  ast_context()->ReplaceValue();
}


void AstGraphBuilder::VisitLogicalExpression(BinaryOperation* expr) {
  bool is_logical_and = expr->op() == Token::AND;
  IfBuilder compare_if(this);
  VisitForValue(expr->left());
  Node* condition = environment()->Top();
  compare_if.If(BuildToBoolean(condition));
  compare_if.Then();
  if (is_logical_and) {
    environment()->Pop();
    Visit(expr->right());
  } else if (ast_context()->IsEffect()) {
    environment()->Pop();
  }
  compare_if.Else();
  if (!is_logical_and) {
    environment()->Pop();
    Visit(expr->right());
  } else if (ast_context()->IsEffect()) {
    environment()->Pop();
  }
  compare_if.End();
  ast_context()->ReplaceValue();
}


Node* AstGraphBuilder::ProcessArguments(const Operator* op, int arity) {
  DCHECK(environment()->stack_height() >= arity);
  Node** all = info()->zone()->NewArray<Node*>(arity);
  for (int i = arity - 1; i >= 0; --i) {
    all[i] = environment()->Pop();
  }
  Node* value = NewNode(op, arity, all);
  return value;
}


Node* AstGraphBuilder::BuildLocalFunctionContext(Node* context, Node* closure) {
  int heap_slots = info()->num_heap_slots() - Context::MIN_CONTEXT_SLOTS;
  if (heap_slots <= 0) return context;
  set_current_context(context);

  // Allocate a new local context.
  const Operator* op = javascript()->CreateFunctionContext();
  Node* local_context = NewNode(op, closure);
  set_current_context(local_context);

  // Copy parameters into context if necessary.
  int num_parameters = info()->scope()->num_parameters();
  for (int i = 0; i < num_parameters; i++) {
    Variable* variable = info()->scope()->parameter(i);
    if (!variable->IsContextSlot()) continue;
    // Temporary parameter node. The parameter indices are shifted by 1
    // (receiver is parameter index -1 but environment index 0).
    Node* parameter = NewNode(common()->Parameter(i + 1), graph()->start());
    // Context variable (at bottom of the context chain).
    DCHECK_EQ(0, info()->scope()->ContextChainLength(variable->scope()));
    const Operator* op = javascript()->StoreContext(0, variable->index());
    NewNode(op, local_context, parameter);
  }

  return local_context;
}


Node* AstGraphBuilder::BuildArgumentsObject(Variable* arguments) {
  if (arguments == NULL) return NULL;

  // Allocate and initialize a new arguments object.
  Node* callee = GetFunctionClosure();
  const Operator* op = javascript()->Runtime(Runtime::kNewArguments, 1);
  Node* object = NewNode(op, callee);

  // Assign the object to the arguments variable.
  DCHECK(arguments->IsContextSlot() || arguments->IsStackAllocated());
  // This should never lazy deopt, so it is fine to send invalid bailout id.
  BuildVariableAssignment(arguments, object, Token::ASSIGN, BailoutId::None());

  return object;
}


Node* AstGraphBuilder::BuildHoleCheckSilent(Node* value, Node* for_hole,
                                            Node* not_hole) {
  IfBuilder hole_check(this);
  Node* the_hole = jsgraph()->TheHoleConstant();
  Node* check = NewNode(javascript()->StrictEqual(), value, the_hole);
  hole_check.If(check);
  hole_check.Then();
  environment()->Push(for_hole);
  hole_check.Else();
  environment()->Push(not_hole);
  hole_check.End();
  return environment()->Pop();
}


Node* AstGraphBuilder::BuildHoleCheckThrow(Node* value, Variable* variable,
                                           Node* not_hole) {
  IfBuilder hole_check(this);
  Node* the_hole = jsgraph()->TheHoleConstant();
  Node* check = NewNode(javascript()->StrictEqual(), value, the_hole);
  hole_check.If(check);
  hole_check.Then();
  environment()->Push(BuildThrowReferenceError(variable));
  hole_check.Else();
  environment()->Push(not_hole);
  hole_check.End();
  return environment()->Pop();
}


Node* AstGraphBuilder::BuildVariableLoad(Variable* variable,
                                         BailoutId bailout_id,
                                         ContextualMode contextual_mode) {
  Node* the_hole = jsgraph()->TheHoleConstant();
  VariableMode mode = variable->mode();
  switch (variable->location()) {
    case Variable::UNALLOCATED: {
      // Global var, const, or let variable.
      Node* global = BuildLoadGlobalObject();
      Unique<Name> name = MakeUnique(variable->name());
      const Operator* op = javascript()->LoadNamed(name, contextual_mode);
      Node* node = NewNode(op, global);
      PrepareFrameState(node, bailout_id, kPushOutput);
      return node;
    }
    case Variable::PARAMETER:
    case Variable::LOCAL: {
      // Local var, const, or let variable.
      Node* value = environment()->Lookup(variable);
      if (mode == CONST_LEGACY) {
        // Perform check for uninitialized legacy const variables.
        if (value->op() == the_hole->op()) {
          value = jsgraph()->UndefinedConstant();
        } else if (value->opcode() == IrOpcode::kPhi) {
          Node* undefined = jsgraph()->UndefinedConstant();
          value = BuildHoleCheckSilent(value, undefined, value);
        }
      } else if (mode == LET || mode == CONST) {
        // Perform check for uninitialized let/const variables.
        if (value->op() == the_hole->op()) {
          value = BuildThrowReferenceError(variable);
        } else if (value->opcode() == IrOpcode::kPhi) {
          value = BuildHoleCheckThrow(value, variable, value);
        }
      }
      return value;
    }
    case Variable::CONTEXT: {
      // Context variable (potentially up the context chain).
      int depth = current_scope()->ContextChainLength(variable->scope());
      bool immutable = variable->maybe_assigned() == kNotAssigned;
      const Operator* op =
          javascript()->LoadContext(depth, variable->index(), immutable);
      Node* value = NewNode(op, current_context());
      // TODO(titzer): initialization checks are redundant for already
      // initialized immutable context loads, but only specialization knows.
      // Maybe specializer should be a parameter to the graph builder?
      if (mode == CONST_LEGACY) {
        // Perform check for uninitialized legacy const variables.
        Node* undefined = jsgraph()->UndefinedConstant();
        value = BuildHoleCheckSilent(value, undefined, value);
      } else if (mode == LET || mode == CONST) {
        // Perform check for uninitialized let/const variables.
        value = BuildHoleCheckThrow(value, variable, value);
      }
      return value;
    }
    case Variable::LOOKUP: {
      // Dynamic lookup of context variable (anywhere in the chain).
      Node* name = jsgraph()->Constant(variable->name());
      Runtime::FunctionId function_id =
          (contextual_mode == CONTEXTUAL)
              ? Runtime::kLoadLookupSlot
              : Runtime::kLoadLookupSlotNoReferenceError;
      const Operator* op = javascript()->Runtime(function_id, 2);
      Node* pair = NewNode(op, current_context(), name);
      return NewNode(common()->Projection(0), pair);
    }
  }
  UNREACHABLE();
  return NULL;
}


Node* AstGraphBuilder::BuildVariableDelete(Variable* variable) {
  switch (variable->location()) {
    case Variable::UNALLOCATED: {
      // Global var, const, or let variable.
      Node* global = BuildLoadGlobalObject();
      Node* name = jsgraph()->Constant(variable->name());
      const Operator* op = javascript()->DeleteProperty(strict_mode());
      return NewNode(op, global, name);
    }
    case Variable::PARAMETER:
    case Variable::LOCAL:
    case Variable::CONTEXT:
      // Local var, const, or let variable or context variable.
      return variable->is_this() ? jsgraph()->TrueConstant()
                                 : jsgraph()->FalseConstant();
    case Variable::LOOKUP: {
      // Dynamic lookup of context variable (anywhere in the chain).
      Node* name = jsgraph()->Constant(variable->name());
      const Operator* op = javascript()->Runtime(Runtime::kDeleteLookupSlot, 2);
      return NewNode(op, current_context(), name);
    }
  }
  UNREACHABLE();
  return NULL;
}


Node* AstGraphBuilder::BuildVariableAssignment(Variable* variable, Node* value,
                                               Token::Value op,
                                               BailoutId bailout_id) {
  Node* the_hole = jsgraph()->TheHoleConstant();
  VariableMode mode = variable->mode();
  switch (variable->location()) {
    case Variable::UNALLOCATED: {
      // Global var, const, or let variable.
      Node* global = BuildLoadGlobalObject();
      Unique<Name> name = MakeUnique(variable->name());
      const Operator* op = javascript()->StoreNamed(strict_mode(), name);
      Node* store = NewNode(op, global, value);
      PrepareFrameState(store, bailout_id);
      return store;
    }
    case Variable::PARAMETER:
    case Variable::LOCAL:
      // Local var, const, or let variable.
      if (mode == CONST_LEGACY && op == Token::INIT_CONST_LEGACY) {
        // Perform an initialization check for legacy const variables.
        Node* current = environment()->Lookup(variable);
        if (current->op() != the_hole->op()) {
          value = BuildHoleCheckSilent(current, value, current);
        }
      } else if (mode == CONST_LEGACY && op != Token::INIT_CONST_LEGACY) {
        // Non-initializing assignments to legacy const is ignored.
        return value;
      } else if (mode == LET && op != Token::INIT_LET) {
        // Perform an initialization check for let declared variables.
        // Also note that the dynamic hole-check is only done to ensure that
        // this does not break in the presence of do-expressions within the
        // temporal dead zone of a let declared variable.
        Node* current = environment()->Lookup(variable);
        if (current->op() == the_hole->op()) {
          value = BuildThrowReferenceError(variable);
        } else if (value->opcode() == IrOpcode::kPhi) {
          value = BuildHoleCheckThrow(current, variable, value);
        }
      } else if (mode == CONST && op != Token::INIT_CONST) {
        // All assignments to const variables are early errors.
        UNREACHABLE();
      }
      environment()->Bind(variable, value);
      return value;
    case Variable::CONTEXT: {
      // Context variable (potentially up the context chain).
      int depth = current_scope()->ContextChainLength(variable->scope());
      if (mode == CONST_LEGACY && op == Token::INIT_CONST_LEGACY) {
        // Perform an initialization check for legacy const variables.
        const Operator* op =
            javascript()->LoadContext(depth, variable->index(), false);
        Node* current = NewNode(op, current_context());
        value = BuildHoleCheckSilent(current, value, current);
      } else if (mode == CONST_LEGACY && op != Token::INIT_CONST_LEGACY) {
        // Non-initializing assignments to legacy const is ignored.
        return value;
      } else if (mode == LET && op != Token::INIT_LET) {
        // Perform an initialization check for let declared variables.
        const Operator* op =
            javascript()->LoadContext(depth, variable->index(), false);
        Node* current = NewNode(op, current_context());
        value = BuildHoleCheckThrow(current, variable, value);
      } else if (mode == CONST && op != Token::INIT_CONST) {
        // All assignments to const variables are early errors.
        UNREACHABLE();
      }
      const Operator* op = javascript()->StoreContext(depth, variable->index());
      return NewNode(op, current_context(), value);
    }
    case Variable::LOOKUP: {
      // Dynamic lookup of context variable (anywhere in the chain).
      Node* name = jsgraph()->Constant(variable->name());
      Node* strict = jsgraph()->Constant(strict_mode());
      // TODO(mstarzinger): Use Runtime::kInitializeLegacyConstLookupSlot for
      // initializations of const declarations.
      const Operator* op = javascript()->Runtime(Runtime::kStoreLookupSlot, 4);
      return NewNode(op, value, current_context(), name, strict);
    }
  }
  UNREACHABLE();
  return NULL;
}


Node* AstGraphBuilder::BuildLoadObjectField(Node* object, int offset) {
  // TODO(sigurds) Use simplified load here once it is ready.
  Node* field_load = NewNode(jsgraph()->machine()->Load(kMachAnyTagged), object,
                             jsgraph()->Int32Constant(offset - kHeapObjectTag));
  return field_load;
}


Node* AstGraphBuilder::BuildLoadBuiltinsObject() {
  Node* global = BuildLoadGlobalObject();
  Node* builtins =
      BuildLoadObjectField(global, JSGlobalObject::kBuiltinsOffset);
  return builtins;
}


Node* AstGraphBuilder::BuildLoadGlobalObject() {
  Node* context = GetFunctionContext();
  const Operator* load_op =
      javascript()->LoadContext(0, Context::GLOBAL_OBJECT_INDEX, true);
  return NewNode(load_op, context);
}


Node* AstGraphBuilder::BuildToBoolean(Node* value) {
  // TODO(mstarzinger): Possible optimization is to NOP for boolean values.
  return NewNode(javascript()->ToBoolean(), value);
}


Node* AstGraphBuilder::BuildThrowReferenceError(Variable* variable) {
  // TODO(mstarzinger): Should be unified with the VisitThrow implementation.
  Node* variable_name = jsgraph()->Constant(variable->name());
  const Operator* op = javascript()->Runtime(Runtime::kThrowReferenceError, 1);
  return NewNode(op, variable_name);
}


Node* AstGraphBuilder::BuildBinaryOp(Node* left, Node* right, Token::Value op) {
  const Operator* js_op;
  switch (op) {
    case Token::BIT_OR:
      js_op = javascript()->BitwiseOr();
      break;
    case Token::BIT_AND:
      js_op = javascript()->BitwiseAnd();
      break;
    case Token::BIT_XOR:
      js_op = javascript()->BitwiseXor();
      break;
    case Token::SHL:
      js_op = javascript()->ShiftLeft();
      break;
    case Token::SAR:
      js_op = javascript()->ShiftRight();
      break;
    case Token::SHR:
      js_op = javascript()->ShiftRightLogical();
      break;
    case Token::ADD:
      js_op = javascript()->Add();
      break;
    case Token::SUB:
      js_op = javascript()->Subtract();
      break;
    case Token::MUL:
      js_op = javascript()->Multiply();
      break;
    case Token::DIV:
      js_op = javascript()->Divide();
      break;
    case Token::MOD:
      js_op = javascript()->Modulus();
      break;
    default:
      UNREACHABLE();
      js_op = NULL;
  }
  return NewNode(js_op, left, right);
}


void AstGraphBuilder::PrepareFrameState(Node* node, BailoutId ast_id,
                                        OutputFrameStateCombine combine) {
  if (OperatorProperties::HasFrameStateInput(node->op())) {
    DCHECK(NodeProperties::GetFrameStateInput(node)->opcode() ==
           IrOpcode::kDead);
    NodeProperties::ReplaceFrameStateInput(
        node, environment()->Checkpoint(ast_id, combine));
  }
}

}
}
}  // namespace v8::internal::compiler
