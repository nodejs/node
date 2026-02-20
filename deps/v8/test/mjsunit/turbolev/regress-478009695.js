// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax


function foo(f, permissive = true) {
    return f();
}
function bar(x) {
  let y = foo(() => (x ? 1 : 0x80000000) | 0);
}

%PrepareFunctionForOptimization(bar);
bar();
%OptimizeMaglevOnNextCall(bar);
bar();
