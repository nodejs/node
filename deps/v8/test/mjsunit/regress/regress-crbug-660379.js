// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function InlinedThrowAtEndOfTry() {
  function g() {
    %DeoptimizeFunction(f);
    throw "boom";
  }
  function f() {
    try {
      g();  // Right at the end of try.
    } catch (e) {
      assertEquals("boom", e)
    }
  }
  assertDoesNotThrow(f);
  assertDoesNotThrow(f);
  %OptimizeFunctionOnNextCall(f);
  assertDoesNotThrow(f);
})();

(function InlinedThrowInFrontOfTry() {
  function g() {
    %DeoptimizeFunction(f);
    throw "boom";
  }
  function f() {
    g();  // Right in front of try.
    try {
      Math.random();
    } catch (e) {
      assertUnreachable();
    }
  }
  assertThrows(f);
  assertThrows(f);
  %OptimizeFunctionOnNextCall(f);
  assertThrows(f);
})();
