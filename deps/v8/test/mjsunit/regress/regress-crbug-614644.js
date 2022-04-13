// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(a, x) {
  a.shift(2, a.length = 2);
  a[0] = x;
};
%PrepareFunctionForOptimization(f);
f([], 1.1);
f([1], 1.1);
%OptimizeFunctionOnNextCall(f);
f([1], 1.1);
