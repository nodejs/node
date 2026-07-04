// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

// Regression test for crbug.com/504070398. Runtime_DisposeDisposableStack
// must not fail the DCHECK on async-resource detection when a comparator
// uses a synchronous 'using' declaration.

function f(a, b) {
  const a1 = [-5.0, 485782.25, 5.16];
  a1[3] = a1;
  function cmp(x) {
    using resource = { [Symbol.dispose]() {} };
    return x;
  }
  a1.sort(cmp);
  return a;
}

%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
f(1, 2);
