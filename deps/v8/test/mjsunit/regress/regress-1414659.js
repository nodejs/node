// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(b) {
  return (1 / (b ? 0 : -0)) < 0;
}

%PrepareFunctionForOptimization(foo);
assertEquals(false, foo(true));
assertEquals(true, foo(false));
%OptimizeFunctionOnNextCall(foo);
assertEquals(true, foo(false));
