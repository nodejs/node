// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function maybeThrow(maybe) {
  if (maybe) { throw 'lol'; }
}
%NeverOptimizeFunction(maybeThrow);

let double_arr = [1.5, 2.5, /* hole */, 3.5, 4.5];

function foo(n, throwEarly) {
  let b = double_arr[n];
  try {
    maybeThrow(throwEarly);
    b = 42;
    maybeThrow(!throwEarly);
  } catch(e) {
    return b;
  }
  assertUnreachable();
}
%PrepareFunctionForOptimization(foo);
assertEquals(42, foo(1, false));
assertEquals(2.5, foo(1, true));
assertEquals(42, foo(2, false));
assertEquals(undefined, foo(2, true));

%OptimizeMaglevOnNextCall(foo);
assertEquals(42, foo(1, false));
assertEquals(2.5, foo(1, true));
assertEquals(42, foo(2, false));
assertEquals(undefined, foo(2, true));
assertTrue(isMaglevved(foo));
