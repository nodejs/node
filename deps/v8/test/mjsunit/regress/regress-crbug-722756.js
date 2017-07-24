// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var array = [[{}], [1.1]];

function transition() {
  for(var i = 0; i < array.length; i++){
    var arr = array[i];
    arr[0] = {};
  }
}

var double_arr2 = [1.1,2.2];

var flag = 0;
function swap() {
  try {} catch(e) {}  // Prevent Crankshaft from inlining this.
  if (flag == 1) {
    array[1] = double_arr2;
  }
}

var expected = 6.176516726456e-312;
function f(){
  swap();
  double_arr2[0] = 1;
  transition();
  double_arr2[1] = expected;
}

for(var i = 0; i < 3; i++) {
  f();
}
%OptimizeFunctionOnNextCall(f);
flag = 1;
f();
assertEquals(expected, double_arr2[1]);
