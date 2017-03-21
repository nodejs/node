// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  var x = 23;
  function f() { return x; }
  function g() { [x] = [x + 1]; }
  function h() { g(); return x; }

  function boom() { return h() }

  assertEquals(24, boom());
  assertEquals(25, boom());
  assertEquals(26, boom());
  %OptimizeFunctionOnNextCall(boom);
  assertEquals(27, boom());
})();
