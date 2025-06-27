// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test(e, f, v) {
  assertEquals(e, f(v));
  assertEquals(e, f(v));
  assertEquals(e, f(v));
}

function foo(t) {
  for (var x in t) {
    for (var i = 0; i < 2; i++) {
      %OptimizeOsr();
      %PrepareFunctionForOptimization(foo);
    }
  }
  return 5;
}
%PrepareFunctionForOptimization(foo);

test(5, foo, {x:20});

function bar(t) {
  var sum = 0;
  for (var x in t) {
    for (var i = 0; i < 2; i++) {
      %OptimizeOsr();
      sum += t[x];
      %PrepareFunctionForOptimization(bar);
    }
  }
  return sum;
}
%PrepareFunctionForOptimization(bar);

test(62, bar, {x:20,y:11});
