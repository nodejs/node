// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(i) {
  const b = i <= i;
  return 0 + b;
}

%PrepareFunctionForOptimization(foo);
assertEquals(1, foo(5));
%OptimizeFunctionOnNextCall(foo);
assertEquals(1, foo(5));
