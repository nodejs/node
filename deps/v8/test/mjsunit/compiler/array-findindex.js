// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Basic loop peeling test case with Array.prototype.findIndex().
(function() {
  function foo(a, o) {
    return a.findIndex(x => x === o.x);
  }

  assertEquals(2, foo([1, 2, 3], {x:3}));
  assertEquals(-1, foo([0, 1, 2], {x:3}));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(2, foo([1, 2, 3], {x:3}));
  assertEquals(-1, foo([0, 1, 2], {x:3}));
})();
