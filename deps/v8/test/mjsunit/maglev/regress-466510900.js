// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  return Math.atan2(256, -4);
}

%PrepareFunctionForOptimization(foo);
assertEquals(1.5864200554153736, foo());
%OptimizeMaglevOnNextCall(foo);
assertEquals(1.5864200554153736, foo());
