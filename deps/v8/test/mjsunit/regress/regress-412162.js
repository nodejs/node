// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test() {
  Math.abs(-NaN).toString();
};
%PrepareFunctionForOptimization(test);
test();
test();
%OptimizeFunctionOnNextCall(test);
test();
