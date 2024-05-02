// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  function f(o) {
    o.x;
    Object.create(o);
    return o.y.a;
  }

  %PrepareFunctionForOptimization(f);
  f({ x : 0, y : { a : 1 } });
  f({ x : 0, y : { a : 2 } });
  %OptimizeFunctionOnNextCall(f);
  assertEquals(3, f({ x : 0, y : { a : 3 } }));
})();

(function() {
  function f(o) {
    let a = o.y;
    Object.create(o);
    return o.x + a;
  }

  %PrepareFunctionForOptimization(f);
  f({ x : 42, y : 21 });
  f({ x : 42, y : 21 });
  %OptimizeFunctionOnNextCall(f);
  assertEquals(63, f({ x : 42, y : 21 }));
})();
