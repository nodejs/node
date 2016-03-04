// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_SCOPES_H_
#define V8_AST_SCOPES_H_

#include "src/ast/ast.h"
#include "src/hashmap.h"
#include "src/pending-compilation-error-handler.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

class ParseInfo;

// A hash map to support fast variable declaration and lookup.
class VariableMap: public ZoneHashMap {
 public:
  explicit VariableMap(Zone* zone);

  virtual ~VariableMap();

  Variable* Declare(Scope* scope, const AstRawString* name, VariableMode mode,
                    Variable::Kind kind, InitializationFlag initialization_flag,
                    MaybeAssignedFlag maybe_assigned_flag = kNotAssigned,
                    int declaration_group_start = -1);

  Variable* Lookup(const AstRawString* name);

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

  virtual ~SloppyBlockFunctionMap();

  void Declare(const AstRawString* name,
               SloppyBlockFunctionStatement* statement);

  typedef ZoneVector<SloppyBlockFunctionStatement*> Vector;

 private:
  Zone* zone_;
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

  Scope(Zone* zone, Scope* outer_scope, ScopeType scope_type,
        AstValueFactory* value_factory,
        FunctionKind function_kind = kNormalFunction);

  // Compute top scope and allocate variables. For lazy compilation the top
  // scope only contains the single lazily compiled function, so this
  // doesn't re-allocate variables repeatedly.
  static bool Analyze(ParseInfo* info);

  static Scope* DeserializeScopeChain(Isolate* isolate, Zone* zone,
                                      Context* context, Scope* script_scope);

  // The scope name is only used for printing/debugging.
  void SetScopeName(const AstRawString* scope_name) {
    scope_name_ = scope_name;
  }

  void Initialize();

  // Checks if the block scope is redundant, i.e. it does not contain any
  // block scoped declarations. In that case it is removed from the scope
  // tree and its children are reparented.
  Scope* FinalizeBlockScope();

  // Inserts outer_scope into this scope's scope chain (and removes this
  // from the current outer_scope_'s inner_scopes_).
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

  // This lookup corresponds to a lookup in the "intermediate" scope sitting
  // between this scope and the outer scope. (ECMA-262, 3rd., requires that
  // the name of named function literal is kept in an intermediate scope
  // in between this scope and the next outer scope.)
  Variable* LookupFunctionVar(const AstRawString* name,
                              AstNodeFactory* factory);

  // Lookup a variable in this scope or outer scopes.
  // Returns the variable or NULL if not found.
  Variable* Lookup(const AstRawString* name);

  // Declare the function variable for a function literal. This variable
  // is in an intermediate scope between this function scope and the the
  // outer scope. Only possible for function scopes; at most one variable.
  void DeclareFunctionVar(VariableDeclaration* declaration) {
    DCHECK(is_function_scope());
    // Handle implicit declaration of the function name in named function
    // expressions before other declarations.
    decls_.InsertAt(0, declaration, zone());
    function_ = declaration;
  }

  // Declare a parameter in this scope.  When there are duplicated
  // parameters the rightmost one 'wins'.  However, the implementation
  // expects all parameters to be declared and from left to right.
  Variable* DeclareParameter(
      const AstRawString* name, VariableMode mode,
      bool is_optional, bool is_rest, bool* is_duplicate);

  // Declare a local variable in this scope. If the variable has been
  // declared before, the previously declared variable is returned.
  Variable* DeclareLocal(const AstRawString* name, VariableMode mode,
                         InitializationFlag init_flag, Variable::Kind kind,
                         MaybeAssignedFlag maybe_assigned_flag = kNotAssigned,
                         int declaration_group_start = -1);

  // Declare an implicit global variable in this scope which must be a
  // script scope.  The variable was introduced (possibly from an inner
  // scope) by a reference to an unresolved variable with no intervening
  // with statements or eval calls.
  Variable* DeclareDynamicGlobal(const AstRawString* name);

  // Create a new unresolved variable.
  VariableProxy* NewUnresolved(AstNodeFactory* factory,
                               const AstRawString* name,
                               Variable::Kind kind = Variable::NORMAL,
                               int start_position = RelocInfo::kNoPosition,
                               int end_position = RelocInfo::kNoPosition) {
    // Note that we must not share the unresolved variables with
    // the same name because they may be removed selectively via
    // RemoveUnresolved().
    DCHECK(!already_resolved());
    VariableProxy* proxy =
        factory->NewVariableProxy(name, kind, start_position, end_position);
    unresolved_.Add(proxy, zone_);
    return proxy;
  }

  void AddUnresolved(VariableProxy* proxy) {
    DCHECK(!already_resolved());
    DCHECK(!proxy->is_resolved());
    unresolved_.Add(proxy, zone_);
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
  Variable* NewTemporary(const AstRawString* name);

  // Remove a temporary variable. This is for adjusting the scope of
  // temporaries used when desugaring parameter initializers.
  bool RemoveTemporary(Variable* var);

  // Adds a temporary variable in this scope's TemporaryScope. This is for
  // adjusting the scope of temporaries used when desugaring parameter
  // initializers.
  void AddTemporary(Variable* var) { temps_.Add(var, zone()); }

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

  // Retrieve the illegal redeclaration expression. Do not call if the
  // scope doesn't have an illegal redeclaration node.
  Expression* GetIllegalRedeclaration();

  // Check if the scope has (at least) one illegal redeclaration.
  bool HasIllegalRedeclaration() const { return illegal_redecl_ != NULL; }

  // For harmony block scoping mode: Check if the scope has conflicting var
  // declarations, i.e. a var declaration that has been hoisted from a nested
  // scope over a let binding of the same name.
  Declaration* CheckConflictingVarDeclarations();

  // ---------------------------------------------------------------------------
  // Scope-specific info.

  // Inform the scope that the corresponding code contains a with statement.
  void RecordWithStatement() { scope_contains_with_ = true; }

  // Inform the scope that the corresponding code contains an eval call.
  void RecordEvalCall() { scope_calls_eval_ = true; }

  // Inform the scope that the corresponding code uses "arguments".
  void RecordArgumentsUsage() { scope_uses_arguments_ = true; }

  // Inform the scope that the corresponding code uses "super".
  void RecordSuperPropertyUsage() { scope_uses_super_property_ = true; }

  // Set the language mode flag (unless disabled by a global flag).
  void SetLanguageMode(LanguageMode language_mode) {
    language_mode_ = language_mode;
  }

  // Set the ASM module flag.
  void SetAsmModule() { asm_module_ = true; }

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

  // In some cases we want to force context allocation for a whole scope.
  void ForceContextAllocation() {
    DCHECK(!already_resolved());
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
  bool is_arrow_scope() const {
    return is_function_scope() && IsArrowFunction(function_kind_);
  }
  bool is_declaration_scope() const { return is_declaration_scope_; }

  void set_is_declaration_scope() { is_declaration_scope_ = true; }

  // Information about which scopes calls eval.
  bool calls_eval() const { return scope_calls_eval_; }
  bool calls_sloppy_eval() const {
    return scope_calls_eval_ && is_sloppy(language_mode_);
  }
  bool outer_scope_calls_sloppy_eval() const {
    return outer_scope_calls_sloppy_eval_;
  }
  bool asm_module() const { return asm_module_; }
  bool asm_function() const { return asm_function_; }

  // Is this scope inside a with statement.
  bool inside_with() const { return scope_inside_with_; }

  // Does this scope access "arguments".
  bool uses_arguments() const { return scope_uses_arguments_; }
  // Does this scope access "super" property (super.foo).
  bool uses_super_property() const { return scope_uses_super_property_; }
  // Does this scope have the potential to execute declarations non-linearly?
  bool is_nonlinear() const { return scope_nonlinear_; }

  // Whether this needs to be represented by a runtime context.
  bool NeedsContext() const {
    // Catch and module scopes always have heap slots.
    DCHECK(!is_catch_scope() || num_heap_slots() > 0);
    DCHECK(!is_module_scope() || num_heap_slots() > 0);
    return is_with_scope() || num_heap_slots() > 0;
  }

  bool NeedsHomeObject() const {
    return scope_uses_super_property_ ||
           ((scope_calls_eval_ || inner_scope_calls_eval_) &&
            (IsConciseMethod(function_kind()) ||
             IsAccessorFunction(function_kind()) ||
             IsClassConstructor(function_kind())));
  }

  const Scope* NearestOuterEvalScope() const {
    if (is_eval_scope()) return this;
    if (outer_scope() == nullptr) return nullptr;
    return outer_scope()->NearestOuterEvalScope();
  }

  // ---------------------------------------------------------------------------
  // Accessors.

  // The type of this scope.
  ScopeType scope_type() const { return scope_type_; }

  FunctionKind function_kind() const { return function_kind_; }

  // The language mode of this scope.
  LanguageMode language_mode() const { return language_mode_; }

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
  VariableDeclaration* function() const {
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

  int num_parameters() const { return params_.length(); }

  // A function can have at most one rest parameter. Returns Variable* or NULL.
  Variable* rest_parameter(int* index) const {
    *index = rest_index_;
    if (rest_index_ < 0) return NULL;
    return rest_parameter_;
  }

  bool has_rest_parameter() const { return rest_index_ >= 0; }

  bool has_simple_parameters() const {
    return has_simple_parameters_;
  }

  // TODO(caitp): manage this state in a better way. PreParser must be able to
  // communicate that the scope is non-simple, without allocating any parameters
  // as the Parser does. This is necessary to ensure that TC39's proposed early
  // error can be reported consistently regardless of whether lazily parsed or
  // not.
  void SetHasNonSimpleParameters() {
    DCHECK(is_function_scope());
    has_simple_parameters_ = false;
  }

  // Retrieve `IsSimpleParameterList` of current or outer function.
  bool HasSimpleParameters() {
    Scope* scope = ClosureScope();
    return !scope->is_function_scope() || scope->has_simple_parameters();
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

  // Declarations list.
  ZoneList<Declaration*>* declarations() { return &decls_; }

  // Inner scope list.
  ZoneList<Scope*>* inner_scopes() { return &inner_scopes_; }

  // The scope immediately surrounding this scope, or NULL.
  Scope* outer_scope() const { return outer_scope_; }

  // The ModuleDescriptor for this scope; only for module scopes.
  ModuleDescriptor* module() const { return module_descriptor_; }


  void set_class_declaration_group_start(int position) {
    class_declaration_group_start_ = position;
  }

  int class_declaration_group_start() const {
    return class_declaration_group_start_;
  }

  // ---------------------------------------------------------------------------
  // Variable allocation.

  // Collect stack and context allocated local variables in this scope. Note
  // that the function variable - if present - is not collected and should be
  // handled separately.
  void CollectStackAndContextLocals(
      ZoneList<Variable*>* stack_locals, ZoneList<Variable*>* context_locals,
      ZoneList<Variable*>* context_globals,
      ZoneList<Variable*>* strong_mode_free_variables = nullptr);

  // Current number of var or const locals.
  int num_var_or_const() { return num_var_or_const_; }

  // Result of variable allocation.
  int num_stack_slots() const { return num_stack_slots_; }
  int num_heap_slots() const { return num_heap_slots_; }
  int num_global_slots() const { return num_global_slots_; }

  int StackLocalCount() const;
  int ContextLocalCount() const;
  int ContextGlobalCount() const;

  // Make sure this scope and all outer scopes are eagerly compiled.
  void ForceEagerCompilation()  { force_eager_compilation_ = true; }

  // Determine if we can parse a function literal in this scope lazily.
  bool AllowsLazyParsing() const;

  // Determine if we can use lazy compilation for this scope.
  bool AllowsLazyCompilation() const;

  // Determine if we can use lazy compilation for this scope without a context.
  bool AllowsLazyCompilationWithoutContext() const;

  // True if the outer context of this scope is always the native context.
  bool HasTrivialOuterContext() const;

  // The number of contexts between this and scope; zero if this == scope.
  int ContextChainLength(Scope* scope);

  // The maximum number of nested contexts required for this scope and any inner
  // scopes.
  int MaxNestedContextChainLength();

  // Find the first function, script, eval or (declaration) block scope. This is
  // the scope where var declarations will be hoisted to in the implementation.
  Scope* DeclarationScope();

  // Find the first non-block declaration scope. This should be either a script,
  // function, or eval scope. Same as DeclarationScope(), but skips
  // declaration "block" scopes. Used for differentiating associated
  // function objects (i.e., the scope for which a function prologue allocates
  // a context) or declaring temporaries.
  Scope* ClosureScope();

  // Find the first (non-arrow) function or script scope.  This is where
  // 'this' is bound, and what determines the function kind.
  Scope* ReceiverScope();

  Handle<ScopeInfo> GetScopeInfo(Isolate* isolate);

  // Get the chain of nested scopes within this scope for the source statement
  // position. The scopes will be added to the list from the outermost scope to
  // the innermost scope. Only nested block, catch or with scopes are tracked
  // and will be returned, but no inner function scopes.
  void GetNestedScopeChain(Isolate* isolate, List<Handle<ScopeInfo> >* chain,
                           int statement_position);

  void CollectNonLocals(HashMap* non_locals);

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

  bool IsDeclaredParameter(const AstRawString* name) {
    // If IsSimpleParameterList is false, duplicate parameters are not allowed,
    // however `arguments` may be allowed if function is not strict code. Thus,
    // the assumptions explained above do not hold.
    return params_.Contains(variables_.Lookup(name));
  }

  SloppyBlockFunctionMap* sloppy_block_function_map() {
    return &sloppy_block_function_map_;
  }

  // Error handling.
  void ReportMessage(int start_position, int end_position,
                     MessageTemplate::Template message,
                     const AstRawString* arg);

  // ---------------------------------------------------------------------------
  // Debugging.

#ifdef DEBUG
  void Print(int n = 0);  // n = indentation; n < 0 => don't print recursively
#endif

  // ---------------------------------------------------------------------------
  // Implementation.
 private:
  // Scope tree.
  Scope* outer_scope_;  // the immediately enclosing outer scope, or NULL
  ZoneList<Scope*> inner_scopes_;  // the immediately enclosed inner scopes

  // The scope type.
  ScopeType scope_type_;
  // If the scope is a function scope, this is the function kind.
  FunctionKind function_kind_;

  // Debugging support.
  const AstRawString* scope_name_;

  // The variables declared in this scope:
  //
  // All user-declared variables (incl. parameters).  For script scopes
  // variables may be implicitly 'declared' by being used (possibly in
  // an inner scope) with no intervening with statements or eval calls.
  VariableMap variables_;
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
  // new.target variable, function scopes only.
  Variable* new_target_;
  // Convenience variable; function scopes only.
  Variable* arguments_;
  // Convenience variable; Subclass constructor only
  Variable* this_function_;
  // Module descriptor; module scopes only.
  ModuleDescriptor* module_descriptor_;

  // Map of function names to lists of functions defined in sloppy blocks
  SloppyBlockFunctionMap sloppy_block_function_map_;

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
  // This scope uses "arguments".
  bool scope_uses_arguments_;
  // This scope uses "super" property ('super.foo').
  bool scope_uses_super_property_;
  // This scope contains an "use asm" annotation.
  bool asm_module_;
  // This scope's outer context is an asm module.
  bool asm_function_;
  // This scope's declarations might not be executed in order (e.g., switch).
  bool scope_nonlinear_;
  // The language mode of this scope.
  LanguageMode language_mode_;
  // Source positions.
  int start_position_;
  int end_position_;

  // Computed via PropagateScopeInfo.
  bool outer_scope_calls_sloppy_eval_;
  bool inner_scope_calls_eval_;
  bool force_eager_compilation_;
  bool force_context_allocation_;

  // True if it doesn't need scope resolution (e.g., if the scope was
  // constructed based on a serialized scope info or a catch context).
  bool already_resolved_;

  // True if it holds 'var' declarations.
  bool is_declaration_scope_;

  // Computed as variables are declared.
  int num_var_or_const_;

  // Computed via AllocateVariables; function, block and catch scopes only.
  int num_stack_slots_;
  int num_heap_slots_;
  int num_global_slots_;

  // Info about the parameter list of a function.
  int arity_;
  bool has_simple_parameters_;
  Variable* rest_parameter_;
  int rest_index_;

  // Serialized scope info support.
  Handle<ScopeInfo> scope_info_;
  bool already_resolved() { return already_resolved_; }

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
  // scope. If the code is executed because of a call to 'eval', the context
  // parameter should be set to the calling context of 'eval'.
  Variable* LookupRecursive(VariableProxy* proxy, BindingKind* binding_kind,
                            AstNodeFactory* factory);
  MUST_USE_RESULT
  bool ResolveVariable(ParseInfo* info, VariableProxy* proxy,
                       AstNodeFactory* factory);
  MUST_USE_RESULT
  bool ResolveVariablesRecursively(ParseInfo* info, AstNodeFactory* factory);

  bool CheckStrongModeDeclaration(VariableProxy* proxy, Variable* var);

  // If this scope is a method scope of a class, return the corresponding
  // class variable, otherwise nullptr.
  ClassVariable* ClassVariableForMethod() const;

  // Scope analysis.
  void PropagateScopeInfo(bool outer_scope_calls_sloppy_eval);
  bool HasTrivialContext() const;

  // Predicates.
  bool MustAllocate(Variable* var);
  bool MustAllocateInContext(Variable* var);
  bool HasArgumentsParameter(Isolate* isolate);

  // Variable allocation.
  void AllocateStackSlot(Variable* var);
  void AllocateHeapSlot(Variable* var);
  void AllocateParameterLocals(Isolate* isolate);
  void AllocateNonParameterLocal(Isolate* isolate, Variable* var);
  void AllocateDeclaredGlobal(Isolate* isolate, Variable* var);
  void AllocateNonParameterLocalsAndDeclaredGlobals(Isolate* isolate);
  void AllocateVariablesRecursively(Isolate* isolate);
  void AllocateParameter(Variable* var, int index);
  void AllocateReceiver();

  // Resolve and fill in the allocation information for all variables
  // in this scopes. Must be called *after* all scopes have been
  // processed (parsed) to ensure that unresolved variables can be
  // resolved properly.
  //
  // In the case of code compiled and run using 'eval', the context
  // parameter is the context in which eval was called.  In all other
  // cases the context parameter is an empty handle.
  MUST_USE_RESULT
  bool AllocateVariables(ParseInfo* info, AstNodeFactory* factory);

  // Construct a scope based on the scope info.
  Scope(Zone* zone, Scope* inner_scope, ScopeType type,
        Handle<ScopeInfo> scope_info, AstValueFactory* value_factory);

  // Construct a catch scope with a binding for the name.
  Scope(Zone* zone, Scope* inner_scope, const AstRawString* catch_variable_name,
        AstValueFactory* value_factory);

  void AddInnerScope(Scope* inner_scope) {
    if (inner_scope != NULL) {
      inner_scopes_.Add(inner_scope, zone_);
      inner_scope->outer_scope_ = this;
    }
  }

  void RemoveInnerScope(Scope* inner_scope) {
    DCHECK_NOT_NULL(inner_scope);
    for (int i = 0; i < inner_scopes_.length(); i++) {
      if (inner_scopes_[i] == inner_scope) {
        inner_scopes_.Remove(i);
        break;
      }
    }
  }

  void SetDefaults(ScopeType type, Scope* outer_scope,
                   Handle<ScopeInfo> scope_info,
                   FunctionKind function_kind = kNormalFunction);

  AstValueFactory* ast_value_factory_;
  Zone* zone_;

  PendingCompilationErrorHandler pending_error_handler_;

  // For tracking which classes are declared consecutively. Needed for strong
  // mode.
  int class_declaration_group_start_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_SCOPES_H_
