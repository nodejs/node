// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --noconcurrent-recompilation

var a = {};

function init() {
  a = [];
  for (var __v_1 = 0; __v_1 < 10016; __v_1++) {
    a.push({});
  }
  a.map(function() {}) + '';
}
init();

function foo() {
  a.push((a + "!", 23));
  return a;
};
%PrepareFunctionForOptimization(foo);
assertEquals(23, foo()[10016]);
assertEquals(23, foo()[10017]);
assertEquals(23, foo()[10018]);
%OptimizeFunctionOnNextCall(foo);
assertEquals(23, foo()[10019]);
