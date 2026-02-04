// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const a = 0;

function foo(b) {
  return !(a * b);
}

%PrepareFunctionForOptimization(foo);
foo(0);
foo(0);
%OptimizeFunctionOnNextCall(foo);
assertTrue(foo(0));
