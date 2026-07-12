// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --maglev-truncation

function foo(a, b) {
  // This must be "x = Int32AddWithOverflow(Int32AddWithOverflow(a, b), b)".
  // Cannot use "x = Int32Add(Int32AddWithOverflow(a, b), b)" here, since not all
  // uses of x are truncating.
  let x = a + b + b;
  return x - (x|0);
}
%PrepareFunctionForOptimization(foo);

foo(0, 1);

%OptimizeMaglevOnNextCall(foo);

foo(0, 1);

assertEquals(4294967296, foo(1073741823, 1073741823));

// Deopt because the addition overflows.
assertFalse(isMaglevved(foo));
