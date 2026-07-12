// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var __caught = 0;
function foo() {
  try {
    "".startsWith(/a/);
  } catch (e) {
    __caught++;
  }
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
assertEquals(__caught, 2);
