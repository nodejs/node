// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(arr1, arr2) {
  let x = arr1[0];
  for (let i = 0; i < 4; i++) {
    arr2[i] = x;
    x = arr1[i];
  }
  +x;
  return `${x}`;
}

let arr1 = [1.1, 2.2, 3.3, 4.4, 5.5];
delete arr1[4];
let arr2 = [1.1, 2.2, 3.3, 4.4, 5.5];
delete arr2[4];

%PrepareFunctionForOptimization(foo);
for (let i = 0; i < 20; i++) {
  assertEquals("4.4", foo(arr1, arr2));
}
%OptimizeMaglevOnNextCall(foo);
assertEquals("4.4", foo(arr1, arr2));

// `x` is now the hole after the loop, which stringifies to "undefined".
delete arr1[3];
assertEquals("undefined", foo(arr1, arr2));

function bar(arr1, arr2) {
  let x = arr1[0];
  for (let i = 0; i < 4; i++) {
    arr2[i] = x;
    x = arr1[i];
  }
  +x;
  return `${x}`;
}

let int1 = [1.5, 2, 3, 4, 5];
delete int1[4];
let int2 = [1.5, 2, 3, 4, 5];
delete int2[4];

%PrepareFunctionForOptimization(bar);
for (let i = 0; i < 20; i++) {
  assertEquals("4", bar(int1, int2));
}
%OptimizeMaglevOnNextCall(bar);
assertEquals("4", bar(int1, int2));
