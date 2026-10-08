// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev

function ca() {
  return [];
}
%PrepareFunctionForOptimization(ca);

function f1(a2, a3 = true) {
  // Might be an Array or an ArrayIterator
  const v4 = a2();
  v4.length = 1;
  v4[0] = 0;
  return v4;
}
%PrepareFunctionForOptimization(f1);

function f5() {
  const arrayIterator = f1(ca).keys();
  arrayIterator[0] = 0;
  function f10() {
    // The bug happens after this gets inlined
    return arrayIterator;
  }
  %PrepareFunctionForOptimization(f10);
  f1(f10);
}
%PrepareFunctionForOptimization(f5);

// Both calls below are needed.
f5();
f5();
%OptimizeFunctionOnNextCall(f5);
f5();
