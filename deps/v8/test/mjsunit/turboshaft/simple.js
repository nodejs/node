// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --turboshaft --allow-natives-syntax

function f(x) {
  return x.a + x.b;
}

%PrepareFunctionForOptimization(f);
assertEquals(5, f({a: 2, b: 3}));
assertEquals(7, f({a: 2, b: 5}));

%OptimizeFunctionOnNextCall(f);
assertEquals(5, f({a: 2, b: 3}));
assertEquals(7, f({a: 2, b: 5}));
