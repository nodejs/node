// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/accessors.h"
#include "src/bootstrapper.h"
#include "src/messages.h"
#include "src/parser.h"
#include "src/scopeinfo.h"
#include "src/scopes.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// Implementation of LocalsMap
//
// Note: We are storing the handle locations as key values in the hash map.
//       When inserting a new variable via Declare(), we rely on the fact that
//       the handle location remains alive for the duration of that variable
//       use. Because a Variable holding a handle with the same location exists
//       this is ensured.

VariableMap::VariableMap(Zone* zone)
    : ZoneHashMap(ZoneHashMap::PointersMatch, 8, ZoneAllocationPolicy(zone)),
      zone_(zone) {}
VariableMap::~VariableMap() {}


Variable* VariableMap::Declare(Scope* scope, const AstRawString* name,
                               VariableMode mode, Variable::Kind kind,
                               InitializationFlag initialization_flag,
                               MaybeAssignedFlag maybe_assigned_flag,
                               int declaration_group_start) {
  // AstRawStrings are unambiguous, i.e., the same string is always represented
  // by the same AstRawString*.
  // FIXME(marja): fix the type of Lookup.
  Entry* p =
      ZoneHashMap::LookupOrInsert(const_cast<AstRawString*>(name), name->hash(),
                                  ZoneAllocationPolicy(zone()));
  if (p->value == NULL) {
    // The variable has not been declared yet -> insert it.
    DCHECK(p->key == name);
    if (kind == Variable::CLASS) {
      p->value = new (zone())
          ClassVariable(scope, name, mode, initialization_flag,
                        maybe_assigned_flag, declaration_group_start);
    } else {
      p->value = new (zone()) Variable(
          scope, name, mode, kind, initialization_flag, maybe_assigned_flag);
    }
  }
  return reinterpret_cast<Variable*>(p->value);
}


Variable* VariableMap::Lookup(const AstRawString* name) {
  Entry* p = ZoneHashMap::Lookup(const_cast<AstRawString*>(name), name->hash());
  if (p != NULL) {
    DCHECK(reinterpret_cast<const AstRawString*>(p->key) == name);
    DCHECK(p->value != NULL);
    return reinterpret_cast<Variable*>(p->value);
  }
  return NULL;
}


// ----------------------------------------------------------------------------
// Implementation of Scope

Scope::Scope(Zone* zone, Scope* outer_scope, ScopeType scope_type,
             AstValueFactory* ast_value_factory, FunctionKind function_kind)
    : inner_scopes_(4, zone),
      variables_(zone),
      internals_(4, zone),
      temps_(4, zone),
      params_(4, zone),
      unresolved_(16, zone),
      decls_(4, zone),
      module_descriptor_(
          scope_type == MODULE_SCOPE ? ModuleDescriptor::New(zone) : NULL),
      already_resolved_(false),
      ast_value_factory_(ast_value_factory),
      zone_(zone),
      class_declaration_group_start_(-1) {
  SetDefaults(scope_type, outer_scope, Handle<ScopeInfo>::null(),
              function_kind);
  // The outermost scope must be a script scope.
  DCHECK(scope_type == SCRIPT_SCOPE || outer_scope != NULL);
  DCHECK(!HasIllegalRedeclaration());
}


Scope::Scope(Zone* zone, Scope* inner_scope, ScopeType scope_type,
             Handle<ScopeInfo> scope_info, AstValueFactory* value_factory)
    : inner_scopes_(4, zone),
      variables_(zone),
      internals_(4, zone),
      temps_(4, zone),
      params_(4, zone),
      unresolved_(16, zone),
      decls_(4, zone),
      module_descriptor_(NULL),
      already_resolved_(true),
      ast_value_factory_(value_factory),
      zone_(zone),
      class_declaration_group_start_(-1) {
  SetDefaults(scope_type, NULL, scope_info);
  if (!scope_info.is_null()) {
    num_heap_slots_ = scope_info_->ContextLength();
  }
  // Ensure at least MIN_CONTEXT_SLOTS to indicate a materialized context.
  num_heap_slots_ = Max(num_heap_slots_,
                        static_cast<int>(Context::MIN_CONTEXT_SLOTS));
  AddInnerScope(inner_scope);
}


Scope::Scope(Zone* zone, Scope* inner_scope,
             const AstRawString* catch_variable_name,
             AstValueFactory* value_factory)
    : inner_scopes_(1, zone),
      variables_(zone),
      internals_(0, zone),
      temps_(0, zone),
      params_(0, zone),
      unresolved_(0, zone),
      decls_(0, zone),
      module_descriptor_(NULL),
      already_resolved_(true),
      ast_value_factory_(value_factory),
      zone_(zone),
      class_declaration_group_start_(-1) {
  SetDefaults(CATCH_SCOPE, NULL, Handle<ScopeInfo>::null());
  AddInnerScope(inner_scope);
  ++num_var_or_const_;
  num_heap_slots_ = Context::MIN_CONTEXT_SLOTS;
  Variable* variable = variables_.Declare(this,
                                          catch_variable_name,
                                          VAR,
                                          Variable::NORMAL,
                                          kCreatedInitialized);
  AllocateHeapSlot(variable);
}


void Scope::SetDefaults(ScopeType scope_type, Scope* outer_scope,
                        Handle<ScopeInfo> scope_info,
                        FunctionKind function_kind) {
  outer_scope_ = outer_scope;
  scope_type_ = scope_type;
  function_kind_ = function_kind;
  block_scope_is_class_scope_ = false;
  scope_name_ = ast_value_factory_->empty_string();
  dynamics_ = NULL;
  receiver_ = NULL;
  new_target_ = nullptr;
  function_ = NULL;
  arguments_ = NULL;
  illegal_redecl_ = NULL;
  scope_inside_with_ = false;
  scope_contains_with_ = false;
  scope_calls_eval_ = false;
  scope_uses_arguments_ = false;
  scope_uses_super_property_ = false;
  scope_uses_this_ = false;
  asm_module_ = false;
  asm_function_ = outer_scope != NULL && outer_scope->asm_module_;
  // Inherit the language mode from the parent scope.
  language_mode_ = outer_scope != NULL ? outer_scope->language_mode_ : SLOPPY;
  outer_scope_calls_sloppy_eval_ = false;
  inner_scope_calls_eval_ = false;
  inner_scope_uses_arguments_ = false;
  inner_scope_uses_this_ = false;
  inner_scope_uses_super_property_ = false;
  force_eager_compilation_ = false;
  force_context_allocation_ = (outer_scope != NULL && !is_function_scope())
      ? outer_scope->has_forced_context_allocation() : false;
  num_var_or_const_ = 0;
  num_stack_slots_ = 0;
  num_heap_slots_ = 0;
  num_modules_ = 0;
  module_var_ = NULL,
  rest_parameter_ = NULL;
  rest_index_ = -1;
  scope_info_ = scope_info;
  start_position_ = RelocInfo::kNoPosition;
  end_position_ = RelocInfo::kNoPosition;
  if (!scope_info.is_null()) {
    scope_calls_eval_ = scope_info->CallsEval();
    language_mode_ = scope_info->language_mode();
    block_scope_is_class_scope_ = scope_info->block_scope_is_class_scope();
    function_kind_ = scope_info->function_kind();
  }
}


Scope* Scope::DeserializeScopeChain(Isolate* isolate, Zone* zone,
                                    Context* context, Scope* script_scope) {
  // Reconstruct the outer scope chain from a closure's context chain.
  Scope* current_scope = NULL;
  Scope* innermost_scope = NULL;
  bool contains_with = false;
  while (!context->IsNativeContext()) {
    if (context->IsWithContext()) {
      Scope* with_scope = new (zone)
          Scope(zone, current_scope, WITH_SCOPE, Handle<ScopeInfo>::null(),
                script_scope->ast_value_factory_);
      current_scope = with_scope;
      // All the inner scopes are inside a with.
      contains_with = true;
      for (Scope* s = innermost_scope; s != NULL; s = s->outer_scope()) {
        s->scope_inside_with_ = true;
      }
    } else if (context->IsScriptContext()) {
      ScopeInfo* scope_info = ScopeInfo::cast(context->extension());
      current_scope = new (zone) Scope(zone, current_scope, SCRIPT_SCOPE,
                                       Handle<ScopeInfo>(scope_info),
                                       script_scope->ast_value_factory_);
    } else if (context->IsModuleContext()) {
      ScopeInfo* scope_info = ScopeInfo::cast(context->module()->scope_info());
      current_scope = new (zone) Scope(zone, current_scope, MODULE_SCOPE,
                                       Handle<ScopeInfo>(scope_info),
                                       script_scope->ast_value_factory_);
    } else if (context->IsFunctionContext()) {
      ScopeInfo* scope_info = context->closure()->shared()->scope_info();
      current_scope = new (zone) Scope(zone, current_scope, FUNCTION_SCOPE,
                                       Handle<ScopeInfo>(scope_info),
                                       script_scope->ast_value_factory_);
      if (scope_info->IsAsmFunction()) current_scope->asm_function_ = true;
      if (scope_info->IsAsmModule()) current_scope->asm_module_ = true;
    } else if (context->IsBlockContext()) {
      ScopeInfo* scope_info = ScopeInfo::cast(context->extension());
      current_scope = new (zone)
          Scope(zone, current_scope, BLOCK_SCOPE, Handle<ScopeInfo>(scope_info),
                script_scope->ast_value_factory_);
    } else {
      DCHECK(context->IsCatchContext());
      String* name = String::cast(context->extension());
      current_scope = new (zone) Scope(
          zone, current_scope,
          script_scope->ast_value_factory_->GetString(Handle<String>(name)),
          script_scope->ast_value_factory_);
    }
    if (contains_with) current_scope->RecordWithStatement();
    if (innermost_scope == NULL) innermost_scope = current_scope;

    // Forget about a with when we move to a context for a different function.
    if (context->previous()->closure() != context->closure()) {
      contains_with = false;
    }
    context = context->previous();
  }

  script_scope->AddInnerScope(current_scope);
  script_scope->PropagateScopeInfo(false);
  return (innermost_scope == NULL) ? script_scope : innermost_scope;
}


bool Scope::Analyze(ParseInfo* info) {
  DCHECK(info->function() != NULL);
  DCHECK(info->scope() == NULL);
  Scope* scope = info->function()->scope();
  Scope* top = scope;

  // Traverse the scope tree up to the first unresolved scope or the global
  // scope and start scope resolution and variable allocation from that scope.
  while (!top->is_script_scope() &&
         !top->outer_scope()->already_resolved()) {
    top = top->outer_scope();
  }

  // Allocate the variables.
  {
    AstNodeFactory ast_node_factory(info->ast_value_factory());
    if (!top->AllocateVariables(info, &ast_node_factory)) {
      DCHECK(top->pending_error_handler_.has_pending_error());
      top->pending_error_handler_.ThrowPendingError(info->isolate(),
                                                    info->script());
      return false;
    }
  }

#ifdef DEBUG
  if (info->isolate()->bootstrapper()->IsActive()
          ? FLAG_print_builtin_scopes
          : FLAG_print_scopes) {
    scope->Print();
  }
#endif

  info->set_scope(scope);
  return true;
}


void Scope::Initialize() {
  bool subclass_constructor = IsSubclassConstructor(function_kind_);
  DCHECK(!already_resolved());

  // Add this scope as a new inner scope of the outer scope.
  if (outer_scope_ != NULL) {
    outer_scope_->inner_scopes_.Add(this, zone());
    scope_inside_with_ = outer_scope_->scope_inside_with_ || is_with_scope();
  } else {
    scope_inside_with_ = is_with_scope();
  }

  // Declare convenience variables.
  // Declare and allocate receiver (even for the script scope, and even
  // if naccesses_ == 0).
  // NOTE: When loading parameters in the script scope, we must take
  // care not to access them as properties of the global object, but
  // instead load them directly from the stack. Currently, the only
  // such parameter is 'this' which is passed on the stack when
  // invoking scripts
  if (is_declaration_scope()) {
    DCHECK(!subclass_constructor || is_function_scope());
    Variable* var = variables_.Declare(
        this, ast_value_factory_->this_string(),
        subclass_constructor ? CONST : VAR, Variable::THIS,
        subclass_constructor ? kNeedsInitialization : kCreatedInitialized);
    var->AllocateTo(Variable::PARAMETER, -1);
    receiver_ = var;

    if (subclass_constructor) {
      new_target_ =
          variables_.Declare(this, ast_value_factory_->new_target_string(),
                             CONST, Variable::NEW_TARGET, kCreatedInitialized);
      new_target_->AllocateTo(Variable::PARAMETER, -2);
      new_target_->set_is_used();
    }
  } else {
    DCHECK(outer_scope() != NULL);
    receiver_ = outer_scope()->receiver();
  }

  if (is_function_scope() && !is_arrow_scope()) {
    // Declare 'arguments' variable which exists in all non arrow functions.
    // Note that it might never be accessed, in which case it won't be
    // allocated during variable allocation.
    variables_.Declare(this,
                       ast_value_factory_->arguments_string(),
                       VAR,
                       Variable::ARGUMENTS,
                       kCreatedInitialized);
  }
}


Scope* Scope::FinalizeBlockScope() {
  DCHECK(is_block_scope());
  DCHECK(internals_.is_empty());
  DCHECK(temps_.is_empty());
  DCHECK(params_.is_empty());

  if (num_var_or_const() > 0) return this;

  // Remove this scope from outer scope.
  for (int i = 0; i < outer_scope_->inner_scopes_.length(); i++) {
    if (outer_scope_->inner_scopes_[i] == this) {
      outer_scope_->inner_scopes_.Remove(i);
      break;
    }
  }

  // Reparent inner scopes.
  for (int i = 0; i < inner_scopes_.length(); i++) {
    outer_scope()->AddInnerScope(inner_scopes_[i]);
  }

  // Move unresolved variables
  for (int i = 0; i < unresolved_.length(); i++) {
    outer_scope()->unresolved_.Add(unresolved_[i], zone());
  }

  // Propagate usage flags to outer scope.
  if (uses_arguments()) outer_scope_->RecordArgumentsUsage();
  if (uses_super_property()) outer_scope_->RecordSuperPropertyUsage();
  if (uses_this()) outer_scope_->RecordThisUsage();
  if (scope_calls_eval_) outer_scope_->RecordEvalCall();

  return NULL;
}


Variable* Scope::LookupLocal(const AstRawString* name) {
  Variable* result = variables_.Lookup(name);
  if (result != NULL || scope_info_.is_null()) {
    return result;
  }
  Handle<String> name_handle = name->string();
  // The Scope is backed up by ScopeInfo. This means it cannot operate in a
  // heap-independent mode, and all strings must be internalized immediately. So
  // it's ok to get the Handle<String> here.
  // If we have a serialized scope info, we might find the variable there.
  // There should be no local slot with the given name.
  DCHECK(scope_info_->StackSlotIndex(*name_handle) < 0 || is_block_scope());

  // Check context slot lookup.
  VariableMode mode;
  Variable::Location location = Variable::CONTEXT;
  InitializationFlag init_flag;
  MaybeAssignedFlag maybe_assigned_flag;
  int index = ScopeInfo::ContextSlotIndex(scope_info_, name_handle, &mode,
                                          &init_flag, &maybe_assigned_flag);
  if (index < 0) {
    // Check parameters.
    index = scope_info_->ParameterIndex(*name_handle);
    if (index < 0) return NULL;

    mode = DYNAMIC;
    location = Variable::LOOKUP;
    init_flag = kCreatedInitialized;
    // Be conservative and flag parameters as maybe assigned. Better information
    // would require ScopeInfo to serialize the maybe_assigned bit also for
    // parameters.
    maybe_assigned_flag = kMaybeAssigned;
  }

  // TODO(marja, rossberg): Declare variables of the right Kind.
  Variable* var = variables_.Declare(this, name, mode, Variable::NORMAL,
                                     init_flag, maybe_assigned_flag);
  var->AllocateTo(location, index);
  return var;
}


Variable* Scope::LookupFunctionVar(const AstRawString* name,
                                   AstNodeFactory* factory) {
  if (function_ != NULL && function_->proxy()->raw_name() == name) {
    return function_->proxy()->var();
  } else if (!scope_info_.is_null()) {
    // If we are backed by a scope info, try to lookup the variable there.
    VariableMode mode;
    int index = scope_info_->FunctionContextSlotIndex(*(name->string()), &mode);
    if (index < 0) return NULL;
    Variable* var = new (zone())
        Variable(this, name, mode, Variable::NORMAL, kCreatedInitialized);
    VariableProxy* proxy = factory->NewVariableProxy(var);
    VariableDeclaration* declaration = factory->NewVariableDeclaration(
        proxy, mode, this, RelocInfo::kNoPosition);
    DeclareFunctionVar(declaration);
    var->AllocateTo(Variable::CONTEXT, index);
    return var;
  } else {
    return NULL;
  }
}


Variable* Scope::Lookup(const AstRawString* name) {
  for (Scope* scope = this;
       scope != NULL;
       scope = scope->outer_scope()) {
    Variable* var = scope->LookupLocal(name);
    if (var != NULL) return var;
  }
  return NULL;
}


Variable* Scope::DeclareParameter(const AstRawString* name, VariableMode mode,
                                  bool is_rest, bool* is_duplicate) {
  DCHECK(!already_resolved());
  DCHECK(is_function_scope());
  Variable* var = variables_.Declare(this, name, mode, Variable::NORMAL,
                                     kCreatedInitialized);
  if (is_rest) {
    DCHECK_NULL(rest_parameter_);
    rest_parameter_ = var;
    rest_index_ = num_parameters();
  }
  // TODO(wingo): Avoid O(n^2) check.
  *is_duplicate = IsDeclaredParameter(name);
  params_.Add(var, zone());
  return var;
}


Variable* Scope::DeclareLocal(const AstRawString* name, VariableMode mode,
                              InitializationFlag init_flag, Variable::Kind kind,
                              MaybeAssignedFlag maybe_assigned_flag,
                              int declaration_group_start) {
  DCHECK(!already_resolved());
  // This function handles VAR, LET, and CONST modes.  DYNAMIC variables are
  // introduces during variable allocation, INTERNAL variables are allocated
  // explicitly, and TEMPORARY variables are allocated via NewTemporary().
  DCHECK(IsDeclaredVariableMode(mode));
  ++num_var_or_const_;
  return variables_.Declare(this, name, mode, kind, init_flag,
                            maybe_assigned_flag, declaration_group_start);
}


Variable* Scope::DeclareDynamicGlobal(const AstRawString* name) {
  DCHECK(is_script_scope());
  return variables_.Declare(this,
                            name,
                            DYNAMIC_GLOBAL,
                            Variable::NORMAL,
                            kCreatedInitialized);
}


void Scope::RemoveUnresolved(VariableProxy* var) {
  // Most likely (always?) any variable we want to remove
  // was just added before, so we search backwards.
  for (int i = unresolved_.length(); i-- > 0;) {
    if (unresolved_[i] == var) {
      unresolved_.Remove(i);
      return;
    }
  }
}


Variable* Scope::NewInternal(const AstRawString* name) {
  DCHECK(!already_resolved());
  Variable* var = new(zone()) Variable(this,
                                       name,
                                       INTERNAL,
                                       Variable::NORMAL,
                                       kCreatedInitialized);
  internals_.Add(var, zone());
  return var;
}


Variable* Scope::NewTemporary(const AstRawString* name) {
  DCHECK(!already_resolved());
  Variable* var = new(zone()) Variable(this,
                                       name,
                                       TEMPORARY,
                                       Variable::NORMAL,
                                       kCreatedInitialized);
  temps_.Add(var, zone());
  return var;
}


void Scope::AddDeclaration(Declaration* declaration) {
  decls_.Add(declaration, zone());
}


void Scope::SetIllegalRedeclaration(Expression* expression) {
  // Record only the first illegal redeclaration.
  if (!HasIllegalRedeclaration()) {
    illegal_redecl_ = expression;
  }
  DCHECK(HasIllegalRedeclaration());
}


void Scope::VisitIllegalRedeclaration(AstVisitor* visitor) {
  DCHECK(HasIllegalRedeclaration());
  illegal_redecl_->Accept(visitor);
}


Declaration* Scope::CheckConflictingVarDeclarations() {
  int length = decls_.length();
  for (int i = 0; i < length; i++) {
    Declaration* decl = decls_[i];
    if (decl->mode() != VAR) continue;
    const AstRawString* name = decl->proxy()->raw_name();

    // Iterate through all scopes until and including the declaration scope.
    Scope* previous = NULL;
    Scope* current = decl->scope();
    do {
      // There is a conflict if there exists a non-VAR binding.
      Variable* other_var = current->variables_.Lookup(name);
      if (other_var != NULL && other_var->mode() != VAR) {
        return decl;
      }
      previous = current;
      current = current->outer_scope_;
    } while (!previous->is_declaration_scope());
  }
  return NULL;
}


class VarAndOrder {
 public:
  VarAndOrder(Variable* var, int order) : var_(var), order_(order) { }
  Variable* var() const { return var_; }
  int order() const { return order_; }
  static int Compare(const VarAndOrder* a, const VarAndOrder* b) {
    return a->order_ - b->order_;
  }

 private:
  Variable* var_;
  int order_;
};


void Scope::CollectStackAndContextLocals(
    ZoneList<Variable*>* stack_locals, ZoneList<Variable*>* context_locals,
    ZoneList<Variable*>* strong_mode_free_variables) {
  DCHECK(stack_locals != NULL);
  DCHECK(context_locals != NULL);

  // Collect internals which are always allocated on the heap.
  for (int i = 0; i < internals_.length(); i++) {
    Variable* var = internals_[i];
    if (var->is_used()) {
      DCHECK(var->IsContextSlot());
      context_locals->Add(var, zone());
    }
  }

  // Collect temporaries which are always allocated on the stack, unless the
  // context as a whole has forced context allocation.
  for (int i = 0; i < temps_.length(); i++) {
    Variable* var = temps_[i];
    if (var->is_used()) {
      if (var->IsContextSlot()) {
        DCHECK(has_forced_context_allocation());
        context_locals->Add(var, zone());
      } else {
        DCHECK(var->IsStackLocal());
        stack_locals->Add(var, zone());
      }
    }
  }

  // Collect declared local variables.
  ZoneList<VarAndOrder> vars(variables_.occupancy(), zone());
  for (VariableMap::Entry* p = variables_.Start();
       p != NULL;
       p = variables_.Next(p)) {
    Variable* var = reinterpret_cast<Variable*>(p->value);
    if (strong_mode_free_variables && var->has_strong_mode_reference() &&
        var->mode() == DYNAMIC_GLOBAL) {
      strong_mode_free_variables->Add(var, zone());
    }

    if (var->is_used()) {
      vars.Add(VarAndOrder(var, p->order), zone());
    }
  }
  vars.Sort(VarAndOrder::Compare);
  int var_count = vars.length();
  for (int i = 0; i < var_count; i++) {
    Variable* var = vars[i].var();
    if (var->IsStackLocal()) {
      stack_locals->Add(var, zone());
    } else if (var->IsContextSlot()) {
      context_locals->Add(var, zone());
    }
  }
}


bool Scope::AllocateVariables(ParseInfo* info, AstNodeFactory* factory) {
  // 1) Propagate scope information.
  bool outer_scope_calls_sloppy_eval = false;
  if (outer_scope_ != NULL) {
    outer_scope_calls_sloppy_eval =
        outer_scope_->outer_scope_calls_sloppy_eval() |
        outer_scope_->calls_sloppy_eval();
  }
  PropagateScopeInfo(outer_scope_calls_sloppy_eval);

  // 2) Allocate module instances.
  if (FLAG_harmony_modules && is_script_scope()) {
    DCHECK(num_modules_ == 0);
    AllocateModules();
  }

  // 3) Resolve variables.
  if (!ResolveVariablesRecursively(info, factory)) return false;

  // 4) Allocate variables.
  AllocateVariablesRecursively(info->isolate());

  return true;
}


bool Scope::HasTrivialContext() const {
  // A function scope has a trivial context if it always is the global
  // context. We iteratively scan out the context chain to see if
  // there is anything that makes this scope non-trivial; otherwise we
  // return true.
  for (const Scope* scope = this; scope != NULL; scope = scope->outer_scope_) {
    if (scope->is_eval_scope()) return false;
    if (scope->scope_inside_with_) return false;
    if (scope->num_heap_slots_ > 0) return false;
  }
  return true;
}


bool Scope::HasTrivialOuterContext() const {
  Scope* outer = outer_scope_;
  if (outer == NULL) return true;
  // Note that the outer context may be trivial in general, but the current
  // scope may be inside a 'with' statement in which case the outer context
  // for this scope is not trivial.
  return !scope_inside_with_ && outer->HasTrivialContext();
}


bool Scope::HasLazyCompilableOuterContext() const {
  Scope* outer = outer_scope_;
  if (outer == NULL) return true;
  // We have to prevent lazy compilation if this scope is inside a with scope
  // and all declaration scopes between them have empty contexts. Such
  // declaration scopes may become invisible during scope info deserialization.
  outer = outer->DeclarationScope();
  bool found_non_trivial_declarations = false;
  for (const Scope* scope = outer; scope != NULL; scope = scope->outer_scope_) {
    if (scope->is_with_scope() && !found_non_trivial_declarations) return false;
    if (scope->is_block_scope() && !scope->decls_.is_empty()) return false;
    if (scope->is_declaration_scope() && scope->num_heap_slots() > 0) {
      found_non_trivial_declarations = true;
    }
  }
  return true;
}


bool Scope::AllowsLazyCompilation() const {
  return !force_eager_compilation_ && HasLazyCompilableOuterContext();
}


bool Scope::AllowsLazyCompilationWithoutContext() const {
  return !force_eager_compilation_ && HasTrivialOuterContext();
}


int Scope::ContextChainLength(Scope* scope) {
  int n = 0;
  for (Scope* s = this; s != scope; s = s->outer_scope_) {
    DCHECK(s != NULL);  // scope must be in the scope chain
    if (s->is_with_scope() || s->num_heap_slots() > 0) n++;
    // Catch and module scopes always have heap slots.
    DCHECK(!s->is_catch_scope() || s->num_heap_slots() > 0);
    DCHECK(!s->is_module_scope() || s->num_heap_slots() > 0);
  }
  return n;
}


Scope* Scope::ScriptScope() {
  Scope* scope = this;
  while (!scope->is_script_scope()) {
    scope = scope->outer_scope();
  }
  return scope;
}


Scope* Scope::DeclarationScope() {
  Scope* scope = this;
  while (!scope->is_declaration_scope()) {
    scope = scope->outer_scope();
  }
  return scope;
}


Handle<ScopeInfo> Scope::GetScopeInfo(Isolate* isolate) {
  if (scope_info_.is_null()) {
    scope_info_ = ScopeInfo::Create(isolate, zone(), this);
  }
  return scope_info_;
}


void Scope::GetNestedScopeChain(Isolate* isolate,
                                List<Handle<ScopeInfo> >* chain, int position) {
  if (!is_eval_scope()) chain->Add(Handle<ScopeInfo>(GetScopeInfo(isolate)));

  for (int i = 0; i < inner_scopes_.length(); i++) {
    Scope* scope = inner_scopes_[i];
    int beg_pos = scope->start_position();
    int end_pos = scope->end_position();
    DCHECK(beg_pos >= 0 && end_pos >= 0);
    if (beg_pos <= position && position < end_pos) {
      scope->GetNestedScopeChain(isolate, chain, position);
      return;
    }
  }
}


void Scope::ReportMessage(int start_position, int end_position,
                          const char* message, const AstRawString* arg) {
  // Propagate the error to the topmost scope targeted by this scope analysis
  // phase.
  Scope* top = this;
  while (!top->is_script_scope() && !top->outer_scope()->already_resolved()) {
    top = top->outer_scope();
  }

  top->pending_error_handler_.ReportMessageAt(start_position, end_position,
                                              message, arg, kReferenceError);
}


#ifdef DEBUG
static const char* Header(ScopeType scope_type) {
  switch (scope_type) {
    case EVAL_SCOPE: return "eval";
    case FUNCTION_SCOPE: return "function";
    case MODULE_SCOPE: return "module";
    case SCRIPT_SCOPE: return "global";
    case CATCH_SCOPE: return "catch";
    case BLOCK_SCOPE: return "block";
    case WITH_SCOPE: return "with";
    case ARROW_SCOPE: return "arrow";
  }
  UNREACHABLE();
  return NULL;
}


static void Indent(int n, const char* str) {
  PrintF("%*s%s", n, "", str);
}


static void PrintName(const AstRawString* name) {
  PrintF("%.*s", name->length(), name->raw_data());
}


static void PrintLocation(Variable* var) {
  switch (var->location()) {
    case Variable::UNALLOCATED:
      break;
    case Variable::PARAMETER:
      PrintF("parameter[%d]", var->index());
      break;
    case Variable::LOCAL:
      PrintF("local[%d]", var->index());
      break;
    case Variable::CONTEXT:
      PrintF("context[%d]", var->index());
      break;
    case Variable::LOOKUP:
      PrintF("lookup");
      break;
  }
}


static void PrintVar(int indent, Variable* var) {
  if (var->is_used() || !var->IsUnallocated()) {
    Indent(indent, Variable::Mode2String(var->mode()));
    PrintF(" ");
    PrintName(var->raw_name());
    PrintF(";  // ");
    PrintLocation(var);
    bool comma = !var->IsUnallocated();
    if (var->has_forced_context_allocation()) {
      if (comma) PrintF(", ");
      PrintF("forced context allocation");
      comma = true;
    }
    if (var->maybe_assigned() == kMaybeAssigned) {
      if (comma) PrintF(", ");
      PrintF("maybe assigned");
    }
    PrintF("\n");
  }
}


static void PrintMap(int indent, VariableMap* map) {
  for (VariableMap::Entry* p = map->Start(); p != NULL; p = map->Next(p)) {
    Variable* var = reinterpret_cast<Variable*>(p->value);
    PrintVar(indent, var);
  }
}


void Scope::Print(int n) {
  int n0 = (n > 0 ? n : 0);
  int n1 = n0 + 2;  // indentation

  // Print header.
  Indent(n0, Header(scope_type_));
  if (!scope_name_->IsEmpty()) {
    PrintF(" ");
    PrintName(scope_name_);
  }

  // Print parameters, if any.
  if (is_function_scope()) {
    PrintF(" (");
    for (int i = 0; i < params_.length(); i++) {
      if (i > 0) PrintF(", ");
      PrintName(params_[i]->raw_name());
    }
    PrintF(")");
  }

  PrintF(" { // (%d, %d)\n", start_position(), end_position());

  // Function name, if any (named function literals, only).
  if (function_ != NULL) {
    Indent(n1, "// (local) function name: ");
    PrintName(function_->proxy()->raw_name());
    PrintF("\n");
  }

  // Scope info.
  if (HasTrivialOuterContext()) {
    Indent(n1, "// scope has trivial outer context\n");
  }
  if (is_strong(language_mode())) {
    Indent(n1, "// strong mode scope\n");
  } else if (is_strict(language_mode())) {
    Indent(n1, "// strict mode scope\n");
  }
  if (scope_inside_with_) Indent(n1, "// scope inside 'with'\n");
  if (scope_contains_with_) Indent(n1, "// scope contains 'with'\n");
  if (scope_calls_eval_) Indent(n1, "// scope calls 'eval'\n");
  if (scope_uses_arguments_) Indent(n1, "// scope uses 'arguments'\n");
  if (scope_uses_super_property_)
    Indent(n1, "// scope uses 'super' property\n");
  if (scope_uses_this_) Indent(n1, "// scope uses 'this'\n");
  if (inner_scope_uses_arguments_) {
    Indent(n1, "// inner scope uses 'arguments'\n");
  }
  if (inner_scope_uses_super_property_)
    Indent(n1, "// inner scope uses 'super' property\n");
  if (inner_scope_uses_this_) Indent(n1, "// inner scope uses 'this'\n");
  if (outer_scope_calls_sloppy_eval_) {
    Indent(n1, "// outer scope calls 'eval' in sloppy context\n");
  }
  if (inner_scope_calls_eval_) Indent(n1, "// inner scope calls 'eval'\n");
  if (num_stack_slots_ > 0) { Indent(n1, "// ");
  PrintF("%d stack slots\n", num_stack_slots_); }
  if (num_heap_slots_ > 0) { Indent(n1, "// ");
  PrintF("%d heap slots\n", num_heap_slots_); }

  // Print locals.
  if (function_ != NULL) {
    Indent(n1, "// function var:\n");
    PrintVar(n1, function_->proxy()->var());
  }

  if (temps_.length() > 0) {
    Indent(n1, "// temporary vars:\n");
    for (int i = 0; i < temps_.length(); i++) {
      PrintVar(n1, temps_[i]);
    }
  }

  if (internals_.length() > 0) {
    Indent(n1, "// internal vars:\n");
    for (int i = 0; i < internals_.length(); i++) {
      PrintVar(n1, internals_[i]);
    }
  }

  if (variables_.Start() != NULL) {
    Indent(n1, "// local vars:\n");
    PrintMap(n1, &variables_);
  }

  if (dynamics_ != NULL) {
    Indent(n1, "// dynamic vars:\n");
    PrintMap(n1, dynamics_->GetMap(DYNAMIC));
    PrintMap(n1, dynamics_->GetMap(DYNAMIC_LOCAL));
    PrintMap(n1, dynamics_->GetMap(DYNAMIC_GLOBAL));
  }

  // Print inner scopes (disable by providing negative n).
  if (n >= 0) {
    for (int i = 0; i < inner_scopes_.length(); i++) {
      PrintF("\n");
      inner_scopes_[i]->Print(n1);
    }
  }

  Indent(n0, "}\n");
}
#endif  // DEBUG


Variable* Scope::NonLocal(const AstRawString* name, VariableMode mode) {
  if (dynamics_ == NULL) dynamics_ = new (zone()) DynamicScopePart(zone());
  VariableMap* map = dynamics_->GetMap(mode);
  Variable* var = map->Lookup(name);
  if (var == NULL) {
    // Declare a new non-local.
    InitializationFlag init_flag = (mode == VAR)
        ? kCreatedInitialized : kNeedsInitialization;
    var = map->Declare(NULL,
                       name,
                       mode,
                       Variable::NORMAL,
                       init_flag);
    // Allocate it by giving it a dynamic lookup.
    var->AllocateTo(Variable::LOOKUP, -1);
  }
  return var;
}


Variable* Scope::LookupRecursive(VariableProxy* proxy,
                                 BindingKind* binding_kind,
                                 AstNodeFactory* factory) {
  DCHECK(binding_kind != NULL);
  if (already_resolved() && is_with_scope()) {
    // Short-cut: if the scope is deserialized from a scope info, variable
    // allocation is already fixed.  We can simply return with dynamic lookup.
    *binding_kind = DYNAMIC_LOOKUP;
    return NULL;
  }

  // Try to find the variable in this scope.
  Variable* var = LookupLocal(proxy->raw_name());

  // We found a variable and we are done. (Even if there is an 'eval' in
  // this scope which introduces the same variable again, the resulting
  // variable remains the same.)
  if (var != NULL) {
    *binding_kind = BOUND;
    return var;
  }

  // We did not find a variable locally. Check against the function variable,
  // if any. We can do this for all scopes, since the function variable is
  // only present - if at all - for function scopes.
  *binding_kind = UNBOUND;
  var = LookupFunctionVar(proxy->raw_name(), factory);
  if (var != NULL) {
    *binding_kind = BOUND;
  } else if (outer_scope_ != NULL) {
    var = outer_scope_->LookupRecursive(proxy, binding_kind, factory);
    if (*binding_kind == BOUND && (is_function_scope() || is_with_scope())) {
      var->ForceContextAllocation();
    }
  } else {
    DCHECK(is_script_scope());
  }

  if (is_with_scope()) {
    DCHECK(!already_resolved());
    // The current scope is a with scope, so the variable binding can not be
    // statically resolved. However, note that it was necessary to do a lookup
    // in the outer scope anyway, because if a binding exists in an outer scope,
    // the associated variable has to be marked as potentially being accessed
    // from inside of an inner with scope (the property may not be in the 'with'
    // object).
    if (var != NULL && proxy->is_assigned()) var->set_maybe_assigned();
    *binding_kind = DYNAMIC_LOOKUP;
    return NULL;
  } else if (calls_sloppy_eval()) {
    // A variable binding may have been found in an outer scope, but the current
    // scope makes a sloppy 'eval' call, so the found variable may not be
    // the correct one (the 'eval' may introduce a binding with the same name).
    // In that case, change the lookup result to reflect this situation.
    if (*binding_kind == BOUND) {
      *binding_kind = BOUND_EVAL_SHADOWED;
    } else if (*binding_kind == UNBOUND) {
      *binding_kind = UNBOUND_EVAL_SHADOWED;
    }
  }
  return var;
}


bool Scope::ResolveVariable(ParseInfo* info, VariableProxy* proxy,
                            AstNodeFactory* factory) {
  DCHECK(info->script_scope()->is_script_scope());

  // If the proxy is already resolved there's nothing to do
  // (functions and consts may be resolved by the parser).
  if (proxy->is_resolved()) return true;

  // Otherwise, try to resolve the variable.
  BindingKind binding_kind;
  Variable* var = LookupRecursive(proxy, &binding_kind, factory);
  switch (binding_kind) {
    case BOUND:
      // We found a variable binding.
      if (is_strong(language_mode())) {
        if (!CheckStrongModeDeclaration(proxy, var)) return false;
      }
      break;

    case BOUND_EVAL_SHADOWED:
      // We either found a variable binding that might be shadowed by eval  or
      // gave up on it (e.g. by encountering a local with the same in the outer
      // scope which was not promoted to a context, this can happen if we use
      // debugger to evaluate arbitrary expressions at a break point).
      if (var->IsGlobalObjectProperty()) {
        var = NonLocal(proxy->raw_name(), DYNAMIC_GLOBAL);
      } else if (var->is_dynamic()) {
        var = NonLocal(proxy->raw_name(), DYNAMIC);
      } else {
        Variable* invalidated = var;
        var = NonLocal(proxy->raw_name(), DYNAMIC_LOCAL);
        var->set_local_if_not_shadowed(invalidated);
      }
      break;

    case UNBOUND:
      // No binding has been found. Declare a variable on the global object.
      var = info->script_scope()->DeclareDynamicGlobal(proxy->raw_name());
      break;

    case UNBOUND_EVAL_SHADOWED:
      // No binding has been found. But some scope makes a sloppy 'eval' call.
      var = NonLocal(proxy->raw_name(), DYNAMIC_GLOBAL);
      break;

    case DYNAMIC_LOOKUP:
      // The variable could not be resolved statically.
      var = NonLocal(proxy->raw_name(), DYNAMIC);
      break;
  }

  DCHECK(var != NULL);
  if (proxy->is_assigned()) var->set_maybe_assigned();

  if (is_strong(language_mode())) {
    // Record that the variable is referred to from strong mode. Also, record
    // the position.
    var->RecordStrongModeReference(proxy->position(), proxy->end_position());
  }

  proxy->BindTo(var);

  return true;
}


bool Scope::CheckStrongModeDeclaration(VariableProxy* proxy, Variable* var) {
  // Check for declaration-after use (for variables) in strong mode. Note that
  // we can only do this in the case where we have seen the declaration. And we
  // always allow referencing functions (for now).

  // This might happen during lazy compilation; we don't keep track of
  // initializer positions for variables stored in ScopeInfo, so we cannot check
  // bindings against them. TODO(marja, rossberg): remove this hack.
  if (var->initializer_position() == RelocInfo::kNoPosition) return true;

  // Allow referencing the class name from methods of that class, even though
  // the initializer position for class names is only after the body.
  Scope* scope = this;
  while (scope) {
    if (scope->ClassVariableForMethod() == var) return true;
    scope = scope->outer_scope();
  }

  // Allow references from methods to classes declared later, if we detect no
  // problematic dependency cycles. Note that we can be inside multiple methods
  // at the same time, and it's enough if we find one where the reference is
  // allowed.
  if (var->is_class() &&
      var->AsClassVariable()->declaration_group_start() >= 0) {
    for (scope = this; scope && scope != var->scope();
         scope = scope->outer_scope()) {
      ClassVariable* class_var = scope->ClassVariableForMethod();
      // A method is referring to some other class, possibly declared
      // later. Referring to a class declared earlier is always OK and covered
      // by the code outside this if. Here we only need to allow special cases
      // for referring to a class which is declared later.

      // Referring to a class C declared later is OK under the following
      // circumstances:

      // 1. The class declarations are in a consecutive group with no other
      // declarations or statements in between, and

      // 2. There is no dependency cycle where the first edge is an
      // initialization time dependency (computed property name or extends
      // clause) from C to something that depends on this class directly or
      // transitively.
      if (class_var &&
          class_var->declaration_group_start() ==
              var->AsClassVariable()->declaration_group_start()) {
        return true;
      }

      // TODO(marja,rossberg): implement the dependency cycle detection. Here we
      // undershoot the target and allow referring to any class in the same
      // consectuive declaration group.

      // The cycle detection can work roughly like this: 1) detect init-time
      // references here (they are free variables which are inside the class
      // scope but not inside a method scope - no parser changes needed to
      // detect them) 2) if we encounter an init-time reference here, allow it,
      // but record it for a later dependency cycle check 3) also record
      // non-init-time references here 4) after scope analysis is done, analyse
      // the dependency cycles: an illegal cycle is one starting with an
      // init-time reference and leading back to the starting point with either
      // non-init-time and init-time references.
    }
  }

  // If both the use and the declaration are inside an eval scope (possibly
  // indirectly), or one of them is, we need to check whether they are inside
  // the same eval scope or different ones.

  // TODO(marja,rossberg): Detect errors across different evals (depends on the
  // future of eval in strong mode).
  const Scope* eval_for_use = NearestOuterEvalScope();
  const Scope* eval_for_declaration = var->scope()->NearestOuterEvalScope();

  if (proxy->position() != RelocInfo::kNoPosition &&
      proxy->position() < var->initializer_position() && !var->is_function() &&
      eval_for_use == eval_for_declaration) {
    DCHECK(proxy->end_position() != RelocInfo::kNoPosition);
    ReportMessage(proxy->position(), proxy->end_position(),
                  "strong_use_before_declaration", proxy->raw_name());
    return false;
  }
  return true;
}


ClassVariable* Scope::ClassVariableForMethod() const {
  // TODO(marja, rossberg): This fails to find a class variable in the following
  // cases:
  // let A = class { ... }
  // It needs to be investigated whether this causes any practical problems.
  if (!is_function_scope()) return nullptr;
  if (IsInObjectLiteral(function_kind_)) return nullptr;
  if (!IsConciseMethod(function_kind_) && !IsConstructor(function_kind_) &&
      !IsAccessorFunction(function_kind_)) {
    return nullptr;
  }
  DCHECK_NOT_NULL(outer_scope_);
  DCHECK(outer_scope_->is_class_scope());
  // The class scope contains at most one variable, the class name.
  DCHECK(outer_scope_->variables_.occupancy() <= 1);
  if (outer_scope_->variables_.occupancy() == 0) return nullptr;
  VariableMap::Entry* p = outer_scope_->variables_.Start();
  Variable* var = reinterpret_cast<Variable*>(p->value);
  if (!var->is_class()) return nullptr;
  return var->AsClassVariable();
}


bool Scope::ResolveVariablesRecursively(ParseInfo* info,
                                        AstNodeFactory* factory) {
  DCHECK(info->script_scope()->is_script_scope());

  // Resolve unresolved variables for this scope.
  for (int i = 0; i < unresolved_.length(); i++) {
    if (!ResolveVariable(info, unresolved_[i], factory)) return false;
  }

  // Resolve unresolved variables for inner scopes.
  for (int i = 0; i < inner_scopes_.length(); i++) {
    if (!inner_scopes_[i]->ResolveVariablesRecursively(info, factory))
      return false;
  }

  return true;
}


void Scope::PropagateScopeInfo(bool outer_scope_calls_sloppy_eval ) {
  if (outer_scope_calls_sloppy_eval) {
    outer_scope_calls_sloppy_eval_ = true;
  }

  bool calls_sloppy_eval =
      this->calls_sloppy_eval() || outer_scope_calls_sloppy_eval_;
  for (int i = 0; i < inner_scopes_.length(); i++) {
    Scope* inner = inner_scopes_[i];
    inner->PropagateScopeInfo(calls_sloppy_eval);
    if (inner->scope_calls_eval_ || inner->inner_scope_calls_eval_) {
      inner_scope_calls_eval_ = true;
    }
    // If the inner scope is an arrow function, propagate the flags tracking
    // usage of arguments/super/this, but do not propagate them out from normal
    // functions.
    if (!inner->is_function_scope() || inner->is_arrow_scope()) {
      if (inner->scope_uses_arguments_ || inner->inner_scope_uses_arguments_) {
        inner_scope_uses_arguments_ = true;
      }
      if (inner->scope_uses_super_property_ ||
          inner->inner_scope_uses_super_property_) {
        inner_scope_uses_super_property_ = true;
      }
      if (inner->scope_uses_this_ || inner->inner_scope_uses_this_) {
        inner_scope_uses_this_ = true;
      }
    }
    if (inner->force_eager_compilation_) {
      force_eager_compilation_ = true;
    }
    if (asm_module_ && inner->scope_type() == FUNCTION_SCOPE) {
      inner->asm_function_ = true;
    }
  }
}


bool Scope::MustAllocate(Variable* var) {
  // Give var a read/write use if there is a chance it might be accessed
  // via an eval() call.  This is only possible if the variable has a
  // visible name.
  if ((var->is_this() || var->is_new_target() || !var->raw_name()->IsEmpty()) &&
      (var->has_forced_context_allocation() || scope_calls_eval_ ||
       inner_scope_calls_eval_ || scope_contains_with_ || is_catch_scope() ||
       is_block_scope() || is_module_scope() || is_script_scope())) {
    var->set_is_used();
    if (scope_calls_eval_ || inner_scope_calls_eval_) var->set_maybe_assigned();
  }
  // Global variables do not need to be allocated.
  return !var->IsGlobalObjectProperty() && var->is_used();
}


bool Scope::MustAllocateInContext(Variable* var) {
  // If var is accessed from an inner scope, or if there is a possibility
  // that it might be accessed from the current or an inner scope (through
  // an eval() call or a runtime with lookup), it must be allocated in the
  // context.
  //
  // Exceptions: If the scope as a whole has forced context allocation, all
  // variables will have context allocation, even temporaries.  Otherwise
  // temporary variables are always stack-allocated.  Catch-bound variables are
  // always context-allocated.
  if (has_forced_context_allocation()) return true;
  if (var->mode() == TEMPORARY) return false;
  if (var->mode() == INTERNAL) return true;
  if (is_catch_scope() || is_module_scope()) return true;
  if (is_script_scope() && IsLexicalVariableMode(var->mode())) return true;
  return var->has_forced_context_allocation() ||
      scope_calls_eval_ ||
      inner_scope_calls_eval_ ||
      scope_contains_with_;
}


bool Scope::HasArgumentsParameter(Isolate* isolate) {
  for (int i = 0; i < params_.length(); i++) {
    if (params_[i]->name().is_identical_to(
            isolate->factory()->arguments_string())) {
      return true;
    }
  }
  return false;
}


void Scope::AllocateStackSlot(Variable* var) {
  if (is_block_scope()) {
    DeclarationScope()->AllocateStackSlot(var);
  } else {
    var->AllocateTo(Variable::LOCAL, num_stack_slots_++);
  }
}


void Scope::AllocateHeapSlot(Variable* var) {
  var->AllocateTo(Variable::CONTEXT, num_heap_slots_++);
}


void Scope::AllocateParameterLocals(Isolate* isolate) {
  DCHECK(is_function_scope());
  Variable* arguments = LookupLocal(ast_value_factory_->arguments_string());
  // Functions have 'arguments' declared implicitly in all non arrow functions.
  DCHECK(arguments != nullptr || is_arrow_scope());

  bool uses_sloppy_arguments = false;

  if (arguments != nullptr && MustAllocate(arguments) &&
      !HasArgumentsParameter(isolate)) {
    // 'arguments' is used. Unless there is also a parameter called
    // 'arguments', we must be conservative and allocate all parameters to
    // the context assuming they will be captured by the arguments object.
    // If we have a parameter named 'arguments', a (new) value is always
    // assigned to it via the function invocation. Then 'arguments' denotes
    // that specific parameter value and cannot be used to access the
    // parameters, which is why we don't need to allocate an arguments
    // object in that case.

    // We are using 'arguments'. Tell the code generator that is needs to
    // allocate the arguments object by setting 'arguments_'.
    arguments_ = arguments;

    // In strict mode 'arguments' does not alias formal parameters.
    // Therefore in strict mode we allocate parameters as if 'arguments'
    // were not used.
    uses_sloppy_arguments = is_sloppy(language_mode());
  }

  if (rest_parameter_ && !MustAllocate(rest_parameter_)) {
    rest_parameter_ = NULL;
  }

  // The same parameter may occur multiple times in the parameters_ list.
  // If it does, and if it is not copied into the context object, it must
  // receive the highest parameter index for that parameter; thus iteration
  // order is relevant!
  for (int i = params_.length() - 1; i >= 0; --i) {
    Variable* var = params_[i];
    if (var == rest_parameter_) continue;

    DCHECK(var->scope() == this);
    if (uses_sloppy_arguments || has_forced_context_allocation()) {
      // Force context allocation of the parameter.
      var->ForceContextAllocation();
    }

    if (MustAllocate(var)) {
      if (MustAllocateInContext(var)) {
        DCHECK(var->IsUnallocated() || var->IsContextSlot());
        if (var->IsUnallocated()) {
          AllocateHeapSlot(var);
        }
      } else {
        DCHECK(var->IsUnallocated() || var->IsParameter());
        if (var->IsUnallocated()) {
          var->AllocateTo(Variable::PARAMETER, i);
        }
      }
    }
  }
}


void Scope::AllocateNonParameterLocal(Isolate* isolate, Variable* var) {
  DCHECK(var->scope() == this);
  DCHECK(!var->IsVariable(isolate->factory()->dot_result_string()) ||
         !var->IsStackLocal());
  if (var->IsUnallocated() && MustAllocate(var)) {
    if (MustAllocateInContext(var)) {
      AllocateHeapSlot(var);
    } else {
      AllocateStackSlot(var);
    }
  }
}


void Scope::AllocateNonParameterLocals(Isolate* isolate) {
  // All variables that have no rewrite yet are non-parameter locals.
  for (int i = 0; i < temps_.length(); i++) {
    AllocateNonParameterLocal(isolate, temps_[i]);
  }

  for (int i = 0; i < internals_.length(); i++) {
    AllocateNonParameterLocal(isolate, internals_[i]);
  }

  ZoneList<VarAndOrder> vars(variables_.occupancy(), zone());
  for (VariableMap::Entry* p = variables_.Start();
       p != NULL;
       p = variables_.Next(p)) {
    Variable* var = reinterpret_cast<Variable*>(p->value);
    vars.Add(VarAndOrder(var, p->order), zone());
  }
  vars.Sort(VarAndOrder::Compare);
  int var_count = vars.length();
  for (int i = 0; i < var_count; i++) {
    AllocateNonParameterLocal(isolate, vars[i].var());
  }

  // For now, function_ must be allocated at the very end.  If it gets
  // allocated in the context, it must be the last slot in the context,
  // because of the current ScopeInfo implementation (see
  // ScopeInfo::ScopeInfo(FunctionScope* scope) constructor).
  if (function_ != NULL) {
    AllocateNonParameterLocal(isolate, function_->proxy()->var());
  }

  if (rest_parameter_) {
    AllocateNonParameterLocal(isolate, rest_parameter_);
  }
}


void Scope::AllocateVariablesRecursively(Isolate* isolate) {
  if (!already_resolved()) {
    num_stack_slots_ = 0;
  }
  // Allocate variables for inner scopes.
  for (int i = 0; i < inner_scopes_.length(); i++) {
    inner_scopes_[i]->AllocateVariablesRecursively(isolate);
  }

  // If scope is already resolved, we still need to allocate
  // variables in inner scopes which might not had been resolved yet.
  if (already_resolved()) return;
  // The number of slots required for variables.
  num_heap_slots_ = Context::MIN_CONTEXT_SLOTS;

  // Allocate variables for this scope.
  // Parameters must be allocated first, if any.
  if (is_function_scope()) AllocateParameterLocals(isolate);
  AllocateNonParameterLocals(isolate);

  // Force allocation of a context for this scope if necessary. For a 'with'
  // scope and for a function scope that makes an 'eval' call we need a context,
  // even if no local variables were statically allocated in the scope.
  // Likewise for modules.
  bool must_have_context = is_with_scope() || is_module_scope() ||
                           (is_function_scope() && calls_sloppy_eval());

  // If we didn't allocate any locals in the local context, then we only
  // need the minimal number of slots if we must have a context.
  if (num_heap_slots_ == Context::MIN_CONTEXT_SLOTS && !must_have_context) {
    num_heap_slots_ = 0;
  }

  // Allocation done.
  DCHECK(num_heap_slots_ == 0 || num_heap_slots_ >= Context::MIN_CONTEXT_SLOTS);
}


void Scope::AllocateModules() {
  DCHECK(is_script_scope());
  DCHECK(!already_resolved());
  for (int i = 0; i < inner_scopes_.length(); i++) {
    Scope* scope = inner_scopes_.at(i);
    if (scope->is_module_scope()) {
      DCHECK(!scope->already_resolved());
      DCHECK(scope->module_descriptor_->IsFrozen());
      DCHECK_NULL(scope->module_var_);
      scope->module_var_ = NewInternal(ast_value_factory_->dot_module_string());
      ++num_modules_;
    }
  }
}


int Scope::StackLocalCount() const {
  return num_stack_slots() -
      (function_ != NULL && function_->proxy()->var()->IsStackLocal() ? 1 : 0);
}


int Scope::ContextLocalCount() const {
  if (num_heap_slots() == 0) return 0;
  return num_heap_slots() - Context::MIN_CONTEXT_SLOTS -
      (function_ != NULL && function_->proxy()->var()->IsContextSlot() ? 1 : 0);
}
} }  // namespace v8::internal
