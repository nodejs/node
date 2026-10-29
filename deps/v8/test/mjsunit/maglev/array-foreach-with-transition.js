// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

let change_elements = false

function maybe_change_elements(a) {
  if (change_elements) {
    let old = a[1];
    // Transition a to DOUBLE_ELEMENTS
    a[1] = 0.5;
    a[1] = old;
  }
}

function foo(a, f) {
  let sum = 0;
  a.forEach(v=>{
    sum += v;
    maybe_change_elements(a);
  })
  return sum;
}

%NeverOptimizeFunction(maybe_change_elements);
%PrepareFunctionForOptimization(foo);
assertEquals(3, foo([0,1,2]));
assertEquals(3, foo([0,1,2]));
%OptimizeMaglevOnNextCall(foo);
assertEquals(3, foo([0,1,2]));
change_elements = true;
foo([0,1,2]);
