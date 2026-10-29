// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev --no-lazy-feedback-allocation

const v2 = 0[1];
function f() {
  const x = (() => [100.1, v2][1])();
  return Array(x, 219);
}
%PrepareFunctionForOptimization(f);
assertEquals(undefined, f()[0]);
assertEquals(undefined, f()[0]);
%OptimizeFunctionOnNextCall(f);
assertEquals(undefined, f()[0]);
