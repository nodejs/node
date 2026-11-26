// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function typeOfNumber(v) {
  return typeof v;
}

function foo(arr, i, smi_branch, bailout, other) {
  const x = arr[i];
  let cmp = false;
  if (smi_branch) {
    const tx = typeof x;
    cmp = tx === 'number';
  } else {
    cmp = x === other;
    if (bailout) return '';
  }
  const t = typeOfNumber(x);
  return t + ':' + String(x) + ':' + cmp;
}

const smi_arr = [1, 2];
const holey = [1, , 2.5];

%PrepareFunctionForOptimization(typeOfNumber);
%PrepareFunctionForOptimization(foo);
assertEquals('number', typeOfNumber(1.5));

assertEquals('number:1:true', foo(smi_arr, 0, true, false, null));
assertEquals('', foo(holey, 1, false, true, null));

%OptimizeFunctionOnNextCall(foo);
assertEquals('number:1:true', foo(holey, 0, true, false, null));
assertEquals('undefined:undefined:false', foo(holey, 1, false, false, null));
