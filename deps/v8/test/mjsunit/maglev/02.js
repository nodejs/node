// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function f(x) {
  if (x < 0) return -1;
  return 1;
}

%PrepareFunctionForOptimization(f);
assertEquals(-1, f(-2));
assertEquals(1, f(0));
assertEquals(1, f(2));

%OptimizeMaglevOnNextCall(f);
assertEquals(-1, f(-2));
assertEquals(1, f(0));
assertEquals(1, f(2));
