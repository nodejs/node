// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev-assert-types --turbolev

function f(x) {
  if (x.a) {
    return x.b + 1;
  }
  return x.c;
}

%PrepareFunctionForOptimization(f);
assertEquals(10, f({a: true, b: 9}));
assertEquals(30, f({a: false, b: 9, c: 30}));

%OptimizeFunctionOnNextCall(f);
assertEquals(10, f({a: true, b: 9}));
assertEquals(30, f({a: false, b: 9, c: 30}));
