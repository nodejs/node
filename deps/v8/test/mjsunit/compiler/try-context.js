// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function TryBlockCatch() {
  var global = 0;
  function f(a) {
    var x = a + 23
    try {
      let y = a + 42;
      function capture() { return x + y }
      throw "boom!";
    } catch(e) {
      global = x;
    }
    return x;
  }
  assertEquals(23, f(0));
  assertEquals(24, f(1));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(25, f(2));
  assertEquals(25, global);
})();

(function TryBlockFinally() {
  var global = 0;
  function f(a) {
    var x = a + 23
    try {
      let y = a + 42;
      function capture() { return x + y }
      throw "boom!";
    } finally {
      global = x;
    }
    return x;
  }
  assertThrows(function() { f(0) });
  assertThrows(function() { f(1) });
  %OptimizeFunctionOnNextCall(f);
  assertThrows(function() { f(2) });
  assertEquals(25, global);
})();

(function TryCatchCatch() {
  var global = 0;
  function f(a) {
    var x = a + 23
    try {
      try {
        throw "boom!";
      } catch(e2) {
        function capture() { return x + y }
        throw "boom!";
      }
    } catch(e) {
      global = x;
    }
    return x;
  }
  assertEquals(23, f(0));
  assertEquals(24, f(1));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(25, f(2));
  assertEquals(25, global);
})();

(function TryWithCatch() {
  var global = 0;
  function f(a) {
    var x = a + 23
    try {
      with ({ y : a + 42 }) {
        function capture() { return x + y }
        throw "boom!";
      }
    } catch(e) {
      global = x;
    }
    return x;
  }
  assertEquals(23, f(0));
  assertEquals(24, f(1));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(25, f(2));
  assertEquals(25, global);
})();
