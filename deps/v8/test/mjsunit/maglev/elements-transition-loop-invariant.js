// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Loops whose body transitions elements kinds (HOLEY_SMI_ELEMENTS ->
// HOLEY_DOUBLE_ELEMENTS) while other arrays are accessed with loop-invariant
// elements loads.

(function TestInvariantOuterElements() {
  function g(cost, previousNode, values, segmentCount) {
    for (var s = 0; s < values.length; s++) {
      var row = cost[s];
      for (var k = 0; k < segmentCount; k++) {
        if (previousNode[s][k] === undefined) continue;
        for (var e = s + 1; e < values.length; e++) {
          var t = row[k] + values[e];
          if (previousNode[e][k + 1] === undefined || t < cost[e][k + 1]) {
            cost[e][k + 1] = t;
            previousNode[e][k + 1] = s;
          }
        }
      }
    }
  }

  function setup(n, sc) {
    var values = new Array(n);
    for (var i = 0; i < n; i++) values[i] = i * 1.3;
    var cost = new Array(n);
    for (var i = 0; i < n; i++) {
      cost[i] = new Array(sc + 1);
      for (var j = 0; j <= sc; j++) cost[i][j] = 0;
    }
    var previousNode = new Array(n);
    for (var i = 0; i < n; i++) previousNode[i] = new Array(sc + 1);
    previousNode[0] = [-1];
    return [cost, previousNode, values, sc];
  }

  %PrepareFunctionForOptimization(g);
  var warm1 = setup(30, 6);
  g(warm1[0], warm1[1], warm1[2], warm1[3]);
  var expected = setup(30, 6);
  g(expected[0], expected[1], expected[2], expected[3]);

  %OptimizeFunctionOnNextCall(g);
  var actual = setup(30, 6);
  g(actual[0], actual[1], actual[2], actual[3]);

  assertEquals(expected[0], actual[0]);
  assertEquals(expected[1], actual[1]);
})();

(function TestTransitionThroughAlias() {
  function f(a, b, n) {
    var sum = 0;
    for (var i = 0; i < n; i++) {
      sum += a[0];
      b[1] = 1.5;
      sum += a[1];
    }
    return sum;
  }

  function make() {
    var a = new Array(4);
    a[0] = 1;
    return a;
  }

  %PrepareFunctionForOptimization(f);
  var r1 = f(make(), make(), 3);
  var a = make();
  var expected = f(a, a, 3);
  %OptimizeFunctionOnNextCall(f);
  var b = make();
  assertEquals(expected, f(b, b, 3));
})();

(function TestOuterArrayTransitionsInLoop() {
  function f(m, n) {
    var sum = 0;
    for (var i = 0; i < n; i++) {
      var x = m[0];
      if (x !== undefined) sum += x;
      if (i === 1) m[0] = 0.5;
    }
    return sum;
  }

  function make() {
    var m = new Array(4);
    m[0] = 7;
    return m;
  }

  %PrepareFunctionForOptimization(f);
  var expected = f(make(), 5);
  f(make(), 5);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(expected, f(make(), 5));
})();
