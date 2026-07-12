// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var arr = new Array();
function f0(a) {
  return a < 0 ? + __v_5 : a;
}

function f1(a, b) {
  return f0(a + b & 0xffffffff);
}

function f2( a) {
  var b = 0;
  for (var i = 0; i < 32; ++i) {
    b = f1(b, a << i);
  }
}

function loop() {
  for (var i = 1; i < 10; i++) {
    arr[i] = f1(f2());
  }
}

%PrepareFunctionForOptimization(f0);
%PrepareFunctionForOptimization(f1);
%PrepareFunctionForOptimization(f2);
%PrepareFunctionForOptimization(loop);
loop();
loop();
%OptimizeFunctionOnNextCall(loop);
loop();
