// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --turbolev-escape-analysis
// Flags: --turbolev-non-eager-loop-peeling

function test(cond, cond2) {
  let base = { a: 0 };
  let y = 0;
  if (cond) {
    base.a = 1;
  } else {
    base.a = 2;
  }
  while (cond2) {
    y = base.a;
    base.a = y + 1;
    cond2 = false;
  }
  return y;
}

%PrepareFunctionForOptimization(test);
assertEquals(1, test(true, true));
assertEquals(2, test(false, true));
%OptimizeFunctionOnNextCall(test);
assertEquals(1, test(true, true));
