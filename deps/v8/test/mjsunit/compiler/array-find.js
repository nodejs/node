// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Basic loop peeling test case with Array.prototype.find().
(function() {
  function foo(a, o) {
    return a.find(x => x === o.x);
  }

  assertEquals(3, foo([1, 2, 3], {x:3}));
  assertEquals(undefined, foo([0, 1, 2], {x:3}));
  %OptimizeFunctionOnNextCall(foo);
  assertEquals(3, foo([1, 2, 3], {x:3}));
  assertEquals(undefined, foo([0, 1, 2], {x:3}));
})();
