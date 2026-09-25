// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --additive-safe-int-feedback

let a = [0, 1, , 2];
function foo(i) {
  return (a[i] + 4294967295) | 0;
}

%PrepareFunctionForOptimization(foo);
foo(1);
foo(1);
%OptimizeFunctionOnNextCall(foo);
foo(1);
assertTrue(isOptimized(foo));
assertEquals(0, foo(2));
assertFalse(isOptimized(foo));
