// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

// HOLEY_DOUBLE_ELEMENTS.
const arr = [1, , , , , 1.1];

function opt_me() {
  for (let i = 0; i < 5; i++) {
    const ele = arr[i];
    const arr2 = Array(ele, i);
    function inner() {
      arr2.join();
      arr.__proto__ = ele;
    }
    inner();
  }
}

%PrepareFunctionForOptimization(opt_me);
opt_me();
%OptimizeMaglevOnNextCall(opt_me);
opt_me();
