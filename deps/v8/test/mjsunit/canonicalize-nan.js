// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var buf = new Float32Array(1);

function fill() {
  buf[0] = 0;
};

// Have single precision zero set.
%PrepareFunctionForOptimization(fill);
fill();
%OptimizeFunctionOnNextCall(fill);
fill();

var array = Array(2).fill(0);

assertEquals(0, array[1])
array[1] = 0.5;
assertEquals(0.5, array[1]);
