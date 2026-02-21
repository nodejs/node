// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev --maglev-truncation

function foo(a, b) {
  // We can have "x = Int32Add(Int32AddWithOverflow(a, b), b)", since
  // all uses of x are truncating.
  let x = a + b + b;
  return (x|0);
}
%PrepareFunctionForOptimization(foo);

foo(0, 1);

%OptimizeMaglevOnNextCall(foo);

assertEquals(10, foo(6, 2));

assertEquals(-1073741827, foo(1073741823, 1073741823));

// No deopts even if the addition overflows.
assertTrue(isMaglevved(foo));
