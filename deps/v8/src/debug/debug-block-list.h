// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_BLOCK_LIST_H_
#define V8_DEBUG_DEBUG_BLOCK_LIST_H_

#include "src/common/globals.h"
#include "src/handles/handles.h"

namespace v8 {
namespace internal {

class DebugScriptScope;
class Isolate;
class SharedFunctionInfo;
class StringSet;

// Ensures that the locals block-lists for `shared_info` and all its enclosing
// context-allocating scopes within the script are calculated and stored in
// `Isolate::locals_block_list_cache()`.
//
// Each scope `S` with a `ScopeInfo` receives a block-list (`StringSet`)
// containing the names of all stack-allocated variables declared in `[S, K)`
// (from `S` inclusive up to the next enclosing context-allocating scope `K`
// exclusive). Returns the block-list for `shared_info->scope_info()`.
V8_EXPORT_PRIVATE Handle<StringSet> EnsureLocalsBlockList(
    Isolate* isolate, DirectHandle<SharedFunctionInfo> shared_info);

// Calculates the block-list of stack-allocated variables declared in
// `[scope, K)`, walking from `scope` (inclusive) up to the nearest enclosing
// context-allocating scope `K` (exclusive). Used when evaluating in an outer
// scope that does not have its own runtime `Context`.
V8_EXPORT_PRIVATE Handle<StringSet> CalculateScopeBlockList(
    Isolate* isolate, DebugScriptScope scope);

}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_BLOCK_LIST_H_
