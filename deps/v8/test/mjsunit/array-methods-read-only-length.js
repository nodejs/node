// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function testAdd(mode) {
  var a = [];
  Object.defineProperty(a, "length", { writable : false});

  function check(f) {
    assertThrows(function() { f(a) }, TypeError);
    assertFalse(0 in a);
    assertEquals(0, a.length);
  }

  function push(a) {
    a.push(3);
  }

  if (mode == "fast properties") %ToFastProperties(a);

  %PrepareFunctionForOptimization(push);
  check(push);
  check(push);
  check(push);
  %OptimizeFunctionOnNextCall(push);
  check(push);

  function unshift(a) {
    a.unshift(3);
  }

  %PrepareFunctionForOptimization(unshift);
  check(unshift);
  check(unshift);
  check(unshift);
  %OptimizeFunctionOnNextCall(unshift);
  check(unshift);

  function splice(a) {
    a.splice(0, 0, 3);
  }

  %PrepareFunctionForOptimization(splice);
  check(splice);
  check(splice);
  check(splice);
  %OptimizeFunctionOnNextCall(splice);
  check(splice);
}

testAdd("fast properties");

testAdd("normalized");

function testRemove(a, mode) {
  Object.defineProperty(a, "length", { writable : false});

  function check(f) {
    assertThrows(function() { f(a) }, TypeError);
    assertEquals(3, a.length);
  }

  if (mode == "fast properties") %ToFastProperties(a);

  function pop(a) {
    a.pop();
  }

  %PrepareFunctionForOptimization(pop);
  check(pop);
  check(pop);
  check(pop);
  %OptimizeFunctionOnNextCall(pop);
  check(pop);

  function shift(a) {
    a.shift();
  }

  %PrepareFunctionForOptimization(shift);
  check(shift);
  check(shift);
  check(shift);
  %OptimizeFunctionOnNextCall(shift);
  check(shift);

  function splice(a) {
    a.splice(0, 1);
  }

  %PrepareFunctionForOptimization(splice);
  check(splice);
  check(splice);
  check(splice);
  %OptimizeFunctionOnNextCall(splice);
  check(splice);

  %ClearFunctionFeedback(pop);
  %ClearFunctionFeedback(shift);
  %ClearFunctionFeedback(splice);
}

for (var i = 0; i < 3; i++) {
  var a = [1, 2, 3];
  if (i == 1) {
    a = [1, 2, 3.5];
  } else if (i == 2) {
    a = [1, 2, "string"];
  }
  testRemove(a, "fast properties");
  testRemove(a, "normalized");
}

var b = [];
Object.defineProperty(b.__proto__, "0", {
  set : function(v) {
    b.x = v;
    Object.defineProperty(b, "length", { writable : false });
  },
  get: function() {
    return b.x;
  }
});

b = [];
try {
  b.push(3, 4, 5);
} catch(e) { }
assertFalse(1 in b);
assertFalse(2 in b);
assertEquals(0, b.length);

b = [];
try {
  b.unshift(3, 4, 5);
} catch(e) { }
assertFalse(1 in b);
assertFalse(2 in b);
assertEquals(0, b.length);

b = [1, 2];
try {
  b.unshift(3, 4, 5);
} catch(e) { }
assertEquals(3, b[0]);
assertEquals(4, b[1]);
assertEquals(5, b[2]);
assertEquals(1, b[3]);
assertEquals(2, b[4]);
assertEquals(5, b.length);

b = [1, 2];

Object.defineProperty(b.__proto__, "4", {
  set : function(v) {
    b.z = v;
    Object.defineProperty(b, "length", { writable : false });
  },
  get: function() {
    return b.z;
  }
});

try {
  b.unshift(3, 4, 5);
} catch(e) { }

assertFalse(2 in b);
assertFalse(3 in b);
assertEquals(2, b.length);
