// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(x) {
  return x / -1;
}

%PrepareFunctionForOptimization(foo);
assertEquals(-8, foo(8));
%OptimizeMaglevOnNextCall(foo);
assertEquals(-Infinity, 1/foo(0));
