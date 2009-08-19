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

#ifndef V8_SCOPES_H_
#define V8_SCOPES_H_

#include "ast.h"
#include "hashmap.h"

namespace v8 {
namespace internal {


// A hash map to support fast variable declaration and lookup.
class VariableMap: public HashMap {
 public:
  VariableMap();

  // Dummy constructor.  This constructor doesn't set up the map
  // properly so don't use it unless you have a good reason.
  explicit VariableMap(bool gotta_love_static_overloading);

  virtual ~VariableMap();

  Variable* Declare(Scope* scope,
                    Handle<String> name,
                    Variable::Mode mode,
                    bool is_valid_lhs,
                    Variable::Kind kind);

  Variable* Lookup(Handle<String> name);
};


// The dynamic scope part holds hash maps for the variables that will
// be looked up dynamically from within eval and with scopes. The objects
// are allocated on-demand from Scope::NonLocal to avoid wasting memory
// and setup time for scopes that don't need them.
class DynamicScopePart : public ZoneObject {
 public:
  VariableMap* GetMap(Variable::Mode mode) {
    int index = mode - Variable::DYNAMIC;
    ASSERT(index >= 0 && index < 3);
    return &maps_[index];
  }

 private:
  VariableMap maps_[3];
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

  enum Type {
    EVAL_SCOPE,     // the top-level scope for an 'eval' source
    FUNCTION_SCOPE,  // the top-level scope for a function
    GLOBAL_SCOPE    // the top-level scope for a program or a top-level eval
  };

  Scope(Scope* outer_scope, Type type);

  virtual ~Scope() { }

  // The scope name is only used for printing/debugging.
  void SetScopeName(Handle<String> scope_name)  { scope_name_ = scope_name; }

  void Initialize(bool inside_with);


  // ---------------------------------------------------------------------------
  // Declarations

  // Lookup a variable in this scope. Returns the variable or NULL if not found.
  virtual Variable* LocalLookup(Handle<String> name);

  // Lookup a variable in this scope or outer scopes.
  // Returns the variable or NULL if not found.
  virtual Variable* Lookup(Handle<String> name);

  // Declare the function variable for a function literal. This variable
  // is in an intermediate scope between this function scope and the the
  // outer scope. Only possible for function scopes; at most one variable.
  Variable* DeclareFunctionVar(Handle<String> name);

  // Declare a local variable in this scope. If the variable has been
  // declared before, the previously declared variable is returned.
  virtual Variable* DeclareLocal(Handle<String> name, Variable::Mode mode);

  // Declare an implicit global variable in this scope which must be a
  // global scope.  The variable was introduced (possibly from an inner
  // scope) by a reference to an unresolved variable with no intervening
  // with statements or eval calls.
  Variable* DeclareGlobal(Handle<String> name);

  // Add a parameter to the parameter list. The parameter must have been
  // declared via Declare. The same parameter may occur more than once in
  // the parameter list; they must be added in source order, from left to
  // right.
  void AddParameter(Variable* var);

  // Create a new unresolved variable.
  virtual VariableProxy* NewUnresolved(Handle<String> name, bool inside_with);

  // Remove a unresolved variable. During parsing, an unresolved variable
  // may have been added optimistically, but then only the variable name
  // was used (typically for labels). If the variable was not declared, the
  // addition introduced a new unresolved variable which may end up being
  // allocated globally as a "ghost" variable. RemoveUnresolved removes
  // such a variable again if it was added; otherwise this is a no-op.
  void RemoveUnresolved(VariableProxy* var);

  // Creates a new temporary variable in this scope and binds a proxy to it.
  // The name is only used for printing and cannot be used to find the variable.
  // In particular, the only way to get hold of the temporary is by keeping the
  // VariableProxy* around.
  virtual VariableProxy* NewTemporary(Handle<String> name);

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


  // ---------------------------------------------------------------------------
  // Scope-specific info.

  // Inform the scope that the corresponding code contains a with statement.
  void RecordWithStatement()  { scope_contains_with_ = true; }

  // Inform the scope that the corresponding code contains an eval call.
  void RecordEvalCall()  { scope_calls_eval_ = true; }


  // ---------------------------------------------------------------------------
  // Predicates.

  // Specific scope types.
  bool is_eval_scope() const  { return type_ == EVAL_SCOPE; }
  bool is_function_scope() const  { return type_ == FUNCTION_SCOPE; }
  bool is_global_scope() const  { return type_ == GLOBAL_SCOPE; }

  // Information about which scopes calls eval.
  bool calls_eval() const  { return scope_calls_eval_; }
  bool outer_scope_calls_eval() const  { return outer_scope_calls_eval_; }

  // Is this scope inside a with statement.
  bool inside_with() const  { return scope_inside_with_; }
  // Does this scope contain a with statement.
  bool contains_with() const  { return scope_contains_with_; }

  // The scope immediately surrounding this scope, or NULL.
  Scope* outer_scope() const  { return outer_scope_; }

  // ---------------------------------------------------------------------------
  // Accessors.

  // The variable corresponding to the (function) receiver.
  VariableProxy* receiver() const  { return receiver_; }

  // The variable holding the function literal for named function
  // literals, or NULL.
  // Only valid for function scopes.
  Variable* function() const  {
    ASSERT(is_function_scope());
    return function_;
  }

  // Parameters. The left-most parameter has index 0.
  // Only valid for function scopes.
  Variable* parameter(int index) const  {
    ASSERT(is_function_scope());
    return params_[index];
  }

  int num_parameters() const  { return params_.length(); }

  // The local variable 'arguments' if we need to allocate it; NULL otherwise.
  // If arguments() exist, arguments_shadow() exists, too.
  VariableProxy* arguments()  const  { return arguments_; }

  // The '.arguments' shadow variable if we need to allocate it; NULL otherwise.
  // If arguments_shadow() exist, arguments() exists, too.
  VariableProxy* arguments_shadow()  const  { return arguments_shadow_; }

  // Declarations list.
  ZoneList<Declaration*>* declarations() { return &decls_; }



  // ---------------------------------------------------------------------------
  // Variable allocation.

  // Collect all used locals in this scope.
  template<class Allocator>
  void CollectUsedVariables(List<Variable*, Allocator>* locals);

  // Resolve and fill in the allocation information for all variables
  // in this scopes. Must be called *after* all scopes have been
  // processed (parsed) to ensure that unresolved variables can be
  // resolved properly.
  //
  // In the case of code compiled and run using 'eval', the context
  // parameter is the context in which eval was called.  In all other
  // cases the context parameter is an empty handle.
  void AllocateVariables(Handle<Context> context);

  // Result of variable allocation.
  int num_stack_slots() const  { return num_stack_slots_; }
  int num_heap_slots() const  { return num_heap_slots_; }

  // Make sure this scope and all outer scopes are eagerly compiled.
  void ForceEagerCompilation()  { force_eager_compilation_ = true; }

  // Determine if we can use lazy compilation for this scope.
  bool AllowsLazyCompilation() const;

  // True if the outer context of this scope is always the global context.
  bool HasTrivialOuterContext() const;

  // The number of contexts between this and scope; zero if this == scope.
  int ContextChainLength(Scope* scope);


  // ---------------------------------------------------------------------------
  // Debugging.

#ifdef DEBUG
  void Print(int n = 0);  // n = indentation; n < 0 => don't print recursively
#endif

  // ---------------------------------------------------------------------------
  // Implementation.
 protected:
  friend class ParserFactory;

  explicit Scope(Type type);

  // Scope tree.
  Scope* outer_scope_;  // the immediately enclosing outer scope, or NULL
  ZoneList<Scope*> inner_scopes_;  // the immediately enclosed inner scopes

  // The scope type.
  Type type_;

  // Debugging support.
  Handle<String> scope_name_;

  // The variables declared in this scope:
  //
  // All user-declared variables (incl. parameters).  For global scopes
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
  VariableProxy* receiver_;
  // Function variable, if any; function scopes only.
  Variable* function_;
  // Convenience variable; function scopes only.
  VariableProxy* arguments_;
  // Convenience variable; function scopes only.
  VariableProxy* arguments_shadow_;

  // Illegal redeclaration.
  Expression* illegal_redecl_;

  // Scope-specific information.
  bool scope_inside_with_;  // this scope is inside a 'with' of some outer scope
  bool scope_contains_with_;  // this scope contains a 'with' statement
  bool scope_calls_eval_;  // this scope contains an 'eval' call

  // Computed via PropagateScopeInfo.
  bool outer_scope_calls_eval_;
  bool inner_scope_calls_eval_;
  bool outer_scope_is_eval_scope_;
  bool force_eager_compilation_;

  // Computed via AllocateVariables; function scopes only.
  int num_stack_slots_;
  int num_heap_slots_;

  // Create a non-local variable with a given name.
  // These variables are looked up dynamically at runtime.
  Variable* NonLocal(Handle<String> name, Variable::Mode mode);

  // Variable resolution.
  Variable* LookupRecursive(Handle<String> name,
                            bool inner_lookup,
                            Variable** invalidated_local);
  void ResolveVariable(Scope* global_scope,
                       Handle<Context> context,
                       VariableProxy* proxy);
  void ResolveVariablesRecursively(Scope* global_scope,
                                   Handle<Context> context);

  // Scope analysis.
  bool PropagateScopeInfo(bool outer_scope_calls_eval,
                          bool outer_scope_is_eval_scope);
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
};


class DummyScope : public Scope {
 public:
  DummyScope() : Scope(GLOBAL_SCOPE) {
    outer_scope_ = this;
  }

  virtual Variable* Lookup(Handle<String> name)  { return NULL; }
  virtual Variable* Declare(Handle<String> name, Variable::Mode mode) {
    return NULL;
  }
  virtual VariableProxy* NewUnresolved(Handle<String> name, bool inside_with) {
    return NULL;
  }
  virtual VariableProxy* NewTemporary(Handle<String> name)  { return NULL; }
};


} }  // namespace v8::internal

#endif  // V8_SCOPES_H_
