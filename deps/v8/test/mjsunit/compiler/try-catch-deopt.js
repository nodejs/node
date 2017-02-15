// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function LazyDeoptFromTryBlock() {
  function g(dummy) {
    %DeoptimizeFunction(f);
    throw 42;
  }

  function f() {
    var a = 1;
    try {
      var dummy = 2;  // perturb the stack height.
      g(dummy);
    } catch (e) {
      return e + a;
    }
  }

  assertEquals(43, f());
  assertEquals(43, f());
  %NeverOptimizeFunction(g);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(43, f());
})();


(function LazyDeoptDoublyNestedTryBlock() {
  function g(dummy) {
    %DeoptimizeFunction(f);
    throw 42;
  }

  function f() {
    var b;
    try {
      var a = 1;
      try {
        var dummy = 2;  // perturb the stack height.
        g(dummy);
      } catch (e) {
        b = e + a;
      }
    } catch (e) {
      return 0;
    }
    return b;
  }

  assertEquals(43, f());
  assertEquals(43, f());
  %NeverOptimizeFunction(g);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(43, f());
})();

(function LazyDeoptInlinedTry() {
  function g(dummy) {
    %DeoptimizeFunction(f);
    %DeoptimizeFunction(h);
    throw 42;
  }

  function h() {
    var a = 1;
    try {
      var dummy = 2;  // perturb the stack height.
      g(dummy);
    } catch (e) {
      b = e + a;
    }
    return b;
  }

  function f() {
    var c = 1;
    return h() + 1;
  }

  assertEquals(44, f());
  assertEquals(44, f());
  %NeverOptimizeFunction(g);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(44, f());
})();

(function LazyDeoptInlinedIntoTry() {
  function g(c) {
    %DeoptimizeFunction(f);
    %DeoptimizeFunction(h);
    throw c;
  }

  function h(c) {
    return g(c);
  }

  function f() {
    var a = 1;
    try {
      var c = 42;  // perturb the stack height.
      h(c);
    } catch (e) {
      a += e;
    }
    return a;
  }

  assertEquals(43, f());
  assertEquals(43, f());
  %NeverOptimizeFunction(g);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(43, f());
})();

(function LazyDeoptTryBlockContextCatch() {
  var global = 0;

  function g() {
    %DeoptimizeFunction(f);
    throw "boom!";
  }

  function f(a) {
    var x = a + 23
    try {
      let y = a + 42;
      function capture() { return x + y }
      g();
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

(function LazyDeoptTryBlockFinally() {
  var global = 0;

  function g() {
    %DeoptimizeFunction(f);
    throw "boom!";
  }

  function f(a) {
    var x = a + 23
    try {
      let y = a + 42;
      function capture() { return x + y }
      g();
    } finally {
      global = x;
    }
    return x;
  }
  assertThrows(function() { f(0) });
  assertThrows(function() { f(1) });
  %OptimizeFunctionOnNextCall(f);
  assertThrowsEquals(function() { f(2) }, "boom!");
  assertEquals(25, global);
})();

(function LazyDeoptTryCatchContextCatch() {
  var global = 0;

  function g() {
    %DeoptimizeFunction(f);
    throw 5;
  }

  function f(a) {
    var x = a + 23
    try {
      try {
        throw 1;
      } catch(e2) {
        function capture() { return x + y }
        g();
      }
    } catch(e) {
      global = x + e;
    }
    return x;
  }
  assertEquals(23, f(0));
  assertEquals(24, f(1));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(25, f(2));
  assertEquals(30, global);
})();

(function LazyDeoptTryWithContextCatch() {
  var global = 0;

  function g() {
    %DeoptimizeFunction(f);
    throw 5;
  }

  function f(a) {
    var x = a + 23
    try {
      with ({ y : a + 42 }) {
        function capture() { return x + y }
        g();
      }
    } catch(e) {
      global = x + e;
    }
    return x;
  }
  assertEquals(23, f(0));
  assertEquals(24, f(1));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(25, f(2));
  assertEquals(30, global);
})();
