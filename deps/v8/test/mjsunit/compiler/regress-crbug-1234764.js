// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(arg_true) {
  let o = {c0: 0};
  let c0a = arg_true ? 0 : "x";
  let c0 = Math.max(c0a, 0) + c0a;
  let v01 = 2**32 + (o.c0 & 1);
  let ra = ((2**32 - 1) >>> c0) - v01;
  let rb = (-1) << (32 - c0);
  return (ra^rb) >> 31;
}

%PrepareFunctionForOptimization(foo);
assertEquals(0, foo(true));
assertEquals(0, foo(true));
%OptimizeFunctionOnNextCall(foo);
assertEquals(0, foo(true));
