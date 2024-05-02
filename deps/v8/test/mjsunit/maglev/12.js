// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

function g() {
  %CollectGarbage(42);
  return 43;
}
%NeverOptimizeFunction(g);

const o = { g: g };

function f(o, x) {
  var y = 42;
  if (x) y = 43;
  // Using CallProperty since plain calls are still unimplemented.
  o.g();
  return y;
}

%PrepareFunctionForOptimization(f);
assertEquals(43, f(o, true));

%OptimizeMaglevOnNextCall(f);
assertEquals(43, f(o, true));
