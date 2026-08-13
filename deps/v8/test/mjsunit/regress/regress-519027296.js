// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-externalize-string

// Test CSA StringFromSingleCharCode signed vs unsigned caller check invariant.
(function TestCSAStringFromCharCode() {
  assertEquals("\uffff", String.fromCharCode(-1));
  assertEquals("\u00ff", String.fromCharCode(255));
  assertTrue(isOneByteString(String.fromCharCode(255)));
})();

// Test Maglev IR BuiltinStringFromCharCode constant folding boundary
// condition (<= 255).
(function TestMaglevStringFromCharCodeBoundary() {
  function foo() {
    return String.fromCharCode(255);
  }
  %PrepareFunctionForOptimization(foo);
  assertEquals("\u00ff", foo());
  assertTrue(isOneByteString(foo()));
  assertEquals("\u00ff", foo());
  assertTrue(isOneByteString(foo()));
  %OptimizeMaglevOnNextCall(foo);
  assertEquals("\u00ff", foo());
  assertTrue(isOneByteString(foo()));
})();
