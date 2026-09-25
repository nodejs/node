// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(val) {
  return -1 >>> val;
}
%PrepareFunctionForOptimization(foo);
foo(0);
%OptimizeFunctionOnNextCall(foo);
assertEquals(4294967295, foo(0));
