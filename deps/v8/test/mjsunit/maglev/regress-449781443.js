// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(n) {
  return n + -0.0;
}

%PrepareFunctionForOptimization(foo);
assertEquals(NaN, foo(undefined));

%OptimizeMaglevOnNextCall(foo);
assertEquals(NaN, foo(undefined));
