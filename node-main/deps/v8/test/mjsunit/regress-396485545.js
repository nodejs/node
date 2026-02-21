// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(a) {
  a += 1;
  a += 1;
  return a;
}

%PrepareFunctionForOptimization(foo);
assertEquals(1073741825, foo(1073741823));
%OptimizeFunctionOnNextCall(foo);
assertEquals(NaN, foo());
