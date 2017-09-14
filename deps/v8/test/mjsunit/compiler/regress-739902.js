// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function() {
  function f(x) {
    return String.fromCharCode(x >>> 24);
  };

  var e = 0x41000001;

  f(e);
  %OptimizeFunctionOnNextCall(f);
  assertEquals("A", f(e));
})();

(function() {
  function f(x) {
    return (x >>> 24) & 0xffff;
  };

  f(1);
  %OptimizeFunctionOnNextCall(f);
  assertEquals(0, f(1));
  assertEquals(100, f((100 << 24) + 42));
})();
