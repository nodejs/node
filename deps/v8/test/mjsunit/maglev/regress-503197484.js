// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

let hn3 = %AllocateHeapNumberWithValue(3);

function foo(str) {
  return str.indexOf("b", hn3);
}

%PrepareFunctionForOptimization(foo);
assertEquals(4, foo("...abc"));

%OptimizeMaglevOnNextCall(foo);
assertEquals(4, foo("...abc"));
