// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Check that IfException/IfSuccess rewiring works in JSInliner
function test() {
  var spread = function(value) { return { ...value }; }
  try {
    assertEquals({}, spread());
  } catch (e) {}
};

%PrepareFunctionForOptimization(test);
test();
test();
test();
%OptimizeFunctionOnNextCall(test);
test();
