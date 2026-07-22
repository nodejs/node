// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

function f0(a) {
  while (a > 0) {
    [115,109,-1,96,127,5,,1,9223372036854775806,17,2,0,103];
    const v35 = Math.round(-1e-15);
    Math[v35];
    a = 0;
  }
  return f0;
}

%PrepareFunctionForOptimization(f0);
f0(1);
%OptimizeMaglevOnNextCall(f0);
f0(1);
