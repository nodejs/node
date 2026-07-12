// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f() {
  return [...[, /*hole*/ 2.3]];
};
%PrepareFunctionForOptimization(f);
assertEquals(undefined, f()[0]);
assertEquals(undefined, f()[0]);
%OptimizeFunctionOnNextCall(f);
assertEquals(undefined, f()[0]);
