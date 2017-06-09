// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var m = (function() {
  "use asm";
  function f(x) {
    return x < 0;
  }
  function g(x) {
    return 0 < x;
  }
  return { f: f, g: g };
})();
var f = m.f;
var g = m.g;

var counter = 0;

function deopt(f) {
  return {
    toString : function() {
      %DeoptimizeFunction(f);
      counter++;
      return "2";
    }
  };
}

assertEquals(false, f(deopt(f)));
assertEquals(1, counter);

assertEquals(true, g(deopt(g)));
assertEquals(2, counter);

%OptimizeFunctionOnNextCall(f);
assertEquals(false, f(deopt(f)));
assertEquals(3, counter);

%OptimizeFunctionOnNextCall(g);
assertEquals(true, g(deopt(g)));
assertEquals(4, counter);
