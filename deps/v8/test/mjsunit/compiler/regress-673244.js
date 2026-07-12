// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  var accumulator = false;
  for (var i = 0; i < 4; i++) {
    accumulator = accumulator.hasOwnProperty(3);
    if (i === 1) %OptimizeOsr();
  }
}

%PrepareFunctionForOptimization(f);
f();
