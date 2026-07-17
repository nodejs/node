// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --verify-heap

function test(x) {
  [x, , ];
};
%PrepareFunctionForOptimization(test);
test(0);
test(0);
%OptimizeFunctionOnNextCall(test);
test(0);
