// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --noturbo-inlining

// Helper function to compare two arguments objects of strict mode functions.
// Access to arguments.callee results in TypeError for them, so assertEquals
// can't be used directly.
function assertEqualsArgumentsStrict(a, b) {
  assertEquals(JSON.stringify(a), JSON.stringify(b));
}

// Ensure that arguments in sloppy mode function works
// properly when called directly from optimized code.
(function() {
  function g() { return arguments; }
  function f() { return g(1, 2, 3); }

  %PrepareFunctionForOptimization(f);
  assertEquals(g(1, 2, 3), f());
  assertEquals(g(1, 2, 3), f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(g(1, 2, 3), f());
  %PrepareFunctionForOptimization(g);
  %OptimizeFunctionOnNextCall(g);
  assertEquals(g(1, 2, 3), f());
})();

// Ensure that arguments in strict mode function works
// properly when called directly from optimized code.
(function() {
  "use strict";
  function g() { return arguments; }
  function f() { return g(1, 2, 3); }

  %PrepareFunctionForOptimization(f);
  assertEqualsArgumentsStrict(g(1, 2, 3), f());
  assertEqualsArgumentsStrict(g(1, 2, 3), f());
  %OptimizeFunctionOnNextCall(f);
  assertEqualsArgumentsStrict(g(1, 2, 3), f());
  %PrepareFunctionForOptimization(g);
  %OptimizeFunctionOnNextCall(g);
  assertEqualsArgumentsStrict(g(1, 2, 3), f());
})();

// Ensure that arguments in sloppy mode function works
// properly when called directly from optimized code,
// and the access to "arguments" is hidden inside eval().
(function() {
  function g() { return eval("arguments"); }
  function f() { return g(1, 2, 3); }

  %PrepareFunctionForOptimization(f);
  assertEquals(g(1, 2, 3), f());
  assertEquals(g(1, 2, 3), f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(g(1, 2, 3), f());
  %PrepareFunctionForOptimization(g);
  %OptimizeFunctionOnNextCall(g);
  assertEquals(g(1, 2, 3), f());
})();

// Ensure that arguments in strict mode function works
// properly when called directly from optimized code,
// and the access to "arguments" is hidden inside eval().
(function() {
  "use strict";
  function g() { return eval("arguments"); }
  function f() { return g(1, 2, 3); }

  %PrepareFunctionForOptimization(f);
  assertEqualsArgumentsStrict(g(1, 2, 3), f());
  assertEqualsArgumentsStrict(g(1, 2, 3), f());
  %OptimizeFunctionOnNextCall(f);
  assertEqualsArgumentsStrict(g(1, 2, 3), f());
  %PrepareFunctionForOptimization(g);
  assertEqualsArgumentsStrict(g(1, 2, 3), f());
  %OptimizeFunctionOnNextCall(g);
  assertEqualsArgumentsStrict(g(1, 2, 3), f());
})();

// Ensure that `Function.arguments` accessor does the
// right thing in sloppy mode functions called directly
// from optimized code.
(function() {
  function h() { return g.arguments; }
  function g() { return h(); }
  function f() { return g(1, 2, 3); }

  %PrepareFunctionForOptimization(f);
  assertEquals(g(1, 2, 3), f());
  assertEquals(g(1, 2, 3), f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(g(1, 2, 3), f());
  %PrepareFunctionForOptimization(g);
  assertEquals(g(1, 2, 3), f());
  %OptimizeFunctionOnNextCall(g);
  assertEquals(g(1, 2, 3), f());
})();
(function() {
  function h() { return g.arguments; }
  function g() { return h(); }
  function f() { "use strict"; return g(1, 2, 3); }

  %PrepareFunctionForOptimization(f);
  assertEquals(g(1, 2, 3), f());
  assertEquals(g(1, 2, 3), f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(g(1, 2, 3), f());
  %PrepareFunctionForOptimization(g);
  assertEquals(g(1, 2, 3), f());
  %OptimizeFunctionOnNextCall(g);
  assertEquals(g(1, 2, 3), f());
})();
(function() {
  function h() { "use strict"; return g.arguments; }
  function g() { return h(); }
  function f() { return g(1, 2, 3); }

  %PrepareFunctionForOptimization(f);
  assertEquals(g(1, 2, 3), f());
  assertEquals(g(1, 2, 3), f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(g(1, 2, 3), f());
  %PrepareFunctionForOptimization(g);
  assertEquals(g(1, 2, 3), f());
  %OptimizeFunctionOnNextCall(g);
  assertEquals(g(1, 2, 3), f());
})();
(function() {
  function h() { "use strict"; return g.arguments; }
  function g() { return h(); }
  function f() { "use strict"; return g(1, 2, 3); }

  %PrepareFunctionForOptimization(f);
  assertEquals(g(1, 2, 3), f());
  assertEquals(g(1, 2, 3), f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(g(1, 2, 3), f());
  %PrepareFunctionForOptimization(g);
  assertEquals(g(1, 2, 3), f());
  %OptimizeFunctionOnNextCall(g);
  assertEquals(g(1, 2, 3), f());
})();

// Ensure that `Function.arguments` works properly in
// combination with the `Function.caller` proper.
(function() {
  function h() { return h.caller.arguments; }
  function g() { return h(); }
  function f() { return g(1, 2, 3); }

  %PrepareFunctionForOptimization(f);
  assertEquals(g(1, 2, 3), f());
  assertEquals(g(1, 2, 3), f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(g(1, 2, 3), f());
  %PrepareFunctionForOptimization(g);
  assertEquals(g(1, 2, 3), f());
  %OptimizeFunctionOnNextCall(g);
  assertEquals(g(1, 2, 3), f());
})();
(function() {
  function h() { return h.caller.arguments; }
  function g() { return h(); }
  function f() { "use strict"; return g(1, 2, 3); }

  %PrepareFunctionForOptimization(f);
  assertEquals(g(1, 2, 3), f());
  assertEquals(g(1, 2, 3), f());
  %OptimizeFunctionOnNextCall(f);
  assertEquals(g(1, 2, 3), f());
  %PrepareFunctionForOptimization(g);
  assertEquals(g(1, 2, 3), f());
  %OptimizeFunctionOnNextCall(g);
  assertEquals(g(1, 2, 3), f());
})();
