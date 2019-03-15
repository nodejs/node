// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Flags: --allow-natives-syntax --expose-gc


// Simple test of capture
(function testCapturedArguments() {
  function h() {
    return g.arguments[0];
  }

  function g(x) {
    return h();
  }

  function f() {
    var l = { y : { z : 4 }, x : 2 }
    var r = g(l);
    assertEquals(2, r.x);
    assertEquals(2, l.x);
    l.x = 3;
    l.y.z = 5;
    // Test that the arguments object is properly
    // aliased
    assertEquals(3, r.x);
    assertEquals(3, l.x);
    assertEquals(5, r.y.z);
  }

  %PrepareFunctionForOptimization(f);
  f(); f(); f();
  %OptimizeFunctionOnNextCall(f);
  f();
})();


// Get the arguments object twice, test aliasing
(function testTwoCapturedArguments() {
  function h() {
    return g.arguments[0];
  }

  function i() {
    return g.arguments[0];
  }

  function g(x) {
    return {h : h() , i : i()};
  }

  function f() {
    var l = { y : { z : 4 }, x : 2 }
    var r = g(l);
    assertEquals(2, r.h.x)
    l.y.z = 3;
    assertEquals(3, r.h.y.z);
    assertEquals(3, r.i.y.z);
  }

  %PrepareFunctionForOptimization(f);
  f(); f(); f();
  %OptimizeFunctionOnNextCall(f);
  f();
})();


// Nested arguments object test
(function testTwoCapturedArgumentsNested() {
  function i() {
    return { gx : g.arguments[0], hx : h.arguments[0] };
  }

  function h(x) {
    return i();
  }

  function g(x) {
    return h(x.y);
  }

  function f() {
    var l = { y : { z : 4 }, x : 2 }
    var r = g(l);
    assertEquals(2, r.gx.x)
    assertEquals(4, r.gx.y.z)
    assertEquals(4, r.hx.z)
    l.y.z = 3;
    assertEquals(3, r.gx.y.z)
    assertEquals(3, r.hx.z)
    assertEquals(3, l.y.z)
  }

  %PrepareFunctionForOptimization(f);
  f(); f(); f();
  %OptimizeFunctionOnNextCall(f);
  f();
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeFunctionOnNextCall(f);
  f(); f();
})();


// Nested arguments object test with different inlining
(function testTwoCapturedArgumentsNested2() {
  function i() {
    return { gx : g.arguments[0], hx : h.arguments[0] };
  }

  function h(x) {
    return i();
  }

  function g(x) {
    return h(x.y);
  }

  function f() {
    var l = { y : { z : 4 }, x : 2 }
    var r = g(l);
    assertEquals(2, r.gx.x)
    assertEquals(4, r.gx.y.z)
    assertEquals(4, r.hx.z)
    l.y.z = 3;
    assertEquals(3, r.gx.y.z)
    assertEquals(3, r.hx.z)
    assertEquals(3, l.y.z)
  }

  %PrepareFunctionForOptimization(f);
  %NeverOptimizeFunction(i);
  f(); f(); f();
  %OptimizeFunctionOnNextCall(f);
  f();
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeFunctionOnNextCall(f);
  f(); f();
})();


// Multiple captured argument test
(function testTwoArgumentsCapture() {
  function h() {
    return { a : g.arguments[1], b : g.arguments[0] };
  }

  function g(x, y) {
    return h();
  }

  function f() {
    var l = { y : { z : 4 }, x : 2 }
    var k = { t : { u : 3 } };
    var r = g(k, l);
    assertEquals(2, r.a.x)
    assertEquals(4, r.a.y.z)
    assertEquals(3, r.b.t.u)
    l.y.z = 6;
    r.b.t.u = 7;
    assertEquals(6, r.a.y.z)
    assertEquals(7, k.t.u)
  }

  %PrepareFunctionForOptimization(f);
  f(); f(); f();
  %OptimizeFunctionOnNextCall(f);
  f();
  %PrepareFunctionForOptimization(f);
  f();
  %OptimizeFunctionOnNextCall(f);
  f(); f();
})();

// Test variable index access to strict arguments
// with up to 2 elements.
(function testArgumentsVariableIndexStrict() {
  function g() {
    "use strict";
    var s = 0;
    for (var i = 0; i < arguments.length; ++i) s += arguments[i];
    return s;
  }

  function f(x, y) {
    // (a) arguments[i] is dead code since arguments.length is 0.
    const a = g();
    // (b) arguments[i] always yields the first element.
    const b = g(x);
    // (c) arguments[i] can yield either x or y.
    const c = g(x, y);
    return a + b + c;
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(4, f(1, 2));
  assertEquals(5, f(2, 1));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(4, f(1, 2));
  assertEquals(5, f(2, 1));
})();

// Test variable index access to sloppy arguments
// with up to 2 elements.
(function testArgumentsVariableIndexSloppy() {
  function g() {
    var s = 0;
    for (var i = 0; i < arguments.length; ++i) s += arguments[i];
    return s;
  }

  function f(x, y) {
    // (a) arguments[i] is dead code since arguments.length is 0.
    const a = g();
    // (b) arguments[i] always yields the first element.
    const b = g(x);
    // (c) arguments[i] can yield either x or y.
    const c = g(x, y);
    return a + b + c;
  }

  %PrepareFunctionForOptimization(f);
  assertEquals(4, f(1, 2));
  assertEquals(5, f(2, 1));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(4, f(1, 2));
  assertEquals(5, f(2, 1));
})();
