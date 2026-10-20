// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-future --maglev-assert-types

function f9(a10, a11) {
  let v12;
  if (a10) {
    v12 = 4294443007;
  } else {
    v12 = 3.5;
  }
  v12 | 0;
  v12 ?? 1337;
}
%PrepareFunctionForOptimization(f9);
f9();
%OptimizeFunctionOnNextCall(f9);
f9();
