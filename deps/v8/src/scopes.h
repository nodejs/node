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

#ifndef V8_SCOPES_H_
#define V8_SCOPES_H_

#include "ast.h"
#include "zone.h"

namespace v8 {
namespace internal {

class CompilationInfo;


// A hash map to support fast variable declaration and lookup.
class VariableMap: public ZoneHashMap {
 public:
  explicit VariableMap(Zone* zone);

  virtual ~VariableMap();

  Variable* Declare(Scope* scope,
                    Handle<String> name,
                    VariableMode mode,
                    bool is_valid_lhs,
                    Variable::Kind kind,
                    InitializationFlag initialization_flag,
                    Interface* interface = Interface::NewValue());

  Variable* Lookup(Handle<String> name);

  Zone* zone() const { return zone_; }

 private:
  Zone* zone_;
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
    ASSERT(index >= 0 && index < 3);
    return maps_[index];
  }

 private:
  VariableMap *maps_[3];
};


// Global invariants after AST construction: Each reference (i.e. identifier)
// to a JavaScript variable (including global properties) is represented by a
// VariableProxy node. Immediately after AST construction and before variable
// allocation, most VariableProxy nodes are "unresolved", i.e. not bound to a
// corresponding variable (though some are bound during parse time). Variable
// allocation binds each unresolved VariableProxy to one Variable and assigns
// a location. Note that many VariableProxy nodes may refer to the same Java-
// Script variable.

class Scope: public ZoneObject {
 public:
  // ---------------------------------------------------------------------------
  // Construction

  Scope(Scope* outer_scope, ScopeType type, Zone* zone);

  // Compute top scope and allocate variables. For lazy compilation the top
  // scope only contains the single lazily compiled function, so this
  // doesn't re-allocate variables repeatedly.
  static bool Analyze(CompilationInfo* info);

  static Scope* DeserializeScopeChain(Context* context, Scope* global_scope,
                                      Zone* zone);

  // The scope name is only used for printing/debugging.
  void SetScopeName(Handle<String> scope_name) { scope_name_ = scope_name; }

  void Initialize();

  // Checks if the block scope is redundant, i.e. it does not contain any
  // block scoped declarations. In that case it is removed from the scope
  // tree and its children are reparented.
  Scope* FinalizeBlockScope();

  Zone* zone() const { return zone_; }

  // ---------------------------------------------------------------------------
  // Declarations

  // Lookup a variable in this scope. Returns the variable or NULL if not found.
  Variable* LocalLookup(Handle<String> name);

  // This lookup corresponds to a lookup in the "intermediate" scope sitting
  // between this scope and the outer scope. (ECMA-262, 3rd., requires that
  // the name of named function literal is kept in an intermediate scope
  // in between this scope and the next outer scope.)
  Variable* LookupFunctionVar(Handle<String> name,
                              AstNodeFactory<AstNullVisitor>* factory);

  // Lookup a variable in this scope or outer scopes.
  // Returns the variable or NULL if not found.
  Variable* Lookup(Handle<String> name);

  // Declare the function variable for a function literal. This variable
  // is in an intermediate scope between this function scope and the the
  // outer scope. Only possible for function scopes; at most one variable.
  void DeclareFunctionVar(VariableDeclaration* declaration) {
    ASSERT(is_function_scope());
    function_ = declaration;
  }

  // Declare a parameter in this scope.  When there are duplicated
  // parameters the rightmost one 'wins'.  However, the implementation
  // expects all parameters to be declared and from left to right.
  void DeclareParameter(Handle<String> name, VariableMode mode);

  // Declare a local variable in this scope. If the variable has been
  // declared before, the previously declared variable is returned.
  Variable* DeclareLocal(Handle<String> name,
                         VariableMode mode,
                         InitializationFlag init_flag,
                         Interface* interface = Interface::NewValue());

  // Declare an implicit global variable in this scope which must be a
  // global scope.  The variable was introduced (possibly from an inner
  // scope) by a reference to an unresolved variable with no intervening
  // with statements or eval calls.
  Variable* DeclareDynamicGlobal(Handle<String> name);

  // Create a new unresolved variable.
  template<class Visitor>
  VariableProxy* NewUnresolved(AstNodeFactory<Visitor>* factory,
                               Handle<String> name,
                               Interface* interface = Interface::NewValue(),
                               int position = RelocInfo::kNoPosition) {
    // Note that we must not share the unresolved variables with
    // the same name because they may be removed selectively via
    // RemoveUnresolved().
    ASSERT(!already_resolved());
    VariableProxy* proxy =
        factory->NewVariableProxy(name, false, interface, position);
    unresolved_.Add(proxy, zone_);
    return proxy;
  }

  // Remove a unresolved variable. During parsing, an unresolved variable
  // may have been added optimistically, but then only the variable name
  // was used (typically for labels). If the variable was not declared, the
  // addition introduced a new unresolved variable which may end up being
  // allocated globally as a "ghost" variable. RemoveUnresolved removes
  // such a variable again if it was added; otherwise this is a no-op.
  void RemoveUnresolved(VariableProxy* var);

  // Creates a new internal variable in this scope.  The name is only used
  // for printing and cannot be used to find the variable.  In particular,
  // the only way to get hold of the temporary is by keeping the Variable*
  // around.
  Variable* NewInternal(Handle<String> name);

  // Creates a new temporary variable in this scope.  The name is only used
  // for printing and cannot be used to find the variable.  In particular,
  // the only way to get hold of the temporary is by keeping the Variable*
  // around.  The name should not clash with a legitimate variable names.
  Variable* NewTemporary(Handle<String> name);

  // Adds the specific declaration node to the list of declarations in
  // this scope. The declarations are processed as part of entering
  // the scope; see codegen.cc:ProcessDeclarations.
  void AddDeclaration(Declaration* declaration);

  // ---------------------------------------------------------------------------
  // Illegal redeclaration support.

  // Set an expression node that will be executed when the scope is
  // entered. We only keep track of one illegal redeclaration node per
  // scope - the first one - so if you try to set it multiple times
  // the additional requests will be silently ignored.
  void SetIllegalRedeclaration(Expression* expression);

  // Visit the illegal redeclaration expression. Do not call if the
  // scope doesn't have an illegal redeclaration node.
  void VisitIllegalRedeclaration(AstVisitor* visitor);

  // Check if the scope has (at least) one illegal redeclaration.
  bool HasIllegalRedeclaration() const { return illegal_redecl_ != NULL; }

  // For harmony block scoping mode: Check if the scope has conflicting var
  // declarations, i.e. a var declaration that has been hoisted from a nested
  // scope over a let binding of the same name.
  Declaration* CheckConflictingVarDeclarations();

  // For harmony block scoping mode: Check if the scope has variable proxies
  // that are used as lvalues and point to const variables. Assumes that scopes
  // have been analyzed and variables been resolved.
  VariableProxy* CheckAssignmentToConst();

  // ---------------------------------------------------------------------------
  // Scope-specific info.

  // Inform the scope that the corresponding code contains a with statement.
  void RecordWithStatement() { scope_contains_with_ = true; }

  // Inform the scope that the corresponding code contains an eval call.
  void RecordEvalCall() { if (!is_global_scope()) scope_calls_eval_ = true; }

  // Set the strict mode flag (unless disabled by a global flag).
  void SetLanguageMode(LanguageMode language_mode) {
    language_mode_ = language_mode;
  }

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
  int start_position() const { return start_position_; }
  void set_start_position(int statement_pos) {
    start_position_ = statement_pos;
  }
  int end_position() const { return end_position_; }
  void set_end_position(int statement_pos) {
    end_position_ = statement_pos;
  }

  // ---------------------------------------------------------------------------
  // Predicates.

  // Specific scope types.
  bool is_eval_scope() const { return type_ == EVAL_SCOPE; }
  bool is_function_scope() const { return type_ == FUNCTION_SCOPE; }
  bool is_module_scope() const { return type_ == MODULE_SCOPE; }
  bool is_global_scope() const { return type_ == GLOBAL_SCOPE; }
  bool is_catch_scope() const { return type_ == CATCH_SCOPE; }
  bool is_block_scope() const { return type_ == BLOCK_SCOPE; }
  bool is_with_scope() const { return type_ == WITH_SCOPE; }
  bool is_declaration_scope() const {
    return is_eval_scope() || is_function_scope() ||
        is_module_scope() || is_global_scope();
  }
  bool is_classic_mode() const {
    return language_mode() == CLASSIC_MODE;
  }
  bool is_extended_mode() const {
    return language_mode() == EXTENDED_MODE;
  }
  bool is_strict_or_extended_eval_scope() const {
    return is_eval_scope() && !is_classic_mode();
  }

  // Information about which scopes calls eval.
  bool calls_eval() const { return scope_calls_eval_; }
  bool calls_non_strict_eval() {
    return scope_calls_eval_ && is_classic_mode();
  }
  bool outer_scope_calls_non_strict_eval() const {
    return outer_scope_calls_non_strict_eval_;
  }

  // Is this scope inside a with statement.
  bool inside_with() const { return scope_inside_with_; }
  // Does this scope contain a with statement.
  bool contains_with() const { return scope_contains_with_; }

  // ---------------------------------------------------------------------------
  // Accessors.

  // The type of this scope.
  ScopeType type() const { return type_; }

  // The language mode of this scope.
  LanguageMode language_mode() const { return language_mode_; }

  // The variable corresponding the 'this' value.
  Variable* receiver() { return receiver_; }

  // The variable holding the function literal for named function
  // literals, or NULL.  Only valid for function scopes.
  VariableDeclaration* function() const {
    ASSERT(is_function_scope());
    return function_;
  }

  // Parameters. The left-most parameter has index 0.
  // Only valid for function scopes.
  Variable* parameter(int index) const {
    ASSERT(is_function_scope());
    return params_[index];
  }

  int num_parameters() const { return params_.length(); }

  // The local variable 'arguments' if we need to allocate it; NULL otherwise.
  Variable* arguments() const { return arguments_; }

  // Declarations list.
  ZoneList<Declaration*>* declarations() { return &decls_; }

  // Inner scope list.
  ZoneList<Scope*>* inner_scopes() { return &inner_scopes_; }

  // The scope immediately surrounding this scope, or NULL.
  Scope* outer_scope() const { return outer_scope_; }

  // The interface as inferred so far; only for module scopes.
  Interface* interface() const { return interface_; }

  // ---------------------------------------------------------------------------
  // Variable allocation.

  // Collect stack and context allocated local variables in this scope. Note
  // that the function variable - if present - is not collected and should be
  // handled separately.
  void CollectStackAndContextLocals(ZoneList<Variable*>* stack_locals,
                                    ZoneList<Variable*>* context_locals);

  // Current number of var or const locals.
  int num_var_or_const() { return num_var_or_const_; }

  // Result of variable allocation.
  int num_stack_slots() const { return num_stack_slots_; }
  int num_heap_slots() const { return num_heap_slots_; }

  int StackLocalCount() const;
  int ContextLocalCount() const;

  // For global scopes, the number of module literals (including nested ones).
  int num_modules() const { return num_modules_; }

  // For module scopes, the host scope's internal variable binding this module.
  Variable* module_var() const { return module_var_; }

  // Make sure this scope and all outer scopes are eagerly compiled.
  void ForceEagerCompilation()  { force_eager_compilation_ = true; }

  // Determine if we can use lazy compilation for this scope.
  bool AllowsLazyCompilation() const;

  // Determine if we can use lazy compilation for this scope without a context.
  bool AllowsLazyCompilationWithoutContext() const;

  // True if the outer context of this scope is always the native context.
  bool HasTrivialOuterContext() const;

  // True if the outer context allows lazy compilation of this scope.
  bool HasLazyCompilableOuterContext() const;

  // The number of contexts between this and scope; zero if this == scope.
  int ContextChainLength(Scope* scope);

  // Find the innermost global scope.
  Scope* GlobalScope();

  // Find the first function, global, or eval scope.  This is the scope
  // where var declarations will be hoisted to in the implementation.
  Scope* DeclarationScope();

  Handle<ScopeInfo> GetScopeInfo();

  // Get the chain of nested scopes within this scope for the source statement
  // position. The scopes will be added to the list from the outermost scope to
  // the innermost scope. Only nested block, catch or with scopes are tracked
  // and will be returned, but no inner function scopes.
  void GetNestedScopeChain(List<Handle<ScopeInfo> >* chain,
                           int statement_position);

  // ---------------------------------------------------------------------------
  // Strict mode support.
  bool IsDeclared(Handle<String> name) {
    // During formal parameter list parsing the scope only contains
    // two variables inserted at initialization: "this" and "arguments".
    // "this" is an invalid parameter name and "arguments" is invalid parameter
    // name in strict mode. Therefore looking up with the map which includes
    // "this" and "arguments" in addition to all formal parameters is safe.
    return variables_.Lookup(name) != NULL;
  }

  // ---------------------------------------------------------------------------
  // Debugging.

#ifdef DEBUG
  void Print(int n = 0);  // n = indentation; n < 0 => don't print recursively
#endif

  // ---------------------------------------------------------------------------
  // Implementation.
 protected:
  friend class ParserFactory;

  Isolate* const isolate_;

  // Scope tree.
  Scope* outer_scope_;  // the immediately enclosing outer scope, or NULL
  ZoneList<Scope*> inner_scopes_;  // the immediately enclosed inner scopes

  // The scope type.
  ScopeType type_;

  // Debugging support.
  Handle<String> scope_name_;

  // The variables declared in this scope:
  //
  // All user-declared variables (incl. parameters).  For global scopes
  // variables may be implicitly 'declared' by being used (possibly in
  // an inner scope) with no intervening with statements or eval calls.
  VariableMap variables_;
  // Compiler-allocated (user-invisible) internals.
  ZoneList<Variable*> internals_;
  // Compiler-allocated (user-invisible) temporaries.
  ZoneList<Variable*> temps_;
  // Parameter list in source order.
  ZoneList<Variable*> params_;
  // Variables that must be looked up dynamically.
  DynamicScopePart* dynamics_;
  // Unresolved variables referred to from this scope.
  ZoneList<VariableProxy*> unresolved_;
  // Declarations.
  ZoneList<Declaration*> decls_;
  // Convenience variable.
  Variable* receiver_;
  // Function variable, if any; function scopes only.
  VariableDeclaration* function_;
  // Convenience variable; function scopes only.
  Variable* arguments_;
  // Interface; module scopes only.
  Interface* interface_;

  // Illegal redeclaration.
  Expression* illegal_redecl_;

  // Scope-specific information computed during parsing.
  //
  // This scope is inside a 'with' of some outer scope.
  bool scope_inside_with_;
  // This scope contains a 'with' statement.
  bool scope_contains_with_;
  // This scope or a nested catch scope or with scope contain an 'eval' call. At
  // the 'eval' call site this scope is the declaration scope.
  bool scope_calls_eval_;
  // The language mode of this scope.
  LanguageMode language_mode_;
  // Source positions.
  int start_position_;
  int end_position_;

  // Computed via PropagateScopeInfo.
  bool outer_scope_calls_non_strict_eval_;
  bool inner_scope_calls_eval_;
  bool force_eager_compilation_;

  // True if it doesn't need scope resolution (e.g., if the scope was
  // constructed based on a serialized scope info or a catch context).
  bool already_resolved_;

  // Computed as variables are declared.
  int num_var_or_const_;

  // Computed via AllocateVariables; function, block and catch scopes only.
  int num_stack_slots_;
  int num_heap_slots_;

  // The number of modules (including nested ones).
  int num_modules_;

  // For module scopes, the host scope's internal variable binding this module.
  Variable* module_var_;

  // Serialized scope info support.
  Handle<ScopeInfo> scope_info_;
  bool already_resolved() { return already_resolved_; }

  // Create a non-local variable with a given name.
  // These variables are looked up dynamically at runtime.
  Variable* NonLocal(Handle<String> name, VariableMode mode);

  // Variable resolution.
  // Possible results of a recursive variable lookup telling if and how a
  // variable is bound. These are returned in the output parameter *binding_kind
  // of the LookupRecursive function.
  enum BindingKind {
    // The variable reference could be statically resolved to a variable binding
    // which is returned. There is no 'with' statement between the reference and
    // the binding and no scope between the reference scope (inclusive) and
    // binding scope (exclusive) makes a non-strict 'eval' call.
    BOUND,

    // The variable reference could be statically resolved to a variable binding
    // which is returned. There is no 'with' statement between the reference and
    // the binding, but some scope between the reference scope (inclusive) and
    // binding scope (exclusive) makes a non-strict 'eval' call, that might
    // possibly introduce variable bindings shadowing the found one. Thus the
    // found variable binding is just a guess.
    BOUND_EVAL_SHADOWED,

    // The variable reference could not be statically resolved to any binding
    // and thus should be considered referencing a global variable. NULL is
    // returned. The variable reference is not inside any 'with' statement and
    // no scope between the reference scope (inclusive) and global scope
    // (exclusive) makes a non-strict 'eval' call.
    UNBOUND,

    // The variable reference could not be statically resolved to any binding
    // NULL is returned. The variable reference is not inside any 'with'
    // statement, but some scope between the reference scope (inclusive) and
    // global scope (exclusive) makes a non-strict 'eval' call, that might
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
  // scope. If the code is executed because of a call to 'eval', the context
  // parameter should be set to the calling context of 'eval'.
  Variable* LookupRecursive(Handle<String> name,
                            BindingKind* binding_kind,
                            AstNodeFactory<AstNullVisitor>* factory);
  MUST_USE_RESULT
  bool ResolveVariable(CompilationInfo* info,
                       VariableProxy* proxy,
                       AstNodeFactory<AstNullVisitor>* factory);
  MUST_USE_RESULT
  bool ResolveVariablesRecursively(CompilationInfo* info,
                                   AstNodeFactory<AstNullVisitor>* factory);

  // Scope analysis.
  bool PropagateScopeInfo(bool outer_scope_calls_non_strict_eval);
  bool HasTrivialContext() const;

  // Predicates.
  bool MustAllocate(Variable* var);
  bool MustAllocateInContext(Variable* var);
  bool HasArgumentsParameter();

  // Variable allocation.
  void AllocateStackSlot(Variable* var);
  void AllocateHeapSlot(Variable* var);
  void AllocateParameterLocals();
  void AllocateNonParameterLocal(Variable* var);
  void AllocateNonParameterLocals();
  void AllocateVariablesRecursively();
  void AllocateModulesRecursively(Scope* host_scope);

  // Resolve and fill in the allocation information for all variables
  // in this scopes. Must be called *after* all scopes have been
  // processed (parsed) to ensure that unresolved variables can be
  // resolved properly.
  //
  // In the case of code compiled and run using 'eval', the context
  // parameter is the context in which eval was called.  In all other
  // cases the context parameter is an empty handle.
  MUST_USE_RESULT
  bool AllocateVariables(CompilationInfo* info,
                         AstNodeFactory<AstNullVisitor>* factory);

 private:
  // Construct a scope based on the scope info.
  Scope(Scope* inner_scope, ScopeType type, Handle<ScopeInfo> scope_info,
        Zone* zone);

  // Construct a catch scope with a binding for the name.
  Scope(Scope* inner_scope, Handle<String> catch_variable_name, Zone* zone);

  void AddInnerScope(Scope* inner_scope) {
    if (inner_scope != NULL) {
      inner_scopes_.Add(inner_scope, zone_);
      inner_scope->outer_scope_ = this;
    }
  }

  void SetDefaults(ScopeType type,
                   Scope* outer_scope,
                   Handle<ScopeInfo> scope_info);

  Zone* zone_;
};

} }  // namespace v8::internal

#endif  // V8_SCOPES_H_
