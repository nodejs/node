// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(x, y) {
  return (y < x) - (x < x);
}

%PrepareFunctionForOptimization(foo);
assertEquals(0, foo(9));
assertEquals(0, foo(9));

%OptimizeMaglevOnNextCall(foo);
assertEquals(0, foo(9));
