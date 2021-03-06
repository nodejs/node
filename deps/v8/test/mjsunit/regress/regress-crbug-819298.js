// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var a = new Int32Array(2);

function foo(base) {
  a[base - 91] = 1;
};
%PrepareFunctionForOptimization(foo);
foo("");
foo("");
%OptimizeFunctionOnNextCall(foo);
foo(NaN);
assertEquals(0, a[0]);
