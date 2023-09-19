// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

var xyz = 42;

function f(x) {
  if (x) return 1;
  return xyz;
}

%PrepareFunctionForOptimization(f);
assertEquals(1, f(true));
assertEquals(42, f(false));

%OptimizeMaglevOnNextCall(f);
assertEquals(1, f(true));
assertEquals(42, f(false));
