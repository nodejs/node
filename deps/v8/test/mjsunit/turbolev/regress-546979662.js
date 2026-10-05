// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function foo(src, packed) {
  let a = src[0];
  let o = { x: -0.0 };
  let b = o.x;
  let x = a + b;
  packed[0] = x;
}

const src_hole = [1.1, 2.2];
src_hole[8] = 1.5;
delete src_hole[0];

const packed = [1.1, 2.2];

%PrepareFunctionForOptimization(foo);
foo(src_hole, packed);
foo(src_hole, packed);
%OptimizeFunctionOnNextCall(foo);
foo(src_hole, packed);

assertTrue(0 in packed);
assertEquals(NaN, packed[0]);
assertEquals([2.2, NaN], packed.toReversed());
