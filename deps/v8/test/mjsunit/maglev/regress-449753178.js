// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function bar(n) {
  return n / 1.0;
}
%PrepareFunctionForOptimization(bar);
bar(3.5); // Collecting HeapNumber feedback in bar.

function foo(n) {
  n += 0; // Converting n to int32
  return bar(n);
}

%PrepareFunctionForOptimization(foo);
assertEquals(1, foo(1));
assertEquals(1, foo(1));

%OptimizeMaglevOnNextCall(foo);
assertEquals(1, foo(1));
