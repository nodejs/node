// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-untagged-phis

const H0 = %AllocateHeapNumberWithValue(-1.0);
const H1 = %AllocateHeapNumberWithValue(0x20a0a0a1);

function foo(c) {
  let phi1 = c ? H0 : H1;
  phi1 | 0;
  let phi2 = c ? phi1 : "abc";
  return !!phi2;
}

%PrepareFunctionForOptimization(foo);
assertEquals(true, foo(true));

%OptimizeMaglevOnNextCall(foo);
assertEquals(true, foo(true));

assertEquals(true, foo(false));
