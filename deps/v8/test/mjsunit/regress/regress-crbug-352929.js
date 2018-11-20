// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var dummy = new Int32Array(100);
array = new Int32Array(100);
var dummy2 = new Int32Array(100);

array[-17] = 0;
function fun(base,cond) {
  array[base - 1] = 1;
  array[base - 2] = 2;
  if (cond) {
    array[base - 4] = 3;
    array[base - 5] = 4;
  } else {
    array[base - 6] = 5;
    array[base - 100] = 777;
  }
}
fun(5,true);
fun(7,false);
%OptimizeFunctionOnNextCall(fun);
fun(7,false);

for (var i = 0; i < dummy.length; i++) {
  assertEquals(0, dummy[i]);
}
for (var i = 0; i < dummy2.length; i++) {
  assertEquals(0, dummy2[i]);
}
