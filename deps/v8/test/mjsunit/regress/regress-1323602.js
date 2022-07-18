// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan

let g;

function test() {
  const ten = 10;
  const x = 10 / ten;
  const y = Math.floor(x);
  g = x;
  return y + 1;
}

%PrepareFunctionForOptimization(test);
assertEquals(2, test());
%OptimizeFunctionOnNextCall(test);
assertEquals(2, test());
