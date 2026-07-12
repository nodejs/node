// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev-truncated-int32-phis

function bar() {}

function foo() {
  let val = 0;
  for (let i = 0; i < 1; i++) {
    1.1 >> val;
    val = 1.1;
  }
  bar(val);
}
%PrepareFunctionForOptimization(foo);
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
