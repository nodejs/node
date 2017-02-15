// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo-filter=test2

function test(n) {
  return Array(n);
}

function test2() {
  return test(2);
}

function test3(a) {
  a[0] = 1;
}

test(0);

var smi_array = [1,2];
smi_array[2] = 3;
test3(smi_array);

%OptimizeFunctionOnNextCall(test2);

var broken_array = test2();
test3(broken_array);
1+broken_array[0];
