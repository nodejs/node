// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

const arr = [1.5, ,];

function foo(a) {
  let x = a[0];
  let y = a[0];
  for (let i = 0; i < 5; i++) {
    const t = (y | 0) + y;
    y = x;
    x = t;
  }
  return x;
}

%PrepareFunctionForOptimization(foo);
assertEquals(8.5, foo(arr));
assertEquals(8.5, foo(arr));
%OptimizeMaglevOnNextCall(foo);
assertEquals(8.5, foo(arr));
assertOptimized(foo);

// Passing a hole when feedback only saw Number must still deopt and return NaN.
assertEquals(NaN, foo([, 1.5]));
assertUnoptimized(foo);
