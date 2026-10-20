// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev-optimistic-peeled-loops --maglev-untagged-phis

function test() {
  for (let i = 0; i < 5; i++) {
    ~i;
    function f() {}
    i = 0x7fffffff;
  }
}
%PrepareFunctionForOptimization(test);
test();
%OptimizeMaglevOnNextCall(test);
test();
