// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function boom() {
  function getargs() {
    return arguments;
  }
  function f(ar, p) {
    ar[p] = 5.5;
  }
  let arr = getargs(1, 2, 3, 45);
  arr.a1 = 6.6;
  %PrepareFunctionForOptimization(f);
  f(arr, 'a1');
  f([5,6,7], 5);
  f(arr, 4);

  return arr[5];
}
boom();
