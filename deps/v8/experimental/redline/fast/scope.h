// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_FAST_SCOPE_H_
#define V8_EXPERIMENTAL_REDLINE_FAST_SCOPE_H_

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "experimental/redline/fast/ast.h"

namespace redline {
namespace fast {

class Tree;

// Scope analysis: which identifiers are declarations, which are references,
// and which declaration every reference resolves to. This is the substrate
// for every mutation that moves, deletes or introduces code: "what is
// visible here?", "is this name free?", "who uses this variable?".
//
// The analysis is deliberately a static approximation of the spec:
// - `var` and function declarations hoist to the nearest function/script
//   scope, lexical declarations (`let`, `const`, `class`, `using`) bind in
//   the nearest block scope.
// - A function with non-simple parameters gets a separate var scope for
//   its body (as in the spec and in V8's parser); a body `var` that
//   redeclares a parameter is treated as the same binding.
// - A class declaration has one binding (the outer, mutable one); the
//   immutable inner name binding is only modelled for class expressions.
// - Sloppy-mode Annex B function hoisting out of blocks and `var`
//   redeclarations of catch parameters are not modelled; the bindings
//   involved are flagged (Binding::pinned) instead.
// - Direct `eval` and `with` make scopes dynamic; such scopes are flagged
//   and references inside them resolve as usual but must be treated with
//   suspicion by callers (see Scope::is_dynamic()).
// - References that resolve to nothing are globals (builtins, properties
//   of the global object or runtime errors).
// Results are valid until the tree is modified.

enum class ScopeKind {
  kScript,        // the whole program
  kFunction,      // function/method/getter/setter/constructor (params + body)
  kArrow,         // arrow function (no own this/arguments)
  kFunctionBody,  // body of a function with non-simple parameters: its
                  // `var`s live in a scope of their own that parameter
                  // default expressions cannot see
  kBlock,         // { } block, switch body, for-head lexical scope
  kCatch,         // catch (param) binding scope
  kClass,         // class name binding visible inside the class body
  kStaticBlock,   // class static { } block (a var scope)
  kFieldInit,     // class field initializer (a function-like scope)
  kWith,          // with (o) { } body: dynamic
};

const char* ScopeKindName(ScopeKind kind);

enum class BindingKind {
  kVar,
  kLet,
  kConst,
  kUsing,
  kAwaitUsing,
  kFunction,   // function declaration
  kClass,      // class declaration
  kParameter,  // formal parameter (including rest and pattern parts)
  kCatchParameter,
  kFunctionName,  // the name of a named function expression (inner binding)
  kClassName,     // the inner binding of a (named) class
  kImplicit,      // `arguments` in non-arrow functions
};

const char* BindingKindName(BindingKind kind);

struct Scope;

struct Binding {
  std::string_view name;
  BindingKind kind = BindingKind::kVar;
  Scope* scope = nullptr;
  // The Identifier node that declares the binding (nullptr for kImplicit).
  Identifier* identifier = nullptr;
  // The declaring statement/parameter/function node (nullptr for kImplicit).
  Node* declaration = nullptr;
  // Every declaring identifier (`var x; var x;` has two), first one is
  // `identifier`.
  std::vector<Identifier*> declarations;
  // All reference sites, in source order (reads, writes, updates).
  std::vector<Identifier*> references;
  // True if some reference assigns to the binding.
  bool assigned = false;
  // True if some reference sits in a dynamic scope (`with` body, scope
  // with direct eval), where it may not actually refer to this binding.
  bool has_dynamic_reference = false;
  // True if the binding takes part in a sloppy-mode Annex B interaction the
  // analysis does not model precisely: a `var` redeclaring a catch
  // parameter inside its catch block (the initializer assigns the
  // parameter), or a block-level function declaration that is hoisted to
  // the function scope unless a lexical binding of the same name is in the
  // way. Renaming such bindings may change the program.
  bool pinned = false;
  // True if the binding's name is observable as the `name` of a function
  // or class: `let f = function() {}`, `const C = class {}`,
  // `x = () => {}`, `{f = function() {}} = o`.
  bool names_function = false;

  bool is_lexical() const {
    return kind == BindingKind::kLet || kind == BindingKind::kConst ||
           kind == BindingKind::kUsing || kind == BindingKind::kAwaitUsing ||
           kind == BindingKind::kClass;
  }
  bool is_constant() const {
    return kind == BindingKind::kConst || kind == BindingKind::kUsing ||
           kind == BindingKind::kAwaitUsing ||
           kind == BindingKind::kFunctionName ||
           kind == BindingKind::kClassName;
  }
  // True if renaming the binding and all its references cannot change the
  // program's meaning: no dynamic scoping involved, not `arguments`, not a
  // script-level binding (those are reachable by name from other scripts,
  // `new Function` bodies and indirect eval), no Annex B interaction.
  bool safe_to_rename() const;
};

struct Scope {
  ScopeKind kind = ScopeKind::kBlock;
  Scope* parent = nullptr;
  // The node that introduced the scope (Program, function, block, ...).
  Node* node = nullptr;
  std::vector<Scope*> children;
  // Bindings in declaration order.
  std::vector<Binding*> bindings;
  // Name lookup; for duplicate `var`s the first declaration wins. A flat
  // table: one allocation per scope with bindings instead of one per name.
  absl::flat_hash_map<std::string_view, Binding*> by_name;
  bool strict = false;
  bool is_async = false;
  bool is_generator = false;
  // A direct `eval(...)` call occurs in this scope (not in nested
  // functions): any name may be introduced or read dynamically.
  bool has_direct_eval = false;
  // This scope or a nested one has direct eval or is a `with` scope, so
  // bindings declared here may be accessed by name from such code.
  bool dynamic_inside = false;
  // Source range of the scope's node.
  int32_t start = -1;
  int32_t end = -1;

  // True for scopes that can declare `var`s: script, function (or its
  // separate body scope), static block, field initializer.
  bool is_var_scope() const {
    return kind == ScopeKind::kScript || kind == ScopeKind::kFunction ||
           kind == ScopeKind::kArrow || kind == ScopeKind::kFunctionBody ||
           kind == ScopeKind::kStaticBlock || kind == ScopeKind::kFieldInit;
  }
  // True if names in this scope or an enclosing one may be resolved
  // dynamically (`with`, direct eval).
  bool is_dynamic() const;
  // The nearest enclosing function-like scope (or the script scope).
  Scope* var_scope();
  const Scope* var_scope() const;
  // The nearest enclosing non-arrow function scope, or nullptr at the
  // script level: where `this`/`arguments`/`new.target` come from.
  const Scope* this_scope() const;
  Binding* Lookup(std::string_view name) const;
  int Depth() const;
};

class ScopeAnalysis {
 public:
  ScopeAnalysis();
  ~ScopeAnalysis();
  ScopeAnalysis(const ScopeAnalysis&) = delete;
  ScopeAnalysis& operator=(const ScopeAnalysis&) = delete;

  // Analyzes the whole tree.
  static std::unique_ptr<ScopeAnalysis> Analyze(Tree* tree);

  Scope* root() const { return root_; }
  // All scopes / bindings in creation order (pre-order of the tree).
  const std::vector<Scope*>& scopes() const { return scopes_; }
  const std::vector<Binding*>& bindings() const { return bindings_; }

  // The innermost scope that contains `node` (for scope-introducing nodes:
  // the scope they introduce). nullptr for nodes not in the analyzed tree.
  Scope* ScopeOf(const Node* node) const;
  // The binding a reference resolves to, or nullptr for globals and for
  // identifiers that are not references (property names, labels, ...).
  Binding* BindingOf(const Identifier* identifier) const;
  // True if `identifier` is a variable reference (as opposed to a property
  // name, label or binding identifier).
  bool IsReference(const Identifier* identifier) const;
  // True if `identifier` declares a binding.
  bool IsDeclaration(const Identifier* identifier) const;
  // Reference identifiers that resolve to no binding (globals/builtins).
  const std::vector<Identifier*>& unresolved() const { return unresolved_; }

  // All bindings visible to an expression at `node` (i.e. to code replacing
  // it), innermost first; shadowed bindings are omitted. For
  // scope-introducing nodes (functions, classes, blocks, loops with lexical
  // declarations, ...) this is the enclosing scope: code replacing a function
  // expression does not see its parameters. With `initialized_only`, lexical
  // bindings declared textually after `node` are omitted, as are `let`/`const`
  // bindings whose own initializer contains `node` (TDZ approximation).
  std::vector<Binding*> VisibleAt(const Node* node,
                                  bool initialized_only = true) const;

  // All bindings visible to a statement inserted into the statement list of
  // `owner` (Program, BlockStatement, SwitchCase, StaticBlock) at text
  // position `position` (the end of the preceding statement, or the owner's
  // start for the first slot): the scope `owner` holds its statements in plus
  // the enclosing scopes. With `initialized_only`, lexical bindings declared
  // after `position` are omitted (TDZ approximation).
  std::vector<Binding*> VisibleIn(const Node* owner, int32_t position,
                                  bool initialized_only = true) const;

  // True if no binding named `name` is visible at `node` and no reference
  // to `name` resolves through `node`'s scope chain, i.e. introducing a
  // declaration of `name` at `node` cannot change the meaning of existing
  // code.
  bool IsNameFreeAt(const Node* node, std::string_view name) const;
  // A name of the form `<prefix><n>` that occurs nowhere in the program.
  std::string FreshName(std::string_view prefix) const;
  // Every identifier name in the program (declarations, references,
  // property names, labels).
  const absl::flat_hash_set<std::string_view>& all_names() const {
    return all_names_;
  }

  // Human-readable dump (used by `redline parse --scopes`).
  std::string Dump() const;

 private:
  friend class ScopeBuilder;

  // Per-node facts are kept in vectors indexed by Node::id: ids are dense
  // (0 .. Tree::id_limit() at analysis time), so this replaces a hash lookup
  // and a heap node per entry by an array access. Nodes created after the
  // analysis have ids beyond the vectors and are simply unknown.
  enum class IdFlag : uint8_t {
    kIsReference = 1,
    kIsDeclaration = 2,
  };
  void SetScopeOf(const Node* node, Scope* scope);  // overwrites
  void SetScopeIfUnset(const Node* node, Scope* scope);
  void SetBindingOf(const Identifier* identifier, Binding* binding);
  void AddFlag(const Identifier* identifier, IdFlag flag);
  bool HasFlag(const Identifier* identifier, IdFlag flag) const;
  Scope* NewScope();
  Binding* NewBinding();

  std::vector<Binding*> CollectVisible(Scope* scope, int32_t position,
                                       bool initialized_only) const;

  Scope* root_ = nullptr;
  // Object storage: a deque keeps addresses stable without one heap
  // allocation per object. scopes_/bindings_ list the same objects in
  // creation order.
  std::deque<Scope> scope_storage_;
  std::deque<Binding> binding_storage_;
  std::vector<Scope*> scopes_;
  std::vector<Binding*> bindings_;
  std::vector<Scope*> scope_of_;      // by Node::id
  std::vector<Binding*> binding_of_;  // by Node::id
  std::vector<uint8_t> id_flags_;     // by Node::id, IdFlag bits
  std::vector<Identifier*> unresolved_;
  absl::flat_hash_set<std::string_view> all_names_;
};

// Collects the binding identifiers of a pattern (Identifier, ObjectPattern,
// ArrayPattern, AssignmentPattern, RestElement) in source order. Default
// value expressions and computed keys are not descended into.
void CollectBoundIdentifiers(Node* pattern, std::vector<Identifier*>* out);

// Renames a binding: all its declaration sites and references. `new_name`
// is interned in `tree`. The analysis stays consistent (names are updated
// in place) except for all_names()/FreshName(), which keep the old name.
void RenameBinding(Tree* tree, Binding* binding, std::string_view new_name);

enum class RenameScope {
  kVariables,  // everything whose name is not observable through
               // Function.prototype.name: no function/class names, no
               // variables initialized with anonymous functions/classes
  kAll,
};

// Renames every binding that is safe to rename to `<prefix><n>`. Returns
// the number of renamed bindings. Used as a behavioural self-test of the
// scope analysis: the renamed program must behave like the original.
size_t RenameAllBindings(Tree* tree, ScopeAnalysis* analysis,
                         std::string_view prefix, RenameScope which);

}  // namespace fast
}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_FAST_SCOPE_H_
