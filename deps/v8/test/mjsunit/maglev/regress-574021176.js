// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let g = () => class {};

function* gen(o) {
  // Context-allocated, maybe-assigned variable so the function context uses a
  // ContextCell for `x`.
  let x = 42;
  () => x = 0;

  // Polymorphic property load with a call continuation. The class literal
  // pushes a `CLASS_SCOPE` context (needed for `super.x`) before evaluating
  // `extends g()` and pops it after defining the class. When `g()` aborts in
  // every polymorphic arm, skipping the rest of the continuation must still pop
  // that `CLASS_SCOPE`.
  o.f(class extends g() {
    constructor() {
      super.x;
    }
  });

  // In dead code after all polymorphic arms abort, `SuspendGenerator` records
  // `current_scope_info_` on the `ResumeGenerator` merge state.
  yield;

  // Loads `x` using the `ScopeInfo` from the resume merge state (which must be
  // the generator's `FUNCTION_SCOPE` with cells, not the leaked `CLASS_SCOPE`).
  return x;
}

const suspended = gen({f() {}});
suspended.next();

g = class {};
%PrepareFunctionForOptimization(gen);
assertThrows(() => gen({f() {}}).next(), TypeError);
assertThrows(() => gen({a: 1, f() {}}).next(), TypeError);

%OptimizeFunctionOnNextCall(gen);
assertEquals(42, suspended.next().value);
