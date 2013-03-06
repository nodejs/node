// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "scopes.h"

#include "accessors.h"
#include "bootstrapper.h"
#include "compiler.h"
#include "messages.h"
#include "scopeinfo.h"

#include "allocation-inl.h"

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

static bool Match(void* key1, void* key2) {
  String* name1 = *reinterpret_cast<String**>(key1);
  String* name2 = *reinterpret_cast<String**>(key2);
  ASSERT(name1->IsSymbol());
  ASSERT(name2->IsSymbol());
  return name1 == name2;
}


VariableMap::VariableMap(Zone* zone)
    : ZoneHashMap(Match, 8, ZoneAllocationPolicy(zone)),
      zone_(zone) {}
VariableMap::~VariableMap() {}


Variable* VariableMap::Declare(
    Scope* scope,
    Handle<String> name,
    VariableMode mode,
    bool is_valid_lhs,
    Variable::Kind kind,
    InitializationFlag initialization_flag,
    Interface* interface) {
  Entry* p = ZoneHashMap::Lookup(name.location(), name->Hash(), true,
                                 ZoneAllocationPolicy(zone()));
  if (p->value == NULL) {
    // The variable has not been declared yet -> insert it.
    ASSERT(p->key == name.location());
    p->value = new(zone()) Variable(scope,
                                    name,
                                    mode,
                                    is_valid_lhs,
                                    kind,
                                    initialization_flag,
                                    interface);
  }
  return reinterpret_cast<Variable*>(p->value);
}


Variable* VariableMap::Lookup(Handle<String> name) {
  Entry* p = ZoneHashMap::Lookup(name.location(), name->Hash(), false,
                                 ZoneAllocationPolicy(NULL));
  if (p != NULL) {
    ASSERT(*reinterpret_cast<String**>(p->key) == *name);
    ASSERT(p->value != NULL);
    return reinterpret_cast<Variable*>(p->value);
  }
  return NULL;
}


// ----------------------------------------------------------------------------
// Implementation of Scope

Scope::Scope(Scope* outer_scope, ScopeType type, Zone* zone)
    : isolate_(Isolate::Current()),
      inner_scopes_(4, zone),
      variables_(zone),
      temps_(4, zone),
      params_(4, zone),
      unresolved_(16, zone),
      decls_(4, zone),
      interface_(FLAG_harmony_modules &&
                 (type == MODULE_SCOPE || type == GLOBAL_SCOPE)
                     ? Interface::NewModule(zone) : NULL),
      already_resolved_(false),
      zone_(zone) {
  SetDefaults(type, outer_scope, Handle<ScopeInfo>::null());
  // The outermost scope must be a global scope.
  ASSERT(type == GLOBAL_SCOPE || outer_scope != NULL);
  ASSERT(!HasIllegalRedeclaration());
}


Scope::Scope(Scope* inner_scope,
             ScopeType type,
             Handle<ScopeInfo> scope_info,
             Zone* zone)
    : isolate_(Isolate::Current()),
      inner_scopes_(4, zone),
      variables_(zone),
      temps_(4, zone),
      params_(4, zone),
      unresolved_(16, zone),
      decls_(4, zone),
      interface_(NULL),
      already_resolved_(true),
      zone_(zone) {
  SetDefaults(type, NULL, scope_info);
  if (!scope_info.is_null()) {
    num_heap_slots_ = scope_info_->ContextLength();
  }
  // Ensure at least MIN_CONTEXT_SLOTS to indicate a materialized context.
  num_heap_slots_ = Max(num_heap_slots_,
                        static_cast<int>(Context::MIN_CONTEXT_SLOTS));
  AddInnerScope(inner_scope);
}


Scope::Scope(Scope* inner_scope, Handle<String> catch_variable_name, Zone* zone)
    : isolate_(Isolate::Current()),
      inner_scopes_(1, zone),
      variables_(zone),
      temps_(0, zone),
      params_(0, zone),
      unresolved_(0, zone),
      decls_(0, zone),
      interface_(NULL),
      already_resolved_(true),
      zone_(zone) {
  SetDefaults(CATCH_SCOPE, NULL, Handle<ScopeInfo>::null());
  AddInnerScope(inner_scope);
  ++num_var_or_const_;
  num_heap_slots_ = Context::MIN_CONTEXT_SLOTS;
  Variable* variable = variables_.Declare(this,
                                          catch_variable_name,
                                          VAR,
                                          true,  // Valid left-hand side.
                                          Variable::NORMAL,
                                          kCreatedInitialized);
  AllocateHeapSlot(variable);
}


void Scope::SetDefaults(ScopeType type,
                        Scope* outer_scope,
                        Handle<ScopeInfo> scope_info) {
  outer_scope_ = outer_scope;
  type_ = type;
  scope_name_ = isolate_->factory()->empty_symbol();
  dynamics_ = NULL;
  receiver_ = NULL;
  function_ = NULL;
  arguments_ = NULL;
  illegal_redecl_ = NULL;
  scope_inside_with_ = false;
  scope_contains_with_ = false;
  scope_calls_eval_ = false;
  // Inherit the strict mode from the parent scope.
  language_mode_ = (outer_scope != NULL)
      ? outer_scope->language_mode_ : CLASSIC_MODE;
  outer_scope_calls_non_strict_eval_ = false;
  inner_scope_calls_eval_ = false;
  force_eager_compilation_ = false;
  num_var_or_const_ = 0;
  num_stack_slots_ = 0;
  num_heap_slots_ = 0;
  scope_info_ = scope_info;
  start_position_ = RelocInfo::kNoPosition;
  end_position_ = RelocInfo::kNoPosition;
  if (!scope_info.is_null()) {
    scope_calls_eval_ = scope_info->CallsEval();
    language_mode_ = scope_info->language_mode();
  }
}


Scope* Scope::DeserializeScopeChain(Context* context, Scope* global_scope,
                                    Zone* zone) {
  // Reconstruct the outer scope chain from a closure's context chain.
  Scope* current_scope = NULL;
  Scope* innermost_scope = NULL;
  bool contains_with = false;
  while (!context->IsNativeContext()) {
    if (context->IsWithContext()) {
      Scope* with_scope = new(zone) Scope(current_scope,
                                          WITH_SCOPE,
                                          Handle<ScopeInfo>::null(),
                                          zone);
      current_scope = with_scope;
      // All the inner scopes are inside a with.
      contains_with = true;
      for (Scope* s = innermost_scope; s != NULL; s = s->outer_scope()) {
        s->scope_inside_with_ = true;
      }
    } else if (context->IsGlobalContext()) {
      ScopeInfo* scope_info = ScopeInfo::cast(context->extension());
      current_scope = new(zone) Scope(current_scope,
                                      GLOBAL_SCOPE,
                                      Handle<ScopeInfo>(scope_info),
                                      zone);
    } else if (context->IsModuleContext()) {
      ScopeInfo* scope_info = ScopeInfo::cast(context->module()->scope_info());
      current_scope = new(zone) Scope(current_scope,
                                      MODULE_SCOPE,
                                      Handle<ScopeInfo>(scope_info),
                                      zone);
    } else if (context->IsFunctionContext()) {
      ScopeInfo* scope_info = context->closure()->shared()->scope_info();
      current_scope = new(zone) Scope(current_scope,
                                      FUNCTION_SCOPE,
                                      Handle<ScopeInfo>(scope_info),
                                      zone);
    } else if (context->IsBlockContext()) {
      ScopeInfo* scope_info = ScopeInfo::cast(context->extension());
      current_scope = new(zone) Scope(current_scope,
                                      BLOCK_SCOPE,
                                      Handle<ScopeInfo>(scope_info),
                                      zone);
    } else {
      ASSERT(context->IsCatchContext());
      String* name = String::cast(context->extension());
      current_scope = new(zone) Scope(
          current_scope, Handle<String>(name), zone);
    }
    if (contains_with) current_scope->RecordWithStatement();
    if (innermost_scope == NULL) innermost_scope = current_scope;

    // Forget about a with when we move to a context for a different function.
    if (context->previous()->closure() != context->closure()) {
      contains_with = false;
    }
    context = context->previous();
  }

  global_scope->AddInnerScope(current_scope);
  global_scope->PropagateScopeInfo(false);
  return (innermost_scope == NULL) ? global_scope : innermost_scope;
}


bool Scope::Analyze(CompilationInfo* info) {
  ASSERT(info->function() != NULL);
  Scope* scope = info->function()->scope();
  Scope* top = scope;

  // Traverse the scope tree up to the first unresolved scope or the global
  // scope and start scope resolution and variable allocation from that scope.
  while (!top->is_global_scope() &&
         !top->outer_scope()->already_resolved()) {
    top = top->outer_scope();
  }

  // Allocate the variables.
  {
    AstNodeFactory<AstNullVisitor> ast_node_factory(info->isolate(),
                                                    info->zone());
    if (!top->AllocateVariables(info, &ast_node_factory)) return false;
  }

#ifdef DEBUG
  if (info->isolate()->bootstrapper()->IsActive()
          ? FLAG_print_builtin_scopes
          : FLAG_print_scopes) {
    scope->Print();
  }

  if (FLAG_harmony_modules && FLAG_print_interfaces && top->is_global_scope()) {
    PrintF("global : ");
    top->interface()->Print();
  }
#endif

  if (FLAG_harmony_scoping) {
    VariableProxy* proxy = scope->CheckAssignmentToConst();
    if (proxy != NULL) {
      // Found an assignment to const. Throw a syntax error.
      MessageLocation location(info->script(),
                               proxy->position(),
                               proxy->position());
      Isolate* isolate = info->isolate();
      Factory* factory = isolate->factory();
      Handle<JSArray> array = factory->NewJSArray(0);
      Handle<Object> result =
          factory->NewSyntaxError("harmony_const_assign", array);
      isolate->Throw(*result, &location);
      return false;
    }
  }

  info->SetScope(scope);
  return true;
}


void Scope::Initialize() {
  ASSERT(!already_resolved());

  // Add this scope as a new inner scope of the outer scope.
  if (outer_scope_ != NULL) {
    outer_scope_->inner_scopes_.Add(this, zone());
    scope_inside_with_ = outer_scope_->scope_inside_with_ || is_with_scope();
  } else {
    scope_inside_with_ = is_with_scope();
  }

  // Declare convenience variables.
  // Declare and allocate receiver (even for the global scope, and even
  // if naccesses_ == 0).
  // NOTE: When loading parameters in the global scope, we must take
  // care not to access them as properties of the global object, but
  // instead load them directly from the stack. Currently, the only
  // such parameter is 'this' which is passed on the stack when
  // invoking scripts
  if (is_declaration_scope()) {
    Variable* var =
        variables_.Declare(this,
                           isolate_->factory()->this_symbol(),
                           VAR,
                           false,
                           Variable::THIS,
                           kCreatedInitialized);
    var->AllocateTo(Variable::PARAMETER, -1);
    receiver_ = var;
  } else {
    ASSERT(outer_scope() != NULL);
    receiver_ = outer_scope()->receiver();
  }

  if (is_function_scope()) {
    // Declare 'arguments' variable which exists in all functions.
    // Note that it might never be accessed, in which case it won't be
    // allocated during variable allocation.
    variables_.Declare(this,
                       isolate_->factory()->arguments_symbol(),
                       VAR,
                       true,
                       Variable::ARGUMENTS,
                       kCreatedInitialized);
  }
}


Scope* Scope::FinalizeBlockScope() {
  ASSERT(is_block_scope());
  ASSERT(temps_.is_empty());
  ASSERT(params_.is_empty());

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

  return NULL;
}


Variable* Scope::LocalLookup(Handle<String> name) {
  Variable* result = variables_.Lookup(name);
  if (result != NULL || scope_info_.is_null()) {
    return result;
  }
  // If we have a serialized scope info, we might find the variable there.
  // There should be no local slot with the given name.
  ASSERT(scope_info_->StackSlotIndex(*name) < 0);

  // Check context slot lookup.
  VariableMode mode;
  Variable::Location location = Variable::CONTEXT;
  InitializationFlag init_flag;
  int index = scope_info_->ContextSlotIndex(*name, &mode, &init_flag);
  if (index < 0) {
    // Check parameters.
    index = scope_info_->ParameterIndex(*name);
    if (index < 0) return NULL;

    mode = DYNAMIC;
    location = Variable::LOOKUP;
    init_flag = kCreatedInitialized;
  }

  Variable* var = variables_.Declare(this, name, mode, true, Variable::NORMAL,
                                     init_flag);
  var->AllocateTo(location, index);
  return var;
}


Variable* Scope::LookupFunctionVar(Handle<String> name,
                                   AstNodeFactory<AstNullVisitor>* factory) {
  if (function_ != NULL && function_->proxy()->name().is_identical_to(name)) {
    return function_->proxy()->var();
  } else if (!scope_info_.is_null()) {
    // If we are backed by a scope info, try to lookup the variable there.
    VariableMode mode;
    int index = scope_info_->FunctionContextSlotIndex(*name, &mode);
    if (index < 0) return NULL;
    Variable* var = new(zone()) Variable(
        this, name, mode, true /* is valid LHS */,
        Variable::NORMAL, kCreatedInitialized);
    VariableProxy* proxy = factory->NewVariableProxy(var);
    VariableDeclaration* declaration =
        factory->NewVariableDeclaration(proxy, mode, this);
    DeclareFunctionVar(declaration);
    var->AllocateTo(Variable::CONTEXT, index);
    return var;
  } else {
    return NULL;
  }
}


Variable* Scope::Lookup(Handle<String> name) {
  for (Scope* scope = this;
       scope != NULL;
       scope = scope->outer_scope()) {
    Variable* var = scope->LocalLookup(name);
    if (var != NULL) return var;
  }
  return NULL;
}


void Scope::DeclareParameter(Handle<String> name, VariableMode mode) {
  ASSERT(!already_resolved());
  ASSERT(is_function_scope());
  Variable* var = variables_.Declare(this, name, mode, true, Variable::NORMAL,
                                     kCreatedInitialized);
  params_.Add(var, zone());
}


Variable* Scope::DeclareLocal(Handle<String> name,
                              VariableMode mode,
                              InitializationFlag init_flag,
                              Interface* interface) {
  ASSERT(!already_resolved());
  // This function handles VAR and CONST modes.  DYNAMIC variables are
  // introduces during variable allocation, INTERNAL variables are allocated
  // explicitly, and TEMPORARY variables are allocated via NewTemporary().
  ASSERT(IsDeclaredVariableMode(mode));
  ++num_var_or_const_;
  return variables_.Declare(
      this, name, mode, true, Variable::NORMAL, init_flag, interface);
}


Variable* Scope::DeclareDynamicGlobal(Handle<String> name) {
  ASSERT(is_global_scope());
  return variables_.Declare(this,
                            name,
                            DYNAMIC_GLOBAL,
                            true,
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


Variable* Scope::NewTemporary(Handle<String> name) {
  ASSERT(!already_resolved());
  Variable* var = new(zone()) Variable(this,
                                       name,
                                       TEMPORARY,
                                       true,
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
  ASSERT(HasIllegalRedeclaration());
}


void Scope::VisitIllegalRedeclaration(AstVisitor* visitor) {
  ASSERT(HasIllegalRedeclaration());
  illegal_redecl_->Accept(visitor);
}


Declaration* Scope::CheckConflictingVarDeclarations() {
  int length = decls_.length();
  for (int i = 0; i < length; i++) {
    Declaration* decl = decls_[i];
    if (decl->mode() != VAR) continue;
    Handle<String> name = decl->proxy()->name();

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


VariableProxy* Scope::CheckAssignmentToConst() {
  // Check this scope.
  if (is_extended_mode()) {
    for (int i = 0; i < unresolved_.length(); i++) {
      ASSERT(unresolved_[i]->var() != NULL);
      if (unresolved_[i]->var()->is_const_mode() &&
          unresolved_[i]->IsLValue()) {
        return unresolved_[i];
      }
    }
  }

  // Check inner scopes.
  for (int i = 0; i < inner_scopes_.length(); i++) {
    VariableProxy* proxy = inner_scopes_[i]->CheckAssignmentToConst();
    if (proxy != NULL) return proxy;
  }

  // No assignments to const found.
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


void Scope::CollectStackAndContextLocals(ZoneList<Variable*>* stack_locals,
                                         ZoneList<Variable*>* context_locals) {
  ASSERT(stack_locals != NULL);
  ASSERT(context_locals != NULL);

  // Collect temporaries which are always allocated on the stack.
  for (int i = 0; i < temps_.length(); i++) {
    Variable* var = temps_[i];
    if (var->is_used()) {
      ASSERT(var->IsStackLocal());
      stack_locals->Add(var, zone());
    }
  }

  ZoneList<VarAndOrder> vars(variables_.occupancy(), zone());

  // Collect declared local variables.
  for (VariableMap::Entry* p = variables_.Start();
       p != NULL;
       p = variables_.Next(p)) {
    Variable* var = reinterpret_cast<Variable*>(p->value);
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


bool Scope::AllocateVariables(CompilationInfo* info,
                              AstNodeFactory<AstNullVisitor>* factory) {
  // 1) Propagate scope information.
  bool outer_scope_calls_non_strict_eval = false;
  if (outer_scope_ != NULL) {
    outer_scope_calls_non_strict_eval =
        outer_scope_->outer_scope_calls_non_strict_eval() |
        outer_scope_->calls_non_strict_eval();
  }
  PropagateScopeInfo(outer_scope_calls_non_strict_eval);

  // 2) Resolve variables.
  if (!ResolveVariablesRecursively(info, factory)) return false;

  // 3) Allocate variables.
  AllocateVariablesRecursively();

  // 4) Allocate and link module instance objects.
  if (FLAG_harmony_modules && (is_global_scope() || is_module_scope())) {
    AllocateModules(info);
    LinkModules(info);
  }

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
    ASSERT(s != NULL);  // scope must be in the scope chain
    if (s->num_heap_slots() > 0) n++;
  }
  return n;
}


Scope* Scope::DeclarationScope() {
  Scope* scope = this;
  while (!scope->is_declaration_scope()) {
    scope = scope->outer_scope();
  }
  return scope;
}


Handle<ScopeInfo> Scope::GetScopeInfo() {
  if (scope_info_.is_null()) {
    scope_info_ = ScopeInfo::Create(this, zone());
  }
  return scope_info_;
}


void Scope::GetNestedScopeChain(
    List<Handle<ScopeInfo> >* chain,
    int position) {
  if (!is_eval_scope()) chain->Add(Handle<ScopeInfo>(GetScopeInfo()));

  for (int i = 0; i < inner_scopes_.length(); i++) {
    Scope* scope = inner_scopes_[i];
    int beg_pos = scope->start_position();
    int end_pos = scope->end_position();
    ASSERT(beg_pos >= 0 && end_pos >= 0);
    if (beg_pos <= position && position < end_pos) {
      scope->GetNestedScopeChain(chain, position);
      return;
    }
  }
}


#ifdef DEBUG
static const char* Header(ScopeType type) {
  switch (type) {
    case EVAL_SCOPE: return "eval";
    case FUNCTION_SCOPE: return "function";
    case MODULE_SCOPE: return "module";
    case GLOBAL_SCOPE: return "global";
    case CATCH_SCOPE: return "catch";
    case BLOCK_SCOPE: return "block";
    case WITH_SCOPE: return "with";
  }
  UNREACHABLE();
  return NULL;
}


static void Indent(int n, const char* str) {
  PrintF("%*s%s", n, "", str);
}


static void PrintName(Handle<String> name) {
  SmartArrayPointer<char> s = name->ToCString(DISALLOW_NULLS);
  PrintF("%s", *s);
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
    PrintName(var->name());
    PrintF(";  // ");
    PrintLocation(var);
    if (var->has_forced_context_allocation()) {
      if (!var->IsUnallocated()) PrintF(", ");
      PrintF("forced context allocation");
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
  Indent(n0, Header(type_));
  if (scope_name_->length() > 0) {
    PrintF(" ");
    PrintName(scope_name_);
  }

  // Print parameters, if any.
  if (is_function_scope()) {
    PrintF(" (");
    for (int i = 0; i < params_.length(); i++) {
      if (i > 0) PrintF(", ");
      PrintName(params_[i]->name());
    }
    PrintF(")");
  }

  PrintF(" { // (%d, %d)\n", start_position(), end_position());

  // Function name, if any (named function literals, only).
  if (function_ != NULL) {
    Indent(n1, "// (local) function name: ");
    PrintName(function_->proxy()->name());
    PrintF("\n");
  }

  // Scope info.
  if (HasTrivialOuterContext()) {
    Indent(n1, "// scope has trivial outer context\n");
  }
  switch (language_mode()) {
    case CLASSIC_MODE:
      break;
    case STRICT_MODE:
      Indent(n1, "// strict mode scope\n");
      break;
    case EXTENDED_MODE:
      Indent(n1, "// extended mode scope\n");
      break;
  }
  if (scope_inside_with_) Indent(n1, "// scope inside 'with'\n");
  if (scope_contains_with_) Indent(n1, "// scope contains 'with'\n");
  if (scope_calls_eval_) Indent(n1, "// scope calls 'eval'\n");
  if (outer_scope_calls_non_strict_eval_) {
    Indent(n1, "// outer scope calls 'eval' in non-strict context\n");
  }
  if (inner_scope_calls_eval_) Indent(n1, "// inner scope calls 'eval'\n");
  if (num_stack_slots_ > 0) { Indent(n1, "// ");
  PrintF("%d stack slots\n", num_stack_slots_); }
  if (num_heap_slots_ > 0) { Indent(n1, "// ");
  PrintF("%d heap slots\n", num_heap_slots_); }

  // Print locals.
  Indent(n1, "// function var\n");
  if (function_ != NULL) {
    PrintVar(n1, function_->proxy()->var());
  }

  Indent(n1, "// temporary vars\n");
  for (int i = 0; i < temps_.length(); i++) {
    PrintVar(n1, temps_[i]);
  }

  Indent(n1, "// local vars\n");
  PrintMap(n1, &variables_);

  Indent(n1, "// dynamic vars\n");
  if (dynamics_ != NULL) {
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


Variable* Scope::NonLocal(Handle<String> name, VariableMode mode) {
  if (dynamics_ == NULL) dynamics_ = new(zone()) DynamicScopePart(zone());
  VariableMap* map = dynamics_->GetMap(mode);
  Variable* var = map->Lookup(name);
  if (var == NULL) {
    // Declare a new non-local.
    InitializationFlag init_flag = (mode == VAR)
        ? kCreatedInitialized : kNeedsInitialization;
    var = map->Declare(NULL,
                       name,
                       mode,
                       true,
                       Variable::NORMAL,
                       init_flag);
    // Allocate it by giving it a dynamic lookup.
    var->AllocateTo(Variable::LOOKUP, -1);
  }
  return var;
}


Variable* Scope::LookupRecursive(Handle<String> name,
                                 BindingKind* binding_kind,
                                 AstNodeFactory<AstNullVisitor>* factory) {
  ASSERT(binding_kind != NULL);
  // Try to find the variable in this scope.
  Variable* var = LocalLookup(name);

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
  var = LookupFunctionVar(name, factory);
  if (var != NULL) {
    *binding_kind = BOUND;
  } else if (outer_scope_ != NULL) {
    var = outer_scope_->LookupRecursive(name, binding_kind, factory);
    if (*binding_kind == BOUND && (is_function_scope() || is_with_scope())) {
      var->ForceContextAllocation();
    }
  } else {
    ASSERT(is_global_scope());
  }

  if (is_with_scope()) {
    // The current scope is a with scope, so the variable binding can not be
    // statically resolved. However, note that it was necessary to do a lookup
    // in the outer scope anyway, because if a binding exists in an outer scope,
    // the associated variable has to be marked as potentially being accessed
    // from inside of an inner with scope (the property may not be in the 'with'
    // object).
    *binding_kind = DYNAMIC_LOOKUP;
    return NULL;
  } else if (calls_non_strict_eval()) {
    // A variable binding may have been found in an outer scope, but the current
    // scope makes a non-strict 'eval' call, so the found variable may not be
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


bool Scope::ResolveVariable(CompilationInfo* info,
                            VariableProxy* proxy,
                            AstNodeFactory<AstNullVisitor>* factory) {
  ASSERT(info->global_scope()->is_global_scope());

  // If the proxy is already resolved there's nothing to do
  // (functions and consts may be resolved by the parser).
  if (proxy->var() != NULL) return true;

  // Otherwise, try to resolve the variable.
  BindingKind binding_kind;
  Variable* var = LookupRecursive(proxy->name(), &binding_kind, factory);
  switch (binding_kind) {
    case BOUND:
      // We found a variable binding.
      break;

    case BOUND_EVAL_SHADOWED:
      // We either found a variable binding that might be shadowed by eval  or
      // gave up on it (e.g. by encountering a local with the same in the outer
      // scope which was not promoted to a context, this can happen if we use
      // debugger to evaluate arbitrary expressions at a break point).
      if (var->IsGlobalObjectProperty()) {
        var = NonLocal(proxy->name(), DYNAMIC_GLOBAL);
      } else if (var->is_dynamic()) {
        var = NonLocal(proxy->name(), DYNAMIC);
      } else {
        Variable* invalidated = var;
        var = NonLocal(proxy->name(), DYNAMIC_LOCAL);
        var->set_local_if_not_shadowed(invalidated);
      }
      break;

    case UNBOUND:
      // No binding has been found. Declare a variable on the global object.
      var = info->global_scope()->DeclareDynamicGlobal(proxy->name());
      break;

    case UNBOUND_EVAL_SHADOWED:
      // No binding has been found. But some scope makes a
      // non-strict 'eval' call.
      var = NonLocal(proxy->name(), DYNAMIC_GLOBAL);
      break;

    case DYNAMIC_LOOKUP:
      // The variable could not be resolved statically.
      var = NonLocal(proxy->name(), DYNAMIC);
      break;
  }

  ASSERT(var != NULL);
  proxy->BindTo(var);

  if (FLAG_harmony_modules) {
    bool ok;
#ifdef DEBUG
    if (FLAG_print_interface_details)
      PrintF("# Resolve %s:\n", var->name()->ToAsciiArray());
#endif
    proxy->interface()->Unify(var->interface(), zone(), &ok);
    if (!ok) {
#ifdef DEBUG
      if (FLAG_print_interfaces) {
        PrintF("SCOPES TYPE ERROR\n");
        PrintF("proxy: ");
        proxy->interface()->Print();
        PrintF("var: ");
        var->interface()->Print();
      }
#endif

      // Inconsistent use of module. Throw a syntax error.
      // TODO(rossberg): generate more helpful error message.
      MessageLocation location(info->script(),
                               proxy->position(),
                               proxy->position());
      Isolate* isolate = Isolate::Current();
      Factory* factory = isolate->factory();
      Handle<JSArray> array = factory->NewJSArray(1);
      USE(JSObject::SetElement(array, 0, var->name(), NONE, kStrictMode));
      Handle<Object> result =
          factory->NewSyntaxError("module_type_error", array);
      isolate->Throw(*result, &location);
      return false;
    }
  }

  return true;
}


bool Scope::ResolveVariablesRecursively(
    CompilationInfo* info,
    AstNodeFactory<AstNullVisitor>* factory) {
  ASSERT(info->global_scope()->is_global_scope());

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


bool Scope::PropagateScopeInfo(bool outer_scope_calls_non_strict_eval ) {
  if (outer_scope_calls_non_strict_eval) {
    outer_scope_calls_non_strict_eval_ = true;
  }

  bool calls_non_strict_eval =
      this->calls_non_strict_eval() || outer_scope_calls_non_strict_eval_;
  for (int i = 0; i < inner_scopes_.length(); i++) {
    Scope* inner_scope = inner_scopes_[i];
    if (inner_scope->PropagateScopeInfo(calls_non_strict_eval)) {
      inner_scope_calls_eval_ = true;
    }
    if (inner_scope->force_eager_compilation_) {
      force_eager_compilation_ = true;
    }
  }

  return scope_calls_eval_ || inner_scope_calls_eval_;
}


bool Scope::MustAllocate(Variable* var) {
  // Give var a read/write use if there is a chance it might be accessed
  // via an eval() call.  This is only possible if the variable has a
  // visible name.
  if ((var->is_this() || var->name()->length() > 0) &&
      (var->has_forced_context_allocation() ||
       scope_calls_eval_ ||
       inner_scope_calls_eval_ ||
       scope_contains_with_ ||
       is_catch_scope() ||
       is_block_scope() ||
       is_module_scope() ||
       is_global_scope())) {
    var->set_is_used(true);
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
  // Exceptions: temporary variables are never allocated in a context;
  // catch-bound variables are always allocated in a context.
  if (var->mode() == TEMPORARY) return false;
  if (is_catch_scope() || is_block_scope() || is_module_scope()) return true;
  if (is_global_scope() && IsLexicalVariableMode(var->mode())) return true;
  return var->has_forced_context_allocation() ||
      scope_calls_eval_ ||
      inner_scope_calls_eval_ ||
      scope_contains_with_;
}


bool Scope::HasArgumentsParameter() {
  for (int i = 0; i < params_.length(); i++) {
    if (params_[i]->name().is_identical_to(
            isolate_->factory()->arguments_symbol())) {
      return true;
    }
  }
  return false;
}


void Scope::AllocateStackSlot(Variable* var) {
  var->AllocateTo(Variable::LOCAL, num_stack_slots_++);
}


void Scope::AllocateHeapSlot(Variable* var) {
  var->AllocateTo(Variable::CONTEXT, num_heap_slots_++);
}


void Scope::AllocateParameterLocals() {
  ASSERT(is_function_scope());
  Variable* arguments = LocalLookup(isolate_->factory()->arguments_symbol());
  ASSERT(arguments != NULL);  // functions have 'arguments' declared implicitly

  bool uses_nonstrict_arguments = false;

  if (MustAllocate(arguments) && !HasArgumentsParameter()) {
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
    uses_nonstrict_arguments = is_classic_mode();
  }

  // The same parameter may occur multiple times in the parameters_ list.
  // If it does, and if it is not copied into the context object, it must
  // receive the highest parameter index for that parameter; thus iteration
  // order is relevant!
  for (int i = params_.length() - 1; i >= 0; --i) {
    Variable* var = params_[i];
    ASSERT(var->scope() == this);
    if (uses_nonstrict_arguments) {
      // Force context allocation of the parameter.
      var->ForceContextAllocation();
    }

    if (MustAllocate(var)) {
      if (MustAllocateInContext(var)) {
        ASSERT(var->IsUnallocated() || var->IsContextSlot());
        if (var->IsUnallocated()) {
          AllocateHeapSlot(var);
        }
      } else {
        ASSERT(var->IsUnallocated() || var->IsParameter());
        if (var->IsUnallocated()) {
          var->AllocateTo(Variable::PARAMETER, i);
        }
      }
    }
  }
}


void Scope::AllocateNonParameterLocal(Variable* var) {
  ASSERT(var->scope() == this);
  ASSERT(!var->IsVariable(isolate_->factory()->result_symbol()) ||
         !var->IsStackLocal());
  if (var->IsUnallocated() && MustAllocate(var)) {
    if (MustAllocateInContext(var)) {
      AllocateHeapSlot(var);
    } else {
      AllocateStackSlot(var);
    }
  }
}


void Scope::AllocateNonParameterLocals() {
  // All variables that have no rewrite yet are non-parameter locals.
  for (int i = 0; i < temps_.length(); i++) {
    AllocateNonParameterLocal(temps_[i]);
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
    AllocateNonParameterLocal(vars[i].var());
  }

  // For now, function_ must be allocated at the very end.  If it gets
  // allocated in the context, it must be the last slot in the context,
  // because of the current ScopeInfo implementation (see
  // ScopeInfo::ScopeInfo(FunctionScope* scope) constructor).
  if (function_ != NULL) {
    AllocateNonParameterLocal(function_->proxy()->var());
  }
}


void Scope::AllocateVariablesRecursively() {
  // Allocate variables for inner scopes.
  for (int i = 0; i < inner_scopes_.length(); i++) {
    inner_scopes_[i]->AllocateVariablesRecursively();
  }

  // If scope is already resolved, we still need to allocate
  // variables in inner scopes which might not had been resolved yet.
  if (already_resolved()) return;
  // The number of slots required for variables.
  num_stack_slots_ = 0;
  num_heap_slots_ = Context::MIN_CONTEXT_SLOTS;

  // Allocate variables for this scope.
  // Parameters must be allocated first, if any.
  if (is_function_scope()) AllocateParameterLocals();
  AllocateNonParameterLocals();

  // Force allocation of a context for this scope if necessary. For a 'with'
  // scope and for a function scope that makes an 'eval' call we need a context,
  // even if no local variables were statically allocated in the scope.
  // Likewise for modules.
  bool must_have_context = is_with_scope() || is_module_scope() ||
      (is_function_scope() && calls_eval());

  // If we didn't allocate any locals in the local context, then we only
  // need the minimal number of slots if we must have a context.
  if (num_heap_slots_ == Context::MIN_CONTEXT_SLOTS && !must_have_context) {
    num_heap_slots_ = 0;
  }

  // Allocation done.
  ASSERT(num_heap_slots_ == 0 || num_heap_slots_ >= Context::MIN_CONTEXT_SLOTS);
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


void Scope::AllocateModules(CompilationInfo* info) {
  ASSERT(is_global_scope() || is_module_scope());

  if (is_module_scope()) {
    ASSERT(interface_->IsFrozen());
    ASSERT(scope_info_.is_null());

    // TODO(rossberg): This has to be the initial compilation of this code.
    // We currently do not allow recompiling any module definitions.
    Handle<ScopeInfo> scope_info = GetScopeInfo();
    Factory* factory = info->isolate()->factory();
    Handle<Context> context = factory->NewModuleContext(scope_info);
    Handle<JSModule> instance = factory->NewJSModule(context, scope_info);
    context->set_module(*instance);

    bool ok;
    interface_->MakeSingleton(instance, &ok);
    ASSERT(ok);
  }

  // Allocate nested modules.
  for (int i = 0; i < inner_scopes_.length(); i++) {
    Scope* inner_scope = inner_scopes_.at(i);
    if (inner_scope->is_module_scope()) {
      inner_scope->AllocateModules(info);
    }
  }
}


void Scope::LinkModules(CompilationInfo* info) {
  ASSERT(is_global_scope() || is_module_scope());

  if (is_module_scope()) {
    Handle<JSModule> instance = interface_->Instance();

    // Populate the module instance object.
    const PropertyAttributes ro_attr =
        static_cast<PropertyAttributes>(READ_ONLY | DONT_DELETE | DONT_ENUM);
    const PropertyAttributes rw_attr =
        static_cast<PropertyAttributes>(DONT_DELETE | DONT_ENUM);
    for (Interface::Iterator it = interface_->iterator();
         !it.done(); it.Advance()) {
      if (it.interface()->IsModule()) {
        Handle<Object> value = it.interface()->Instance();
        ASSERT(!value.is_null());
        JSReceiver::SetProperty(
            instance, it.name(), value, ro_attr, kStrictMode);
      } else {
        Variable* var = LocalLookup(it.name());
        ASSERT(var != NULL && var->IsContextSlot());
        PropertyAttributes attr = var->is_const_mode() ? ro_attr : rw_attr;
        Handle<AccessorInfo> info =
            Accessors::MakeModuleExport(it.name(), var->index(), attr);
        Handle<Object> result = SetAccessor(instance, info);
        ASSERT(!(result.is_null() || result->IsUndefined()));
        USE(result);
      }
    }
    USE(JSObject::PreventExtensions(instance));
  }

  // Link nested modules.
  for (int i = 0; i < inner_scopes_.length(); i++) {
    Scope* inner_scope = inner_scopes_.at(i);
    if (inner_scope->is_module_scope()) {
      inner_scope->LinkModules(info);
    }
  }
}


} }  // namespace v8::internal
