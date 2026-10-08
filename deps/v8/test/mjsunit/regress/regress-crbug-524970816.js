// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-code-merge --stress-background-compile

// 1. Eagerly compiled IIFE populates script->infos() with uncompiled inner
// SFIs.
let outer = (function eager_wrapper(val) {
  let ctx_var = val;
  return function parent_closure() {
    return function child_closure() {
      return ctx_var;
    };
  };
})('test_value');

// 2. Execute parent_closure on the main thread to allocate foreground ScopeInfo
// (S1) and attach it to child_closure's outer scope chain.
let child = outer();
%PrepareFunctionForOptimization(child);

// 3. Re-evaluating or triggering background compilation on the same script
// structure forces BackgroundMergeTask::CompleteMergeInForeground to copy
// background-allocated ScopeInfo (S2) into parent_closure SFI while
// child_closure retains S1. VerifyCodeMerge runs synchronously and catches the
// duplicate ScopeInfo collision.
child();
