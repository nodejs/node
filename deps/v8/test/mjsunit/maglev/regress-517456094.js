// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev --maglev-untagged-phis

function foo() {
  for (let i = 0; i < 5; i++) {
    ~i;
    function bar() {}
    i = 0x7fffffff;
  }
}

%PrepareFunctionForOptimization(foo);
foo();
%OptimizeMaglevOnNextCall(foo);
foo();
