// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc --homomorphic-ic

// Initialize with a double (HeapNumber) to make the PropertyCell mutable
// once we also store Smis to it.
var global_var = 1.1;

function test(arr) {
  global_var = arr.length;
}

// Use Array subclasses to easily create 5 distinct JSArray maps.
class A1 extends Array {}
class A2 extends Array {}
class A3 extends Array {}
class A4 extends Array {}
class A5 extends Array {}

let arr1 = new A1();
let arr2 = new A2();
let arr3 = new A3();
let arr4 = new A4();
let arr5 = new A5();

// large_arr has a distinct JSArray map and HeapNumber length (3221225471).
let large_arr = new Array(3221225471);

// Warm up with all 5 maps.
%PrepareFunctionForOptimization(test);
for (let i = 0; i < 5; i++) {
  test(arr1);
  test(arr2);
  test(arr3);
  test(arr4);
  test(arr5);
}

// Optimize with Maglev. It should use the homomorphic array length path!
%OptimizeMaglevOnNextCall(test);

// Call with the large array.
test(large_arr);

// Trigger GC.
gc();
