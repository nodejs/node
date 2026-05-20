// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var ab = new ArrayBuffer(8);
var i_view = new Int32Array(ab);
i_view[0] = %GetHoleNaNUpper();
i_view[1] = %GetHoleNaNLower();
var f_view = new Float64Array(ab);

var fixed_double_elements = new Float64Array(1);
fixed_double_elements[0] = f_view[0];

function A(src) {
  this.x = src[0];
};
%PrepareFunctionForOptimization(A);
new A(fixed_double_elements);
new A(fixed_double_elements);

%OptimizeFunctionOnNextCall(A);

var obj = new A(fixed_double_elements);

function move_x(dst, obj) {
  dst[0] = obj.x;
};
%PrepareFunctionForOptimization(move_x);
var doubles = [0.5];
move_x(doubles, obj);
move_x(doubles, obj);
%OptimizeFunctionOnNextCall(move_x);
move_x(doubles, obj);
assertTrue(doubles[0] !== undefined);
