// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --ignore-unhandled-promises

// Regression test for crbug.com/498089319. Variant that triggers lazy
// deoptimization: the .then getter calls %DeoptimizeFunction on the
// TF-compiled frame, forcing the deoptimizer to reconstruct it at the
// AsyncFunctionAwait call site.

const obj = {};

async function f() { await obj; }

// Install a .then getter on Object.prototype. This invalidates the
// PromiseThenProtector, forcing the AsyncFunctionAwait slow path in TF.
// The getter deoptimizes f, triggering lazy deoptimization of the compiled
// frame at the AsyncFunctionAwait call site.
Object.defineProperty(Object.prototype, "then", {
  get() { %DeoptimizeFunction(f); return undefined; },
  configurable: true,
});

%PrepareFunctionForOptimization(f);
f();
%OptimizeFunctionOnNextCall(f);
f();
