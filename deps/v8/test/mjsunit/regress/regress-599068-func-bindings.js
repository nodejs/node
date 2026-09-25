// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Tests function bindings are correctly handled in ignition.
(function f() {
  function assignSloppy() {
    f = 0;
  }
  assertDoesNotThrow(assignSloppy);

  function assignStrict() {
    'use strict';
    f = 0;
  }
  assertThrows(assignStrict, TypeError);

  function assignStrictLookup() {
    eval("'use strict'; f = 1;");
  }
  assertThrows(assignStrictLookup, TypeError);
})();

// Tests for compound assignments which are handled differently
// in crankshaft.
(function f() {
  function assignSloppy() {
    f += "x";
  };
  %PrepareFunctionForOptimization(assignSloppy);
  assertDoesNotThrow(assignSloppy);
  assertDoesNotThrow(assignSloppy);
  %OptimizeFunctionOnNextCall(assignSloppy);
  assertDoesNotThrow(assignSloppy);

  function assignStrict() {
    'use strict';
    f += "x";
  };
  %PrepareFunctionForOptimization(assignStrict);
  assertThrows(assignStrict, TypeError);
  assertThrows(assignStrict, TypeError);
  %OptimizeFunctionOnNextCall(assignStrict);
  assertThrows(assignStrict, TypeError);
})();
