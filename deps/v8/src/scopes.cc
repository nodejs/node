// Copyright 2011 the V8 project authors. All rights reserved.
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

#include "bootstrapper.h"
#include "compiler.h"
#include "scopeinfo.h"

#include "allocation-inl.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// A Zone allocator for use with LocalsMap.

// TODO(isolates): It is probably worth it to change the Allocator class to
//                 take a pointer to an isolate.
class ZoneAllocator: public Allocator {
 public:
  /* nothing to do */
  virtual ~ZoneAllocator()  {}

  virtual void* New(size_t size)  { return ZONE->New(static_cast<int>(size)); }

  /* ignored - Zone is freed in one fell swoop */
  virtual void Delete(void* p)  {}
};


static ZoneAllocator LocalsMapAllocator;


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


// Dummy constructor
VariableMap::VariableMap(bool gotta_love_static_overloading) : HashMap() {}

VariableMap::VariableMap() : HashMap(Match, &LocalsMapAllocator, 8) {}
VariableMap::~VariableMap() {}


Variable* VariableMap::Declare(Scope* scope,
                               Handle<String> name,
                               Variable::Mode mode,
                               bool is_valid_lhs,
                               Variable::Kind kind) {
  HashMap::Entry* p = HashMap::Lookup(name.location(), name->Hash(), true);
  if (p->value == NULL) {
    // The variable has not been declared yet -> insert it.
    ASSERT(p->key == name.location());
    p->value = new Variable(scope, name, mode, is_valid_lhs, kind);
  }
  return reinterpret_cast<Variable*>(p->value);
}


Variable* VariableMap::Lookup(Handle<String> name) {
  HashMap::Entry* p = HashMap::Lookup(name.location(), name->Hash(), false);
  if (p != NULL) {
    ASSERT(*reinterpret_cast<String**>(p->key) == *name);
    ASSERT(p->value != NULL);
    return reinterpret_cast<Variable*>(p->value);
  }
  return NULL;
}


// ----------------------------------------------------------------------------
// Implementation of Scope


// Dummy constructor
Scope::Scope(Type type)
    : isolate_(Isolate::Current()),
      inner_scopes_(0),
      variables_(false),
      temps_(0),
      params_(0),
      unresolved_(0),
      decls_(0),
      already_resolved_(false) {
  SetDefaults(type, NULL, Handle<SerializedScopeInfo>::null());
}


Scope::Scope(Scope* outer_scope, Type type)
    : isolate_(Isolate::Current()),
      inner_scopes_(4),
      variables_(),
      temps_(4),
      params_(4),
      unresolved_(16),
      decls_(4),
      already_resolved_(false) {
  SetDefaults(type, outer_scope, Handle<SerializedScopeInfo>::null());
  // At some point we might want to provide outer scopes to
  // eval scopes (by walking the stack and reading the scope info).
  // In that case, the ASSERT below needs to be adjusted.
  ASSERT((type == GLOBAL_SCOPE || type == EVAL_SCOPE) == (outer_scope == NULL));
  ASSERT(!HasIllegalRedeclaration());
}


Scope::Scope(Scope* inner_scope,
             Type type,
             Handle<SerializedScopeInfo> scope_info)
    : isolate_(Isolate::Current()),
      inner_scopes_(4),
      variables_(),
      temps_(4),
      params_(4),
      unresolved_(16),
      decls_(4),
      already_resolved_(true) {
  ASSERT(!scope_info.is_null());
  SetDefaults(type, NULL, scope_info);
  if (scope_info->HasHeapAllocatedLocals()) {
    num_heap_slots_ = scope_info_->NumberOfContextSlots();
  }
  AddInnerScope(inner_scope);
}


Scope::Scope(Scope* inner_scope, Handle<String> catch_variable_name)
    : isolate_(Isolate::Current()),
      inner_scopes_(1),
      variables_(),
      temps_(0),
      params_(0),
      unresolved_(0),
      decls_(0),
      already_resolved_(true) {
  SetDefaults(CATCH_SCOPE, NULL, Handle<SerializedScopeInfo>::null());
  AddInnerScope(inner_scope);
  ++num_var_or_const_;
  Variable* variable = variables_.Declare(this,
                                          catch_variable_name,
                                          Variable::VAR,
                                          true,  // Valid left-hand side.
                                          Variable::NORMAL);
  AllocateHeapSlot(variable);
}


void Scope::SetDefaults(Type type,
                        Scope* outer_scope,
                        Handle<SerializedScopeInfo> scope_info) {
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
  strict_mode_ = (outer_scope != NULL) && outer_scope->strict_mode_;
  outer_scope_calls_eval_ = false;
  outer_scope_calls_non_strict_eval_ = false;
  inner_scope_calls_eval_ = false;
  outer_scope_is_eval_scope_ = false;
  force_eager_compilation_ = false;
  num_var_or_const_ = 0;
  num_stack_slots_ = 0;
  num_heap_slots_ = 0;
  scope_info_ = scope_info;
}


Scope* Scope::DeserializeScopeChain(CompilationInfo* info,
                                    Scope* global_scope) {
  // Reconstruct the outer scope chain from a closure's context chain.
  ASSERT(!info->closure().is_null());
  Context* context = info->closure()->context();
  Scope* current_scope = NULL;
  Scope* innermost_scope = NULL;
  bool contains_with = false;
  while (!context->IsGlobalContext()) {
    if (context->IsWithContext()) {
      // All the inner scopes are inside a with.
      contains_with = true;
      for (Scope* s = innermost_scope; s != NULL; s = s->outer_scope()) {
        s->scope_inside_with_ = true;
      }
    } else {
      if (context->IsFunctionContext()) {
        SerializedScopeInfo* scope_info =
            context->closure()->shared()->scope_info();
        current_scope = new Scope(current_scope, FUNCTION_SCOPE,
            Handle<SerializedScopeInfo>(scope_info));
      } else if (context->IsBlockContext()) {
        SerializedScopeInfo* scope_info =
            SerializedScopeInfo::cast(context->extension());
        current_scope = new Scope(current_scope, BLOCK_SCOPE,
            Handle<SerializedScopeInfo>(scope_info));
      } else {
        ASSERT(context->IsCatchContext());
        String* name = String::cast(context->extension());
        current_scope = new Scope(current_scope, Handle<String>(name));
      }
      if (contains_with) current_scope->RecordWithStatement();
      if (innermost_scope == NULL) innermost_scope = current_scope;
    }

    // Forget about a with when we move to a context for a different function.
    if (context->previous()->closure() != context->closure()) {
      contains_with = false;
    }
    context = context->previous();
  }

  global_scope->AddInnerScope(current_scope);
  return (innermost_scope == NULL) ? global_scope : innermost_scope;
}


bool Scope::Analyze(CompilationInfo* info) {
  ASSERT(info->function() != NULL);
  Scope* top = info->function()->scope();

  while (top->outer_scope() != NULL) top = top->outer_scope();
  top->AllocateVariables(info->calling_context());

#ifdef DEBUG
  if (info->isolate()->bootstrapper()->IsActive()
          ? FLAG_print_builtin_scopes
          : FLAG_print_scopes) {
    info->function()->scope()->Print();
  }
#endif

  info->SetScope(info->function()->scope());
  return true;  // Can not fail.
}


void Scope::Initialize(bool inside_with) {
  ASSERT(!already_resolved());

  // Add this scope as a new inner scope of the outer scope.
  if (outer_scope_ != NULL) {
    outer_scope_->inner_scopes_.Add(this);
    scope_inside_with_ = outer_scope_->scope_inside_with_ || inside_with;
  } else {
    scope_inside_with_ = inside_with;
  }

  // Declare convenience variables.
  // Declare and allocate receiver (even for the global scope, and even
  // if naccesses_ == 0).
  // NOTE: When loading parameters in the global scope, we must take
  // care not to access them as properties of the global object, but
  // instead load them directly from the stack. Currently, the only
  // such parameter is 'this' which is passed on the stack when
  // invoking scripts
  if (is_catch_scope() || is_block_scope()) {
    ASSERT(outer_scope() != NULL);
    receiver_ = outer_scope()->receiver();
  } else {
    ASSERT(is_function_scope() ||
           is_global_scope() ||
           is_eval_scope());
    Variable* var =
        variables_.Declare(this,
                           isolate_->factory()->this_symbol(),
                           Variable::VAR,
                           false,
                           Variable::THIS);
    var->AllocateTo(Variable::PARAMETER, -1);
    receiver_ = var;
  }

  if (is_function_scope()) {
    // Declare 'arguments' variable which exists in all functions.
    // Note that it might never be accessed, in which case it won't be
    // allocated during variable allocation.
    variables_.Declare(this,
                       isolate_->factory()->arguments_symbol(),
                       Variable::VAR,
                       true,
                       Variable::ARGUMENTS);
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
    outer_scope()->unresolved_.Add(unresolved_[i]);
  }

  return NULL;
}


Variable* Scope::LocalLookup(Handle<String> name) {
  Variable* result = variables_.Lookup(name);
  if (result != NULL || scope_info_.is_null()) {
    return result;
  }
  // If we have a serialized scope info, we might find the variable there.
  //
  // We should never lookup 'arguments' in this scope as it is implicitly
  // present in every scope.
  ASSERT(*name != *isolate_->factory()->arguments_symbol());
  // There should be no local slot with the given name.
  ASSERT(scope_info_->StackSlotIndex(*name) < 0);

  // Check context slot lookup.
  Variable::Mode mode;
  int index = scope_info_->ContextSlotIndex(*name, &mode);
  if (index < 0) {
    // Check parameters.
    mode = Variable::VAR;
    index = scope_info_->ParameterIndex(*name);
    if (index < 0) {
      // Check the function name.
      index = scope_info_->FunctionContextSlotIndex(*name);
      if (index < 0) return NULL;
    }
  }

  Variable* var =
      variables_.Declare(this, name, mode, true, Variable::NORMAL);
  var->AllocateTo(Variable::CONTEXT, index);
  return var;
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


Variable* Scope::DeclareFunctionVar(Handle<String> name) {
  ASSERT(is_function_scope() && function_ == NULL);
  Variable* function_var =
      new Variable(this, name, Variable::CONST, true, Variable::NORMAL);
  function_ = new(isolate_->zone()) VariableProxy(isolate_, function_var);
  return function_var;
}


void Scope::DeclareParameter(Handle<String> name, Variable::Mode mode) {
  ASSERT(!already_resolved());
  ASSERT(is_function_scope());
  Variable* var =
      variables_.Declare(this, name, mode, true, Variable::NORMAL);
  params_.Add(var);
}


Variable* Scope::DeclareLocal(Handle<String> name, Variable::Mode mode) {
  ASSERT(!already_resolved());
  // This function handles VAR and CONST modes.  DYNAMIC variables are
  // introduces during variable allocation, INTERNAL variables are allocated
  // explicitly, and TEMPORARY variables are allocated via NewTemporary().
  ASSERT(mode == Variable::VAR ||
         mode == Variable::CONST ||
         mode == Variable::LET);
  ++num_var_or_const_;
  return variables_.Declare(this, name, mode, true, Variable::NORMAL);
}


Variable* Scope::DeclareGlobal(Handle<String> name) {
  ASSERT(is_global_scope());
  return variables_.Declare(this, name, Variable::DYNAMIC_GLOBAL,
                            true,
                            Variable::NORMAL);
}


VariableProxy* Scope::NewUnresolved(Handle<String> name,
                                    bool inside_with,
                                    int position) {
  // Note that we must not share the unresolved variables with
  // the same name because they may be removed selectively via
  // RemoveUnresolved().
  ASSERT(!already_resolved());
  VariableProxy* proxy = new(isolate_->zone()) VariableProxy(
      isolate_, name, false, inside_with, position);
  unresolved_.Add(proxy);
  return proxy;
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
  Variable* var = new Variable(this,
                               name,
                               Variable::TEMPORARY,
                               true,
                               Variable::NORMAL);
  temps_.Add(var);
  return var;
}


void Scope::AddDeclaration(Declaration* declaration) {
  decls_.Add(declaration);
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
    if (decl->mode() != Variable::VAR) continue;
    Handle<String> name = decl->proxy()->name();
    bool cond = true;
    for (Scope* scope = decl->scope(); cond ; scope = scope->outer_scope_) {
      // There is a conflict if there exists a non-VAR binding.
      Variable* other_var = scope->variables_.Lookup(name);
      if (other_var != NULL && other_var->mode() != Variable::VAR) {
        return decl;
      }

      // Include declaration scope in the iteration but stop after.
      if (!scope->is_block_scope() && !scope->is_catch_scope()) cond = false;
    }
  }
  return NULL;
}


template<class Allocator>
void Scope::CollectUsedVariables(List<Variable*, Allocator>* locals) {
  // Collect variables in this scope.
  // Note that the function_ variable - if present - is not
  // collected here but handled separately in ScopeInfo
  // which is the current user of this function).
  for (int i = 0; i < temps_.length(); i++) {
    Variable* var = temps_[i];
    if (var->is_used()) {
      locals->Add(var);
    }
  }
  for (VariableMap::Entry* p = variables_.Start();
       p != NULL;
       p = variables_.Next(p)) {
    Variable* var = reinterpret_cast<Variable*>(p->value);
    if (var->is_used()) {
      locals->Add(var);
    }
  }
}


// Make sure the method gets instantiated by the template system.
template void Scope::CollectUsedVariables(
    List<Variable*, FreeStoreAllocationPolicy>* locals);
template void Scope::CollectUsedVariables(
    List<Variable*, PreallocatedStorage>* locals);
template void Scope::CollectUsedVariables(
    List<Variable*, ZoneListAllocationPolicy>* locals);


void Scope::AllocateVariables(Handle<Context> context) {
  ASSERT(outer_scope_ == NULL);  // eval or global scopes only

  // 1) Propagate scope information.
  // If we are in an eval scope, we may have other outer scopes about
  // which we don't know anything at this point. Thus we must be conservative
  // and assume they may invoke eval themselves. Eventually we could capture
  // this information in the ScopeInfo and then use it here (by traversing
  // the call chain stack, at compile time).

  bool eval_scope = is_eval_scope();
  bool outer_scope_calls_eval = false;
  bool outer_scope_calls_non_strict_eval = false;
  if (!is_global_scope()) {
    context->ComputeEvalScopeInfo(&outer_scope_calls_eval,
                                  &outer_scope_calls_non_strict_eval);
  }
  PropagateScopeInfo(outer_scope_calls_eval,
                     outer_scope_calls_non_strict_eval,
                     eval_scope);

  // 2) Resolve variables.
  Scope* global_scope = NULL;
  if (is_global_scope()) global_scope = this;
  ResolveVariablesRecursively(global_scope, context);

  // 3) Allocate variables.
  AllocateVariablesRecursively();
}


bool Scope::AllowsLazyCompilation() const {
  return !force_eager_compilation_ && HasTrivialOuterContext();
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
  while (scope->is_catch_scope() ||
         scope->is_block_scope()) {
    scope = scope->outer_scope();
  }
  return scope;
}


Handle<SerializedScopeInfo> Scope::GetSerializedScopeInfo() {
  if (scope_info_.is_null()) {
    scope_info_ = SerializedScopeInfo::Create(this);
  }
  return scope_info_;
}


#ifdef DEBUG
static const char* Header(Scope::Type type) {
  switch (type) {
    case Scope::EVAL_SCOPE: return "eval";
    case Scope::FUNCTION_SCOPE: return "function";
    case Scope::GLOBAL_SCOPE: return "global";
    case Scope::CATCH_SCOPE: return "catch";
    case Scope::BLOCK_SCOPE: return "block";
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
    if (var->is_accessed_from_inner_scope()) {
      if (!var->IsUnallocated()) PrintF(", ");
      PrintF("inner scope access");
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

  PrintF(" {\n");

  // Function name, if any (named function literals, only).
  if (function_ != NULL) {
    Indent(n1, "// (local) function name: ");
    PrintName(function_->name());
    PrintF("\n");
  }

  // Scope info.
  if (HasTrivialOuterContext()) {
    Indent(n1, "// scope has trivial outer context\n");
  }
  if (is_strict_mode()) Indent(n1, "// strict mode scope\n");
  if (scope_inside_with_) Indent(n1, "// scope inside 'with'\n");
  if (scope_contains_with_) Indent(n1, "// scope contains 'with'\n");
  if (scope_calls_eval_) Indent(n1, "// scope calls 'eval'\n");
  if (outer_scope_calls_eval_) Indent(n1, "// outer scope calls 'eval'\n");
  if (outer_scope_calls_non_strict_eval_) {
    Indent(n1, "// outer scope calls 'eval' in non-strict context\n");
  }
  if (inner_scope_calls_eval_) Indent(n1, "// inner scope calls 'eval'\n");
  if (outer_scope_is_eval_scope_) {
    Indent(n1, "// outer scope is 'eval' scope\n");
  }
  if (num_stack_slots_ > 0) { Indent(n1, "// ");
  PrintF("%d stack slots\n", num_stack_slots_); }
  if (num_heap_slots_ > 0) { Indent(n1, "// ");
  PrintF("%d heap slots\n", num_heap_slots_); }

  // Print locals.
  Indent(n1, "// function var\n");
  if (function_ != NULL) {
    PrintVar(n1, function_->var());
  }

  Indent(n1, "// temporary vars\n");
  for (int i = 0; i < temps_.length(); i++) {
    PrintVar(n1, temps_[i]);
  }

  Indent(n1, "// local vars\n");
  PrintMap(n1, &variables_);

  Indent(n1, "// dynamic vars\n");
  if (dynamics_ != NULL) {
    PrintMap(n1, dynamics_->GetMap(Variable::DYNAMIC));
    PrintMap(n1, dynamics_->GetMap(Variable::DYNAMIC_LOCAL));
    PrintMap(n1, dynamics_->GetMap(Variable::DYNAMIC_GLOBAL));
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


Variable* Scope::NonLocal(Handle<String> name, Variable::Mode mode) {
  if (dynamics_ == NULL) dynamics_ = new DynamicScopePart();
  VariableMap* map = dynamics_->GetMap(mode);
  Variable* var = map->Lookup(name);
  if (var == NULL) {
    // Declare a new non-local.
    var = map->Declare(NULL, name, mode, true, Variable::NORMAL);
    // Allocate it by giving it a dynamic lookup.
    var->AllocateTo(Variable::LOOKUP, -1);
  }
  return var;
}


// Lookup a variable starting with this scope. The result is either
// the statically resolved variable belonging to an outer scope, or
// NULL. It may be NULL because a) we couldn't find a variable, or b)
// because the variable is just a guess (and may be shadowed by
// another variable that is introduced dynamically via an 'eval' call
// or a 'with' statement).
Variable* Scope::LookupRecursive(Handle<String> name,
                                 bool from_inner_scope,
                                 Variable** invalidated_local) {
  // If we find a variable, but the current scope calls 'eval', the found
  // variable may not be the correct one (the 'eval' may introduce a
  // property with the same name). In that case, remember that the variable
  // found is just a guess.
  bool guess = scope_calls_eval_;

  // Try to find the variable in this scope.
  Variable* var = LocalLookup(name);

  if (var != NULL) {
    // We found a variable. If this is not an inner lookup, we are done.
    // (Even if there is an 'eval' in this scope which introduces the
    // same variable again, the resulting variable remains the same.
    // Note that enclosing 'with' statements are handled at the call site.)
    if (!from_inner_scope)
      return var;

  } else {
    // We did not find a variable locally. Check against the function variable,
    // if any. We can do this for all scopes, since the function variable is
    // only present - if at all - for function scopes.
    //
    // This lookup corresponds to a lookup in the "intermediate" scope sitting
    // between this scope and the outer scope. (ECMA-262, 3rd., requires that
    // the name of named function literal is kept in an intermediate scope
    // in between this scope and the next outer scope.)
    if (function_ != NULL && function_->name().is_identical_to(name)) {
      var = function_->var();

    } else if (outer_scope_ != NULL) {
      var = outer_scope_->LookupRecursive(name, true, invalidated_local);
      // We may have found a variable in an outer scope. However, if
      // the current scope is inside a 'with', the actual variable may
      // be a property introduced via the 'with' statement. Then, the
      // variable we may have found is just a guess.
      if (scope_inside_with_)
        guess = true;
    }

    // If we did not find a variable, we are done.
    if (var == NULL)
      return NULL;
  }

  ASSERT(var != NULL);

  // If this is a lookup from an inner scope, mark the variable.
  if (from_inner_scope) {
    var->MarkAsAccessedFromInnerScope();
  }

  // If the variable we have found is just a guess, invalidate the
  // result. If the found variable is local, record that fact so we
  // can generate fast code to get it if it is not shadowed by eval.
  if (guess) {
    if (!var->is_global()) *invalidated_local = var;
    var = NULL;
  }

  return var;
}


void Scope::ResolveVariable(Scope* global_scope,
                            Handle<Context> context,
                            VariableProxy* proxy) {
  ASSERT(global_scope == NULL || global_scope->is_global_scope());

  // If the proxy is already resolved there's nothing to do
  // (functions and consts may be resolved by the parser).
  if (proxy->var() != NULL) return;

  // Otherwise, try to resolve the variable.
  Variable* invalidated_local = NULL;
  Variable* var = LookupRecursive(proxy->name(), false, &invalidated_local);

  if (proxy->inside_with()) {
    // If we are inside a local 'with' statement, all bets are off
    // and we cannot resolve the proxy to a local variable even if
    // we found an outer matching variable.
    // Note that we must do a lookup anyway, because if we find one,
    // we must mark that variable as potentially accessed from this
    // inner scope (the property may not be in the 'with' object).
    var = NonLocal(proxy->name(), Variable::DYNAMIC);

  } else {
    // We are not inside a local 'with' statement.

    if (var == NULL) {
      // We did not find the variable. We have a global variable
      // if we are in the global scope (we know already that we
      // are outside a 'with' statement) or if there is no way
      // that the variable might be introduced dynamically (through
      // a local or outer eval() call, or an outer 'with' statement),
      // or we don't know about the outer scope (because we are
      // in an eval scope).
      if (is_global_scope() ||
          !(scope_inside_with_ || outer_scope_is_eval_scope_ ||
            scope_calls_eval_ || outer_scope_calls_eval_)) {
        // We must have a global variable.
        ASSERT(global_scope != NULL);
        var = global_scope->DeclareGlobal(proxy->name());

      } else if (scope_inside_with_) {
        // If we are inside a with statement we give up and look up
        // the variable at runtime.
        var = NonLocal(proxy->name(), Variable::DYNAMIC);

      } else if (invalidated_local != NULL) {
        // No with statements are involved and we found a local
        // variable that might be shadowed by eval introduced
        // variables.
        var = NonLocal(proxy->name(), Variable::DYNAMIC_LOCAL);
        var->set_local_if_not_shadowed(invalidated_local);

      } else if (outer_scope_is_eval_scope_) {
        // No with statements and we did not find a local and the code
        // is executed with a call to eval.  The context contains
        // scope information that we can use to determine if the
        // variable is global if it is not shadowed by eval-introduced
        // variables.
        if (context->GlobalIfNotShadowedByEval(proxy->name())) {
          var = NonLocal(proxy->name(), Variable::DYNAMIC_GLOBAL);

        } else {
          var = NonLocal(proxy->name(), Variable::DYNAMIC);
        }

      } else {
        // No with statements and we did not find a local and the code
        // is not executed with a call to eval.  We know that this
        // variable is global unless it is shadowed by eval-introduced
        // variables.
        var = NonLocal(proxy->name(), Variable::DYNAMIC_GLOBAL);
      }
    }
  }

  proxy->BindTo(var);
}


void Scope::ResolveVariablesRecursively(Scope* global_scope,
                                        Handle<Context> context) {
  ASSERT(global_scope == NULL || global_scope->is_global_scope());

  // Resolve unresolved variables for this scope.
  for (int i = 0; i < unresolved_.length(); i++) {
    ResolveVariable(global_scope, context, unresolved_[i]);
  }

  // Resolve unresolved variables for inner scopes.
  for (int i = 0; i < inner_scopes_.length(); i++) {
    inner_scopes_[i]->ResolveVariablesRecursively(global_scope, context);
  }
}


bool Scope::PropagateScopeInfo(bool outer_scope_calls_eval,
                               bool outer_scope_calls_non_strict_eval,
                               bool outer_scope_is_eval_scope) {
  if (outer_scope_calls_eval) {
    outer_scope_calls_eval_ = true;
  }

  if (outer_scope_calls_non_strict_eval) {
    outer_scope_calls_non_strict_eval_ = true;
  }

  if (outer_scope_is_eval_scope) {
    outer_scope_is_eval_scope_ = true;
  }

  bool calls_eval = scope_calls_eval_ || outer_scope_calls_eval_;
  bool is_eval = is_eval_scope() || outer_scope_is_eval_scope_;
  bool calls_non_strict_eval =
      (scope_calls_eval_ && !is_strict_mode()) ||
      outer_scope_calls_non_strict_eval_;
  for (int i = 0; i < inner_scopes_.length(); i++) {
    Scope* inner_scope = inner_scopes_[i];
    if (inner_scope->PropagateScopeInfo(calls_eval,
                                        calls_non_strict_eval,
                                        is_eval)) {
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
      (var->is_accessed_from_inner_scope() ||
       scope_calls_eval_ ||
       inner_scope_calls_eval_ ||
       scope_contains_with_ ||
       is_catch_scope() ||
       is_block_scope())) {
    var->set_is_used(true);
  }
  // Global variables do not need to be allocated.
  return !var->is_global() && var->is_used();
}


bool Scope::MustAllocateInContext(Variable* var) {
  // If var is accessed from an inner scope, or if there is a possibility
  // that it might be accessed from the current or an inner scope (through
  // an eval() call or a runtime with lookup), it must be allocated in the
  // context.
  //
  // Exceptions: temporary variables are never allocated in a context;
  // catch-bound variables are always allocated in a context.
  if (var->mode() == Variable::TEMPORARY) return false;
  if (is_catch_scope() || is_block_scope()) return true;
  return var->is_accessed_from_inner_scope() ||
      scope_calls_eval_ ||
      inner_scope_calls_eval_ ||
      scope_contains_with_ ||
      var->is_global();
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
    uses_nonstrict_arguments = !is_strict_mode();
  }

  // The same parameter may occur multiple times in the parameters_ list.
  // If it does, and if it is not copied into the context object, it must
  // receive the highest parameter index for that parameter; thus iteration
  // order is relevant!
  for (int i = params_.length() - 1; i >= 0; --i) {
    Variable* var = params_[i];
    ASSERT(var->scope() == this);
    if (uses_nonstrict_arguments) {
      // Give the parameter a use from an inner scope, to force allocation
      // to the context.
      var->MarkAsAccessedFromInnerScope();
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

  for (VariableMap::Entry* p = variables_.Start();
       p != NULL;
       p = variables_.Next(p)) {
    Variable* var = reinterpret_cast<Variable*>(p->value);
    AllocateNonParameterLocal(var);
  }

  // For now, function_ must be allocated at the very end.  If it gets
  // allocated in the context, it must be the last slot in the context,
  // because of the current ScopeInfo implementation (see
  // ScopeInfo::ScopeInfo(FunctionScope* scope) constructor).
  if (function_ != NULL) {
    AllocateNonParameterLocal(function_->var());
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

  // Allocate context if necessary.
  bool must_have_local_context = false;
  if (scope_calls_eval_ || scope_contains_with_) {
    // The context for the eval() call or 'with' statement in this scope.
    // Unless we are in the global or an eval scope, we need a local
    // context even if we didn't statically allocate any locals in it,
    // and the compiler will access the context variable. If we are
    // not in an inner scope, the scope is provided from the outside.
    must_have_local_context = is_function_scope();
  }

  // If we didn't allocate any locals in the local context, then we only
  // need the minimal number of slots if we must have a local context.
  if (num_heap_slots_ == Context::MIN_CONTEXT_SLOTS &&
      !must_have_local_context) {
    num_heap_slots_ = 0;
  }

  // Allocation done.
  ASSERT(num_heap_slots_ == 0 || num_heap_slots_ >= Context::MIN_CONTEXT_SLOTS);
}

} }  // namespace v8::internal
