// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// Flags: --allow-natives-syntax

// Test instanceof with proxies.

(function TestInstanceOfWithProxies() {
  function foo(x) {
    return x instanceof Array;
  }
  %PrepareFunctionForOptimization(foo);
  assertTrue(foo([]));
  assertFalse(foo({}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo([]));
  assertFalse(foo({}));

  var handler = {
    getPrototypeOf: function(target) { return Array.prototype; }
  };
  var p = new Proxy({}, handler);
  assertTrue(foo(p));
  var o = {};
  o.__proto__ = p;
  assertTrue(foo(o));

  // Make sure we are also correct if the handler throws.
  handler.getPrototypeOf = function(target) {
    throw "uncooperative";
  }
  assertThrows("foo(o)");

  // Including if the optimized function has a catch handler.
  function foo_catch(x) {
    try {
      x instanceof Array;
    } catch(e) {
      assertEquals("uncooperative", e);
      return true;
    }
    return false;
  }
  %PrepareFunctionForOptimization(foo_catch);
  assertTrue(foo_catch(o));
  %OptimizeFunctionOnNextCall(foo_catch);
  assertTrue(foo_catch(o));
  handler.getPrototypeOf = function(target) { return Array.prototype; }
  assertFalse(foo_catch(o));
})();


(function testInstanceOfWithRecursiveProxy() {
  // Make sure we gracefully deal with recursive proxies.
  var proxy = new Proxy({},{});
  proxy.__proto__ = proxy;
  // instanceof will cause an inifinite prototype walk.
  assertThrows(() => { proxy instanceof Object }, RangeError);

  var proxy2 = new Proxy({}, {getPrototypeOf() { return proxy2 }});
  assertThrows(() => { proxy instanceof Object }, RangeError);
})();
