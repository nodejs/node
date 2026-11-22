// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_CORE_BUILDER_H_
#define V8_EXPERIMENTAL_REDLINE_CORE_BUILDER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "experimental/redline/core/random.h"
#include "experimental/redline/fast/ast.h"
#include "experimental/redline/fast/scope.h"
#include "experimental/redline/fast/tree.h"

namespace redline {

// Coarse value types used to pick plausible operands and methods. This is
// not a type system; it is a guess derived from declarations.
enum class JsType : uint8_t {
  kUnknown,
  kNumber,
  kString,
  kBoolean,
  kBigInt,
  kSymbol,
  kNullish,
  kArray,
  kObject,
  kFunction,
  kClass,
  kRegExp,
  kMap,
  kSet,
  kPromise,
  kTypedArray,
  kArrayBuffer,
  kDate,
  kError,
};

const char* JsTypeName(JsType type);

struct Variable {
  std::string_view name;
  JsType type = JsType::kUnknown;
  fast::Binding* binding = nullptr;  // nullptr for globals/builtins
  bool is_constant = false;
};

// What a mutator may do at a node.
enum class ExpressionRole {
  kValue,           // an ordinary expression: may be replaced by any expression
  kTarget,          // assignment target: may only be replaced by another target
  kNotReplaceable,  // declaration name, property key, label, pattern part, ...
};

// A mutable program under construction: a FAST tree plus the analyses and
// editing primitives mutators and generators need. Analyses are recomputed
// lazily after structural edits (Invalidate()).
class Builder {
 public:
  Builder(fast::Tree* tree, Random* rng);
  ~Builder();
  Builder(const Builder&) = delete;
  Builder& operator=(const Builder&) = delete;

  fast::Tree* tree() { return tree_; }
  Random& rng() { return *rng_; }
  fast::Program* root() { return tree_->root(); }

  // Scope analysis and parent map, (re)computed on demand.
  fast::ScopeAnalysis& scopes();
  fast::Node* ParentOf(fast::Node* node);
  // Must be called after every structural edit done outside the Builder's
  // own primitives.
  void Invalidate();

  // --- statement lists ------------------------------------------------------

  // A position in a statement list: `index` may equal list->size() for an
  // insertion at the end.
  struct Slot {
    std::vector<fast::Statement*>* list = nullptr;
    size_t index = 0;
    fast::Node* owner =
        nullptr;  // Program, BlockStatement, SwitchCase, StaticBlock
    bool valid() const { return list != nullptr; }
    fast::Statement* statement() const {
      return index < list->size() ? (*list)[index] : nullptr;
    }
  };

  // Every insertion point (after directive prologues).
  std::vector<Slot> InsertionSlots();
  // Every existing statement position.
  std::vector<Slot> StatementSlots();
  Slot RandomInsertionSlot();
  Slot RandomStatementSlot();
  // The slot holding `statement`, if it sits in a statement list.
  Slot SlotOf(fast::Statement* statement);

  // Inserts `statement` at `slot` (anchoring its source positions so that
  // scope analysis orders it correctly) and invalidates analyses.
  void Insert(const Slot& slot, fast::Statement* statement);
  void InsertAll(const Slot& slot,
                 const std::vector<fast::Statement*>& statements);
  fast::Statement* Remove(const Slot& slot);
  // Replaces `old_node` with `new_node` wherever it is referenced from its
  // parent. Returns false if `old_node` has no parent.
  bool Replace(fast::Node* old_node, fast::Node* new_node);
  // Replaces a single-statement body (if/loop body) by a block so that more
  // statements can be inserted next to it. Returns the slot of the original
  // statement inside the new block.
  Slot Blockify(fast::Statement* statement);

  // --- node queries ---------------------------------------------------------

  std::vector<fast::Node*> Collect(
      const std::function<bool(fast::Node*)>& pred);
  fast::Node* RandomNode(const std::function<bool(fast::Node*)>& pred);
  // Expression nodes in value position (see ExpressionRole).
  std::vector<fast::Expression*> ValueExpressions();
  ExpressionRole RoleOf(fast::Node* node);
  // The nearest enclosing statement of `node`.
  fast::Statement* EnclosingStatement(fast::Node* node);
  // The nearest enclosing function-like node, or nullptr at the top level.
  fast::Node* EnclosingFunction(fast::Node* node);

  // --- syntactic context ----------------------------------------------------

  struct Context {
    bool strict = false;
    bool in_function = false;     // `return` allowed
    bool in_async = false;        // `await` allowed
    bool in_generator = false;    // `yield` allowed
    bool in_loop = false;         // `continue` allowed (`break` too)
    bool in_switch = false;       // `break` allowed
    bool in_method = false;       // `super.x` allowed
    bool in_constructor = false;  // `super()` allowed (if derived)
    bool has_super_class = false;
    bool in_static_block = false;
    bool in_class_field = false;
    bool has_this = true;        // `this` meaningful (always syntactically ok)
    bool has_arguments = false;  // inside a non-arrow function
    std::vector<std::string_view> labels;  // enclosing labels
    bool break_allowed() const { return in_loop || in_switch; }
  };
  Context ContextAt(fast::Node* node);
  Context ContextAtSlot(const Slot& slot);

  // --- variables ------------------------------------------------------------

  // Bindings visible at `node`, innermost first, with type guesses.
  std::vector<Variable> VisibleAt(fast::Node* node);
  std::vector<Variable> VisibleAtSlot(const Slot& slot);
  // Variables of (roughly) the given type among `candidates`.
  static std::vector<Variable> OfType(const std::vector<Variable>& candidates,
                                      JsType type);
  JsType GuessType(const fast::Binding* binding);
  static JsType GuessType(const fast::Expression* expression);

  // Bindings that are declared (only) inside `nodes` and referenced outside
  // them: removing the nodes, or moving them into a block, would turn those
  // references into ReferenceErrors. With `block_scoped_only`, `var`
  // bindings are ignored (they stay visible when hoisted out of a block
  // that is not a function).
  std::vector<fast::Binding*> EscapingBindings(
      const std::vector<fast::Node*>& nodes, bool block_scoped_only);

  // --- names ----------------------------------------------------------------

  // A name of the form <prefix><n> unused anywhere in the program.
  std::string_view FreshName(std::string_view prefix = "v");
  // Marks `names` as taken for FreshName() (e.g. the names of a splice
  // donor). The views may point anywhere; they are copied into the tree.
  void ReserveNames(const absl::flat_hash_set<std::string_view>& names);

  // Node id of the next synthesized node (for anchoring/logging).
  uint32_t id_limit() const { return tree_->id_limit(); }

 private:
  void EnsureAnalysis();
  void BuildParentMap();
  void EnsureUsedNames();
  Variable ToVariable(fast::Binding* binding);
  static int LeadingDirectives(const std::vector<fast::Statement*>& list);
  fast::Node* AnchorNodeFor(const Slot& slot, int32_t* position);
  template <typename F>
  void ForEachStatementList(F&& f);

  fast::Tree* tree_;
  Random* rng_;
  std::unique_ptr<fast::ScopeAnalysis> scopes_;
  // Parent of each node, indexed by Node::id (nullptr for the root and for
  // nodes created after the map was built).
  std::vector<fast::Node*> parents_;
  bool parents_valid_ = false;
  // Names FreshName() must avoid; all views are interned in tree_.
  absl::flat_hash_set<std::string_view> used_names_;
  bool used_names_valid_ = false;
  std::unordered_map<std::string, int> name_counters_;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_CORE_BUILDER_H_
