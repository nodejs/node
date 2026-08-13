// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function trigger(arr) {
  let phi1 = arr[1];
  let phi2 = arr[1];
  let x = 0;
  let i = 0;
  do {
    x = phi2 | 0;
    arr[2] = phi1;
    let old_phi1 = phi1;
    phi1 = arr[0];
    phi2 = old_phi1;
    i++;
  } while (i < 10000);
  return x;
}
let arr = [1.1, , 2.2];
%PrepareFunctionForOptimization(trigger);
trigger(arr);
trigger(arr);
%OptimizeFunctionOnNextCall(trigger);
trigger(arr);
