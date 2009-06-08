// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

#include "prettyprinter.h"
#include "scopeinfo.h"
#include "scopes.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// A Zone allocator for use with LocalsMap.

class ZoneAllocator: public Allocator {
 public:
  /* nothing to do */
  virtual ~ZoneAllocator()  {}

  virtual void* New(size_t size)  { return Zone::New(size); }

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
LocalsMap::LocalsMap(bool gotta_love_static_overloading) : HashMap()  {}

LocalsMap::LocalsMap() : HashMap(Match, &LocalsMapAllocator, 8)  {}
LocalsMap::~LocalsMap()  {}


Variable* LocalsMap::Declare(Scope* scope,
                             Handle<String> name,
                             Variable::Mode mode,
                             bool is_valid_LHS,
                             bool is_this) {
  HashMap::Entry* p = HashMap::Lookup(name.location(), name->Hash(), true);
  if (p->value == NULL) {
    // The variable has not been declared yet -> insert it.
    ASSERT(p->key == name.location());
    p->value = new Variable(scope, name, mode, is_valid_LHS, is_this);
  }
  return reinterpret_cast<Variable*>(p->value);
}


Variable* LocalsMap::Lookup(Handle<String> name) {
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
Scope::Scope()
  : inner_scopes_(0),
    locals_(false),
    temps_(0),
    params_(0),
    dynamics_(NULL),
    unresolved_(0),
    decls_(0) {
}


Scope::Scope(Scope* outer_scope, Type type)
  : outer_scope_(outer_scope),
    inner_scopes_(4),
    type_(type),
    scope_name_(Factory::empty_symbol()),
    temps_(4),
    params_(4),
    dynamics_(NULL),
    unresolved_(16),
    decls_(4),
    receiver_(NULL),
    function_(NULL),
    arguments_(NULL),
    arguments_shadow_(NULL),
    illegal_redecl_(NULL),
    scope_inside_with_(false),
    scope_contains_with_(false),
    scope_calls_eval_(false),
    outer_scope_calls_eval_(false),
    inner_scope_calls_eval_(false),
    outer_scope_is_eval_scope_(false),
    force_eager_compilation_(false),
    num_stack_slots_(0),
    num_heap_slots_(0) {
  // At some point we might want to provide outer scopes to
  // eval scopes (by walking the stack and reading the scope info).
  // In that case, the ASSERT below needs to be adjusted.
  ASSERT((type == GLOBAL_SCOPE || type == EVAL_SCOPE) == (outer_scope == NULL));
  ASSERT(!HasIllegalRedeclaration());
}


void Scope::Initialize(bool inside_with) {
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
  { Variable* var =
      locals_.Declare(this, Factory::this_symbol(), Variable::VAR, false, true);
    var->rewrite_ = new Slot(var, Slot::PARAMETER, -1);
    receiver_ = new VariableProxy(Factory::this_symbol(), true, false);
    receiver_->BindTo(var);
  }

  if (is_function_scope()) {
    // Declare 'arguments' variable which exists in all functions.
    // Note that it may never be accessed, in which case it won't
    // be allocated during variable allocation.
    Declare(Factory::arguments_symbol(), Variable::VAR);
  }
}



Variable* Scope::LookupLocal(Handle<String> name) {
  return locals_.Lookup(name);
}


Variable* Scope::Lookup(Handle<String> name) {
  for (Scope* scope = this;
       scope != NULL;
       scope = scope->outer_scope()) {
    Variable* var = scope->LookupLocal(name);
    if (var != NULL) return var;
  }
  return NULL;
}


Variable* Scope::DeclareFunctionVar(Handle<String> name) {
  ASSERT(is_function_scope() && function_ == NULL);
  function_ = new Variable(this, name, Variable::CONST, true, false);
  return function_;
}


Variable* Scope::Declare(Handle<String> name, Variable::Mode mode) {
  // DYNAMIC variables are introduces during variable allocation,
  // INTERNAL variables are allocated explicitly, and TEMPORARY
  // variables are allocated via NewTemporary().
  ASSERT(mode == Variable::VAR || mode == Variable::CONST);
  return locals_.Declare(this, name, mode, true, false);
}


void Scope::AddParameter(Variable* var) {
  ASSERT(is_function_scope());
  ASSERT(LookupLocal(var->name()) == var);
  params_.Add(var);
}


VariableProxy* Scope::NewUnresolved(Handle<String> name, bool inside_with) {
  // Note that we must not share the unresolved variables with
  // the same name because they may be removed selectively via
  // RemoveUnresolved().
  VariableProxy* proxy = new VariableProxy(name, false, inside_with);
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


VariableProxy* Scope::NewTemporary(Handle<String> name) {
  Variable* var = new Variable(this, name, Variable::TEMPORARY, true, false);
  VariableProxy* tmp = new VariableProxy(name, false, false);
  tmp->BindTo(var);
  temps_.Add(var);
  return tmp;
}


void Scope::AddDeclaration(Declaration* declaration) {
  decls_.Add(declaration);
}


void Scope::SetIllegalRedeclaration(Expression* expression) {
  // Only set the illegal redeclaration expression the
  // first time the function is called.
  if (!HasIllegalRedeclaration()) {
    illegal_redecl_ = expression;
  }
  ASSERT(HasIllegalRedeclaration());
}


void Scope::VisitIllegalRedeclaration(AstVisitor* visitor) {
  ASSERT(HasIllegalRedeclaration());
  illegal_redecl_->Accept(visitor);
}


template<class Allocator>
void Scope::CollectUsedVariables(List<Variable*, Allocator>* locals) {
  // Collect variables in this scope.
  // Note that the function_ variable - if present - is not
  // collected here but handled separately in ScopeInfo
  // which is the current user of this function).
  for (int i = 0; i < temps_.length(); i++) {
    Variable* var = temps_[i];
    if (var->var_uses()->is_used()) {
      locals->Add(var);
    }
  }
  for (LocalsMap::Entry* p = locals_.Start(); p != NULL; p = locals_.Next(p)) {
    Variable* var = reinterpret_cast<Variable*>(p->value);
    if (var->var_uses()->is_used()) {
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
  PropagateScopeInfo(eval_scope, eval_scope);

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


#ifdef DEBUG
static const char* Header(Scope::Type type) {
  switch (type) {
    case Scope::EVAL_SCOPE: return "eval";
    case Scope::FUNCTION_SCOPE: return "function";
    case Scope::GLOBAL_SCOPE: return "global";
  }
  UNREACHABLE();
  return NULL;
}


static void Indent(int n, const char* str) {
  PrintF("%*s%s", n, "", str);
}


static void PrintName(Handle<String> name) {
  SmartPointer<char> s = name->ToCString(DISALLOW_NULLS);
  PrintF("%s", *s);
}


static void PrintVar(PrettyPrinter* printer, int indent, Variable* var) {
  if (var->var_uses()->is_used() || var->rewrite() != NULL) {
    Indent(indent, Variable::Mode2String(var->mode()));
    PrintF(" ");
    PrintName(var->name());
    PrintF(";  // ");
    if (var->rewrite() != NULL) PrintF("%s, ", printer->Print(var->rewrite()));
    if (var->is_accessed_from_inner_scope()) PrintF("inner scope access, ");
    PrintF("var ");
    var->var_uses()->Print();
    PrintF(", obj ");
    var->obj_uses()->Print();
    PrintF("\n");
  }
}


static void PrintMap(PrettyPrinter* printer, int indent, LocalsMap* map) {
  for (LocalsMap::Entry* p = map->Start(); p != NULL; p = map->Next(p)) {
    Variable* var = reinterpret_cast<Variable*>(p->value);
    PrintVar(printer, indent, var);
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
  if (scope_inside_with_) Indent(n1, "// scope inside 'with'\n");
  if (scope_contains_with_) Indent(n1, "// scope contains 'with'\n");
  if (scope_calls_eval_) Indent(n1, "// scope calls 'eval'\n");
  if (outer_scope_calls_eval_) Indent(n1, "// outer scope calls 'eval'\n");
  if (inner_scope_calls_eval_) Indent(n1, "// inner scope calls 'eval'\n");
  if (outer_scope_is_eval_scope_) {
    Indent(n1, "// outer scope is 'eval' scope\n");
  }
  if (num_stack_slots_ > 0) { Indent(n1, "// ");
  PrintF("%d stack slots\n", num_stack_slots_); }
  if (num_heap_slots_ > 0) { Indent(n1, "// ");
  PrintF("%d heap slots\n", num_heap_slots_); }

  // Print locals.
  PrettyPrinter printer;
  Indent(n1, "// function var\n");
  if (function_ != NULL) {
    PrintVar(&printer, n1, function_);
  }

  Indent(n1, "// temporary vars\n");
  for (int i = 0; i < temps_.length(); i++) {
    PrintVar(&printer, n1, temps_[i]);
  }

  Indent(n1, "// local vars\n");
  PrintMap(&printer, n1, &locals_);

  Indent(n1, "// dynamic vars\n");
  if (dynamics_ != NULL) {
    PrintMap(&printer, n1, dynamics_->GetMap(Variable::DYNAMIC));
    PrintMap(&printer, n1, dynamics_->GetMap(Variable::DYNAMIC_LOCAL));
    PrintMap(&printer, n1, dynamics_->GetMap(Variable::DYNAMIC_GLOBAL));
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
  LocalsMap* map = dynamics_->GetMap(mode);
  Variable* var = map->Lookup(name);
  if (var == NULL) {
    // Declare a new non-local.
    var = map->Declare(NULL, name, mode, true, false);
    // Allocate it by giving it a dynamic lookup.
    var->rewrite_ = new Slot(var, Slot::LOOKUP, -1);
  }
  return var;
}


// Lookup a variable starting with this scope. The result is either
// the statically resolved (local!) variable belonging to an outer scope,
// or NULL. It may be NULL because a) we couldn't find a variable, or b)
// because the variable is just a guess (and may be shadowed by another
// variable that is introduced dynamically via an 'eval' call or a 'with'
// statement).
Variable* Scope::LookupRecursive(Handle<String> name,
                                 bool inner_lookup,
                                 Variable** invalidated_local) {
  // If we find a variable, but the current scope calls 'eval', the found
  // variable may not be the correct one (the 'eval' may introduce a
  // property with the same name). In that case, remember that the variable
  // found is just a guess.
  bool guess = scope_calls_eval_;

  // Try to find the variable in this scope.
  Variable* var = LookupLocal(name);

  if (var != NULL) {
    // We found a variable. If this is not an inner lookup, we are done.
    // (Even if there is an 'eval' in this scope which introduces the
    // same variable again, the resulting variable remains the same.
    // Note that enclosing 'with' statements are handled at the call site.)
    if (!inner_lookup)
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
      var = function_;

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
  if (inner_lookup)
    var->is_accessed_from_inner_scope_ = true;

  // If the variable we have found is just a guess, invalidate the result.
  if (guess) {
    *invalidated_local = var;
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
        var = new Variable(global_scope, proxy->name(),
                           Variable::DYNAMIC, true, false);

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
                               bool outer_scope_is_eval_scope) {
  if (outer_scope_calls_eval) {
    outer_scope_calls_eval_ = true;
  }

  if (outer_scope_is_eval_scope) {
    outer_scope_is_eval_scope_ = true;
  }

  bool calls_eval = scope_calls_eval_ || outer_scope_calls_eval_;
  bool is_eval = is_eval_scope() || outer_scope_is_eval_scope_;
  for (int i = 0; i < inner_scopes_.length(); i++) {
    Scope* inner_scope = inner_scopes_[i];
    if (inner_scope->PropagateScopeInfo(calls_eval, is_eval)) {
      inner_scope_calls_eval_ = true;
    }
    if (inner_scope->force_eager_compilation_) {
      force_eager_compilation_ = true;
    }
  }

  return scope_calls_eval_ || inner_scope_calls_eval_;
}


bool Scope::MustAllocate(Variable* var) {
  // Give var a read/write use if there is a chance it might be
  // accessed via an eval() call, or if it is a global variable.
  // This is only possible if the variable has a visible name.
  if ((var->is_this() || var->name()->length() > 0) &&
      (var->is_accessed_from_inner_scope_ ||
       scope_calls_eval_ || inner_scope_calls_eval_ ||
       scope_contains_with_ || var->is_global())) {
    var->var_uses()->RecordAccess(1);
  }
  return var->var_uses()->is_used();
}


bool Scope::MustAllocateInContext(Variable* var) {
  // If var is accessed from an inner scope, or if there is a
  // possibility that it might be accessed from the current or
  // an inner scope (through an eval() call), it must be allocated
  // in the context.
  // Exceptions: Global variables and temporary variables must
  // never be allocated in the (FixedArray part of the) context.
  return
    var->mode() != Variable::TEMPORARY &&
    (var->is_accessed_from_inner_scope_ ||
     scope_calls_eval_ || inner_scope_calls_eval_ ||
     scope_contains_with_ || var->is_global());
}


bool Scope::HasArgumentsParameter() {
  for (int i = 0; i < params_.length(); i++) {
    if (params_[i]->name().is_identical_to(Factory::arguments_symbol()))
      return true;
  }
  return false;
}


void Scope::AllocateStackSlot(Variable* var) {
  var->rewrite_ = new Slot(var, Slot::LOCAL, num_stack_slots_++);
}


void Scope::AllocateHeapSlot(Variable* var) {
  var->rewrite_ = new Slot(var, Slot::CONTEXT, num_heap_slots_++);
}


void Scope::AllocateParameterLocals() {
  ASSERT(is_function_scope());
  Variable* arguments = LookupLocal(Factory::arguments_symbol());
  ASSERT(arguments != NULL);  // functions have 'arguments' declared implicitly
  if (MustAllocate(arguments) && !HasArgumentsParameter()) {
    // 'arguments' is used. Unless there is also a parameter called
    // 'arguments', we must be conservative and access all parameters via
    // the arguments object: The i'th parameter is rewritten into
    // '.arguments[i]' (*). If we have a parameter named 'arguments', a
    // (new) value is always assigned to it via the function
    // invocation. Then 'arguments' denotes that specific parameter value
    // and cannot be used to access the parameters, which is why we don't
    // need to rewrite in that case.
    //
    // (*) Instead of having a parameter called 'arguments', we may have an
    // assignment to 'arguments' in the function body, at some arbitrary
    // point in time (possibly through an 'eval()' call!). After that
    // assignment any re-write of parameters would be invalid (was bug
    // 881452). Thus, we introduce a shadow '.arguments'
    // variable which also points to the arguments object. For rewrites we
    // use '.arguments' which remains valid even if we assign to
    // 'arguments'. To summarize: If we need to rewrite, we allocate an
    // 'arguments' object dynamically upon function invocation. The compiler
    // introduces 2 local variables 'arguments' and '.arguments', both of
    // which originally point to the arguments object that was
    // allocated. All parameters are rewritten into property accesses via
    // the '.arguments' variable. Thus, any changes to properties of
    // 'arguments' are reflected in the variables and vice versa. If the
    // 'arguments' variable is changed, '.arguments' still points to the
    // correct arguments object and the rewrites still work.

    // We are using 'arguments'. Tell the code generator that is needs to
    // allocate the arguments object by setting 'arguments_'.
    arguments_ = new VariableProxy(Factory::arguments_symbol(), false, false);
    arguments_->BindTo(arguments);

    // We also need the '.arguments' shadow variable. Declare it and create
    // and bind the corresponding proxy. It's ok to declare it only now
    // because it's a local variable that is allocated after the parameters
    // have been allocated.
    //
    // Note: This is "almost" at temporary variable but we cannot use
    // NewTemporary() because the mode needs to be INTERNAL since this
    // variable may be allocated in the heap-allocated context (temporaries
    // are never allocated in the context).
    Variable* arguments_shadow =
        new Variable(this, Factory::arguments_shadow_symbol(),
                     Variable::INTERNAL, true, false);
    arguments_shadow_ =
        new VariableProxy(Factory::arguments_shadow_symbol(), false, false);
    arguments_shadow_->BindTo(arguments_shadow);
    temps_.Add(arguments_shadow);

    // Allocate the parameters by rewriting them into '.arguments[i]' accesses.
    for (int i = 0; i < params_.length(); i++) {
      Variable* var = params_[i];
      ASSERT(var->scope() == this);
      if (MustAllocate(var)) {
        if (MustAllocateInContext(var)) {
          // It is ok to set this only now, because arguments is a local
          // variable that is allocated after the parameters have been
          // allocated.
          arguments_shadow->is_accessed_from_inner_scope_ = true;
        }
        var->rewrite_ =
          new Property(arguments_shadow_,
                       new Literal(Handle<Object>(Smi::FromInt(i))),
                       RelocInfo::kNoPosition,
                       Property::SYNTHETIC);
        arguments_shadow->var_uses()->RecordUses(var->var_uses());
      }
    }

  } else {
    // The arguments object is not used, so we can access parameters directly.
    // The same parameter may occur multiple times in the parameters_ list.
    // If it does, and if it is not copied into the context object, it must
    // receive the highest parameter index for that parameter; thus iteration
    // order is relevant!
    for (int i = 0; i < params_.length(); i++) {
      Variable* var = params_[i];
      ASSERT(var->scope() == this);
      if (MustAllocate(var)) {
        if (MustAllocateInContext(var)) {
          ASSERT(var->rewrite_ == NULL ||
                 (var->slot() != NULL && var->slot()->type() == Slot::CONTEXT));
          if (var->rewrite_ == NULL) {
            // Only set the heap allocation if the parameter has not
            // been allocated yet.
            AllocateHeapSlot(var);
          }
        } else {
          ASSERT(var->rewrite_ == NULL ||
                 (var->slot() != NULL &&
                  var->slot()->type() == Slot::PARAMETER));
          // Set the parameter index always, even if the parameter
          // was seen before! (We need to access the actual parameter
          // supplied for the last occurrence of a multiply declared
          // parameter.)
          var->rewrite_ = new Slot(var, Slot::PARAMETER, i);
        }
      }
    }
  }
}


void Scope::AllocateNonParameterLocal(Variable* var) {
  ASSERT(var->scope() == this);
  ASSERT(var->rewrite_ == NULL ||
         (!var->IsVariable(Factory::result_symbol())) ||
         (var->slot() == NULL || var->slot()->type() != Slot::LOCAL));
  if (MustAllocate(var) && var->rewrite_ == NULL) {
    if (MustAllocateInContext(var)) {
      AllocateHeapSlot(var);
    } else {
      AllocateStackSlot(var);
    }
  }
}


void Scope::AllocateNonParameterLocals() {
  // Each variable occurs exactly once in the locals_ list; all
  // variables that have no rewrite yet are non-parameter locals.

  // Sort them according to use such that the locals with more uses
  // get allocated first.
  if (FLAG_usage_computation) {
    // This is currently not implemented.
  }

  for (int i = 0; i < temps_.length(); i++) {
    AllocateNonParameterLocal(temps_[i]);
  }

  for (LocalsMap::Entry* p = locals_.Start(); p != NULL; p = locals_.Next(p)) {
    Variable* var = reinterpret_cast<Variable*>(p->value);
    AllocateNonParameterLocal(var);
  }

  // Note: For now, function_ must be allocated at the very end.  If
  // it gets allocated in the context, it must be the last slot in the
  // context, because of the current ScopeInfo implementation (see
  // ScopeInfo::ScopeInfo(FunctionScope* scope) constructor).
  if (function_ != NULL) {
    AllocateNonParameterLocal(function_);
  }
}


void Scope::AllocateVariablesRecursively() {
  // The number of slots required for variables.
  num_stack_slots_ = 0;
  num_heap_slots_ = Context::MIN_CONTEXT_SLOTS;

  // Allocate variables for inner scopes.
  for (int i = 0; i < inner_scopes_.length(); i++) {
    inner_scopes_[i]->AllocateVariablesRecursively();
  }

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
