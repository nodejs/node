// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_SCOPES_H_
#define V8_AST_SCOPES_H_

#include "src/ast/ast.h"
#include "src/base/hashmap.h"
#include "src/globals.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

class ParseInfo;

// A hash map to support fast variable declaration and lookup.
class VariableMap: public ZoneHashMap {
 public:
  explicit VariableMap(Zone* zone);

  Variable* Declare(Zone* zone, Scope* scope, const AstRawString* name,
                    VariableMode mode, Variable::Kind kind,
                    InitializationFlag initialization_flag,
                    MaybeAssignedFlag maybe_assigned_flag = kNotAssigned,
                    bool* added = nullptr);

  Variable* Lookup(const AstRawString* name);
};


// The dynamic scope part holds hash maps for the variables that will
// be looked up dynamically from within eval and with scopes. The objects
// are allocated on-demand from Scope::NonLocal to avoid wasting memory
// and setup time for scopes that don't need them.
class DynamicScopePart : public ZoneObject {
 public:
  explicit DynamicScopePart(Zone* zone) {
    for (int i = 0; i < 3; i++)
      maps_[i] = new(zone->New(sizeof(VariableMap))) VariableMap(zone);
  }

  VariableMap* GetMap(VariableMode mode) {
    int index = mode - DYNAMIC;
    DCHECK(index >= 0 && index < 3);
    return maps_[index];
  }

 private:
  VariableMap *maps_[3];
};


// Sloppy block-scoped function declarations to var-bind
class SloppyBlockFunctionMap : public ZoneHashMap {
 public:
  explicit SloppyBlockFunctionMap(Zone* zone);
  void Declare(Zone* zone, const AstRawString* name,
               SloppyBlockFunctionStatement* statement);
};


// Global invariants after AST construction: Each reference (i.e. identifier)
// to a JavaScript variable (including global properties) is represented by a
// VariableProxy node. Immediately after AST construction and before variable
// allocation, most VariableProxy nodes are "unresolved", i.e. not bound to a
// corresponding variable (though some are bound during parse time). Variable
// allocation binds each unresolved VariableProxy to one Variable and assigns
// a location. Note that many VariableProxy nodes may refer to the same Java-
// Script variable.

// JS environments are represented in the parser using Scope, DeclarationScope
// and ModuleScope. DeclarationScope is used for any scope that hosts 'var'
// declarations. This includes script, module, eval, varblock, and function
// scope. ModuleScope further specializes DeclarationScope.
class Scope: public ZoneObject {
 public:
  // ---------------------------------------------------------------------------
  // Construction

  Scope(Zone* zone, Scope* outer_scope, ScopeType scope_type);

#ifdef DEBUG
  // The scope name is only used for printing/debugging.
  void SetScopeName(const AstRawString* scope_name) {
    scope_name_ = scope_name;
  }
#endif

  // TODO(verwaest): Is this needed on Scope?
  int num_parameters() const;

  DeclarationScope* AsDeclarationScope();
  const DeclarationScope* AsDeclarationScope() const;
  ModuleScope* AsModuleScope();
  const ModuleScope* AsModuleScope() const;

  class Snapshot final BASE_EMBEDDED {
   public:
    explicit Snapshot(Scope* scope);

    void Reparent(DeclarationScope* new_parent) const;

   private:
    Scope* outer_scope_;
    Scope* top_inner_scope_;
    VariableProxy* top_unresolved_;
    int top_temp_;
  };

  // Compute top scope and allocate variables. For lazy compilation the top
  // scope only contains the single lazily compiled function, so this
  // doesn't re-allocate variables repeatedly.
  static void Analyze(ParseInfo* info);

  enum class DeserializationMode { kDeserializeOffHeap, kKeepScopeInfo };

  static Scope* DeserializeScopeChain(Isolate* isolate, Zone* zone,
                                      Context* context,
                                      DeclarationScope* script_scope,
                                      AstValueFactory* ast_value_factory,
                                      DeserializationMode deserialization_mode);

  // Checks if the block scope is redundant, i.e. it does not contain any
  // block scoped declarations. In that case it is removed from the scope
  // tree and its children are reparented.
  Scope* FinalizeBlockScope();

  // Inserts outer_scope into this scope's scope chain (and removes this
  // from the current outer_scope_'s inner scope list).
  // Assumes outer_scope_ is non-null.
  void ReplaceOuterScope(Scope* outer_scope);

  // Propagates any eagerly-gathered scope usage flags (such as calls_eval())
  // to the passed-in scope.
  void PropagateUsageFlagsToScope(Scope* other);

  Zone* zone() const { return zone_; }

  // ---------------------------------------------------------------------------
  // Declarations

  // Lookup a variable in this scope. Returns the variable or NULL if not found.
  Variable* LookupLocal(const AstRawString* name);

  // Lookup a variable in this scope or outer scopes.
  // Returns the variable or NULL if not found.
  Variable* Lookup(const AstRawString* name);

  // Declare a local variable in this scope. If the variable has been
  // declared before, the previously declared variable is returned.
  Variable* DeclareLocal(const AstRawString* name, VariableMode mode,
                         InitializationFlag init_flag, Variable::Kind kind,
                         MaybeAssignedFlag maybe_assigned_flag = kNotAssigned);

  // Declarations list.
  ZoneList<Declaration*>* declarations() { return &decls_; }

  // Create a new unresolved variable.
  VariableProxy* NewUnresolved(AstNodeFactory* factory,
                               const AstRawString* name,
                               int start_position = kNoSourcePosition,
                               int end_position = kNoSourcePosition,
                               Variable::Kind kind = Variable::NORMAL) {
    // Note that we must not share the unresolved variables with
    // the same name because they may be removed selectively via
    // RemoveUnresolved().
    DCHECK(!already_resolved_);
    DCHECK_EQ(factory->zone(), zone());
    VariableProxy* proxy =
        factory->NewVariableProxy(name, kind, start_position, end_position);
    proxy->set_next_unresolved(unresolved_);
    unresolved_ = proxy;
    return proxy;
  }

  void AddUnresolved(VariableProxy* proxy) {
    DCHECK(!already_resolved_);
    DCHECK(!proxy->is_resolved());
    proxy->set_next_unresolved(unresolved_);
    unresolved_ = proxy;
  }

  // Remove a unresolved variable. During parsing, an unresolved variable
  // may have been added optimistically, but then only the variable name
  // was used (typically for labels). If the variable was not declared, the
  // addition introduced a new unresolved variable which may end up being
  // allocated globally as a "ghost" variable. RemoveUnresolved removes
  // such a variable again if it was added; otherwise this is a no-op.
  bool RemoveUnresolved(VariableProxy* var);

  // Creates a new temporary variable in this scope's TemporaryScope.  The
  // name is only used for printing and cannot be used to find the variable.
  // In particular, the only way to get hold of the temporary is by keeping the
  // Variable* around.  The name should not clash with a legitimate variable
  // names.
  // TODO(verwaest): Move to DeclarationScope?
  Variable* NewTemporary(const AstRawString* name);

  // Adds the specific declaration node to the list of declarations in
  // this scope. The declarations are processed as part of entering
  // the scope; see codegen.cc:ProcessDeclarations.
  void AddDeclaration(Declaration* declaration);

  // ---------------------------------------------------------------------------
  // Illegal redeclaration support.

  // Check if the scope has conflicting var
  // declarations, i.e. a var declaration that has been hoisted from a nested
  // scope over a let binding of the same name.
  Declaration* CheckConflictingVarDeclarations();

  // Check if the scope has a conflicting lexical declaration that has a name in
  // the given list. This is used to catch patterns like
  // `try{}catch(e){let e;}`,
  // which is an error even though the two 'e's are declared in different
  // scopes.
  Declaration* CheckLexDeclarationsConflictingWith(
      const ZoneList<const AstRawString*>& names);

  // ---------------------------------------------------------------------------
  // Scope-specific info.

  // Inform the scope and outer scopes that the corresponding code contains an
  // eval call.
  void RecordEvalCall() {
    scope_calls_eval_ = true;
    for (Scope* scope = this; scope != nullptr; scope = scope->outer_scope()) {
      scope->inner_scope_calls_eval_ = true;
    }
  }

  // Set the language mode flag (unless disabled by a global flag).
  void SetLanguageMode(LanguageMode language_mode) {
    DCHECK(!is_module_scope() || is_strict(language_mode));
    set_language_mode(language_mode);
  }

  // Inform the scope that the scope may execute declarations nonlinearly.
  // Currently, the only nonlinear scope is a switch statement. The name is
  // more general in case something else comes up with similar control flow,
  // for example the ability to break out of something which does not have
  // its own lexical scope.
  // The bit does not need to be stored on the ScopeInfo because none of
  // the three compilers will perform hole check elimination on a variable
  // located in VariableLocation::CONTEXT. So, direct eval and closures
  // will not expose holes.
  void SetNonlinear() { scope_nonlinear_ = true; }

  // Position in the source where this scope begins and ends.
  //
  // * For the scope of a with statement
  //     with (obj) stmt
  //   start position: start position of first token of 'stmt'
  //   end position: end position of last token of 'stmt'
  // * For the scope of a block
  //     { stmts }
  //   start position: start position of '{'
  //   end position: end position of '}'
  // * For the scope of a function literal or decalaration
  //     function fun(a,b) { stmts }
  //   start position: start position of '('
  //   end position: end position of '}'
  // * For the scope of a catch block
  //     try { stms } catch(e) { stmts }
  //   start position: start position of '('
  //   end position: end position of ')'
  // * For the scope of a for-statement
  //     for (let x ...) stmt
  //   start position: start position of '('
  //   end position: end position of last token of 'stmt'
  // * For the scope of a switch statement
  //     switch (tag) { cases }
  //   start position: start position of '{'
  //   end position: end position of '}'
  int start_position() const { return start_position_; }
  void set_start_position(int statement_pos) {
    start_position_ = statement_pos;
  }
  int end_position() const { return end_position_; }
  void set_end_position(int statement_pos) {
    end_position_ = statement_pos;
  }

  // Scopes created for desugaring are hidden. I.e. not visible to the debugger.
  bool is_hidden() const { return is_hidden_; }
  void set_is_hidden() { is_hidden_ = true; }

  // In some cases we want to force context allocation for a whole scope.
  void ForceContextAllocation() {
    DCHECK(!already_resolved_);
    force_context_allocation_ = true;
  }
  bool has_forced_context_allocation() const {
    return force_context_allocation_;
  }

  // ---------------------------------------------------------------------------
  // Predicates.

  // Specific scope types.
  bool is_eval_scope() const { return scope_type_ == EVAL_SCOPE; }
  bool is_function_scope() const { return scope_type_ == FUNCTION_SCOPE; }
  bool is_module_scope() const { return scope_type_ == MODULE_SCOPE; }
  bool is_script_scope() const { return scope_type_ == SCRIPT_SCOPE; }
  bool is_catch_scope() const { return scope_type_ == CATCH_SCOPE; }
  bool is_block_scope() const { return scope_type_ == BLOCK_SCOPE; }
  bool is_with_scope() const { return scope_type_ == WITH_SCOPE; }
  bool is_declaration_scope() const { return is_declaration_scope_; }

  // Information about which scopes calls eval.
  bool calls_eval() const { return scope_calls_eval_; }
  bool calls_sloppy_eval() const {
    return scope_calls_eval_ && is_sloppy(language_mode());
  }
  bool IsAsmModule() const;
  bool IsAsmFunction() const;
  // Does this scope have the potential to execute declarations non-linearly?
  bool is_nonlinear() const { return scope_nonlinear_; }

  // Whether this needs to be represented by a runtime context.
  bool NeedsContext() const {
    // Catch scopes always have heap slots.
    DCHECK(!is_catch_scope() || num_heap_slots() > 0);
    return num_heap_slots() > 0;
  }

  // ---------------------------------------------------------------------------
  // Accessors.

  // The type of this scope.
  ScopeType scope_type() const { return scope_type_; }

  // The language mode of this scope.
  LanguageMode language_mode() const { return is_strict_ ? STRICT : SLOPPY; }

  // inner_scope() and sibling() together implement the inner scope list of a
  // scope. Inner scope points to the an inner scope of the function, and
  // "sibling" points to a next inner scope of the outer scope of this scope.
  Scope* inner_scope() const { return inner_scope_; }
  Scope* sibling() const { return sibling_; }

  // The scope immediately surrounding this scope, or NULL.
  Scope* outer_scope() const { return outer_scope_; }

  const AstRawString* catch_variable_name() const {
    DCHECK(is_catch_scope());
    DCHECK_EQ(1, num_var());
    return static_cast<AstRawString*>(variables_.Start()->key);
  }

  // ---------------------------------------------------------------------------
  // Variable allocation.

  // Collect stack and context allocated local variables in this scope. Note
  // that the function variable - if present - is not collected and should be
  // handled separately.
  void CollectStackAndContextLocals(ZoneList<Variable*>* stack_locals,
                                    ZoneList<Variable*>* context_locals,
                                    ZoneList<Variable*>* context_globals);

  // Result of variable allocation.
  int num_stack_slots() const { return num_stack_slots_; }
  int num_heap_slots() const { return num_heap_slots_; }
  int num_global_slots() const { return num_global_slots_; }

  int StackLocalCount() const;
  int ContextLocalCount() const;
  int ContextGlobalCount() const;

  // Determine if we can parse a function literal in this scope lazily.
  bool AllowsLazyParsing() const;

  // The number of contexts between this and scope; zero if this == scope.
  int ContextChainLength(Scope* scope) const;

  // The number of contexts between this and the outermost context that has a
  // sloppy eval call. One if this->calls_sloppy_eval().
  int ContextChainLengthUntilOutermostSloppyEval() const;

  // The maximum number of nested contexts required for this scope and any inner
  // scopes.
  int MaxNestedContextChainLength();

  // Find the first function, script, eval or (declaration) block scope. This is
  // the scope where var declarations will be hoisted to in the implementation.
  DeclarationScope* GetDeclarationScope();

  // Find the first non-block declaration scope. This should be either a script,
  // function, or eval scope. Same as DeclarationScope(), but skips declaration
  // "block" scopes. Used for differentiating associated function objects (i.e.,
  // the scope for which a function prologue allocates a context) or declaring
  // temporaries.
  DeclarationScope* GetClosureScope();

  // Find the first (non-arrow) function or script scope.  This is where
  // 'this' is bound, and what determines the function kind.
  DeclarationScope* GetReceiverScope();

  // Creates a scope info if it doesn't already exist.
  Handle<ScopeInfo> GetScopeInfo(Isolate* isolate);

  // GetScopeInfo() must have been called once to create the ScopeInfo.
  Handle<ScopeInfo> scope_info() {
    DCHECK(!scope_info_.is_null());
    return scope_info_;
  }

  // ---------------------------------------------------------------------------
  // Strict mode support.
  bool IsDeclared(const AstRawString* name) {
    // During formal parameter list parsing the scope only contains
    // two variables inserted at initialization: "this" and "arguments".
    // "this" is an invalid parameter name and "arguments" is invalid parameter
    // name in strict mode. Therefore looking up with the map which includes
    // "this" and "arguments" in addition to all formal parameters is safe.
    return variables_.Lookup(name) != NULL;
  }

  int num_var() const { return variables_.occupancy(); }

  // ---------------------------------------------------------------------------
  // Debugging.

#ifdef DEBUG
  void Print(int n = 0);  // n = indentation; n < 0 => don't print recursively

  // Check that the scope has positions assigned.
  void CheckScopePositions();

  // Check that all Scopes in the scope tree use the same Zone.
  void CheckZones();
#endif

  // Retrieve `IsSimpleParameterList` of current or outer function.
  bool HasSimpleParameters();
  void set_is_debug_evaluate_scope() { is_debug_evaluate_scope_ = true; }

 protected:
  // Creates a script scope.
  explicit Scope(Zone* zone);

  void set_language_mode(LanguageMode language_mode) {
    is_strict_ = is_strict(language_mode);
  }

 private:
  Variable* Declare(Zone* zone, Scope* scope, const AstRawString* name,
                    VariableMode mode, Variable::Kind kind,
                    InitializationFlag initialization_flag,
                    MaybeAssignedFlag maybe_assigned_flag = kNotAssigned) {
    bool added;
    Variable* var =
        variables_.Declare(zone, scope, name, mode, kind, initialization_flag,
                           maybe_assigned_flag, &added);
    if (added) ordered_variables_.Add(var, zone);
    return var;
  }
  Zone* zone_;

  // Scope tree.
  Scope* outer_scope_;  // the immediately enclosing outer scope, or NULL
  Scope* inner_scope_;  // an inner scope of this scope
  Scope* sibling_;  // a sibling inner scope of the outer scope of this scope.

  // The variables declared in this scope:
  //
  // All user-declared variables (incl. parameters).  For script scopes
  // variables may be implicitly 'declared' by being used (possibly in
  // an inner scope) with no intervening with statements or eval calls.
  VariableMap variables_;
  // In case of non-scopeinfo-backed scopes, this contains the variables of the
  // map above in order of addition.
  // TODO(verwaest): Thread through Variable.
  ZoneList<Variable*> ordered_variables_;
  // Variables that must be looked up dynamically.
  DynamicScopePart* dynamics_;
  // Unresolved variables referred to from this scope. The proxies themselves
  // form a linked list of all unresolved proxies.
  VariableProxy* unresolved_;
  // Declarations.
  ZoneList<Declaration*> decls_;

  // Serialized scope info support.
  Handle<ScopeInfo> scope_info_;
// Debugging support.
#ifdef DEBUG
  const AstRawString* scope_name_;

  // True if it doesn't need scope resolution (e.g., if the scope was
  // constructed based on a serialized scope info or a catch context).
  bool already_resolved_ : 1;
#endif

  // Source positions.
  int start_position_;
  int end_position_;

  // Computed via AllocateVariables.
  int num_stack_slots_;
  int num_heap_slots_;
  int num_global_slots_;

  // The scope type.
  const ScopeType scope_type_;

  // Scope-specific information computed during parsing.
  //
  // The language mode of this scope.
  STATIC_ASSERT(LANGUAGE_END == 2);
  bool is_strict_ : 1;
  // This scope or a nested catch scope or with scope contain an 'eval' call. At
  // the 'eval' call site this scope is the declaration scope.
  bool scope_calls_eval_ : 1;
  // This scope's declarations might not be executed in order (e.g., switch).
  bool scope_nonlinear_ : 1;
  bool is_hidden_ : 1;
  // Temporary workaround that allows masking of 'this' in debug-evalute scopes.
  bool is_debug_evaluate_scope_ : 1;

  bool inner_scope_calls_eval_ : 1;
  bool force_context_allocation_ : 1;

  // True if it holds 'var' declarations.
  bool is_declaration_scope_ : 1;

  // Create a non-local variable with a given name.
  // These variables are looked up dynamically at runtime.
  Variable* NonLocal(const AstRawString* name, VariableMode mode);

  // Variable resolution.
  // Possible results of a recursive variable lookup telling if and how a
  // variable is bound. These are returned in the output parameter *binding_kind
  // of the LookupRecursive function.
  enum BindingKind {
    // The variable reference could be statically resolved to a variable binding
    // which is returned. There is no 'with' statement between the reference and
    // the binding and no scope between the reference scope (inclusive) and
    // binding scope (exclusive) makes a sloppy 'eval' call.
    BOUND,

    // The variable reference could be statically resolved to a variable binding
    // which is returned. There is no 'with' statement between the reference and
    // the binding, but some scope between the reference scope (inclusive) and
    // binding scope (exclusive) makes a sloppy 'eval' call, that might
    // possibly introduce variable bindings shadowing the found one. Thus the
    // found variable binding is just a guess.
    BOUND_EVAL_SHADOWED,

    // The variable reference could not be statically resolved to any binding
    // and thus should be considered referencing a global variable. NULL is
    // returned. The variable reference is not inside any 'with' statement and
    // no scope between the reference scope (inclusive) and script scope
    // (exclusive) makes a sloppy 'eval' call.
    UNBOUND,

    // The variable reference could not be statically resolved to any binding
    // NULL is returned. The variable reference is not inside any 'with'
    // statement, but some scope between the reference scope (inclusive) and
    // script scope (exclusive) makes a sloppy 'eval' call, that might
    // possibly introduce a variable binding. Thus the reference should be
    // considered referencing a global variable unless it is shadowed by an
    // 'eval' introduced binding.
    UNBOUND_EVAL_SHADOWED,

    // The variable could not be statically resolved and needs to be looked up
    // dynamically. NULL is returned. There are two possible reasons:
    // * A 'with' statement has been encountered and there is no variable
    //   binding for the name between the variable reference and the 'with'.
    //   The variable potentially references a property of the 'with' object.
    // * The code is being executed as part of a call to 'eval' and the calling
    //   context chain contains either a variable binding for the name or it
    //   contains a 'with' context.
    DYNAMIC_LOOKUP
  };

  // Lookup a variable reference given by name recursively starting with this
  // scope, and stopping when reaching the outer_scope_end scope. If the code is
  // executed because of a call to 'eval', the context parameter should be set
  // to the calling context of 'eval'.
  Variable* LookupRecursive(VariableProxy* proxy, BindingKind* binding_kind,
                            AstNodeFactory* factory,
                            Scope* outer_scope_end = nullptr);
  void ResolveTo(ParseInfo* info, BindingKind binding_kind,
                 VariableProxy* proxy, Variable* var);
  void ResolveVariable(ParseInfo* info, VariableProxy* proxy,
                       AstNodeFactory* factory);
  void ResolveVariablesRecursively(ParseInfo* info, AstNodeFactory* factory);

  // Finds free variables of this scope. This mutates the unresolved variables
  // list along the way, so full resolution cannot be done afterwards.
  // If a ParseInfo* is passed, non-free variables will be resolved.
  VariableProxy* FetchFreeVariables(DeclarationScope* max_outer_scope,
                                    ParseInfo* info = nullptr,
                                    VariableProxy* stack = nullptr);

  // Scope analysis.
  void PropagateScopeInfo();

  // Predicates.
  bool MustAllocate(Variable* var);
  bool MustAllocateInContext(Variable* var);

  // Variable allocation.
  void AllocateStackSlot(Variable* var);
  void AllocateHeapSlot(Variable* var);
  void AllocateNonParameterLocal(Variable* var);
  void AllocateDeclaredGlobal(Variable* var);
  void AllocateNonParameterLocalsAndDeclaredGlobals();
  void AllocateVariablesRecursively();

  // Construct a scope based on the scope info.
  Scope(Zone* zone, Scope* inner_scope, ScopeType type,
        Handle<ScopeInfo> scope_info);

  // Construct a catch scope with a binding for the name.
  Scope(Zone* zone, Scope* inner_scope,
        const AstRawString* catch_variable_name);

  void AddInnerScope(Scope* inner_scope) {
    inner_scope->sibling_ = inner_scope_;
    inner_scope_ = inner_scope;
    inner_scope->outer_scope_ = this;
  }

  void RemoveInnerScope(Scope* inner_scope) {
    DCHECK_NOT_NULL(inner_scope);
    if (inner_scope == inner_scope_) {
      inner_scope_ = inner_scope_->sibling_;
      return;
    }
    for (Scope* scope = inner_scope_; scope != nullptr;
         scope = scope->sibling_) {
      if (scope->sibling_ == inner_scope) {
        scope->sibling_ = scope->sibling_->sibling_;
        return;
      }
    }
  }

  void SetDefaults();

  void DeserializeScopeInfo(Isolate* isolate,
                            AstValueFactory* ast_value_factory);

  friend class DeclarationScope;
};

class DeclarationScope : public Scope {
 public:
  DeclarationScope(Zone* zone, Scope* outer_scope, ScopeType scope_type,
                   FunctionKind function_kind = kNormalFunction);
  DeclarationScope(Zone* zone, Scope* inner_scope, ScopeType scope_type,
                   Handle<ScopeInfo> scope_info);
  // Creates a script scope.
  explicit DeclarationScope(Zone* zone);

  bool IsDeclaredParameter(const AstRawString* name) {
    // If IsSimpleParameterList is false, duplicate parameters are not allowed,
    // however `arguments` may be allowed if function is not strict code. Thus,
    // the assumptions explained above do not hold.
    return params_.Contains(variables_.Lookup(name));
  }

  FunctionKind function_kind() const { return function_kind_; }

  bool is_arrow_scope() const {
    return is_function_scope() && IsArrowFunction(function_kind_);
  }

  // Inform the scope that the corresponding code uses "super".
  void RecordSuperPropertyUsage() { scope_uses_super_property_ = true; }
  // Does this scope access "super" property (super.foo).
  bool uses_super_property() const { return scope_uses_super_property_; }

  bool NeedsHomeObject() const {
    return scope_uses_super_property_ ||
           (inner_scope_calls_eval_ && (IsConciseMethod(function_kind()) ||
                                        IsAccessorFunction(function_kind()) ||
                                        IsClassConstructor(function_kind())));
  }

  bool asm_module() const { return asm_module_; }
  void set_asm_module() { asm_module_ = true; }
  bool asm_function() const { return asm_function_; }
  void set_asm_function() { asm_module_ = true; }

  void DeclareThis(AstValueFactory* ast_value_factory);
  void DeclareDefaultFunctionVariables(AstValueFactory* ast_value_factory);

  // This lookup corresponds to a lookup in the "intermediate" scope sitting
  // between this scope and the outer scope. (ECMA-262, 3rd., requires that
  // the name of named function literal is kept in an intermediate scope
  // in between this scope and the next outer scope.)
  Variable* LookupFunctionVar(const AstRawString* name);

  // Declare the function variable for a function literal. This variable
  // is in an intermediate scope between this function scope and the the
  // outer scope. Only possible for function scopes; at most one variable.
  Variable* DeclareFunctionVar(const AstRawString* name);

  // Declare a parameter in this scope.  When there are duplicated
  // parameters the rightmost one 'wins'.  However, the implementation
  // expects all parameters to be declared and from left to right.
  Variable* DeclareParameter(const AstRawString* name, VariableMode mode,
                             bool is_optional, bool is_rest, bool* is_duplicate,
                             AstValueFactory* ast_value_factory);

  // Declare an implicit global variable in this scope which must be a
  // script scope.  The variable was introduced (possibly from an inner
  // scope) by a reference to an unresolved variable with no intervening
  // with statements or eval calls.
  Variable* DeclareDynamicGlobal(const AstRawString* name,
                                 Variable::Kind variable_kind);

  // The variable corresponding to the 'this' value.
  Variable* receiver() {
    DCHECK(has_this_declaration());
    DCHECK_NOT_NULL(receiver_);
    return receiver_;
  }

  // TODO(wingo): Add a GLOBAL_SCOPE scope type which will lexically allocate
  // "this" (and no other variable) on the native context.  Script scopes then
  // will not have a "this" declaration.
  bool has_this_declaration() const {
    return (is_function_scope() && !is_arrow_scope()) || is_module_scope();
  }

  // The variable corresponding to the 'new.target' value.
  Variable* new_target_var() { return new_target_; }

  // The variable holding the function literal for named function
  // literals, or NULL.  Only valid for function scopes.
  Variable* function_var() const {
    DCHECK(is_function_scope());
    return function_;
  }

  // Parameters. The left-most parameter has index 0.
  // Only valid for function scopes.
  Variable* parameter(int index) const {
    DCHECK(is_function_scope());
    return params_[index];
  }

  // Returns the default function arity excluding default or rest parameters.
  int default_function_length() const { return arity_; }

  // Returns the number of formal parameters, up to but not including the
  // rest parameter index (if the function has rest parameters), i.e. it
  // says 2 for
  //
  //   function foo(a, b) { ... }
  //
  // and
  //
  //   function foo(a, b, ...c) { ... }
  //
  // but for
  //
  //   function foo(a, b, c = 1) { ... }
  //
  // we return 3 here.
  int num_parameters() const {
    return has_rest_parameter() ? params_.length() - 1 : params_.length();
  }

  // A function can have at most one rest parameter. Returns Variable* or NULL.
  Variable* rest_parameter(int* index) const {
    *index = rest_index_;
    if (rest_index_ < 0) return nullptr;
    return params_[rest_index_];
  }

  bool has_rest_parameter() const { return rest_index_ >= 0; }

  bool has_simple_parameters() const { return has_simple_parameters_; }

  // TODO(caitp): manage this state in a better way. PreParser must be able to
  // communicate that the scope is non-simple, without allocating any parameters
  // as the Parser does. This is necessary to ensure that TC39's proposed early
  // error can be reported consistently regardless of whether lazily parsed or
  // not.
  void SetHasNonSimpleParameters() {
    DCHECK(is_function_scope());
    has_simple_parameters_ = false;
  }

  // The local variable 'arguments' if we need to allocate it; NULL otherwise.
  Variable* arguments() const {
    DCHECK(!is_arrow_scope() || arguments_ == nullptr);
    return arguments_;
  }

  Variable* this_function_var() const {
    // This is only used in derived constructors atm.
    DCHECK(this_function_ == nullptr ||
           (is_function_scope() && (IsClassConstructor(function_kind()) ||
                                    IsConciseMethod(function_kind()) ||
                                    IsAccessorFunction(function_kind()))));
    return this_function_;
  }

  // Adds a temporary variable in this scope's TemporaryScope. This is for
  // adjusting the scope of temporaries used when desugaring parameter
  // initializers.
  void AddTemporary(Variable* var) {
    DCHECK(!already_resolved_);
    // Temporaries are only placed in ClosureScopes.
    DCHECK_EQ(GetClosureScope(), this);
    temps_.Add(var, zone());
  }

  ZoneList<Variable*>* temps() { return &temps_; }

  void DeclareSloppyBlockFunction(const AstRawString* name,
                                  SloppyBlockFunctionStatement* statement) {
    sloppy_block_function_map_.Declare(zone(), name, statement);
  }

  SloppyBlockFunctionMap* sloppy_block_function_map() {
    return &sloppy_block_function_map_;
  }

  // Resolve and fill in the allocation information for all variables
  // in this scopes. Must be called *after* all scopes have been
  // processed (parsed) to ensure that unresolved variables can be
  // resolved properly.
  //
  // In the case of code compiled and run using 'eval', the context
  // parameter is the context in which eval was called.  In all other
  // cases the context parameter is an empty handle.
  void AllocateVariables(ParseInfo* info, AstNodeFactory* factory);

  // To be called during parsing. Do just enough scope analysis that we can
  // discard the Scope for lazily compiled functions. In particular, this
  // records variables which cannot be resolved inside the Scope (we don't yet
  // know what they will resolve to since the outer Scopes are incomplete) and
  // migrates them into migrate_to.
  void AnalyzePartially(DeclarationScope* migrate_to,
                        AstNodeFactory* ast_node_factory);

  Handle<StringSet> CollectNonLocals(ParseInfo* info,
                                     Handle<StringSet> non_locals);

  // Determine if we can use lazy compilation for this scope.
  bool AllowsLazyCompilation() const;

  // Determine if we can use lazy compilation for this scope without a context.
  bool AllowsLazyCompilationWithoutContext() const;

  // Make sure this closure and all outer closures are eagerly compiled.
  void ForceEagerCompilation() {
    DCHECK_EQ(this, GetClosureScope());
    for (DeclarationScope* s = this; !s->is_script_scope();
         s = s->outer_scope()->GetClosureScope()) {
      s->force_eager_compilation_ = true;
    }
  }

#ifdef DEBUG
  void PrintParameters();
#endif

  void AllocateLocals();
  void AllocateParameterLocals();
  void AllocateReceiver();

 private:
  void AllocateParameter(Variable* var, int index);

  void SetDefaults();

  // If the scope is a function scope, this is the function kind.
  const FunctionKind function_kind_;

  bool has_simple_parameters_ : 1;
  // This scope contains an "use asm" annotation.
  bool asm_module_ : 1;
  // This scope's outer context is an asm module.
  bool asm_function_ : 1;
  bool force_eager_compilation_ : 1;
  // This scope has a parameter called "arguments".
  bool has_arguments_parameter_ : 1;
  // This scope uses "super" property ('super.foo').
  bool scope_uses_super_property_ : 1;

  // Info about the parameter list of a function.
  int arity_;
  int rest_index_;
  // Compiler-allocated (user-invisible) temporaries.
  ZoneList<Variable*> temps_;
  // Parameter list in source order.
  ZoneList<Variable*> params_;
  // Map of function names to lists of functions defined in sloppy blocks
  SloppyBlockFunctionMap sloppy_block_function_map_;
  // Convenience variable.
  Variable* receiver_;
  // Function variable, if any; function scopes only.
  Variable* function_;
  // new.target variable, function scopes only.
  Variable* new_target_;
  // Convenience variable; function scopes only.
  Variable* arguments_;
  // Convenience variable; Subclass constructor only
  Variable* this_function_;
};

class ModuleScope final : public DeclarationScope {
 public:
  ModuleScope(Zone* zone, DeclarationScope* script_scope,
              AstValueFactory* ast_value_factory);

  ModuleDescriptor* module() const {
    DCHECK_NOT_NULL(module_descriptor_);
    return module_descriptor_;
  }

  // Set MODULE as VariableLocation for all variables that will live in some
  // module's export table.
  void AllocateModuleVariables();

 private:
  ModuleDescriptor* module_descriptor_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_SCOPES_H_
