// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --expose-gc

var v1 = {};
v1 = 0;
var v2 = {};
v2 = 0;
gc();

var minus_zero = {z:-0.0}.z;
var nan = undefined + 1;
function f() {
  v1 = minus_zero;
  v2 = nan;
};
%OptimizeFunctionOnNextCall(f);
f();
gc();  // Boom!
