// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan --turbolev

// This test is building a ThrowSuperNotCalledIfHole surrounded by a try-catch,
// where the catch handler has never been reached before, which means that the
// ThrowSuperNotCalledIfHole will be marked as LazyDeoptOnThrow. Then, we want
// the MaglevOptimizer to realize that this ThrowSuperNotCalledIfHole can be
// constant-folded into an unconditional Throw (and we want everything to keep
// working properly then).

class Base {}
class Derived extends Base {
  constructor(c) {
    if (c) {
      super();
    }
    this;
  }
}
%PrepareFunctionForOptimization(Base);
%PrepareFunctionForOptimization(Derived);
new Derived(true);

function foo(c1, c2) {
  if (c1) return 17;

  try {
    return new Derived(c2);
  } catch (e) {
    return 42;
  }
}

%PrepareFunctionForOptimization(foo);
foo(false, true);

function get_false(x) {
  x+x+x+x+x+x+x+x+x+x+x; // Increasing bytecode size to prevent eager-inlining.
  return false;
}
%PrepareFunctionForOptimization(get_false);
get_false();


function bar(c1, c2) {
  let false_value;
  if (c2 == 0) {
    false_value = get_false();
  } else {
    false_value = get_false();
  }

  return foo(c1, false_value);
}

%PrepareFunctionForOptimization(bar);
bar(true, 1);
bar(true, 1);
bar(true, 1);
bar(true, 1);
bar(true, 1);

function baz(c) {
  return bar(c, 0);
}

%PrepareFunctionForOptimization(baz);
assertEquals(17, baz(true));

%OptimizeFunctionOnNextCall(baz);
assertEquals(17, baz(true));
assertOptimized(baz);

// Triggering lazy deopt because hitting the catch for the 1st time.
assertEquals(42, baz(false));
assertUnoptimized(baz);

// Re-optimizing, to make sure that everything works properly there as well.
%OptimizeFunctionOnNextCall(baz);
assertEquals(42, baz(false));
assertOptimized(baz);
