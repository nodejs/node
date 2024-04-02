// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --maglev

const ys = [0,1,2];

function g() {
  %CollectGarbage(42);
  return [0,1,2];
}
%NeverOptimizeFunction(g);

const o = { g: g };

function f(o) {
  // Using CallProperty since plain calls are still unimplemented.
  return o.g();
}

%PrepareFunctionForOptimization(f);
assertEquals(ys, f(o));

%OptimizeMaglevOnNextCall(f);
assertEquals(ys, f(o));
