// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function f(i, j) {
  var x = 1;
  var y = 2;
  if (i) {
    x = y;
    if (j) {
      x = 3
    }
  }
  return x;
}

%PrepareFunctionForOptimization(f);
assertEquals(1, f(false, true));

%OptimizeMaglevOnNextCall(f);
assertEquals(1, f(false, false));
assertEquals(2, f(true, false));
assertEquals(3, f(true, true));
