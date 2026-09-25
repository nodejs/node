// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function loader(dst, src, i) {
  dst[i] = src[i];
};
%PrepareFunctionForOptimization(loader);
var ab = new ArrayBuffer(8);
var i_view = new Int32Array(ab);
i_view[0] = %GetHoleNaNUpper();
i_view[1] = %GetHoleNaNLower();
var f_view = new Float64Array(ab);

var fixed_double_elements = new Float64Array(1);

function opt_store() {
  fixed_double_elements[0] = f_view[0];
};
%PrepareFunctionForOptimization(opt_store);
opt_store();
opt_store();
%OptimizeFunctionOnNextCall(opt_store);
opt_store();

var i32 = new Int32Array(fixed_double_elements.buffer);
assertEquals(i_view[0], i32[0]);
assertEquals(i_view[1], i32[1]);

var doubles = [0.5];
loader(doubles, fixed_double_elements, 0);
loader(doubles, fixed_double_elements, 0);
%OptimizeFunctionOnNextCall(loader);
loader(doubles, fixed_double_elements, 0);
assertTrue(doubles[0] !== undefined);
