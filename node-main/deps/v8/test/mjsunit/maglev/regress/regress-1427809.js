// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function f0() {
  try {
    return Math.pow(129n);
  } catch(e3) {
    return 42;
  }
}
%PrepareFunctionForOptimization(f0);
assertEquals(f0(), 42);
assertEquals(f0(), 42);
%OptimizeMaglevOnNextCall(f0);
assertEquals(f0(), 42);
