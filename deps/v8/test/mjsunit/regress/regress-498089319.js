// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --ignore-unhandled-promises

// Regression test for crbug.com/498089319. The AsyncFunctionAwait builtin can
// invoke user-defined .then getters on the awaited value. If such a getter
// throws (creating an Error with a stack trace), OptimizedJSFrame::Summarize
// is called on the TF-compiled frame. Without kNeedsFrameState on the
// AsyncFunctionAwait call descriptor, no deoptimization entry is emitted at
// the call site, causing Summarize to fatal.
//
// Trigger condition: PromiseThenProtector must be invalidated (so the typed
// lowering fast path is skipped and generic lowering is used). We do this by
// installing a .then getter on Object.prototype before TF compilation.

// Install a .then getter on Object.prototype. This invalidates the
// PromiseThenProtector, forcing the AsyncFunctionAwait slow path in TF.
// The getter accesses an undeclared variable, triggering a ReferenceError
// (and thus a stack capture via OptimizedJSFrame::Summarize).
Object.defineProperty(Object.prototype, "then", {
  get() { return undeclaredVariable; },
  configurable: true,
});

const obj = {};

async function f() {
  for (let i = 0; i < 3; i++) {
    await obj;
  }
}

%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f();
