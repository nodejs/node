// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-tailcalls
"use strict";

Error.prepareStackTrace = (e,s) => s;

function CheckStackTrace(expected) {
  var stack = (new Error()).stack;
  assertEquals("CheckStackTrace", stack[0].getFunctionName());
  for (var i = 0; i < expected.length; i++) {
    assertEquals(expected[i].name, stack[i + 1].getFunctionName());
  }
}


// Tail call proxy function when caller does not have an arguments
// adaptor frame.
(function test() {
  // Caller and callee have same number of arguments.
  function f1(a) {
    CheckStackTrace([f1, test]);
    return 10 + a;
  }
  var p1 = new Proxy(f1, {});
  function g1(a) { return p1(2); }
  assertEquals(12, g1(1));

  // Caller has more arguments than callee.
  function f2(a) {
    CheckStackTrace([f2, test]);
    return 10 + a;
  }
  var p2 = new Proxy(f2, {});
  function g2(a, b, c) { return p2(2); }
  assertEquals(12, g2(1, 2, 3));

  // Caller has less arguments than callee.
  function f3(a, b, c) {
    CheckStackTrace([f3, test]);
    return 10 + a + b + c;
  }
  var p3 = new Proxy(f3, {});
  function g3(a) { return p3(2, 3, 4); }
  assertEquals(19, g3(1));

  // Callee has arguments adaptor frame.
  function f4(a, b, c) {
    CheckStackTrace([f4, test]);
    return 10 + a;
  }
  var p4 = new Proxy(f4, {});
  function g4(a) { return p4(2); }
  assertEquals(12, g4(1));
})();


// Tail call proxy function when caller has an arguments adaptor frame.
(function test() {
  // Caller and callee have same number of arguments.
  function f1(a) {
    CheckStackTrace([f1, test]);
    return 10 + a;
  }
  var p1 = new Proxy(f1, {});
  function g1(a) { return p1(2); }
  assertEquals(12, g1());

  // Caller has more arguments than callee.
  function f2(a) {
    CheckStackTrace([f2, test]);
    return 10 + a;
  }
  var p2 = new Proxy(f2, {});
  function g2(a, b, c) { return p2(2); }
  assertEquals(12, g2());

  // Caller has less arguments than callee.
  function f3(a, b, c) {
    CheckStackTrace([f3, test]);
    return 10 + a + b + c;
  }
  var p3 = new Proxy(f3, {});
  function g3(a) { return p3(2, 3, 4); }
  assertEquals(19, g3());

  // Callee has arguments adaptor frame.
  function f4(a, b, c) {
    CheckStackTrace([f4, test]);
    return 10 + a;
  }
  var p4 = new Proxy(f4, {});
  function g4(a) { return p4(2); }
  assertEquals(12, g4());
})();
