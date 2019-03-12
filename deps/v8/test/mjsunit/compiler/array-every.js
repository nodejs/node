// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Basic loop peeling test case with Array.prototype.every().
(function() {
  function foo(a, o) {
    return a.every(x => x === o.x);
  }

  assertTrue(foo([3, 3, 3], {x:3}));
  assertFalse(foo([3, 3, 2], {x:3}));
  %OptimizeFunctionOnNextCall(foo);
  assertTrue(foo([3, 3, 3], {x:3}));
  assertFalse(foo([3, 3, 2], {x:3}));
})();
