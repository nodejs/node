// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Repeated FixedArray element reads with intervening writes that cannot
// touch a FixedArray payload (double stores, elements-kind transitions).

(function TestReadStoreDoubleRead() {
  function f(prev, cost, e, k) {
    var a = prev[e];
    cost[k] = 1.5;
    var b = prev[e];
    return [a, b];
  }

  function make() {
    var prev = new Array(4);
    prev[0] = {x: 1};
    var cost = new Array(4);
    cost[0] = 0;
    return [prev, cost];
  }

  %PrepareFunctionForOptimization(f);
  var w = make();
  f(w[0], w[1], 0, 1);
  var e1 = make();
  var expected = f(e1[0], e1[1], 0, 1);
  %OptimizeFunctionOnNextCall(f);
  var a1 = make();
  assertEquals(expected, f(a1[0], a1[1], 0, 1));
})();

(function TestSameArrayStoreBetweenReads() {
  function f(a, i, j, v) {
    var x = a[i];
    a[j] = v;
    var y = a[i];
    return [x, y];
  }

  function make() {
    var a = new Array(4);
    a[0] = 10;
    a[1] = 20;
    return a;
  }

  %PrepareFunctionForOptimization(f);
  f(make(), 0, 1, 30);
  f(make(), 0, 0, 30);
  var expected_other = f(make(), 0, 1, 30);
  var expected_same = f(make(), 0, 0, 30);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(expected_other, f(make(), 0, 1, 30));
  assertEquals(expected_same, f(make(), 0, 0, 30));
})();

(function TestAliasStoreBetweenReads() {
  function f(a, b, i) {
    var x = a[i];
    b[i] = x + 1;
    var y = a[i];
    return y;
  }

  function make() {
    var a = new Array(4);
    a[2] = 5;
    return a;
  }

  %PrepareFunctionForOptimization(f);
  var w = make();
  f(w, w, 2);
  var e = make();
  var expected = f(e, e, 2);
  %OptimizeFunctionOnNextCall(f);
  var r = make();
  assertEquals(expected, f(r, r, 2));
})();

(function TestThrowBetweenReads() {
  function g(x) {
    if (x) throw new Error("boom");
    return 1;
  }

  function f(a, i, x) {
    var s = 0;
    var before = a[i];
    try {
      s += g(x);
    } catch (e) {
      s += 100;
    }
    var after = a[i];
    return s + before + after;
  }

  function make() {
    var a = new Array(4);
    a[1] = 7;
    return a;
  }

  %PrepareFunctionForOptimization(f);
  f(make(), 1, false);
  f(make(), 1, true);
  var expected_nothrow = f(make(), 1, false);
  var expected_throw = f(make(), 1, true);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(expected_nothrow, f(make(), 1, false));
  assertEquals(expected_throw, f(make(), 1, true));
})();

(function TestMoreKeysThanTheCacheHolds() {
  var terms = [];
  for (var k = 0; k < 200; k++) terms.push("a[i + " + k + "]");
  var f = new Function("a", "i", "return " + terms.join(" + ") + ";");

  function make() {
    var a = new Array(256);
    for (var k = 0; k < 256; k++) a[k] = k;
    return a;
  }

  %PrepareFunctionForOptimization(f);
  f(make(), 1);
  var expected = f(make(), 1);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(expected, f(make(), 1));
})();

(function TestNestedLoopsWithTransitions() {
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
  var warm = setup(20, 5);
  g(warm[0], warm[1], warm[2], warm[3]);
  var exp = setup(20, 5);
  g(exp[0], exp[1], exp[2], exp[3]);
  %OptimizeFunctionOnNextCall(g);
  var act = setup(20, 5);
  g(act[0], act[1], act[2], act[3]);
  assertEquals(exp[0], act[0]);
  assertEquals(exp[1], act[1]);
})();
