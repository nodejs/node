// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var a = new Int8Array(100);

function has(i) {
  return i in a;
};
%PrepareFunctionForOptimization(has);
assertTrue(has(0));
assertTrue(has(0));
%OptimizeFunctionOnNextCall(has);
assertTrue(has(0));
assertTrue(has(1));

function get(i) {
  return a[i];
};
%PrepareFunctionForOptimization(get);
assertEquals(0, get(0));
assertEquals(0, get(0));
%OptimizeFunctionOnNextCall(get);
assertEquals(0, get(0));
assertEquals(0, get(1));

function set(i) {
  return a[i] = 42 + i;
};
%PrepareFunctionForOptimization(set);
assertEquals(42, set(0));
assertEquals(42, a[0]);
assertEquals(42, set(0));
assertEquals(42, a[0]);
%OptimizeFunctionOnNextCall(set);
assertEquals(42, set(0));
assertEquals(42, a[0]);
assertEquals(43, set(1));
assertEquals(43, a[1]);
