// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-block-list.h"

#include <optional>

#include "src/codegen/compiler.h"
#include "src/debug/debug-scope-info.h"
#include "src/execution/isolate.h"
#include "src/objects/scope-info-inl.h"
#include "src/objects/script-inl.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/string-set-inl.h"

namespace v8 {
namespace internal {

namespace {

Handle<StringSet> AddScopeStackLocalsToBlockList(Isolate* isolate,
                                                 Handle<StringSet> blocklist,
                                                 DebugScriptScope scope) {
  if (scope.is_hidden() || scope.is_script_scope()) {
    return blocklist;
  }
  for (int i = 0; i < scope.variable_count(); ++i) {
    DebugVariableInfo var = scope.variable(i);
    if (var.location == VariableLocation::PARAMETER ||
        var.location == VariableLocation::LOCAL) {
      DirectHandle<String> name(var.name, isolate);
      blocklist = StringSet::Add(isolate, blocklist, name);
    }
  }
  return blocklist;
}

}  // namespace

Handle<StringSet> EnsureLocalsBlockList(
    Isolate* isolate, DirectHandle<SharedFunctionInfo> shared_info) {
  IsCompiledScope is_compiled_scope(shared_info->is_compiled_scope(isolate));
  if (!is_compiled_scope.is_compiled() &&
      !Compiler::Compile(isolate, handle(*shared_info, isolate),
                         Compiler::CLEAR_EXCEPTION, &is_compiled_scope)) {
    return StringSet::New(isolate);
  }

  DirectHandle<ScopeInfo> closure_scope_info(shared_info->scope_info(),
                                             isolate);
  Tagged<UnionOf<TheHole, StringSet>> existing =
      isolate->LocalsBlockListCacheGet(closure_scope_info);
  if (IsStringSet(existing)) {
    return handle(Cast<StringSet>(existing), isolate);
  }

  if (!IsScript(shared_info->script())) {
    return StringSet::New(isolate);
  }
  DirectHandle<Script> script(Cast<Script>(shared_info->script()), isolate);
  Handle<DebugScriptScopeInfo> debug_scope_info =
      EnsureDebugScriptScopeInfo(isolate, script);
  if (debug_scope_info.is_null()) {
    return StringSet::New(isolate);
  }

  std::optional<DebugScriptScope> closure_scope =
      closure_scope_info->scope_type() == FUNCTION_SCOPE
          ? FindClosureScope(debug_scope_info, shared_info->StartPosition(),
                             shared_info->EndPosition(),
                             closure_scope_info->scope_type())
          : DebugScriptScope::FromIndex(debug_scope_info, 0);
  DCHECK(closure_scope.has_value());
  if (!closure_scope.has_value()) {
    return StringSet::New(isolate);
  }

  DirectHandle<ScopeInfo> current_scope_info = closure_scope_info;
  Handle<StringSet> current_blocklist = StringSet::New(isolate);
  Handle<StringSet> closure_blocklist;

  for (std::optional<DebugScriptScope> scope = closure_scope; scope.has_value();
       scope = scope->parent()) {
    if (scope->scope_index() != closure_scope->scope_index() &&
        scope->needs_context()) {
      DCHECK(current_scope_info->HasOuterScopeInfo());
      DirectHandle<ScopeInfo> next_scope_info(
          current_scope_info->OuterScopeInfo(), isolate);
      DCHECK_EQ(scope->unique_id_in_script(),
                next_scope_info->UniqueIdInScript());
      isolate->LocalsBlockListCacheSet(current_scope_info, next_scope_info,
                                       current_blocklist);
      if (closure_blocklist.is_null()) {
        closure_blocklist = current_blocklist;
      }
      current_scope_info = next_scope_info;
      current_blocklist = StringSet::New(isolate);
    }

    current_blocklist =
        AddScopeStackLocalsToBlockList(isolate, current_blocklist, *scope);
  }

  DirectHandle<ScopeInfo> outer_scope_info =
      current_scope_info->HasOuterScopeInfo()
          ? direct_handle(current_scope_info->OuterScopeInfo(), isolate)
          : DirectHandle<ScopeInfo>();
  isolate->LocalsBlockListCacheSet(current_scope_info, outer_scope_info,
                                   current_blocklist);
  if (closure_blocklist.is_null()) {
    closure_blocklist = current_blocklist;
  }

  return closure_blocklist;
}

Handle<StringSet> CalculateScopeBlockList(Isolate* isolate,
                                          DebugScriptScope scope) {
  Handle<StringSet> blocklist = StringSet::New(isolate);
  for (std::optional<DebugScriptScope> current = scope;
       current.has_value() && !current->needs_context();
       current = current->parent()) {
    blocklist = AddScopeStackLocalsToBlockList(isolate, blocklist, *current);
  }
  return blocklist;
}

}  // namespace internal
}  // namespace v8
