// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

const holey = [1.5, , 2.5];
const out = [1.5, , 2.5];

function foo(arr, i) {
  const res = Math.min(arr[i]);
  out[0] = res;
  return res;
}

%PrepareFunctionForOptimization(foo);
foo(holey, 0);
%OptimizeFunctionOnNextCall(foo);
assertEquals(NaN, foo(holey, 1));
assertTrue(0 in out);
assertEquals(NaN, out[0]);
