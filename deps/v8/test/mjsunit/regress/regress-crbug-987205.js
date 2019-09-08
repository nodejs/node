// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(x) {
  // This used to generate two distinct stores to #undefined, violating the load
  // elimination invariant that there are no two store to the same const field:
  var obj1 = {
    [undefined]: 1,
    'undefined': 1
  };
  // This one cannot be discharged statically:
  var obj2 = {
    [undefined]: x,
    'undefined': 1
  };
  // Some more variations that exercise AllocateFastLiteral:
  var obj3 = {
    'undefined': 1,
    [undefined]: x
  };
  var obj4 = {
    'undefined': x,
    [undefined]: 1
  };
  assertEquals(obj1.undefined, 1);
  assertEquals(obj2.undefined, 1);
  assertEquals(obj3.undefined, x);
  assertEquals(obj4.undefined, 1);
}

%PrepareFunctionForOptimization(f);
f(1);
f(1);
%OptimizeFunctionOnNextCall(f);
f(2);


function g(x) {
  var obj1 = {
    [undefined]: 1,
    [undefined]: 1
  };
  var obj2 = {
    [undefined]: 1,
    [undefined]: x
  };
  var obj3 = {
    [undefined]: x,
    [undefined]: 1
  };
  var obj4 = {
    [undefined]: x,
    [undefined]: x
  };
  assertEquals(obj1.undefined, 1);
  assertEquals(obj2.undefined, x);
  assertEquals(obj3.undefined, 1);
  assertEquals(obj4.undefined, x);
}

%PrepareFunctionForOptimization(g);
g(1);
g(1);
%OptimizeFunctionOnNextCall(g);
g(2);
