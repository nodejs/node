// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function id(a) {
  return a;
}

(function LiteralCompareNullDeopt() {
  function f() {
   return id(null == %DeoptimizeNow());
  }

  %OptimizeFunctionOnNextCall(f);
  assertTrue(f());
})();

(function LiteralCompareUndefinedDeopt() {
  function f() {
   return id(undefined == %DeoptimizeNow());
  }

  %OptimizeFunctionOnNextCall(f);
  assertTrue(f());
})();

(function LiteralCompareTypeofDeopt() {
  function f() {
   return id("undefined" == typeof(%DeoptimizeNow()));
  }

  %OptimizeFunctionOnNextCall(f);
  assertTrue(f());
})();
