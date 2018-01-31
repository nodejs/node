// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  var x = 1;
  x.__proto__.f = function() { return 1; }

  function g() {}
  g.prototype.f =  function() { return 3; };
  var y = new g();

  function f(obj) {
    return obj.f();
  }

  f(x);
  f(y);
  f(x);
  f(y);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(1, f(x));
  assertEquals(3, f(y));
})();

(function() {
  function f() { return 1; }
  function g() { return 2; }

  var global;

  function h(s) {
    var fg;
    var a = 0;
    if (s) {
      global = 0;
      a = 1;
      fg = f;
    } else {
      global = 1
      fg = g;
    }
    return fg() + a;
  }

  h(0);
  h(0);
  h(1);
  h(1);
  %OptimizeFunctionOnNextCall(h);
  assertEquals(2, h(0));
})();
