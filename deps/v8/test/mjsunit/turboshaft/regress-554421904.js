// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function set7(arr, v) { arr[7] = v; }
function set0(arr, v) { arr[0] = v; }
function carrier(...rest) {
  const receiver = rest[32765] ? rest[32766] : doubleArray;
  set0(receiver, 1.1);
}
function victim(payload) {
  const staleArray = Array(8);
  const target = [];
  if (!trigger) staleArray.x = 0;
  boundCarrier(trigger, staleArray);
  set7(staleArray, payload);
  return { target, staleArray };
}

%PrepareFunctionForOptimization(set0);
%PrepareFunctionForOptimization(set7);
%PrepareFunctionForOptimization(carrier);
%PrepareFunctionForOptimization(victim);

const doubleArray = Array(8);
set0(doubleArray, 1.1);
set7(doubleArray, 0);
set0(Array(8), 0);

// 32765 bound arguments + 2 call arguments = 32767 (kMaxArguments).
const boundCarrier = carrier.bind(null, ...Array(32765).fill(0));

let trigger = 0;
victim(0);

trigger = 1;
boundCarrier(trigger, doubleArray);

trigger = 0;
victim(0);

%OptimizeFunctionOnNextCall(victim);
trigger = 1;
const { target, staleArray } = victim(1.1);

assertEquals(0, target.length);
target[0] = 1;

assertEquals(1.1, staleArray[0]);
assertEquals(1.1, staleArray[7]);
