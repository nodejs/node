// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function f(a, holey_double_arr) {
  let int32 = a + 1;
  // Make sure an int32 value can be an input to a holeyfloat64 phi.
  let holey_float64_phi = a ? int32 : holey_double_arr[0];
  return holey_float64_phi + 0.5;
}

%PrepareFunctionForOptimization(f);
assertEquals(43.5, f(42, [0.5,]));
assertEquals(1, f(0, [0.5,]));
%OptimizeMaglevOnNextCall(f);
assertEquals(43.5, f(42, [0.5,]));
assertEquals(1, f(0, [0.5,]));
assertEquals(NaN, f(0, [,0.5]));


function g(a, holey_double_arr) {
  let int32 = a + 1;
  let int32phi = a ? a + 2 : int32;
  // Make sure an int32 phi can be an input to a holeyfloat64 phi.
  let holey_float64_phi = a ? int32phi : holey_double_arr[0];
  return holey_float64_phi + 0.5;
}

%PrepareFunctionForOptimization(g);
assertEquals(44.5, g(42, [0.5,]));
assertEquals(1, g(0, [0.5,]));
%OptimizeMaglevOnNextCall(g);
assertEquals(44.5, g(42, [0.5,]));
assertEquals(1, g(0, [0.5,]));
assertEquals(NaN, g(0, [,0.5]));
