// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var a = "a".repeat(%StringMaxLength());

(function() {
  function foo(a) {
    try {
      return "longer than ConsString:kMinLength" + a + "0123456789012";
    } catch (e) {
      return e;
    }
  }

  %PrepareFunctionForOptimization(foo);
  foo("a");
  foo("a");
  // Optimize with string length protector check.
  %OptimizeFunctionOnNextCall(foo);
  foo("a");
  assertInstanceof(foo(a), RangeError);
  // Optimize without string length protector check.
  %PrepareFunctionForOptimization(foo);
  %OptimizeFunctionOnNextCall(foo);
  foo("a");
  assertInstanceof(foo(a), RangeError);
})();
