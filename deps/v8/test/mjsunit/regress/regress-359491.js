// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function () {
  function f(a, b, mode) {
    if (mode) {
      return a === b;
    } else {
      return a === b;
    }
  }

  // Gather type feedback for both branches.
  f("a", "b", 1);
  f("c", "d", 1);
  f("a", "b", 0);
  f("c", "d", 0);

  function g(mode) {
    var x = 1e10 | 0;
    f(x, x, mode);
  }

  // Gather type feedback for g, but only on one branch for f.
  g(1);
  g(1);
  %OptimizeFunctionOnNextCall(g);
  // Optimize g, which inlines f. Both branches in f will see the constant.
  g(0);
})();

(function () {
  function f(a, b, mode) {
    if (mode) {
      return a === b;
    } else {
      return a === b;
    }
  }

  // Gather type feedback for both branches.
  f({ a : 1}, {b : 1}, 1);
  f({ c : 1}, {d : 1}, 1);
  f({ a : 1}, {c : 1}, 0);
  f({ b : 1}, {d : 1}, 0);

  function g(mode) {
    var x = 1e10 | 0;
    f(x, x, mode);
  }

  // Gather type feedback for g, but only on one branch for f.
  g(1);
  g(1);
  %OptimizeFunctionOnNextCall(g);
  // Optimize g, which inlines f. Both branches in f will see the constant.
  g(0);
})();
