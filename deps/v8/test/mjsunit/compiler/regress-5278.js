// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(a, b) {
  return a % b;
}
%PrepareFunctionForOptimization(foo);
foo(2, 1);
foo(2, 1);
%OptimizeFunctionOnNextCall(foo);
assertEquals(-0, foo(-2, 1));
