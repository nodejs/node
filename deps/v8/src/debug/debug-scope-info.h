// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_SCOPE_INFO_H_
#define V8_DEBUG_DEBUG_SCOPE_INFO_H_

#include <optional>
#include <utility>

#include "src/base/macros.h"
#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/objects/debug-objects.h"

namespace v8 {
namespace internal {

class DeclarationScope;
class InternalizedString;
class Isolate;
class Script;

// Structure holding deserialized variable information for debugger inspection.
struct DebugVariableInfo {
  Tagged<InternalizedString> name;
  VariableLocation location;
  int index;
  VariableMode mode;
  int initializer_position;
  bool is_synthetic;
  bool is_receiver;

  // LINT.IfChange(VariableIsExport)
  bool is_export() const {
    DCHECK_EQ(location, VariableLocation::MODULE);
    DCHECK_NE(index, 0);
    return index > 0;
  }
  // LINT.ThenChange(/src/ast/variables.h:VariableIsExport)
};

// Stack-allocated cursor for navigating and querying serialized scope trees
// stored in DebugScriptScopeInfo.
class V8_EXPORT_PRIVATE DebugScriptScope {
  V8_STACK_ALLOCATED();

 public:
  static DebugScriptScope FromIndex(DirectHandle<DebugScriptScopeInfo> info,
                                    int scope_index);

  // Tree Navigation
  std::optional<DebugScriptScope> parent() const;
  std::optional<DebugScriptScope> first_child() const;
  std::optional<DebugScriptScope> next_sibling() const;

  // Position & Type Accessors
  int start_position() const;
  int end_position() const;
  int scope_index() const { return scope_index_; }
  ScopeType scope_type() const;
  LanguageMode language_mode() const;

  // Returns true if `position` lies within this scope.
  //
  // `is_closure_found` controls how strict we are about the end position:
  // while the closure scope hasn't been determined yet we also accept
  // `position == end_position()`, since nested arrow functions can share their
  // end position with the function they are embedded in.
  bool ContainsPosition(int position, bool is_closure_found) const;

  // Scope Predicates
  bool is_script_scope() const;
  bool is_function_scope() const;
  bool is_block_scope() const;
  bool is_declaration_scope() const;
  bool is_arrow_scope() const;
  bool is_class_scope() const;
  bool is_with_scope() const;
  bool is_module_scope() const;
  bool is_eval_scope() const;
  bool is_catch_scope() const;
  bool is_repl_mode_scope() const;
  bool is_hidden() const;
  bool has_this_declaration() const;
  bool has_this_reference() const;
  bool has_simple_parameters() const;
  bool has_arguments() const;
  bool has_function_variable() const;
  bool sloppy_eval_can_extend_vars() const;
  bool needs_context() const;

  // Context Info, returns a valid ID only when needs_context() == true.
  int unique_id_in_script() const;

  // Special Variables Info
  // Returns a pair of {VariableAllocationInfo, index}. When allocated, the
  // second element represents the stack slot index or context slot index of the
  // receiver variable (or -1 if none/unallocated).
  std::pair<VariableAllocationInfo, int> receiver_info() const;
  // Returns a pair of {VariableAllocationInfo, index}. When allocated, the
  // second element represents the stack slot index or context slot index of the
  // arguments variable (or -1 if none/unallocated).
  std::pair<VariableAllocationInfo, int> arguments_info() const;
  std::pair<VariableAllocationInfo, int> function_variable_info() const;
  Tagged<InternalizedString> function_variable_name() const;

  // Local Variables Info
  int variable_count() const;
  DebugVariableInfo variable(int index) const;

 private:
  DebugScriptScope(DirectHandle<DebugScriptScopeInfo> info, int scope_index,
                   uint32_t offset)
      : info_(info), scope_index_(scope_index), offset_(offset) {}

  const uint8_t* payload() const;
  const uint8_t* function_variable_payload() const;
  const uint8_t* variables_payload() const;
  uint32_t flags() const;
  int parent_index() const;

  // Chained offset calculation methods (private to DebugScriptScope).
  size_t next_sibling_offset() const;
  size_t context_id_offset() const;
  size_t receiver_info_offset() const;
  size_t arguments_info_offset() const;
  size_t function_variable_offset() const;
  size_t variables_offset() const;
  size_t record_size() const;

  friend class DebugScriptScopeInfo;

  DirectHandle<DebugScriptScopeInfo> info_;
  int scope_index_;
  uint32_t offset_;
};

// Serializes the start/end positions of the AST scopes rooted at `script_scope`
// into a DebugScriptScopeInfo.
V8_EXPORT_PRIVATE Handle<DebugScriptScopeInfo> SerializeDebugScriptScopeInfo(
    Isolate* isolate, DeclarationScope* script_scope);

// Ensures that `script` has an associated DebugScriptScopeInfo. If not yet
// created, parses the script once eagerly, builds the scope info, and
// caches it in the Debug ephemeron side table.
//
// All necessary scoping information (caller ScopeInfo for eval scripts,
// wrapped arguments for wrapped scripts) is retained on the Script itself.
V8_EXPORT_PRIVATE Handle<DebugScriptScopeInfo> EnsureDebugScriptScopeInfo(
    Isolate* isolate, DirectHandle<Script> script);

// Returns the number of scopes serialized in `info`.
V8_EXPORT_PRIVATE int DebugScriptScopeCount(Tagged<DebugScriptScopeInfo> info);

// Returns the scope that exactly matches the extent of the function described
// by `start_position`, `end_position` and `scope_type`, or nullopt if `info`
// doesn't contain such a scope.
//
// Matching the positions alone is not enough: scopes can share their exact
// extent with a scope of a different type. A class declaration that spans the
// whole script for example has the same positions as the script scope.
V8_EXPORT_PRIVATE std::optional<DebugScriptScope> FindClosureScope(
    DirectHandle<DebugScriptScopeInfo> info, int start_position,
    int end_position, ScopeType scope_type);

// Returns the innermost scope around `position` at or below `closure_scope`,
// i.e. the scope a debugger paused at `position` starts iterating from.
//
// V8's scope tree doesn't guarantee that siblings don't overlap, so all
// descendants of `closure_scope` are considered and the one with the tightest
// bounds around `position` wins. `closure_scope` itself is returned if no
// descendant is a better fit.
V8_EXPORT_PRIVATE DebugScriptScope
FindInnermostScope(DebugScriptScope closure_scope, int position);

}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_SCOPE_INFO_H_
