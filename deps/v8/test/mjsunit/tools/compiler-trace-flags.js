// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --trace-turbo --trace-turbo-graph
// Flags: --trace-turbo-cfg-file=test/mjsunit/tools/turbo.cfg
// Flags: --trace-turbo-path=test/mjsunit/tools

// Only trace the "add" function:
// Flags: --trace-turbo-filter=add

// The idea behind this test is to make sure we do not crash when using the
// --trace-turbo flag given different sort of inputs.

(function testOptimizedJS() {
  function add(a, b) {
    return a + b;
  }

  add(21, 21);
  %OptimizeFunctionOnNextCall(add);
  add(20, 22);
})();
