// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(a, b) {
  var val;
  assertEquals(undefined, val);
  try {
    Object.defineProperty(  {
      get: function () {
        val = this;
      }
    });
  } catch (e) {}

  (function () {
    1[1] = b;
    typeof a;
  })();
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
