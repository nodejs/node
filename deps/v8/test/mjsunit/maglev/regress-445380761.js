// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --assert-types --allow-natives-syntax

function Fn(n, a) {
  if (n) {
    a = -2
  }
  const t = n * a;
  const o = (t >>> 0) + 4294967296;
  return o == 2147483648;
}

%PrepareFunctionForOptimization(Fn);
Fn(1, 0);

%OptimizeFunctionOnNextCall(Fn);
Fn(1, 0);
