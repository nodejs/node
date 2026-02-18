// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// This tests aims at emitting a ThrowSuperNotCalledIfHole node that is inside a
// try-catch and that gets a hole input in a way that MaglevOptimizer can
// replace the ThrowSuperNotCalledIfHole by an unconditional Throw, but we don't
// want this folding to happen during graph building.
// Here is how we achieve this:
//
//   - class `B` only conditionally calls `super()` and then tries to access
//     `this`, in a catch block.
//
//   - function `foo` always calls `B` with `false`, which leads `B` to not call
//     `super()` and thus the `this` access should always throw.
//
//   - function `foo` gets the `false` to pass to `B` in a somewhat opaque way
//     by calling a function that returns `false`. We make sure that this
//     function has lower frequency that the call to `new B()` so that it gets
//     inlined after `B` and not before, which means that the MaglevOptimizer
//     will see the `false` but the graph builder won't.

class A {}
%PrepareFunctionForOptimization(A);

class B extends A {
  constructor(v) {
    try {
      if (v) {
        super();
      }
      this;
    } catch (e) {
      super();
    }
  }
}

// Warming up B's constructor so that there is feedback for the `super()` call.
%PrepareFunctionForOptimization(B);
new B(true);
new B(false);

function get_false(x) {
  x+x+x+x+x+x+x+x+x+x+x; // Increasing bytecode size to prevent eager-inlining.
  return false;
}
%PrepareFunctionForOptimization(get_false);

function foo(x) {
  let false_val;
  if (x) {
    false_val = get_false();
  } else {
    false_val = get_false();
  }

  return new B(false_val);
}

 %PrepareFunctionForOptimization(foo);
// Making sure that we have feedback for both sides of the `if (x)` in foo.
foo(true);
foo(false);
// Making sure that the frequency of the `else` is higher so that when inlining
// inside `foo`, we'll inline `new B()` before inlining `get_false` in the
// `else` branch (so that constant-folding cannot be eagerly done but has to be
// done later in the MaglevOptimizer).
foo(false);
foo(false);
foo(false);
foo(false);

function bar() {
  return foo(true);
}

%PrepareFunctionForOptimization(bar);
assertEquals(new B(false), bar());
assertEquals(new B(false), bar());

%OptimizeFunctionOnNextCall(bar);
assertEquals(new B(false), bar());
