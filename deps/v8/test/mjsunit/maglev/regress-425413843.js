// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(x) {
  let y = ~x;
  return 0 >>((y << y) / y);
}

%PrepareFunctionForOptimization(foo);
assertEquals(0, foo());
%OptimizeFunctionOnNextCall(foo);
assertEquals(0, foo());
