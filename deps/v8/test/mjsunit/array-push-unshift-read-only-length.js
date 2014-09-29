// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test(mode) {
  var a = [];
  Object.defineProperty(a, "length", { writable : false});

  function check(f) {
    try {
      f(a);
    } catch(e) { }
    assertFalse(0 in a);
    assertEquals(0, a.length);
  }

  function push(a) {
    a.push(3);
  }

  if (mode == "fast properties") %ToFastProperties(a);

  check(push);
  check(push);
  check(push);
  %OptimizeFunctionOnNextCall(push);
  check(push);

  function unshift(a) {
    a.unshift(3);
  }

  check(unshift);
  check(unshift);
  check(unshift);
  %OptimizeFunctionOnNextCall(unshift);
  check(unshift);
}

test("fast properties");

test("normalized");

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

// TODO(ulan): According to the ECMA-262 unshift should throw an exception
// when moving b[0] to b[3] (see 15.4.4.13 step 6.d.ii). This is difficult
// to do with our current implementation of SmartMove() in src/array.js and
// it will regress performance. Uncomment the following line once acceptable
// solution is found:
// assertFalse(2 in b);
// assertFalse(3 in b);
// assertEquals(2, b.length);
