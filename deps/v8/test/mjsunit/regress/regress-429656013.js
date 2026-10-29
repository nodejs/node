// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

const elems = [-null, undefined, true];

function foo(x) { return [x]; }

for (const e of elems) {
  const result = foo(e);
  assertEquals(e, result[0]);
  %PrepareFunctionForOptimization(foo);
  %OptimizeMaglevOnNextCall(foo);
}
