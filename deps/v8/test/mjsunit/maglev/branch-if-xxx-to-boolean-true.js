// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --maglev --allow-natives-syntax

///////////////////////////////////////////////////////////////
// Without Phis

function basic_int32(a) {
  let c = a + 2;
  if (c) {
    return 42;
  } else {
    return 19;
  }
}

%PrepareFunctionForOptimization(basic_int32);
assertEquals(19, basic_int32(-2));
assertEquals(42, basic_int32(3));
%OptimizeMaglevOnNextCall(basic_int32);
assertEquals(19, basic_int32(-2));
assertEquals(42, basic_int32(3));


function basic_float64(a) {
  let c = a + 2.5;
  if (c) {
    return 1.6;
  } else {
    if (c == c) {
      return 2.42;
    } else {
      return 3.11;
    }
  }
}

%PrepareFunctionForOptimization(basic_float64);
assertEquals(2.42, basic_float64(-2.5));
assertEquals(1.6, basic_float64(3.5));
assertEquals(3.11, basic_float64(NaN));
%OptimizeMaglevOnNextCall(basic_float64);
assertEquals(2.42, basic_float64(-2.5));
assertEquals(1.6, basic_float64(3.5));
assertEquals(3.11, basic_float64(NaN));

function basic_holey_float64(a) {
  let c = a + 2.5;
  if (c) {
    return 1.6;
  } else {
    if (a == a) {
      return 3.6 + a;
    } else {
      return 3.11;
    }
  }
}

%PrepareFunctionForOptimization(basic_holey_float64);
assertEquals(1.1, basic_holey_float64(-2.5));
assertEquals(1.6, basic_holey_float64(3.5));
assertEquals(3.11, basic_holey_float64(NaN));
assertEquals(NaN, basic_holey_float64([1.1, , 2.2][1]));
%OptimizeMaglevOnNextCall(basic_holey_float64);
assertEquals(1.1, basic_holey_float64(-2.5));
assertEquals(1.6, basic_holey_float64(3.5));
assertEquals(3.11, basic_holey_float64(NaN));
assertEquals(NaN, basic_holey_float64([1.1, , 2.2][1]));

///////////////////////////////////////////////////////////////
// With Phis as condition.
//
// When Phi untagging is enabled, the condition should start as
// BranchIfToBooleanTrue, and later be updated to BranchIfInt32ToBooleanTrue or
// BranchIfFloat64ToBooleanTrue.

function phi_int32(a) {
  let phi = a ? 4 : 0;
  if (phi) {
    return phi + 5;
  } else {
    return phi + 3;
  }
}

%PrepareFunctionForOptimization(phi_int32);
assertEquals(3, phi_int32(0));
assertEquals(9, phi_int32(1));
%OptimizeMaglevOnNextCall(phi_int32);
assertEquals(3, phi_int32(0));
assertEquals(9, phi_int32(1));



function phi_float64_no_nan(a) {
  let phi = a ? 3.5 : 0;
  if (phi) {
    return phi + 1.6;
  } else {
    return phi + 2.5;
  }
}

%PrepareFunctionForOptimization(phi_float64_no_nan);
assertEquals(2.5, phi_float64_no_nan(0));
assertEquals(5.1, phi_float64_no_nan(1));
%OptimizeMaglevOnNextCall(phi_float64_no_nan);
assertEquals(2.5, phi_float64_no_nan(0));
assertEquals(5.1, phi_float64_no_nan(1));



function phi_float64_nan(a, b) {
  let nan = 0 / a;
  let f64 = b + 0.5;
  let phi = a ? f64 : nan;
  if (phi) {
    return phi + 4.5;
  } else {
    if (phi == phi) {
      return phi + 2.1;
    } else {
      return 4.25;
    }
  }
}

%PrepareFunctionForOptimization(phi_float64_nan);
assertEquals(4.25, phi_float64_nan(0, 0.5));
assertEquals(2.1, phi_float64_nan(1, -0.5));
assertEquals(5.5, phi_float64_nan(1, 0.5));
%OptimizeMaglevOnNextCall(phi_float64_nan);
assertEquals(4.25, phi_float64_nan(0, 0.5));
assertEquals(2.1, phi_float64_nan(1, -0.5));
assertEquals(5.5, phi_float64_nan(1, 0.5));



let arr = [1.5, , NaN, 0.0];

function phi_float64_holey(a, arr) {
  let holey_f64 = arr[a];
  let phi = arr ? holey_f64 : 4;
  if (phi) {
    return phi + 1.7;
  } else {
    // We should get here for:
    //  a == 1 ==> the_hole
    //  a == 2 ==> NaN
    //  a == 3 ==> 0.0
    // To distinguish between the 3 cases, we add a branch on "a".
    if (a != 2) {
      return phi + 2.1;
    } else {
      return 4.25;
    }
  }
}

%PrepareFunctionForOptimization(phi_float64_holey);
assertEquals(3.2, phi_float64_holey(0, arr));
// We don't load the_hole during feedback collection, as this would lead to
// megamorphic feedback.
// assertEquals(NaN, phi_float64_holey(1, arr));
assertEquals(4.25, phi_float64_holey(2, arr));
assertEquals(2.1, phi_float64_holey(3, arr));
%OptimizeMaglevOnNextCall(phi_float64_holey);
assertEquals(3.2, phi_float64_holey(0, arr));
assertEquals(NaN, phi_float64_holey(1, arr));
assertEquals(4.25, phi_float64_holey(2, arr));
assertEquals(2.1, phi_float64_holey(3, arr));
