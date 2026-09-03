// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function pure_idx(a, b) {
  a = a | 0;
  b = b & 0xffff;
  return ( (a % b) + 11 ) | 0;
}

%PrepareFunctionForOptimization(pure_idx);
pure_idx(0, 42);
pure_idx(123456789123, 42);

%OptimizeFunctionOnNextCall(pure_idx);
assertEquals(0, pure_idx(0xdeadbeef, 0));
