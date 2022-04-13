// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class C extends Array {};
%NeverOptimizeFunction(C);

for (let i = 0; i < 3; i++) {

  function store_global() { global = new C(); };
  store_global();
  %PrepareFunctionForOptimization(store_global);
  store_global();
  %OptimizeFunctionOnNextCall(store_global);
  store_global();

  new C(42);

  function load_global() { global.p1 = {}; global.p2 = {}; }
  if (i) {
    load_global();
    %PrepareFunctionForOptimization(load_global);
    load_global();
    %OptimizeFunctionOnNextCall(load_global);
    load_global();
  }

}
