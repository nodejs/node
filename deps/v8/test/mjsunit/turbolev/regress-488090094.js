// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

let big_index = 0x80000000;

let ta = new Uint8Array(big_index+0x40);
ta[big_index] = 42;

function foo(ta, index) {
  index |= 0;
  return ta[index];
}

%PrepareFunctionForOptimization(foo);
assertEquals(0, foo(ta, 10));
assertEquals(0, foo(ta, 0x40000000));

%OptimizeFunctionOnNextCall(foo);
assertEquals(0, foo(ta, 10));
assertEquals(undefined, foo(ta, -2147483648));
